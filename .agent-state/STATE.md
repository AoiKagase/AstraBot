# State

Status: in_progress
Goal: entire Phase 3; P3-03 ground and Walk/motor checklists have offline evidence.
Main: fast-forward integrated 5a34298 -> ef16095 on user request; not pushed.
Current branch: codex/p303-stairs, based on 73bc116; not merged or pushed.
Current boundary: hull-footprint floor/ground observations, explicit NAV height
tolerance and bounded up/across/down stair probes. Fake-engine ascending and
descending 16-unit tread arrival at 8/16/100 ms; ceiling and 19-unit riser stop.
18-unit portable boundary passes. Query profile: 21 queries/4 samples, horizon48,
step/drop18, NAV height tolerance18, measured support tolerance4; no nearest.
QueryRequest/RouteOptions retain default tolerance2; console explicitly selects18.
Cached-segment guards stay strict and may stop until the next 25 Hz refresh
after a discontinuous stair height. Commit boundary verified and ready.
Verification: Windows x86 NMake Debug adapter+portable30/30 PASS; WSL Debian
GCC -m32 Debug portable25/25 PASS; Release x86 DLL PASS, exactly six exports.
All warnings-as-errors; SDK SHA 7ec9b014f8c0a947a724644aebe34eb33706e44b verified.
Report: docs/reports/p3-03-stairs.md.
Next: existing P3-03 ordinary door use/wait, wall avoidance and narrow
passage speed/lateral correction. Then remaining P3-04..P3-08.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial. No real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI for this branch pending.
