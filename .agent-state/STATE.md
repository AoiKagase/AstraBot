# State

Status: in_progress
Goal: entire Phase 3. P3-03/P3-04 implementation checks have offline evidence.
Main: 3f85e3f, fast-forward merged in .worktrees/main-integration; clean, not pushed.
Current branch: codex/p305-jump-inspection, based on integrated main 3f85e3f.
Current boundary: JumpProbe launch inspection implemented, ready for commit.
Report: docs/reports/p3-05-jump-inspection.md.
JumpProbe consumes explicit current-bound ballistic physics and measured velocity;
three support queries, two stationary sweeps and up to eight paired flight sweeps.
Hard maximum21 queries; no retries; stale/malformed/missing data discard all proof.
SimpleJump launch proof now binds step and observed velocity. Acceleration does
not require flight proof until observed launch speed is reached; Press does.
Transition geometry, approach and Walk/host integration remain pending.
The host still rejects Jump hints; P3-05 Simple Jump checklist stays open.
Verification: Windows x86 NMake Debug adapter+portable38/38 PASS; WSL Debian
GCC -m32 Debug portable33/33 PASS; Release x86 DLL/six exports PASS. Werror.
Targeted added16/32-unit rises and diagonal trajectory cases PASS both platforms.
Next: derive transition geometry/approach and establish the host physics model,
then Walk/host integration with dispatch-time revalidation and landing cooldown.
Inspected seams: MovementSnapshot has velocity/hull but no gravity/jump physics;
adapter/cstrike/nav/motion.cpp ready() and segmentAllows() assume grounded Walk.
Add an explicit bounded physics input before predicting flight. Preserve existing
ground guards; introduce Jump-specific ownership and dispatch checks separately.
JumpPhysics now supplies that portable input but the host does not produce it.
The pinned ReGameDLL pm_shared.cpp has API/cvar jump-height overrides; synthetic
constants cannot be assumed live. Account for engine integration/numeric error
before dispatch. Native fixed Hull adapter query integration is tested offline.
Require measured takeoff/landing support and conservative flight clearance;
do not treat synthetic ballistic constants as verified live physics.
Then P3-06 ladders, P3-07 finite recovery, P3-08 offline matrix.
Keep Phase 3 goal active. No subagents/live server/project-wide Finish.
Real NAV compatibility remains partial; real NAV/BSP inputs are read-only.
Preserve untracked .focalspan.json and .serena/. No push; hosted CI pending.
