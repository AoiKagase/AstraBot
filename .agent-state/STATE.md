# State

Status: in_progress
Goal: entire Phase 3; P3-03 ground and Walk/motor checklists have offline evidence.
Main: fast-forward integrated 5a34298 -> ef16095 on user request; not pushed.
Current branch: codex/p303-walk-host, based on b7fdef6; not merged or pushed.
Current boundary: console Walk/pump/motor submission after dispatch; exact
pending cancellation, pre-dispatch freshness/segment guards, one-shot neutral
stop and bounded motion trace implemented. Fake-engine goto reaches supported
goals at 8/16/100 ms frames. Reentrant map/goal changes preserve old dispatch
identity and measured msec. Commit boundary verified and ready.
Verification: Windows x86 NMake Debug adapter+portable30/30 PASS; WSL Debian
GCC -m32 Debug portable25/25 PASS; Release x86 DLL PASS, exactly six exports.
All warnings-as-errors; SDK SHA 7ec9b014f8c0a947a724644aebe34eb33706e44b verified.
Report: docs/reports/p3-03-walk-host.md.
Next: existing P3-03 ordinary door use/wait, stairs, wall avoidance and narrow
passage speed/lateral correction. Then remaining P3-04..P3-08.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial. No real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI for this branch pending.
