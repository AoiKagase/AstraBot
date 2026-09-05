# State

Status: in_progress
Goal: entire Phase 3. P3-03/P3-04 implementation checks have offline evidence.
Main: 4e6a7d2 in .worktrees/main-integration, clean; no push.
Current branch: codex/p305-crouch, based on d2461d1; not merged/pushed.
Current boundary: portable constraints and crouch posture gate foundation.
New files: src/nav/local/traversal_constraints.hpp, crouch.hpp/cpp,
tests/nav/crouch_tests.cpp. Report: docs/reports/p3-05-crouch-foundation.md.
Hints verified from manifest-pinned ReGameDLL nav.h in original Phase0 temp
checkout. Crouch1/Jump2/Precise4/NoJump8; conflicting/unknown/unsupported fail.
Posture waits for actual hull/duck observations; standing release uses stamped
headroom clearance at preserved foot position. Low ceiling holds duck at zero
motion with finite timeout; no arrival/path permission is inferred.
Verification: Windows x86 NMake Debug adapter+portable36/36 PASS; WSL Debian
GCC -m32 Debug portable31/31 PASS; Release x86 DLL/six exports PASS. Werror.
P3-05 checklists remain open: foundation is not connected to Walk or host yet.
Next: integrate Crouch gate/constraints with measured Walk ground/portal crossing,
shared21-query budget, Hold on cached movement, observed hull transitions and
safe cancellation/release. Existing Walk still rejects special hints.
Then Simple Jump approach/align/accelerate/takeoff/airborne/land/cooldown;
P3-06 ladder discovery/lifecycle, P3-07 finite recovery, P3-08 offline matrix.
P3-04 has one automatic replan per explicit goal with1s directed-edge exclusion.
Keep goal active. No subagents/live server/project-wide Finish.
Real NAV compatibility remains partial; real NAV/BSP inputs are read-only.
Preserve untracked .focalspan.json and .serena/. Hosted CI pending.
