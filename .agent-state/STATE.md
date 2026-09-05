# State

Status: in_progress
Goal: entire Phase 3; P3-03 ground and Walk/motor checklists have offline evidence.
Main: fast-forward integrated ef16095 -> d32cc21 on user request; not pushed.
Integrated main verification: Windows x86 Debug adapter+portable30/30 PASS.
Current branch: codex/p303-door-host, based on 1d41079; not merged or pushed.
Current boundary: Use-only door observations connect DoorWait -> Walk -> Motor
and later-tick dispatch. One stationary press, generation/view revalidation at
dispatch, 1-second timeout from accumulated pump simulation time, no retries.
Fresh clearance is neutral; later full ground/segment inspection resumes Walk.
Fixed caps: 21 queries/decision, 33 sphere calls/selection, one trace-free Use
guard/frame (66 sphere calls including a simultaneous decision). No private data.
Use target proof conservatively rejects unknown competitors in the 64-unit sphere.
Verification: Windows x86 NMake Debug adapter+portable31/31 PASS; WSL Debian
GCC -m32 Debug portable26/26 PASS. Release x86 DLL with six exports PASS.
All warnings-as-errors. Final review adjustments have affected tests rerun.
Report: docs/reports/p3-03-door-host.md. Fake-engine Use, delay/open/arrival and
failure/queue-invalidation scenarios at 8/16/100 ms; query/trace counts checked.
Next: ordinary touch-door contact handling, wall avoidance/narrow passage,
then remaining P3-04..P3-08. Use-only capability is not inferred for touch doors.
The combined P3-03 door/stairs/wall/narrow checklist remains unchecked.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial. No real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI for this branch pending.
