---
phase: 6
plan: 06
subsystem: zbot-nav-runtime
tags: [zbot, nav, off-mesh, runtime, diagnostics, x86]
requires:
  - plan: 06-05
provides:
  - nearest-point Nav matches and bounded off-mesh recovery intent
  - typed NavRoamDecision stages for Core and adapter evidence
  - feet-based runtime locomotion observation and diagnostic logging
affects:
  - phase-8
actuals:
  tasks: 4
  commits: 0
  verification: passed-offline
  requirements-completed: []
  requirements-progress: [PAR-01]
---

# Plan 06-06 Summary

Extended the current Nav runtime slice using ZBot `GetNearestNavArea()` and
`StayOnNavMesh()` behavior as the comparator. `NavQuery` now supplies nearest
area points; `NavRoamController` emits bounded off-mesh recovery intent, keeps
directed route selection deterministic, and exposes typed decision stages. The
Metamod adapter converts entity origin to feet-space for Nav/loco observation
and records decision stages alongside raw origin and command identity.

## Verification

- TDD RED: movement contract failed because `NavRoamDecision` and staged
  logging were absent.
- Windows x86 Debug `astrabot_mm` build passed with the initialized Visual
  Studio x86 environment.
- Windows x86 CTest: `40/40 passed`.
- `python tests/runtime_movement_integration_contract.py`: passed.
- `python tests/phase6_verification.py`: `OK (9 checks)`.
- Source manifest: `OK (121 entries, 114 C/C++ files)`.
- PE artifact: `OK (PE x86, 7 exact exports)`.
- Real-server autonomous movement remains unverified; Phase 8 is still partial.
