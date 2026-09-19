---
phase: 8
plan: 03
subsystem: live-acceptance
tags: [hlds, rehlds, metamod, windows, debian, x86, partial, phase8]
requires:
  - plan: 08-01
  - plan: 08-02
provides:
  - live acceptance protocol
  - platform-specific prerequisite reports
  - explicit external-input blocker
affects:
  - 08-04 final audit
actuals:
  tasks: 1
  verification: partial
  requirements-completed: []
  requirements-progress: [PAR-06, TEST-04]
production-commit: uncommitted-working-tree-checkpoint
---

# Plan 08-03 Summary

Created the live acceptance protocol and platform reports under
`docs/evidence/phase8-live/`. A live Windows x86 HLDS/ReGameDLL-CS/Metamod-P
server is now running and its logs provide real evidence for Bot team entry,
human damage/death, and the failure of autonomous post-join action.

The Windows run remains partial: `RunPlayerMove` dispatches did not produce
sustained autonomous action and all four managed Bots were later removed by
`Game_idle_kick`. PAR-06 and TEST-04 remain pending. TEST-03 also remains
pending because 08-02 contains only a synthetic contract sample and no pinned
reference trace.

## Files

- `docs/evidence/phase8-live-acceptance.md`
- `docs/evidence/phase8-live/windows-x86.md`
- `docs/evidence/phase8-live/linux-x86.md`
- `docs/source-manifest.json`
- `.planning/STATE.md`

## Verification

- Windows x86 real-server run: plugin load, team entry, human USP damage/death, and idle-kick failure recorded.
- Autonomous post-join movement/action: failed; no sustained route progress or independent behavior was observed.
- Debian Linux x86 live evidence: blocked/not run.
- Offline artifact matrix remains verified in 08-01.
- Differential contract remains verified but synthetic in 08-02.
- Complete single-/multi-Bot lifecycle and stability matrix: incomplete.
- Source manifest: 114 entries / 107 C/C++ files.
- FocalSpan/CRG evidence remains navigation/impact evidence, not live proof.

## Resume condition

Fix or explicitly disposition the autonomous-action failure, then supply or
verify the Debian Linux x86 HLDS/ReHLDS run, complete the remaining Windows
lifecycle/gameplay matrix, and capture the pinned reference trace before
closing 08-03.
