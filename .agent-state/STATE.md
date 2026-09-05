# State

Status: in_progress
Goal: entire Phase 3. P3-03/P3-04 implementation checks have offline evidence.
Main: 3f85e3f, fast-forward merged in .worktrees/main-integration; clean, not pushed.
Current branch: codex/p305-jump-inspection, based on integrated main 3f85e3f.
Current boundary: bounded world-query proof producer after controller foundation.
Report: docs/reports/p3-05-simple-jump-controller.md.
SimpleJump consumes current-stamped inspection and actual dispatch feedback;
one Press, observed airborne, supported landing and grounded cooldown required.
World-query proof generation and Walk/host integration remain pending.
The host still rejects Jump hints; P3-05 Simple Jump checklist stays open.
Verification: Windows x86 NMake Debug adapter+portable37/37 PASS; WSL Debian
GCC -m32 Debug portable32/32 PASS; Release x86 DLL/six exports PASS. Werror.
Next: implement bounded world-query inspection from selected transition and
observed physics, then Walk/host integration with dispatch-time revalidation.
Inspected seams: MovementSnapshot has velocity/hull but no gravity/jump physics;
adapter/cstrike/nav/motion.cpp ready() and segmentAllows() assume grounded Walk.
Add an explicit bounded physics input before predicting flight. Preserve existing
ground guards; introduce Jump-specific ownership and dispatch checks separately.
Require measured takeoff/landing support and conservative flight clearance;
do not treat synthetic ballistic constants as verified live physics.
Then P3-06 ladders, P3-07 finite recovery, P3-08 offline matrix.
Keep Phase 3 goal active. No subagents/live server/project-wide Finish.
Real NAV compatibility remains partial; real NAV/BSP inputs are read-only.
Preserve untracked .focalspan.json and .serena/. No push; hosted CI pending.
