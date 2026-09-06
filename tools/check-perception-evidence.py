# SPDX-License-Identifier: MPL-2.0
"""P4-09 offline evidence: measured snapshots, independent expectations, repeatability."""
import argparse
import copy
import importlib.util
import itertools
import json
import math
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("movement_evidence", Path(__file__).with_name("check-movement-evidence.py"))
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
require = common.require

LIMITS = {"processed": 1056, "visualVisits": 992, "soundVisits": 512,
          "reportVisits": 992, "deliveries": 32, "reportQueue": 256,
          "connections": 256, "mappings": 32, "distributionVisits": 2048,
          "traces": 248, "audience": 1024, "soundQueue": 256}


def validate(manifest, evidence, context):
    require(type(evidence.get("schemaVersion")) is int and evidence["schemaVersion"] == 1, "schema")
    require(evidence.get("context") == context, "source/build context changed")
    require(context["architecture"] == "x86" and context["buildOptions"]["CMAKE_BUILD_TYPE"] == "Debug", "x86 Debug required")
    require(0 < evidence["worldBytes"] <= 1200000 and 0 < evidence["distributionBytes"] <= 1800000, "fixed memory bound")
    expected = set()
    for case in manifest["producers"][evidence["producer"]]["cases"]:
        expected.update((case["scenario"], a, f, n) for a, f, n in itertools.product(case["actors"], case["frameUs"], case["nav"]))
    seen = set()
    for row in evidence["results"]:
        key = (row["scenario"], row["actors"], row["frameUs"], row["nav"])
        require(key in expected and key not in seen, "unexpected/duplicate row")
        seen.add(key)
        require(0 < len(row["events"]) <= 32768, "bounded complete events")
        phases = {}
        terminal = None
        previous = {}
        allowed = {"timeline": {"sight", "occluded", "sound", "sound_report", "fairness", "decay", "half", "expired", "resee", "round", "reuse", "map", "soak"},
                   "hooks": {"sight", "load", "expired", "resee", "round", "reuse"},
                   "arrival": {"warmup", "motion", "stopped"}, "recovery": {"warmup", "motion", "stopped"}}[row["scenario"]]
        for event in row["events"]:
            if "terminal" in event:
                require(terminal is None and event is row["events"][-1], "terminal order")
                terminal = event
                continue
            actor = event["actor"]
            require(event["phase"] in allowed, "unexpected phase")
            require(common.integer(actor, 1) and actor <= row["actors"], "actor identity")
            for field in ("generation", "targetGeneration", "map", "round"):
                require(common.integer(event[field], 1), "generation")
            for field in ("time", "origin", "sequence", "visualAge", "soundAge", "reportAge", "delay", "distributionDelay", "reporter"):
                require(common.integer(event[field]), "time/identity")
            stamp = (event["map"], event["time"], event["round"], event["targetGeneration"])
            require(actor not in previous or stamp >= previous[actor], "out of order snapshot")
            previous[actor] = stamp
            require(0 <= event["visual"] <= 31 and 0 <= event["sounds"] <= 16 and 0 <= event["reports"] <= 31, "memory capacity")
            for field, cap in LIMITS.items():
                require(common.integer(event[field]) and event[field] <= cap, "frame bound: " + field)
            require(type(event["x"]) in (float, int) and math.isfinite(event["x"]), "position")
            confidence = event["confidence"]
            require(type(confidence) in (float, int) and math.isfinite(confidence) and 0 <= confidence <= 1, "confidence")
            require(event["source"] in (0, 1, 3), "anonymous sound must not identify target")
            if event["source"]:
                age = event["time"] - event["origin"]
                require(0 <= age < 5000000 and event["sequence"] > 0, "original sight lifetime")
                cap = 1 if event["source"] == 1 else 0.5
                require(abs(confidence - cap * (1 - age / 5000000)) < 1e-12, "original-time decay")
                if event["source"] == 3:
                    require(event["reporter"] == 1, "report provenance")
            else:
                require(confidence == 0 and event["x"] == 0, "absent knowledge")
            for kind, lifetime in (("sound", 3000000), ("report", 5000000)):
                require(common.integer(event[kind + "Origin"]) and common.integer(event[kind + "Received"]), "receipt time")
                value = event[kind + "Confidence"]
                require(type(value) in (int, float) and math.isfinite(value), "source confidence")
                if event[kind + "s"]:
                    age = event["time"] - event[kind + "Origin"]
                    require(0 <= age < lifetime and event[kind + "Origin"] <= event[kind + "Received"] <= event["time"], "source lifetime")
                    require(abs(value - 0.5 * (1 - age / lifetime)) < 1e-12, "source decay")
                else:
                    require(value == 0, "retired source")
            if row["scenario"] in ("timeline", "hooks"):
                require(not event["sounds"] or event["soundRegionX"] == 0, "anonymous region input")
                require(not event["reports"] or event["reportX"] == 100, "report original position")
            areas = event["areas"]
            require(len(areas) <= 32, "distribution capacity")
            require([v[0] for v in areas] == sorted(set(v[0] for v in areas)), "canonical areas")
            mass = event["unknown"]
            require(type(mass) in (int, float) and math.isfinite(mass) and 0 <= mass <= 1, "unknown mass")
            for area, weight in areas:
                require(common.integer(area, 1) and type(weight) in (int, float) and math.isfinite(weight) and 0 <= weight <= 1, "area mass")
                mass += weight
            require(not areas or abs(mass - 1) < 1e-12, "conserved mass")
            if not row["nav"]:
                require(not areas, "no NAV has no distribution")
            phases.setdefault((event["phase"], actor), []).append(event)
        scenario = row["scenario"]
        if scenario == "timeline":
            names = ("sight", "occluded", "sound", "sound_report", "half", "expired", "resee", "round", "reuse", "map")
        elif scenario == "hooks":
            names = ("sight", "expired", "resee", "round", "reuse")
        else:
            names = ("warmup",)
            require(terminal and terminal["terminal"] == "Arrived" and terminal["stopped"] is True and terminal["commands"] > 0, "movement terminal")
            require(terminal["recovered"] == (scenario == "recovery"), "recovery observed")
            require(len(phases.get(("motion", 1), [])) == math.ceil(8000000 / row["frameUs"]), "motion completeness")
            require(len(phases.get(("stopped", 1), [])) == 10, "stop completeness")
            require(any(e["source"] == 1 and e["sounds"] > 0 for e in phases[("motion", 1)]), "perception concurrent with movement")
        for actor in range(1, row["actors"] + 1):
            for phase in names:
                values = phases.get((phase, actor), [])
                require(len(values) == 1, "missing/duplicate checkpoint " + phase)
                event = values[0]
                if phase in ("expired", "round", "reuse", "map"):
                    require(event["visual"] == event["sounds"] == event["reports"] == event["source"] == 0, "retired knowledge revived")
                if phase in ("sight", "resee"):
                    visible = scenario == "hooks" or actor != 2
                    require(event["source"] == (1 if visible else 0), "direct sight isolation")
                    require(event["x"] == ((100 if phase == "sight" else 210) if visible else 0), "sighted position")
            if scenario == "timeline":
                soak = phases.get(("soak", actor), [])
                require(len(soak) == 64 and all(e["source"] == e["visual"] == e["sounds"] == e["reports"] == 0 for e in soak), "repeated retirement")
                require(all(soak[i]["targetGeneration"] < soak[i+1]["targetGeneration"] for i in range(63)), "soak generation progression")
                require(len(phases.get(("fairness", actor), [])) == 80, "fairness completeness")
                for phase in ("occluded", "sound", "sound_report", "half"):
                    event = phases[(phase, actor)][0]
                    source = (0 if phase in ("occluded", "sound") else 3) if actor == 2 else 1
                    require(event["source"] == source and event["x"] == (100 if source else 0), "hidden position retention")
                half = phases[("half", actor)][0]
                require(abs(half["confidence"] - (0.25 if actor == 2 else 0.5)) < 1e-12, "half-life checkpoint")
                require(phases[("sound_report", actor)][0]["sounds"] == 1, "anonymous sound publication")
                sound_only = phases[("sound", actor)][0]
                require(sound_only["sounds"] == 1 and sound_only["reports"] == 0, "sound cannot imply an ally report")
                require(phases[("sound_report", actor)][0]["reports"] == (0 if actor == 1 else 1), "explicit recipient separation")
                if row["nav"] and actor != 2:
                    require(len(phases[("fairness", actor)][-1]["areas"]) == 32, "starved distribution actor")
            if scenario == "hooks":
                load = phases.get(("load", actor), [])
                require(len(load) == 160 and load[0]["sounds"] == 16 and load[7]["soundQueue"] == 0, "FIFO/fair audience")
                require(all(e["x"] in (0, 100) for e in load), "hidden current position leaked")
                require(all(e["traces"] == 0 for e in load), "smoke/flash extra trace")
                require(phases[("reuse", actor)][0]["targetGeneration"] > phases[("round", actor)][0]["targetGeneration"], "slot reuse evidence")
    require(seen == expected, "missing matrix rows")
    return len(seen)


def mutations(manifest, evidence, context):
    changes = [lambda d: d["results"].pop(),
               lambda d: d["results"].append(copy.deepcopy(d["results"][0])),
               lambda d: d["results"][0]["events"].pop(0),
               lambda d: d["results"][0]["events"][0].update(confidence=0.123),
               lambda d: d["results"][0]["events"][0].update(x=4000),
               lambda d: d["results"][0]["events"][0].update(traces=249),
               lambda d: d["results"][0]["events"][0].update(source=2),
               lambda d: d["results"][0]["events"][0].update(confidence=float("nan")),
               lambda d: d["context"].update(revision="0" * 40),
               lambda d: d.update(worldBytes=1200001)]
    for change in changes:
        bad = copy.deepcopy(evidence)
        change(bad)
        try:
            validate(manifest, bad, context)
        except (ValueError, KeyError, TypeError):
            continue
        raise ValueError("checker accepted deliberately corrupted evidence")
    return len(changes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("run", "validate"))
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--producer", choices=("portable", "adapter"), required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        manifest = common.load(args.manifest)
        context = common.build_context(root, args.build_dir.resolve(), args.executable.resolve(), manifest, args.producer)
        if args.mode == "run":
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.unlink(missing_ok=True)
            runs = []
            for repeat in range(2):
                raw = args.output.with_suffix(f".run{repeat}.json")
                raw.unlink(missing_ok=True)
                command = [str(args.executable.resolve())]
                if args.producer == "adapter":
                    command.append("--p409-output")
                command.append(str(raw.resolve()))
                with raw.with_suffix(".log").open("w", encoding="utf-8") as log:
                    result = subprocess.run(command, cwd=args.build_dir, stdout=log, stderr=subprocess.STDOUT, timeout=180)
                require(result.returncode == 0, "producer failure; inspect " + str(raw.with_suffix(".log")))
                runs.append(common.load(raw))
            require(runs[0] == runs[1], "independent process replay mismatch")
            evidence = runs[0]
            require(evidence["producer"] == args.producer, "producer mismatch")
            evidence["context"] = context
            evidence["replaySha256"] = common.digest(runs[1])
            args.output.write_text(json.dumps(evidence, separators=(",", ":"), allow_nan=False) + "\n", encoding="utf-8")
        else:
            evidence = common.load(args.output)
        raw = {k: v for k, v in evidence.items() if k not in ("context", "replaySha256")}
        require(evidence["replaySha256"] == common.digest(raw), "replay digest mismatch")
        after = common.build_context(root, args.build_dir.resolve(), args.executable.resolve(), manifest, args.producer)
        count = validate(manifest, evidence, after)
        rejected = mutations(manifest, evidence, after)
        print(f"P4-09 {args.producer}: PASS {count} deterministic rows; {rejected} corruptions rejected; live unverified")
        return 0
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError) as error:
        print("P4-09 FAIL: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
