#!/usr/bin/env python3

import copy
import json
import math
import sys
from pathlib import Path


MAX_RECORDS = 256
MAX_ACTORS = 32
MAX_GENERATION = 65535
MAX_TICK = 1_000_000_000
MAX_ELAPSED_MS = 120_000.0
MAX_STRING_LENGTH = 64

FORBIDDEN_KEYS = {
    "damage_applied",
    "engine_success",
    "hidden_enemy_coordinates",
    "message_delivered",
    "omniscient_state",
    "private_symbol",
    "server_receipt",
}

EXPECTED_REFERENCE_COMMIT = "b0889847fe6d03898be88acc9e366660efb40ab5"
EXPECTED_CANDIDATE_COMMIT = "5a62a98243ce98081ebd64d24c5495295f53ec58"
EXPECTED_OBSERVABLES = {
    "behavior",
    "combat",
    "lifecycle",
    "objective",
    "perception",
    "team",
}


class DifferentialError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise DifferentialError(message)


def reject_forbidden(value, path="root"):
    if isinstance(value, dict):
        for key, nested in value.items():
            require(key not in FORBIDDEN_KEYS, f"{path}.{key} is forbidden")
            reject_forbidden(nested, f"{path}.{key}")
    elif isinstance(value, list):
        for index, nested in enumerate(value):
            reject_forbidden(nested, f"{path}[{index}]")
    elif isinstance(value, float):
        require(math.isfinite(value), f"{path} must be finite")


def validate_string(value, name):
    require(isinstance(value, str), f"{name} must be a string")
    require(0 < len(value) <= MAX_STRING_LENGTH, f"{name} length is out of bounds")
    return value


def validate_uint(value, name, maximum):
    require(isinstance(value, int) and not isinstance(value, bool), f"{name} must be an integer")
    require(0 <= value <= maximum, f"{name} is out of bounds")
    return value


def validate_finite(value, name, maximum):
    require(isinstance(value, (int, float)) and not isinstance(value, bool), f"{name} must be numeric")
    require(math.isfinite(value), f"{name} must be finite")
    require(0.0 <= value <= maximum, f"{name} is out of bounds")
    return value


def validate_record_identity(value, name):
    require(isinstance(value, dict), f"{name} must be an object")
    map_generation = validate_uint(value.get("map_generation"), f"{name}.map_generation", MAX_GENERATION)
    round_generation = validate_uint(value.get("round_generation"), f"{name}.round_generation", MAX_GENERATION)
    tick = validate_uint(value.get("tick"), f"{name}.tick", MAX_TICK)
    elapsed_ms = validate_finite(value.get("elapsed_ms"), f"{name}.elapsed_ms", MAX_ELAPSED_MS)
    actor = value.get("actor")
    require(isinstance(actor, dict), f"{name}.actor must be an object")
    slot = validate_uint(actor.get("slot"), f"{name}.actor.slot", MAX_ACTORS)
    generation = validate_uint(actor.get("generation"), f"{name}.actor.generation", MAX_GENERATION)
    require(slot > 0 and generation > 0, f"{name}.actor must be connected")
    return (map_generation, round_generation, tick, slot, generation, elapsed_ms)


def validate_observables(value, name):
    require(isinstance(value, dict), f"{name} must be an object")
    require(set(value) == EXPECTED_OBSERVABLES, f"{name} has an unexpected observable set")

    lifecycle = value["lifecycle"]
    require(isinstance(lifecycle, dict), f"{name}.lifecycle must be an object")
    require(set(lifecycle) == {"state"}, f"{name}.lifecycle fields are invalid")
    require(lifecycle["state"] in {"Connecting", "Active", "Dead", "Disconnected"}, f"{name}.lifecycle.state is invalid")

    behavior = value["behavior"]
    require(isinstance(behavior, dict), f"{name}.behavior must be an object")
    require(set(behavior) == {"state", "transition"}, f"{name}.behavior fields are invalid")
    require(behavior["state"] in {"Roam", "Engage", "Retreat", "Recover"}, f"{name}.behavior.state is invalid")
    validate_string(behavior["transition"], f"{name}.behavior.transition")

    objective = value["objective"]
    require(isinstance(objective, dict), f"{name}.objective must be an object")
    require(set(objective) == {"kind", "status"}, f"{name}.objective fields are invalid")
    require(objective["kind"] in {"None", "Attack", "Defend", "Bomb", "Hostage"}, f"{name}.objective.kind is invalid")
    require(objective["status"] in {"Unknown", "Proposed", "Active", "Complete", "Failed"}, f"{name}.objective.status is invalid")

    perception = value["perception"]
    require(isinstance(perception, dict), f"{name}.perception must be an object")
    require(set(perception) == {"target_state"}, f"{name}.perception fields are invalid")
    require(perception["target_state"] in {"Unknown", "ObservedAbsent", "ObservedPresent"}, f"{name}.perception.target_state is invalid")

    combat = value["combat"]
    require(isinstance(combat, dict), f"{name}.combat must be an object")
    require(set(combat) == {"aim_intent", "fire_intent", "reload_intent"}, f"{name}.combat fields are invalid")
    require(all(isinstance(combat[field], bool) for field in combat), f"{name}.combat values must be boolean")

    team = value["team"]
    require(isinstance(team, dict), f"{name}.team must be an object")
    require(set(team) == {"radio_intent"}, f"{name}.team fields are invalid")
    require(team["radio_intent"] in {"None", "EnemySpotted", "NeedBackup", "GoingToPlant", "Defusing"}, f"{name}.team.radio_intent is invalid")


def validate_identity_sequence(trace, name):
    require(isinstance(trace, list) and 0 < len(trace) <= MAX_RECORDS, f"{name} length is out of bounds")
    previous = None
    identities = []
    for index, record in enumerate(trace):
        require(isinstance(record, dict), f"{name}[{index}] must be an object")
        require(set(record) == {"identity", "observables"}, f"{name}[{index}] fields are invalid")
        identity = validate_record_identity(record["identity"], f"{name}[{index}].identity")
        validate_observables(record["observables"], f"{name}[{index}].observables")
        order_key = identity[:5]
        if previous is not None:
            require(order_key[:2] == previous[:2], f"{name}[{index}] changes map or round without a boundary")
            require(order_key[3:] == previous[3:5], f"{name}[{index}] changes actor identity within a trace")
            require(identity[2] > previous[2], f"{name}[{index}] is stale or duplicated")
            require(identity[5] >= previous[5], f"{name}[{index}] elapsed time moves backwards")
        previous = identity
        identities.append(order_key)
    return identities


def validate_identity(value, name, expected_commit):
    require(isinstance(value, dict), f"{name} must be an object")
    require(value.get("runtime") in {"AstraBot", "ReGameDLL-CS CSBot"}, f"{name}.runtime is invalid")
    require(value.get("commit") == expected_commit, f"{name}.commit is not pinned")
    require(value.get("capture_status") in {"synthetic-contract", "live-captured"}, f"{name}.capture_status is invalid")
    validate_string(value.get("trace_id"), f"{name}.trace_id")


def validate_fixture(data):
    require(isinstance(data, dict), "fixture must be an object")
    reject_forbidden(data)
    require(data.get("schema_version") == 1, "fixture schema_version is unsupported")
    validate_string(data.get("fixture_id"), "fixture_id")

    provenance = data.get("provenance")
    require(isinstance(provenance, dict), "provenance must be an object")
    require(provenance.get("kind") == "synthetic-contract", "provenance.kind must identify a synthetic contract")
    require(provenance.get("source_independent") is True, "provenance must be source independent")
    require(provenance.get("reference_policy") == "behavior-only-pinned", "provenance.reference_policy is invalid")

    validate_identity(data.get("reference_identity"), "reference_identity", EXPECTED_REFERENCE_COMMIT)
    validate_identity(data.get("candidate_identity"), "candidate_identity", EXPECTED_CANDIDATE_COMMIT)
    reference_ids = validate_identity_sequence(data.get("reference_trace"), "reference_trace")
    candidate_ids = validate_identity_sequence(data.get("candidate_trace"), "candidate_trace")
    require(reference_ids == candidate_ids, "candidate and reference identities do not align")

    expected = data.get("expected")
    require(isinstance(expected, dict), "expected must be an object")
    require(expected.get("observable_match") is True, "expected.observable_match must be true")
    require(expected.get("live_acceptance") == "pending-reference-capture", "expected.live_acceptance must remain pending")
    return reference_ids


def compare_observables(reference, candidate, path, differences):
    if isinstance(reference, dict):
        require(set(reference) == set(candidate), f"{path} observable fields do not align")
        for key in sorted(reference):
            compare_observables(reference[key], candidate[key], f"{path}.{key}", differences)
        return
    if reference != candidate:
        differences.append({"path": path, "reference": reference, "candidate": candidate})


def replay(data):
    validate_fixture(data)
    differences = []
    for index, (reference, candidate) in enumerate(zip(data["reference_trace"], data["candidate_trace"])):
        compare_observables(reference["observables"], candidate["observables"], f"trace[{index}].observables", differences)
    return {
        "status": "match" if not differences else "mismatch",
        "differences": differences,
        "live_acceptance": "accepted" if data["reference_identity"]["capture_status"] == "live-captured" else "pending-reference-capture",
    }


def expect_rejected(data, description):
    try:
        replay(data)
    except DifferentialError:
        return
    raise DifferentialError(f"negative case was accepted: {description}")


def run_self_checks(root):
    fixture_path = root / "tests" / "fixtures" / "phase8" / "differential-observable.json"
    try:
        data = json.loads(fixture_path.read_text(encoding="utf-8"), parse_constant=lambda value: (_ for _ in ()).throw(DifferentialError(f"non-finite JSON constant: {value}")))
    except (OSError, UnicodeError, json.JSONDecodeError, DifferentialError) as error:
        raise DifferentialError(f"cannot read {fixture_path}: {error}")

    result = replay(data)
    require(result == {"status": "match", "differences": [], "live_acceptance": "pending-reference-capture"}, "positive replay result is not deterministic")

    missing_reference_commit = copy.deepcopy(data)
    del missing_reference_commit["reference_identity"]["commit"]
    expect_rejected(missing_reference_commit, "missing reference provenance")

    hidden_state = copy.deepcopy(data)
    hidden_state["candidate_trace"][0]["observables"]["hidden_enemy_coordinates"] = [1.0, 2.0, 3.0]
    expect_rejected(hidden_state, "hidden state")

    stale_actor = copy.deepcopy(data)
    stale_actor["candidate_trace"][1]["identity"]["actor"]["generation"] += 1
    expect_rejected(stale_actor, "actor generation drift")

    non_finite = copy.deepcopy(data)
    non_finite["candidate_trace"][0]["identity"]["elapsed_ms"] = float("nan")
    expect_rejected(non_finite, "non-finite timing")

    unbounded = copy.deepcopy(data)
    unbounded["candidate_trace"] *= MAX_RECORDS + 1
    expect_rejected(unbounded, "unbounded trace")

    identity_drift = copy.deepcopy(data)
    identity_drift["candidate_trace"][1]["identity"]["tick"] += 1
    expect_rejected(identity_drift, "candidate/reference identity drift")

    mismatch = copy.deepcopy(data)
    mismatch["candidate_trace"][1]["observables"]["combat"]["fire_intent"] = True
    mismatch_result = replay(mismatch)
    require(mismatch_result["status"] == "mismatch", "allowed observable mismatch was not reported")
    require(mismatch_result["differences"][0]["path"] == "trace[1].observables.combat.fire_intent", "mismatch path is not deterministic")
    return result


def main():
    root = Path(__file__).resolve().parents[1]
    try:
        result = run_self_checks(root)
    except DifferentialError as error:
        print(f"phase8 differential: FAILED: {error}", file=sys.stderr)
        return 1
    print("phase8 differential: OK (1 synthetic contract match, 6 rejection cases, TEST-03 pending reference capture)")
    return 0 if result["status"] == "match" else 1


if __name__ == "__main__":
    sys.exit(main())
