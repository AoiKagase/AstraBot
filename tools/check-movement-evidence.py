# SPDX-License-Identifier: MPL-2.0
"""Run and validate asset-free P3-08 evidence. Python standard library only."""
import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import sys
import time


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                     allow_nan=False).encode()).hexdigest()


def file_hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def load(path):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "duplicate JSON key: " + key)
            result[key] = value
        return result
    require(path.stat().st_size <= 128 * 1024 * 1024, "evidence file exceeds 128 MiB")
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=pairs,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError("non-finite JSON: " + value)))


def matrix(manifest, producer):
    require(manifest.get("schemaVersion") == 1 and type(manifest["schemaVersion"]) is int,
            "unsupported manifest schema")
    require(integer(manifest.get("seed")), "missing manifest seed")
    require(producer in manifest.get("producers", {}), "unknown producer")
    definition = manifest["producers"][producer]
    require(definition.get("fixtureFiles") and definition.get("cases"), "empty producer contract")
    expected = {}
    for case in definition["cases"]:
        require(isinstance(case.get("scenario"), str) and case["scenario"], "missing scenario")
        for field in ("maxFrames", "maxQueriesPerActorFrame", "traceLimit"):
            require(integer(case.get(field), 1), "invalid manifest " + field)
        require(integer(case.get("maxReplans")), "invalid replan limit")
        for field in ("frameUs", "actors"):
            values = case.get(field)
            require(isinstance(values, list) and values and all(integer(v, 1) for v in values),
                    "invalid manifest " + field)
            require(len(set(values)) == len(values), "duplicate manifest dimension")
        require(case.get("variants"), "missing variants")
        require(isinstance(case.get("terminalOutcomes"), dict)
                and set(case["terminalOutcomes"]) == set(case["variants"]), "missing terminal contract")
        for frame, actors, variant in itertools.product(case["frameUs"], case["actors"], case["variants"]):
            require(variant in ("clean", "map-change"), "unknown variant")
            require(isinstance(case["variants"][variant], str) and case["variants"][variant], "missing expected outcome")
            terminal = case["terminalOutcomes"][variant]
            require(isinstance(terminal, dict) and terminal.get("default") in
                    ("Arrived", "ProbeFailed", "ExpansionLimit", "Unreachable", "Failed", "Aborted", "Cancelled"), "invalid terminal contract")
            key = (case["scenario"], frame, actors, variant)
            require(key not in expected, "duplicate manifest row")
            expected[key] = case
    return definition, expected


def validate_trace(row, producer, case):
    terminal = {}
    inputs = set()
    allowed = {"portable": {"input", "route", "portal", "receipt", "decision", "command", "map-cancel", "terminal"},
               "adapter": {"input", "frame", "marker", "terminal"}}[producer]
    for event in row["trace"]:
        require(isinstance(event, dict) and event.get("type") in allowed, "invalid trace event type")
        require(integer(event.get("actor"), 1) and event["actor"] <= row["actors"], "invalid trace actor")
        require(integer(event.get("tick"), 1) and event["tick"] <= row["frames"] + 1, "invalid trace tick")
        kind = event["type"]
        if kind == "input":
            require(integer(event.get("goalArea"), 1) and integer(event.get("map"), 1) and
                    isinstance(event.get("start"), list) and len(event["start"]) == 3 and
                    all(type(v) in (int, float) for v in event["start"]), "invalid scenario input")
            inputs.add(event["actor"])
        elif kind == "terminal":
            require(event.get("outcome") in ("Arrived", "ProbeFailed", "ExpansionLimit", "Unreachable", "Failed", "Aborted", "Cancelled"),
                    "invalid terminal observation")
            terminal[event["actor"]] = event["outcome"]
        elif kind == "command":
            require(integer(event.get("msec"), 1) and event["msec"] <= 255 and integer(event.get("buttons")), "invalid command")
            for name in ("movement", "view"):
                require(isinstance(event.get(name), list) and len(event[name]) == 3
                        and all(type(v) in (int, float) for v in event[name]), "invalid command vector")
        elif kind == "receipt":
            require(event.get("outcome") in ("dispatched", "map-invalidated") and
                    integer(event.get("queuedTick"), 1) and event["queuedTick"] < event["tick"], "invalid dispatch receipt")
        elif kind in ("frame", "marker"):
            require(isinstance(event.get("data"), str) and event["data"], "missing host event data")
        elif kind == "route":
            require(isinstance(event.get("edges"), list) and integer(event.get("state")) and integer(event.get("reason")), "invalid route event")
        elif kind == "decision":
            require(integer(event.get("state")) and integer(event.get("reason")) and integer(event.get("queries")), "invalid decision event")
        elif kind == "portal":
            require(isinstance(event.get("edge"), list) and len(event["edge"]) == 2, "invalid portal event")
        elif kind == "map-cancel":
            require(integer(event.get("reason")), "missing cancellation reason")
    require(set(terminal) == set(range(1, row["actors"] + 1)), "missing actor terminal observations")
    require(inputs == set(terminal), "missing actor start/goal inputs")
    contract = case["terminalOutcomes"][row["variant"]]
    for actor, outcome in terminal.items():
        require(outcome == contract.get(str(actor), contract["default"]), "actor terminal contradicts expected outcome")


def validate(manifest, evidence, root, expected_context=None):
    require(evidence.get("schemaVersion") == 1 and type(evidence["schemaVersion"]) is int,
            "unsupported evidence schema")
    definition, expected = matrix(manifest, evidence.get("producer"))
    context = evidence.get("context", {})
    if expected_context is not None:
        for field in ("revision", "dirty", "diffSha256", "manifestSha256", "fixtureHashes", "architecture", "buildOptions", "executableSha256", "compiler"):
            require(context.get(field) == expected_context.get(field), "source/build context mismatch: " + field)
    require(context.get("manifestSha256") == digest(manifest), "manifest hash mismatch")
    require(context.get("architecture") == "x86", "only x86 evidence is applicable")
    require(type(context.get("dirty")) is bool, "missing dirty-state provenance")
    require(re.fullmatch(r"[0-9a-f]{40,64}", context.get("revision", "")), "missing source revision")
    for key in ("diffSha256", "executableSha256"):
        require(re.fullmatch(r"[0-9a-f]{64}", context.get(key, "")), "invalid " + key)
    require(context.get("compiler") and context.get("platform") and context.get("buildOptions"),
            "missing build context")
    require(context["buildOptions"].get("CMAKE_BUILD_TYPE") == "Debug", "assert-based tests require Debug")
    hashes = context.get("fixtureHashes", {})
    require(set(hashes) == set(definition["fixtureFiles"]), "fixture inventory mismatch")
    for name in definition["fixtureFiles"]:
        path = (root / name).resolve()
        require(path.is_relative_to(root.resolve()) and path.is_file(), "invalid fixture path")
        require(hashes[name] == file_hash(path), "fixture hash mismatch: " + name)
    rows = evidence.get("results")
    require(isinstance(rows, list), "missing results")
    seen = set()
    for row in rows:
        require(isinstance(row, dict), "invalid result row")
        for name in ("frameUs", "actors", "frames", "elapsedUs", "traceLimit"):
            require(integer(row.get(name), 1), "invalid " + name)
        for name in ("maxQueriesPerActorFrame", "totalQueries", "replans", "seed"):
            require(integer(row.get(name)), "invalid " + name)
        key = (row.get("scenario"), row["frameUs"], row["actors"], row.get("variant"))
        require(key in expected, "unexpected result: " + str(key))
        require(key not in seen, "duplicate result: " + str(key))
        seen.add(key)
        case = expected[key]
        outcome = case["variants"][row["variant"]]
        require(row.get("outcome") == outcome and row.get("expectedOutcome") == outcome,
                "unexpected terminal outcome: " + str(key))
        require(row["seed"] == manifest["seed"], "seed mismatch")
        require(row["frames"] <= case["maxFrames"], "frame budget exceeded")
        require(row["elapsedUs"] == row["frames"] * row["frameUs"], "inconsistent simulated duration")
        require(row["maxQueriesPerActorFrame"] <= case["maxQueriesPerActorFrame"], "query budget exceeded")
        require(row["totalQueries"] <= row["maxQueriesPerActorFrame"] * row["actors"] * row["frames"],
                "inconsistent query count")
        require(row["replans"] <= case["maxReplans"], "replan budget exceeded")
        require(row.get("traceTruncated") is False and row.get("replayEqual") is True,
                "truncated or non-reproducible trace")
        trace = row.get("trace")
        require(isinstance(trace, list) and 0 < len(trace) <= row["traceLimit"] <= case["traceLimit"],
                "missing or oversized trace")
        digest(trace)  # Reject non-finite values even when called directly, without JSON parsing.
        validate_trace(row, evidence["producer"], case)
    require(seen == set(expected), "missing results: " + str(sorted(set(expected) - seen)[:5]))
    return len(rows)


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], stderr=subprocess.PIPE,
                                   env={**os.environ, "GIT_OPTIONAL_LOCKS": "0"})


def build_context(root, build, executable, manifest, producer):
    cache = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        match = re.match(r"([A-Za-z0-9_]+):[^=]+=(.*)", line)
        if match and (match[1].startswith("ASTRABOT_") or match[1] in
                      ("CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER", "CMAKE_CXX_FLAGS", "CMAKE_GENERATOR")):
            cache[match[1]] = match[2]
    header = executable.read_bytes()
    x86 = False
    if header[:2] == b"MZ" and len(header) >= 64:
        offset = struct.unpack_from("<I", header, 60)[0]
        x86 = header[offset:offset+4] == b"PE\0\0" and header[offset+4:offset+6] == b"\x4c\x01"
    elif header[:5] == b"\x7fELF\x01" and len(header) >= 20:
        x86 = header[5] == 1 and struct.unpack_from("<H", header, 18)[0] == 3
    require(x86, "replay executable is not x86 PE/ELF")
    # Refuse to relabel an old executable with a newer checkout. This is a
    # conservative freshness check in addition to content-bound run context.
    folders = ("src",) if producer == "adapter" else ("src/core", "src/host", "src/nav", "src/debug")
    sources = [p for folder in folders for p in (root / folder).rglob("*") if p.suffix in (".cpp", ".hpp", ".h")]
    sources.extend(root / name for name in manifest["producers"][producer]["fixtureFiles"])
    sources.extend((root / "CMakeLists.txt", build / "CMakeCache.txt"))
    sources.extend((root / "tests/nav").rglob("*.hpp"))
    newer = [str(p.relative_to(root)) for p in sources if p.stat().st_mtime_ns > executable.stat().st_mtime_ns]
    require(not newer, "rebuild executable after source changes: " + ", ".join(newer[:5]))
    status = git(root, "status", "--porcelain", "--untracked-files=all")
    changes = hashlib.sha256(git(root, "diff", "HEAD", "--binary"))
    for name in git(root, "ls-files", "--others", "--exclude-standard", "-z").split(b"\0"):
        if name:
            changes.update(name)
            changes.update((root / os.fsdecode(name)).read_bytes())
    compiler = []
    for path in (build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"):
        compiler.extend(line for line in path.read_text().splitlines()
                        if re.match(r"set\(CMAKE_CXX_COMPILER(_ID|_VERSION)? ", line))
    run_id = os.environ.get("GITHUB_RUN_ID")
    hosted = None
    if run_id:
        hosted = {"url": f"{os.environ.get('GITHUB_SERVER_URL', 'https://github.com')}/{os.environ.get('GITHUB_REPOSITORY', '')}/actions/runs/{run_id}",
                  "job": os.environ.get("GITHUB_JOB"), "sha": os.environ.get("GITHUB_SHA"),
                  "attempt": os.environ.get("GITHUB_RUN_ATTEMPT")}
    return {"revision": git(root, "rev-parse", "HEAD").decode().strip(), "dirty": bool(status),
            "diffSha256": changes.hexdigest(), "manifestSha256": digest(manifest),
            "fixtureHashes": {name: file_hash(root / name) for name in manifest["producers"][producer]["fixtureFiles"]},
            "platform": platform.platform(), "architecture": "x86", "buildOptions": cache,
            "executableSha256": file_hash(executable), "compiler": "\n".join(compiler), "hostedRun": hosted}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("run", "validate"))
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--producer", choices=("portable", "adapter"), default="portable")
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    try:
        manifest = load(args.manifest)
        require(args.executable and args.build_dir, "verification requires executable and build-dir")
        executable = args.executable.resolve()
        if args.mode == "run":
            args.output.parent.mkdir(parents=True, exist_ok=True)
            raw = args.output.with_suffix(".raw.json")
            raw.unlink(missing_ok=True)  # Never consume a previous run after a producer failure.
            args.output.unlink(missing_ok=True)
            context = build_context(args.root.resolve(), args.build_dir.resolve(), executable, manifest, args.producer)
            start = time.monotonic()
            with args.output.with_suffix(".log").open("w", encoding="utf-8") as log:
                result = subprocess.run([str(executable), "--output" if args.producer == "portable" else "--p308-output", str(raw.resolve())],
                                        cwd=args.build_dir.resolve(), stdout=log, stderr=subprocess.STDOUT, timeout=180)
            require(result.returncode == 0, "replay failed; inspect retained log/raw evidence")
            evidence = load(raw)
            require(evidence.get("producer") == args.producer, "producer mismatch")
            evidence["context"] = context
            evidence["wallClockSeconds"] = time.monotonic() - start
            args.output.write_text(json.dumps(evidence, separators=(",", ":"), allow_nan=False) + "\n", encoding="utf-8")
        else:
            evidence = load(args.output)
        expected_context = build_context(args.root.resolve(), args.build_dir.resolve(), executable, manifest, args.producer)
        count = validate(manifest, evidence, args.root, expected_context)
        print(f"P3-08 {evidence['producer']}: PASS ({count} complete deterministic rows); live remains unverified")
        return 0
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print("P3-08 evidence FAIL: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
