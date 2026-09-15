---
phase: 02-metamod-lifecycle-and-native-guard
plan: 02
subsystem: lifecycle-generations
tags: [sdk-free-core, generation, map, round, slot, c++14, x86]
requires:
  - plan: 02-01
    provides: adapter lifecycle callbacks and PluginRuntime ownership
provides:
  - deterministic SDK-free LifecycleSession
  - map, round, and per-slot generation tokens
  - adapter translation from public lifecycle callbacks to Core values
affects:
  - 02-03
actuals:
  tasks: 4
  commits: 0
  verification: passed
requirements-completed: [LIFE-01, TEST-02]
---

# Plan 02-02 Summary

Added `LifecycleSession` as a bounded SDK-free Core contract. It owns map,
round, and per-client-slot generations, rejects invalid or stale tokens,
handles disconnect/reuse safely, and detects a conservative frame/time reset
without accessing private GameDLL state. `PluginRuntime` now maps public
Metamod callbacks to that session and exposes only adapter diagnostics needed
by the focused tests.

## Files

- `include/astrabot/runtime/lifecycle.hpp`
- `src/core/runtime/lifecycle.cpp`
- `src/adapter/metamod/plugin_runtime.hpp`
- `src/adapter/metamod/plugin_runtime.cpp`
- `tests/lifecycle_generation_tests.cpp`
- `tests/metamod_hook_table_tests.cpp`
- `CMakeLists.txt`

## Verification

- RED observed before implementation: the lifecycle test failed because the
  requested Core header/API did not exist.
- Windows x86 Debug: lifecycle generation test passed; full Metamod hook test
  passed after adapter wiring.
- Debian WSL Linux x86 Debug: lifecycle generation test and Metamod hook test
  passed.
- The Windows `max`/`min` SDK macro collisions were fixed with localized
  parenthesized standard-library calls; no global macro policy was changed.

## Decisions

- GoldSrc `globalvars_t` has no `framecount`; the adapter supplies a bounded
  local StartFrame counter and passes public `time` to Core.
- Round state is advanced only by the explicit Core API or conservative time
  reset observation. Private ReGameDLL round objects are not inferred.
- Slot indices are accepted only through the public `pfnIndexOfEdict` and
  are bounded to the GoldSrc client range before any array access.

## Next readiness

Plan 02-03 can use the same public engine table and slot observation boundary
for native CSBot suppression and unmanaged FakeClient detection.
