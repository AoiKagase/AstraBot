---
phase: 8
plan: 05
subsystem: autonomous-action-gap-closure
tags: [teaminfo, readiness, locomotion, windows, x86, partial, phase8]
requires:
  - plan: 08-03
provides:
  - TeamInfo-confirmed movement readiness boundary
  - current-source regression coverage
  - pinned rebuilt Windows x86 artifact for operator retest
affects:
  - 08-04
  - 08-06
actuals:
  tasks: 3
  verification: partial
  requirements-completed: []
  requirements-progress: [PAR-01, PAR-03, PAR-06, TEST-04]
  production-commit: uncommitted-working-tree-checkpoint
---

# Plan 08-05 Summary

The live Windows run established a real acceptance gap: Bots entered teams and
could be killed by a human, but did not act autonomously and were later
`Game_idle_kick` removed. Current-source tracing found a readiness mismatch:
`JoinController` accepted valid TeamInfo while raw ReGameDLL entity `team`
remained zero, and movement readiness rejected that state.

The gap was reproduced RED in the current-source x86 compatibility test, then
fixed by adding an explicit `teamConfirmed` movement-state signal populated
from `JoinController::teamConfirmed()`. The raw GameDLL entity field remains
unchanged. Current-source Windows x86 all-target build completed and CTest
passed `41/41`.

## Artifact identity

- Source HEAD: `b5a497ce189b3fc8b77ed966559065327a35632c` with working-tree changes.
- Built DLL: `build-metamod-x86-test/astrabot_mm.dll`.
- Built DLL SHA-256: `956436FD7497D3A656CFAC9E357B0F31543C98B9F01D3F734407BB59FCA353E2`.
- Deployed live DLL SHA-256 for the fresh log-only run: `5FC11FDB0F8CA791904AAAE83130491321F3F3DD10AD56504A04BD4BD13DB321`.
- Fresh log interval began at qconsole line `77406`; the previous deployed DLL was backed up before restart.

## Verification

- RED: `astrabot_compat_actor_command` failed at
  `heartbeat records dispatched spawn-ready physics sample` before the fix.
- GREEN: focused movement/runtime tests passed.
- GREEN: current-source full Windows x86 CTest `41/41` passed.
- Live log verifier self-test: passed.
- New live interval: TeamInfo/readiness and mixed CT/T entry passed, but
  post-spawn movement `failed` after spawn teleports were excluded; the log
  reported `roam_no_intent` with `locomotionResult=Stuck`.
- New live interval: Bot combat `failed` with zero Bot-to-Bot attack lines;
  C4 objective `failed` with zero plant and defuse lines.
- Next: create the NavRoam/physics and action-integration gap closure from the
  captured diagnostics; do not promote Phase 8.
