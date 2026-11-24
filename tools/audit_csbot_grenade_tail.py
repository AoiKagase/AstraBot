#!/usr/bin/env python3
"""Run pinned CSBot path/toss bodies in a checked, deterministic offline host.

This is a reference audit, not an AstraBot parity or HLDS acceptance test.
Run from the MSVC x86 developer environment. An existing output is never reused.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

PIN = "b0889847fe6d03898be88acc9e366660efb40ab5"
ROOT = Path(__file__).resolve().parents[1]
DRIVER = ROOT / "tests/fixtures/csbot_grenade_tail/reference_driver.cpp"


def run(command, **kwargs):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
    if result.returncode:
        raise RuntimeError(f"{command!r}: exit {result.returncode}\n{result.stdout.decode(errors='replace')}")
    return result.stdout


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cxx", default="cl")
    args = parser.parse_args()
    if os.environ.get("VSCMD_ARG_TGT_ARCH") != "x86":
        parser.error("run in the MSVC x86 developer environment")
    args.output.mkdir(parents=True, exist_ok=False)
    hashes = {}

    def source(path):
        data = run(["git", "-C", str(args.reference), "show", f"{PIN}:{path}"])
        hashes[path] = sha(data)
        return data.decode("utf-8")

    def function(text, anchor):
        if text.count(anchor) != 1:
            raise ValueError(f"non-unique reference anchor: {anchor}")
        start = text.index(anchor)
        end = text.index("\n}", start) + 2
        return text[start:end]

    base = "regamedll/dlls/bot/"
    header = source(base + "cs_bot.h")
    pathfind = source(base + "cs_bot_pathfind.cpp")
    functions = [
        function(header, "inline bool CCSBot::HasPath() const"),
        function(header, "inline void CCSBot::DestroyPath()"),
        function(pathfind, "bool CCSBot::ComputePathPositions()"),
        function(pathfind, "void CCSBot::BuildTrivialPath(const Vector *goal)"),
        function(pathfind, "bool CCSBot::ComputePath(CNavArea *goalArea, const Vector *goal, RouteType route)"),
        function(source(base + "cs_bot.cpp"), "void CCSBot::RemovePath(CNavArea *area)"),
        function(source(base + "cs_bot_weapon.cpp"), "bool CCSBot::FindGrenadeTossPathTarget(Vector *pos)"),
    ]
    init = source(base + "cs_bot_init.cpp")
    reset = function(init, "void CCSBot::ResetValues()")
    constructor = function(init, "CCSBot::CCSBot()")
    (args.output / "constructor.txt").write_text(constructor, encoding="utf-8")
    (args.output / "pinned-mathlib.h").write_text(source("regamedll/common/mathlib.h"), encoding="utf-8")
    # Keep the complete reset body as static evidence: it clears counters but
    # neither writes m_path slots nor bulk-clears the enclosing object.
    (args.output / "reset-values.txt").write_text(reset, encoding="utf-8")
    if "m_pathLength = 0;" not in reset or "m_pathIndex = 0;" not in reset or "m_path[" in reset:
        raise ValueError("ResetValues path reset assumption changed")
    vector = source("regamedll/dlls/vector.h")
    code = "\n\n".join(functions) + "\n"
    (args.output / "pinned-vector.h").write_text(vector, encoding="utf-8")
    (args.output / "pinned-functions.inc").write_text(code, encoding="utf-8")
    executable = (args.output / "reference.exe").resolve()
    command = [args.cxx, "/nologo", "/std:c++14", "/EHsc", "/Od", "/fp:precise",
               "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(args.output.resolve()),
               str(DRIVER), "/Fe:" + str(executable),
               "/Fo:" + str((args.output / "reference.obj").resolve())]
    def logged(command, name):
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (args.output / name).write_bytes(result.stdout)
        if result.returncode:
            raise RuntimeError(f"exit {result.returncode}; see {args.output / name}")
        return result.stdout

    logged(command, "build.log")
    raw = logged([str(executable)], "reference.jsonl")
    rows = [json.loads(line) for line in raw.decode().splitlines()]
    expected = {"no-path", "fresh-trivial", "fresh-multi", "reused-trivial", "destroy-rebuild",
                "equal-tail", "hidden-first", "hidden-tail-segment", "remove-clears-tail",
                "max-all-visible", "max-first-hidden",
                "failed-up-fresh", "failed-up-fresh-rebuild",
                "failed-down-fresh", "failed-down-fresh-rebuild",
                "failed-up-reused", "failed-up-reused-rebuild",
                "failed-up-early", "failed-up-early-rebuild"}
    if {row["case"] for row in rows} != expected or len(rows) != len(expected):
        raise ValueError("missing or duplicate reference case")
    # This comparison deliberately falsifies a tail-independent endpoint policy.
    by_name = {row["case"]: row for row in rows}
    if by_name["fresh-trivial"]["target"] == by_name["reused-trivial"]["target"]:
        raise ValueError("history-dependence control failed")
    for name in ("failed-up-fresh", "failed-down-fresh", "failed-up-reused"):
        if by_name[name]["length"] != 0 or by_name[name + "-rebuild"]["target"] != [900, 0, 46]:
            raise ValueError("failed position-write history control failed: " + name)
    if by_name["failed-up-early-rebuild"]["target"] != by_name["reused-trivial"]["target"]:
        raise ValueError("unwritten suffix preservation control failed")
    manifest = {
        "reference_pin": PIN, "reference_source_sha256": hashes,
        "excerpt_sha256": sha(code.encode()), "driver_sha256": sha(DRIVER.read_bytes()),
        "tool_sha256": sha(Path(__file__).read_bytes()), "executable_sha256": sha(executable.read_bytes()),
        "compiler_path": run(["where.exe", args.cxx]).decode().splitlines(),
        "compiler_arch": os.environ.get("VSCMD_ARG_TGT_ARCH"),
        "command": command, "cases": len(rows), "all_cases_passed": True,
        "tail_independent_hypothesis": "refuted: same current trivial path, different previous path",
        "success_only_history_hypothesis": "refuted: failed ComputePath writes prefix before DestroyPath; trivial rebuild exposes it",
        "failure_input_scope": "Scripted search selects a ladder parent edge absent from its source list. Proves pinned failure semantics, not reachability through native NAV search or Astra public inputs.",
        "scope": "Pinned bodies and Vector, scripted flat NAV/visibility/trace host; checked slot 256 traps before any UB. No full GameDLL, no live acceptance, no product parity claim.",
        "static_reset_evidence": "reset-values.txt: counters reset, path slots retained",
        "static_initialization_evidence": "constructor.txt and pinned-vector.h: empty CCSBot constructor body; Vector default constructor zeros positions. Host real_t=float is the non-PLAY_GAMEDLL mathlib branch.",
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"reference_cases_passed": len(rows), "tail_independent_hypothesis": "refuted"}))


if __name__ == "__main__":
    main()
