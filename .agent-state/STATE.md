# State

Status: in_progress
Goal: entire Phase 3; P3-03 ground and Walk/motor checklists have offline evidence.
Main: fast-forward integrated ef16095 -> d32cc21 on user request; not pushed.
Integrated main verification: Windows x86 Debug adapter+portable30/30 PASS.
Current branch: codex/p303-door-wait, based on d32cc21; not merged or pushed.
Current boundary: portable DoorWait lifecycle, one stationary Use Press intent,
finite explicit timeout, stamped observations, identity/clock/replacement rejection.
Motor replay verified at 8/16/100 ms. Clear authorizes ground/clearance inspection
only; never movement or arrival. No host timeout profile has been selected yet.
Verification: Windows x86 NMake Debug adapter+portable31/31 PASS; WSL Debian
GCC -m32 Debug portable26/26 PASS. Both warnings-as-errors.
Report: docs/reports/p3-03-door-wait.md.
Next: connect adapter door observations and host-proven use view to Walk,
including query/clock budgets, touch doors and fake-engine passage scenarios.
Door query is still Unavailable in adapter; Walk does not call DoorWait yet.
Then wall avoidance/narrow passage and remaining P3-04..P3-08.
The combined P3-03 door/stairs/wall/narrow checklist remains unchecked.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial. No real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI for this branch pending.
