# State

Status: in_progress
Goal: entire Phase 3; P3-03 ground and Walk/motor checklists have offline evidence.
Main: fast-forward integrated ef16095 -> d32cc21 on user request; not pushed.
Integrated main verification: Windows x86 Debug adapter+portable30/30 PASS.
Current branch: codex/p303-touch-doors, based on 6b96692; not merged or pushed.
Current boundary: ordinary untargeted touch doors use supported approach, one
contact pulse and finite passive waiting. Touch timeout is 3 seconds including
approach; Use-only remains 1 second. No direct SDK Touch or position edits.
Approach setback .0625, contact distance <=.125, actual guarded travel <=.75.
Contact commands are single-frame, never cached translation or automatically retried.
One host guard trace reserves ordinal 1; Walk remaps its real queries after it.
Total frame/decision budget remains 21 queries and 4 samples, including guard.
Clearance remains neutral; full ground/segment inspection precedes resumed Walk.
Verification: Windows x86 NMake Debug adapter+portable31/31 PASS; WSL Debian
GCC -m32 Debug portable26/26 PASS. Release x86 DLL with six exports PASS.
All warnings-as-errors.
Report: docs/reports/p3-03-touch-doors.md. Fake-engine contact/arrival, locked,
replacement/targetname/cancel, short/expired pulse and lost-ground scenarios at
8/16/100 ms. Portable reserved-ordinal, ground-cache and budget tests pass.
Next: wall avoidance and narrow-passage speed/lateral correction, then P3-04..P3-08.
The combined P3-03 door/stairs/wall/narrow checklist remains unchecked.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial. No real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI for this branch pending.
