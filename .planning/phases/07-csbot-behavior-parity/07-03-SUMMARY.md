---
phase: 7
plan: 03
subsystem: combat-intents
tags: [weapon-state, combat, aim, fire, reload, damage, feedback, sdk-free, x86]
requires:
  - plan: 07-02
    provides: actor-scoped behavior state and generation-aware proposals
provides:
  - bounded weapon inventory and ammo/reload/cooldown observation records
  - actor/frame-stamped aim, fire, and reload intents
  - fail-closed target, friendly-fire, damage, death, and generation feedback boundaries
affects:
  - phase 7 plan 04 bomb, hostage, buy, and round objectives
  - phase 7 plan 06 behavior scenario replay
  - phase 8 live combat parity acceptance
actuals:
  tasks: 4
  commits: 2
  verification: passed
  requirements-completed: []
  requirements-progress: [PAR-03]
  production-commits: [549ba95, 4de616b]
---

# Plan 07-03 Summary

Implemented the SDK-free weapon and combat observation boundary. Fixed-capacity
`WeaponInventory` validates weapon identity, slot, availability, ammo,
reload state, and cooldown before selecting a deterministic legal weapon.
`CombatController` consumes only a stamped `TargetBelief` and actor origin,
computes bounded aim angles, and emits separate Aim, Fire, or Reload intents.

Unknown or unavailable targets, insufficient confidence, and friendly-fire
risk fail closed. Fire intent contains no damage or ammo-success field;
reload intent contains no ammunition confirmation; `DamageObservation` is a
separate explicit feedback record and unknown damage cannot confirm damage or
death. Actor generation and frame checks prevent stale combat decisions.

## Files

- `include/astrabot/combat/weapon_state.hpp`
- `src/core/combat/weapon_state.cpp`
- `include/astrabot/combat/combat_intent.hpp`
- `src/core/combat/combat_intent.cpp`
- `tests/weapon_state_tests.cpp`
- `tests/combat_intent_tests.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- TDD RED observed: both new tests failed before combat headers existed;
  focused Debian GCC14 C++14 builds then passed after implementation.
- Windows x86 portable CTest: 26/26 passed.
- Windows x86 Metamod CTest: 33/33 passed.
- Debian Linux x86 portable CTest: 26/26 passed.
- Debian Linux x86 Metamod CTest: 33/33 passed.
- Phase 6 regression harness: 9/9 checks passed.
- Python regression suite: 6/6 tests passed.
- Source manifest: 102 entries and 95 C/C++ files accepted.
- PE artifact: x86 with 6 exact exports.
- ELF artifact: x86 with 6 exact exports.
- Combat Core SDK boundary scan: no Metamod/HLSDK headers, `edict_t`,
  engine function tables, or ReAPI markers found.
- C++ format checks: no leading-space indentation, trailing whitespace, or
  lines over the 120-column recommended limit in the six new C++ files;
  UTF-8 without BOM and LF line endings confirmed.
- `git diff --cached --check`: passed before production commits.
- FocalSpan: final index fresh and ready; queried weapon, combat intent,
  cooldown, target belief, and feedback contracts.
- code-review-graph: architecture inspection and staged incremental review
  update completed. Its 67 static test-gap suggestions were retained as
  graph evidence only; focused tests and 26/33 CTest runs are the execution
  evidence.

## Issues Encountered

The first behavior implementation correctly exposed that unknown damage
feedback must not be returned as `DamageObserved`; `observeDamage` now returns
`NoAction` for Unknown or Unavailable feedback. Windows x86 builds require
explicit MSVC 14.29 and Windows SDK 10.0.26100.0 x86 include/lib paths in
this checkout because the installed VS 2026 prompt does not populate them.

Live HLDS/ReHLDS combat, damage, stability, and multi-Bot parity remain open
for Phase 8. These contracts emit intents and observations only; they do not
claim engine dispatch, ammunition mutation, damage, or death success.
