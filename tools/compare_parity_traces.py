#!/usr/bin/env python3
"""Compare normalized parity JSONL captures; see docs/parity/TRACE_REPLAY.md."""
import argparse
import json
import math
from pathlib import Path
import re
import sys

SCHEMA = "astrabot-parity-v1"
FIELDS = {
    "think": {"command_tick", "full_think_tick", "task", "state", "attack_overlay", "enemy", "nav_area", "route"},
    "observation": {"self", "weapon", "objective", "visible_enemies", "noise", "remembered_contacts"},
    "rng": {"call_sequence", "call_site", "range", "type", "value"},
    "transition": {"old_state", "new_state", "task_before", "task_after", "reason", "side_effects"},
    "command": {"forwardmove", "sidemove", "upmove", "buttons", "view", "msec", "action"},
    "path": {"source_area", "target_area", "target_position", "route_type", "areas", "transitions", "cost"},
}
# Explicit scalar paths only: no wildcard, RNG, identity, flags, or configuration tolerance.
PRECISION_PATHS = {"/timestamp", "/data/cost", "/data/forwardmove", "/data/sidemove", "/data/upmove"}
PRECISION_PATHS.update("/data/view/" + axis for axis in ("pitch", "yaw", "roll"))
PRECISION_PATHS.update("/data/target_position/" + axis for axis in ("x", "y", "z"))
PRECISION_PATHS.update("/data/self/" + field + "/" + axis
                       for field in ("origin", "velocity") for axis in ("x", "y", "z"))
PRECISION_PATHS.update("/data/self/view/" + axis for axis in ("pitch", "yaw", "roll"))
PRECISION_PATHS.add("/data/self/fov")


class TraceError(ValueError):
    pass


def _integer(value, minimum=0):
    return type(value) is int and value >= minimum


def _finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise TraceError("duplicate JSON key: " + key)
        result[key] = value
    return result


def _reject_constant(value):
    raise TraceError("non-finite JSON number: " + value)


def _check_finite(value):
    if isinstance(value, float) and not math.isfinite(value):
        raise TraceError("non-finite number")
    if isinstance(value, dict):
        for child in value.values():
            _check_finite(child)
    elif isinstance(value, list):
        for child in value:
            _check_finite(child)


def load_trace(path):
    """Return validated metadata and ordered records; reject ambiguous captures."""
    rows = []
    with Path(path).open(encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            if not line.strip():
                raise TraceError(f"{path}:{line_number}: blank JSONL record")
            try:
                row = json.loads(line, object_pairs_hook=_unique_object,
                                 parse_constant=_reject_constant)
                if not isinstance(row, dict):
                    raise TraceError("record must be an object")
                _check_finite(row)
            except (ValueError, OverflowError) as error:
                raise TraceError(f"{path}:{line_number}: {error}") from error
            rows.append(row)
    if len(rows) < 2:
        raise TraceError(f"{path}: metadata and at least one event are required")
    metadata = rows[0]
    required = {"kind", "schema", "capture_kind", "reference_pin", "map", "scenario",
                "profiles", "cvars", "rng_seed", "identity_settings", "coverage"}
    if not required <= metadata.keys() or metadata.get("kind") != "metadata" or metadata.get("schema") != SCHEMA:
        raise TraceError(f"{path}: invalid {SCHEMA} metadata")
    if metadata["capture_kind"] not in ("live", "fixture"):
        raise TraceError(f"{path}: capture_kind must be live or fixture")
    if not isinstance(metadata["reference_pin"], str) or not re.fullmatch(r"[0-9a-f]{40}", metadata["reference_pin"]):
        raise TraceError(f"{path}: reference_pin must be a full lowercase Git SHA")
    if not all(isinstance(metadata[name], dict) for name in ("profiles", "cvars", "identity_settings")):
        raise TraceError(f"{path}: profiles, cvars, identity_settings must be objects")
    if not _integer(metadata["rng_seed"]):
        raise TraceError(f"{path}: rng_seed must be a nonnegative integer")
    if not all(isinstance(metadata[name], str) and metadata[name] for name in ("map", "scenario")):
        raise TraceError(f"{path}: nonempty map and scenario are required")
    coverage = metadata["coverage"]
    if not isinstance(coverage, list) or not coverage or any(not isinstance(kind, str) or kind not in FIELDS for kind in coverage) or len(set(coverage)) != len(coverage):
        raise TraceError(f"{path}: coverage must list distinct supported record kinds")
    records, ids, kinds = rows[1:], set(), set()
    sequence = -1
    for offset, row in enumerate(records, 2):
        prefix = f"{path}:{offset}"
        required = {"id", "kind", "sequence", "timestamp", "actor", "frame", "data"}
        if not required <= row.keys() or not isinstance(row.get("kind"), str) or row["kind"] not in FIELDS:
            raise TraceError(prefix + ": invalid event envelope")
        if not isinstance(row["id"], str) or not row["id"] or row["id"] in ids:
            raise TraceError(prefix + ": event id must be nonempty and unique")
        if not _integer(row["sequence"]) or row["sequence"] <= sequence:
            raise TraceError(prefix + ": sequence must increase strictly")
        if not _finite(row["timestamp"]) or row["timestamp"] < 0:
            raise TraceError(prefix + ": invalid timestamp")
        if row["actor"] is not None and (not isinstance(row["actor"], str) or not re.fullmatch(r"[1-9][0-9]*:[1-9][0-9]*", row["actor"])):
            raise TraceError(prefix + ": actor must be slot:generation or null for global events")
        frame = row["frame"]
        if not isinstance(frame, dict) or set(frame) != {"map_generation", "round_generation", "tick"} or not all(_integer(frame[key], 0 if key == "tick" else 1) for key in frame):
            raise TraceError(prefix + ": invalid frame identity")
        if not isinstance(row["data"], dict) or not FIELDS[row["kind"]] <= row["data"].keys():
            raise TraceError(prefix + ": missing required " + row["kind"] + " data")
        ids.add(row["id"])
        kinds.add(row["kind"])
        sequence = row["sequence"]
    if kinds != set(coverage):
        raise TraceError(f"{path}: events must match declared coverage exactly")
    return metadata, records


def validate_precision(precision, reasons):
    if set(precision) != set(reasons):
        raise TraceError("every precision override requires exactly one matching precision reason")
    for path, epsilon in precision.items():
        if path not in PRECISION_PATHS or not _finite(epsilon) or epsilon <= 0:
            raise TraceError("unsupported precision path or nonpositive/non-finite epsilon: " + path)
        if not isinstance(reasons[path], str) or not reasons[path].strip():
            raise TraceError("precision reason must be nonempty: " + path)


def _differences(left, right, path, precision, findings, event=None):
    def emit():
        finding = {"type": "value", "path": path, "reference": left, "candidate": right}
        if event is not None:
            finding["event"] = event
        findings.append(finding)
    if path in precision and _finite(left) and _finite(right):
        if abs(left - right) > precision[path]:
            emit()
    elif type(left) is not type(right):
        emit()
    elif isinstance(left, dict):
        for key in sorted(set(left) | set(right)):
            child = path + "/" + key.replace("~", "~0").replace("/", "~1")
            if key not in left or key not in right:
                findings.append({"type": "field", "event": event, "path": child,
                                 "missing_from": "reference" if key not in left else "candidate"})
            else:
                _differences(left[key], right[key], child, precision, findings, event)
    elif isinstance(left, list):
        if len(left) != len(right):
            emit()
        else:
            for index, (a, b) in enumerate(zip(left, right)):
                _differences(a, b, path + "/" + str(index), precision, findings, event)
    elif left != right:
        emit()


def compare_traces(reference, candidate, precision=None, reasons=None):
    precision, reasons = precision or {}, reasons or {}
    validate_precision(precision, reasons)
    reference_meta, reference_records = load_trace(reference)
    candidate_meta, candidate_records = load_trace(candidate)
    findings = []
    _differences(reference_meta, candidate_meta, "/metadata", {}, findings)
    left = {record["id"]: record for record in reference_records}
    right = {record["id"]: record for record in candidate_records}
    for event in sorted(left.keys() - right.keys()):
        findings.append({"type": "missing", "event": event})
    for event in sorted(right.keys() - left.keys()):
        findings.append({"type": "extra", "event": event})
    common_left = [record["id"] for record in reference_records if record["id"] in right]
    common_right = [record["id"] for record in candidate_records if record["id"] in left]
    if common_left != common_right:
        findings.append({"type": "reorder", "reference": common_left, "candidate": common_right})
    for event in common_left:
        _differences(left[event], right[event], "", precision, findings, event)
    complete = set(reference_meta["coverage"]) == set(FIELDS) and set(candidate_meta["coverage"]) == set(FIELDS)
    return {"schema": SCHEMA, "match": not findings, "findings": findings,
            "reference_records": len(reference_records), "candidate_records": len(candidate_records),
            "precision": {path: {"absolute_epsilon": precision[path], "reason": reasons[path]} for path in sorted(precision)},
            "capture_kind": reference_meta["capture_kind"], "complete_record_classes": complete,
            "live_parity_accepted": False,
            "acceptance_note": "Comparison only; native provenance, production collector integration and scenario acceptance remain separate open gates."}


def _assignments(values, numeric=False):
    result = {}
    for value in values:
        path, separator, raw = value.partition("=")
        if not separator or path in result:
            raise TraceError("expected a unique PATH=VALUE: " + value)
        result[path] = float(raw) if numeric else raw
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--precision", action="append", default=[], metavar="PATH=EPSILON")
    parser.add_argument("--precision-reason", action="append", default=[], metavar="PATH=EVIDENCE")
    args = parser.parse_args(argv)
    try:
        report = compare_traces(args.reference, args.candidate,
                                _assignments(args.precision, True), _assignments(args.precision_reason))
    except (OSError, ValueError, OverflowError) as error:
        print(json.dumps({"error": str(error)}, sort_keys=True), file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2, sort_keys=True, allow_nan=False))
    return 0 if report["match"] else 1


if __name__ == "__main__":
    sys.exit(main())
