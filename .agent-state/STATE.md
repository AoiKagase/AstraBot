# State

Status: in_progress
Goal: entire Phase 3. P3-03/P3-04 implementation checks have offline evidence.
Main: 3f85e3f, fast-forward merged in .worktrees/main-integration; clean, not pushed.
Current branch: codex/p305-jump-walk, based on ebe2000; not merged/pushed.
Current boundary: Walk owns Jump lifecycle, dispatch feedback and corridor advance.
Report: docs/reports/p3-05-jump-walk.md.
WalkLimits has optional WalkJumpLimits; Walk::update takes optional current
JumpPhysics as its final argument. reportJumpDispatch consumes only the first
result for the active step's exact Press tick. WalkDecision carries Jump state,
reason, probe/geometry failures, plan, physics and press tick for host tickets.
walk_jump.cpp handles support/flight queries with host reservations included.
Only measured landing plus cooldown advances the cursor; final consumed Jump
hint is cleared for goal finishing only. Consecutive jumps retire old Press ticks.
An enabled crouch gate confirms standing before initial Jump entry.
JumpGeometry derives from exactly binding.step of the immutable Corridor; full
takeoff/landing circles plus actual hull/margin must fit their respective areas.
JumpProbe::prepare uses source-bound GroundProbe movement and globally unique
ordinals; cached120ms acceleration coverage cannot be shortened silently.
JumpProbe::land requires measured target support and landing radius/height.
JumpProbe consumes explicit current-bound ballistic physics and measured velocity;
three support queries, two stationary sweeps and up to eight paired flight sweeps.
Hard maximum21 queries; no retries; stale/malformed/missing data discard all proof.
SimpleJump launch proof now binds step and observed velocity. Acceleration does
not require flight proof until observed launch speed is reached; Press does.
The complete portable Walk/Jump pipeline is exercised with the25Hz IntentPump.
Host physics and guarded adapter dispatch remain pending.
The host still rejects Jump hints; P3-05 Simple Jump checklist stays open.
Verification: Windows x86 NMake Debug adapter+portable40/40 PASS; WSL Debian
GCC -m32 Debug portable35/35 PASS; Release x86 DLL/six exports PASS. Werror.
Final Press identity cleanup: Jump/Walk test rebuilt/rerun PASS both platforms.
Single Jump and two consecutive Jumps then ordinary Walk at8/16/100ms,25Hz pump;
dispatch identity/rejection/missing/stale, invalid actor/physics, wrong landing,
budget saturation, abort/release, standing clearance wait/timeout are covered.
Next: establish host physics and connect guarded adapter dispatch. The optional
Walk Jump profile remains unset by the host, so production still rejects Jump.
Inspected seams: MovementSnapshot has velocity/hull but no gravity/jump physics;
adapter/cstrike/nav/motion.cpp ready() and segmentAllows() assume grounded Walk.
Use an explicit bounded physics input before predicting flight. Preserve existing
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
