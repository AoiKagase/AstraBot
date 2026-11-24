#!/usr/bin/env python3
"""Replay captured all-visible grenade geometry through pinned CSBot's real body.

Input is analyze.py's verified live analysis, not an invented NAV reachability case.
Captured engine trace replies are replayed only after checking each request's start
and direction. This establishes geometry parity for those inputs, not full GameDLL
or normal-NAV acceptance. Output directories are exclusive and never overwritten.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

PIN = "b0889847fe6d03898be88acc9e366660efb40ab5"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--analysis", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mutation", choices=["endpoint-only"])
    args = parser.parse_args()
    if os.environ.get("VSCMD_ARG_TGT_ARCH") != "x86":
        parser.error("use the MSVC x86 developer environment")
    args.output.mkdir(parents=True, exist_ok=False)
    source_hashes = {}

    def source(path):
        data = subprocess.check_output(["git", "-C", str(args.reference), "show", f"{PIN}:{path}"])
        source_hashes[path] = sha(data)
        return data.decode("utf-8")

    weapon = source("regamedll/dlls/bot/cs_bot_weapon.cpp")
    anchor = "bool CCSBot::FindGrenadeTossPathTarget(Vector *pos)"
    assert weapon.count(anchor) == 1
    start = weapon.index(anchor)
    body = weapon[start:weapon.index("\n}", start) + 2]
    vector = source("regamedll/dlls/vector.h")
    (args.output / "pinned-vector.h").write_text(vector, encoding="utf-8")
    (args.output / "pinned-function.inc").write_text(body, encoding="utf-8")
    analysis = json.loads(args.analysis.read_text())
    assert analysis["cache_prefix_and_retained_suffix_verified"]
    assert analysis["normal_exit"]
    writes = {row["line"]: row for row in analysis["writes"]}

    def number(value):
        # Round-trip the captured binary32 value, including integer-valued floats.
        return repr(float(value)) + "f"

    def vec(values):
        return "Vector(" + ",".join(number(x) for x in values) + ")"

    def decode(values):
        return [struct.unpack("<f", struct.pack("<I", x))[0] for x in values]

    cases = []
    for i, event in enumerate(analysis["events"]):
        assert event["causal_geometry_verified"]
        assert 0 <= event["next"] < event["length"] < 256
        record = writes[event["history_line"]]
        points = decode(record["points_bits"])
        visibility = []
        visible_results = []
        for offset in range(event["next"] * 3, len(points), 3):
            point = points[offset:offset + 3]
            point[2] += 36.0
            visibility.append(vec(point))
            visible_results.append("true")
        visibility += [vec(row["point"]) for row in event["samples"]]
        visible_results += ["true" if row.get("visible", True) else "false" for row in event["samples"]]
        setup = ["{ CCSBot bot;", f"bot.m_pathLength={event['length']};bot.m_pathIndex={event['next']};"]
        for slot in range(event["length"]):
            setup.append(f"bot.m_path[{slot}].pos={vec(points[slot * 3:slot * 3 + 3])};")
        setup.append(f"bot.m_path[{event['length']}].pos={vec(event['tail'])};")
        if args.mutation:
            # Deliberately wrong tail-independent endpoint policy; it must fail.
            setup.append(f"bot.m_path[{event['length']}].pos=bot.m_path[{event['length'] - 1}].pos;")
        setup += ["visible={" + ",".join(visibility) + "};", "vis=wall=0;"]
        setup.append("visibleResults={" + ",".join(visible_results) + "};")
        setup.append("buffers={" + ",".join("{" + vec(b["spot"]) + "," + number(b["fraction"]) + "," + vec(b["hit"]) + "}" for b in event["buffers"]) + "};")
        setup += ["Vector target;require(bot.FindGrenadeTossPathTarget(&target),\"accepted\");",
                  "require(vis==visible.size()&&wall==4,\"trace counts\");",
                  f"require(target=={vec(event['target'])},\"target equality\");",
                  f'printf("case={i} target=%.9g,%.9g,%.9g visibility=%u walls=%u\\n",target.x,target.y,target.z,vis,wall);', "}"]
        cases.append("\n".join(setup))

    host = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using vec_t=float;using real_t=float;
#define Q_sqrt std::sqrt
#include "pinned-vector.h"
constexpr float HalfHumanHeight=36;
enum { dont_ignore_monsters,ignore_glass };
#define ENT(v) (v)
void require(bool ok,const char* message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
struct TraceResult{float flFraction;Vector vecEndPos;};
struct Reply{Vector start;float fraction;Vector hit;};
std::vector<Vector> visible;std::vector<bool> visibleResults;std::vector<Reply> buffers;unsigned vis=0,wall=0;
void UTIL_TraceLine(const Vector& start,const Vector& end,int,int,void*,TraceResult* result){
 require(wall<4&&wall<buffers.size(),"wall count");auto reply=buffers[wall];
 require(start==reply.start,"wall request start");
 Vector direction(wall==0?999.9f:wall==1?-999.9f:0,wall==2?999.9f:wall==3?-999.9f:0,0);
 require(end==start+direction,"wall request end");
 result->flFraction=reply.fraction;result->vecEndPos=reply.hit;++wall;
}
struct CCSBot{
 struct Segment{Vector pos;};std::array<Segment,256> m_path;
 int m_pathLength=0,m_pathIndex=0;void* pev=nullptr;
 bool HasPath()const{return m_pathLength>0;}
 bool FVisible(const Vector& point){require(vis<visible.size(),"visibility count");require(point==visible[vis],"visibility request");return visibleResults[vis++];}
 bool FindGrenadeTossPathTarget(Vector*);
};
#include "pinned-function.inc"
int main(){
'''
    (args.output / "replay.cpp").write_text(host + "\n".join(cases) + "\n}\n", encoding="utf-8")
    executable = (args.output / "replay.exe").resolve()
    command = ["cl", "/nologo", "/std:c++14", "/EHsc", "/Od", "/fp:precise", "/D_CRT_SECURE_NO_WARNINGS",
               "/I" + str(args.output.resolve()), str((args.output / "replay.cpp").resolve()),
               "/Fe:" + str(executable), "/Fo:" + str((args.output / "replay.obj").resolve())]
    build = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (args.output / "build.log").write_bytes(build.stdout)
    assert build.returncode == 0, args.output / "build.log"
    run = subprocess.run([str(executable)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (args.output / "replay.log").write_bytes(run.stdout)
    result = {"reference_pin": PIN, "source_sha256": source_hashes, "pinned_function_sha256": sha(body.encode()),
              "analysis_sha256": sha(args.analysis.read_bytes()), "tool_sha256": sha(Path(__file__).read_bytes()),
              "executable_sha256": sha(executable.read_bytes()), "cases": len(cases), "exit": run.returncode,
              "mutation": args.mutation, "passed": run.returncode == 0,
              "scope": "Unmodified pinned body and Vector; captured all-visible paths, retained tails and engine trace replies. No native NAV reachability or full GameDLL claim."}
    (args.output / "manifest.json").write_text(json.dumps(result, indent=2) + "\n")
    print(run.stdout.decode(errors="replace"), end="")
    print(json.dumps({"cases": len(cases), "exit": run.returncode, "mutation": args.mutation}))
    raise SystemExit(run.returncode)


if __name__ == "__main__":
    main()
