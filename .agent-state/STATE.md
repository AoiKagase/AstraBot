# State

Status: in_progress
Goal: entire Phase 3. P3-03/P3-04 implementation checks have offline evidence.
Main: 3f85e3f, fast-forward merged in .worktrees/main-integration; clean, not pushed.
Current branch: codex/p305-jump-transition, based on 9d39854; not merged/pushed.
Current boundary: selected-step geometry plus approach/acceleration/landing probes.
Report: docs/reports/p3-05-jump-transition.md.
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
The complete portable query/controller pipeline is exercised offline. Walk/host
physics, guarded dispatch and route advancement after cooldown remain pending.
The host still rejects Jump hints; P3-05 Simple Jump checklist stays open.
Verification: Windows x86 NMake Debug adapter+portable39/39 PASS; WSL Debian
GCC -m32 Debug portable34/34 PASS; Release x86 DLL/six exports PASS. Werror.
Four-direction8/16/100ms pipeline, selected nonzero step, external rejection,
region/height limits, stale helper ordinals, blocked/source-escape/budget failures.
Native queryNavWorld preparation/landing tests added and fake-client rerun PASS.
Next: establish host physics and connect Walk/host dispatch with landing cooldown.
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
