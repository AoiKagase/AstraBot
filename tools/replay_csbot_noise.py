#!/usr/bin/env python3
"""Execute pinned CSBot hearing code and replay the same inputs in AstraBot."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

from compare_parity_traces import compare_traces

PIN = "b0889847fe6d03898be88acc9e366660efb40ab5"
ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "tests/fixtures/csbot_noise_replay"
NATIVE = "regamedll/dlls/bot/"


def execute(command, **kwargs):
    return subprocess.run(command, check=True, stdout=subprocess.PIPE,
                          env=dict(os.environ, LC_ALL="C"), **kwargs).stdout


def digest(content):
    return hashlib.sha256(content).hexdigest()


def native_code(reference):
    hashes = {}

    def source(relative):
        path = NATIVE + relative
        data = execute(["git", "-C", str(reference), "show", f"{PIN}:{path}"])
        hashes[path] = digest(data)
        return data.decode()

    def body(text, start):
        if text.count(start) != 1:
            raise ValueError("ambiguous reference anchor: " + start)
        begin = text.index(start)
        end = text.index("\n}", begin) + 2
        return text[begin:end]

    header = source("cs_bot.h")
    event = body(source("cs_bot_event.cpp"), "const Vector *newNoisePosition = &pPlayer->pev->origin;")
    listen = source("cs_bot_listen.cpp")
    vision = source("cs_bot_vision.cpp")
    look_start = vision.index("void CCSBot::UpdateLookAround(bool updateNow)")
    look_start = vision.index("{", look_start)
    look_end = vision.index("// If we recently saw an enemy", look_start)
    functions = [
        body(header, "inline bool CCSBot::IsNoiseHeard() const"),
        body(header, "inline void CCSBot::ForgetNoise()"),
        body(source("cs_bot.cpp"), "bool CCSBot::IsWellPastSafe() const"),
        body(listen, "bool CCSBot::ShouldInvestigateNoise(float *retNoiseDist)"),
        body(listen, "bool CCSBot::CanHearNearbyEnemyGunfire(float range) const"),
        body(vision, "void CCSBot::InhibitLookAround(float duration)"),
        "void CCSBot::replayStartle()\n" + vision[look_start:look_end].rstrip() + "\n}",
        body(source("states/cs_bot_investigate_noise.cpp"), "void InvestigateNoiseState::AttendCurrentNoise(CCSBot *me)"),
        "void CCSBot::deliver(Player *pPlayer, int event, PriorityType priority, float range)\n{\n" + event,
    ]
    return "\n\n".join(functions) + "\n", hashes


def normalize(raw, destination, fixture_bytes):
    rows = [{"kind": "metadata", "schema": "astrabot-parity-v1", "capture_kind": "fixture",
             "reference_pin": PIN, "scenario": "hearing, investigation and startle", "map": "flat_nav_fixture",
             "profiles": {"1:1": {"aggression": 0.5, "reaction_seconds": 0.125}},
             "cvars": {}, "rng_seed": 0, "rng_mode": "scripted engine callbacks",
             "input_sha256": digest(fixture_bytes), "identity_settings": {"actors": "fixture slot:generation"},
             "coverage": ["observation", "rng", "think", "transition"]}]
    tick, timestamp, call, occurrences = 0, 0.0, 0, Counter()
    for line in raw.decode().splitlines():
        tag, *parts = line.split()
        if tag == "tick":
            new_tick, timestamp = int(parts[0]), float(parts[1])
            if new_tick != tick + 1:
                raise ValueError("fixture tick out of order")
            tick = new_tick
            occurrences.clear()
            continue
        if tick == 0:
            raise ValueError("callback before fixture tick")
        if tag in {"event", "memory", "look-input"}:
            kind = "observation"
            if tag == "event":
                x, priority = parts
                noise = {"delivered": True, "source_x": float(x), "priority": int(priority)}
            elif tag == "memory":
                active, ready, heard, x, priority = parts
                present = active == "1"
                noise = {"active": present, "ready": ready == "1", "heard_at": float(heard) if present else None,
                         "localized_x": float(x) if present else None, "priority": int(priority) if present else None}
            else:
                x, clear = parts
                noise = {"source_x": float(x), "clear_line": clear == "1", "priority": 3}
            data = {"self": {"origin": {"x": 0.0, "y": 0.0, "z": 0.0}}, "weapon": None,
                    "objective": None, "visible_enemies": [], "noise": noise, "remembered_contacts": []}
        elif tag == "rng":
            kind = "rng"
            site, lower, upper, value = parts
            if site not in {"noise.position.x", "noise.position.y", "noise.investigate", "NoiseLook.StartleInhibit"}:
                raise ValueError("unexpected RNG identity: " + site)
            call += 1
            data = {"call_sequence": call, "call_site": site, "range": [float(lower), float(upper)],
                    "type": "float", "value": float(value)}
            tag = site
        elif tag in {"decision", "look"}:
            kind = "think"
            data = {"command_tick": None, "full_think_tick": None, "state": None, "task": None,
                    "attack_overlay": None, "enemy": None, "nav_area": 1, "route": None}
            if tag == "decision":
                data["attend_noise"] = parts == ["1"]
            else:
                data["look_requested"] = parts[0] == "1"
                data["inhibit_until"] = float(parts[1])
        elif tag == "consume":
            kind = "transition"
            data = {"old_state": "noise.remembered", "new_state": "noise.consumed" if parts == ["1"] else "noise.remembered",
                    "task_before": None, "task_after": None, "reason": "noise.attend", "side_effects": []}
        else:
            raise ValueError("unexpected capture record: " + tag)
        occurrences[tag] += 1
        rows.append({"id": f"1:1/{tick}/{tag}/{occurrences[tag]}", "sequence": len(rows), "kind": kind,
                     "timestamp": timestamp, "actor": "1:1",
                     "frame": {"map_generation": 1, "round_generation": 1, "tick": tick}, "data": data})
    expected_ticks = sum(bool(line.strip()) and not line.startswith(b"#") for line in fixture_bytes.splitlines())
    if tick != expected_ticks or sum(row["kind"] == "think" for row in rows) != expected_ticks:
        raise ValueError("capture does not contain one decision per input tick")
    destination.write_text("".join(json.dumps(row, allow_nan=False) + "\n" for row in rows), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--core-library", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    code, source_hashes = native_code(args.reference)
    input_bytes = (FIXTURE / "input.txt").read_bytes()
    library = args.core_library.resolve()
    if not library.is_file():
        parser.error("build the current Linux x86 astrabot_core first")
    args.output.mkdir(parents=True)
    manifest = {"reference_pin": PIN, "reference_source_sha256": source_hashes,
                "reference_excerpt_sha256": digest(code.encode()),
                "input_sha256": digest(input_bytes), "core_library_sha256": digest(library.read_bytes()),
                "driver_sha256": {name: digest((FIXTURE / name).read_bytes())
                                  for name in ("reference_driver.cpp", "astrabot_driver.cpp")},
                "compiler": execute([args.cxx, "--version"]).decode().splitlines()[0],
                "candidate_commit": execute(["git", "-C", str(ROOT), "rev-parse", "HEAD"]).decode().strip(),
                "candidate_dirty": bool(execute(["git", "-C", str(ROOT), "diff", "--name-only", "HEAD"])),
                "scope": "native function bodies compiled with a deterministic test host, no GameDLL or HLDS runtime"}
    with tempfile.TemporaryDirectory(prefix="astrabot-csbot-replay-") as temporary:
        working = Path(temporary)
        (working / "csbot_noise_reference.inc").write_text(code, encoding="utf-8")
        flags = [args.cxx, "-std=c++14", "-m32", "-O0"]
        for name, tail in (
            ("reference", ["-DREGAMEDLL_FIXES", "-I", str(working), str(FIXTURE / "reference_driver.cpp")]),
            ("astrabot", ["-I", str(ROOT / "include"), str(FIXTURE / "astrabot_driver.cpp"), str(library)]),
        ):
            executable = working / name
            execute([*flags, *tail, "-o", str(executable)])
            manifest[name + "_executable_sha256"] = digest(executable.read_bytes())
            raw = execute([str(executable)], input=input_bytes)
            (args.output / (name + ".txt")).write_bytes(raw)
            normalize(raw, args.output / (name + ".jsonl"), input_bytes)
    result = compare_traces(args.output / "reference.jsonl", args.output / "astrabot.jsonl")
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    status = "match" if result["match"] else "different"
    print(f"{status}: {args.output / 'comparison.json'}")
    return 0 if result["match"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
