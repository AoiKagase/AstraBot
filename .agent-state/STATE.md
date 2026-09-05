# State

Status: in_progress
Goal: entire Phase 3. P3-03/P3-04 implementation checks have offline evidence.
Main: 4e6a7d2 in .worktrees/main-integration; clean, not pushed.
Current branch: codex/p305-crouch, based on 2bf3ba3; not merged/pushed.
Current boundary: crouch posture gate connected to Walk and host commands.
Walk combines source/target hints, confirms duck/hull before floor/segment/path
movement, and releases only after fully crossing into clear subsequent area.
Same-area crouched arrival retains Hold. NoJump walking works; Jump, conflicts,
Precise/unknown hints and external traversals still fail closed.
Host reserves dispatch-time standing clearance; cached commands/stops preserve
duck while ducked. Cancel under ceiling is stationary Hold, not forced standing.
Limits:21 queries/4 samples per decision including guards, posture timeout1s.
Report: docs/reports/p3-05-crouch-walk.md.
Verification: Windows x86 NMake Debug adapter+portable36/36 PASS; WSL Debian
GCC -m32 Debug portable31/31 PASS; Release x86 DLL/six exports PASS. Werror.
Final diagnostics rebuilt and fake-client tests rerun PASS. Crouch crossing,
same-area arrival/NoJump and8/16/100ms low ceiling, cancellation, queued-release
rejection/reopen/timeout scenarios covered. First P3-05 checklist is checked.
Next: Simple Jump approach/align/accelerate/takeoff/airborne/land/cooldown;
verify measured takeoff/landing support, one Press, missed/wrong landing/timeout,
abort, no cached press and dispatch feedback. Jump hints do not supply takeoff
geometry; derive/check only bounded supported transition geometry.
Then P3-06 ladder discovery/lifecycle, P3-07 finite recovery, P3-08 offline matrix.
Keep goal active. No subagents/live server/project-wide Finish.
Real NAV compatibility remains partial; real NAV/BSP inputs are read-only.
Preserve untracked .focalspan.json and .serena/. Hosted CI remains pending.
