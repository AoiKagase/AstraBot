---
phase: 7
plan: 01
subsystem: world-perception
tags: [world-snapshot, perception, uncertainty, memory, sdk-free, x86]
requires:
  - phase: 6
    provides: immutable navigation snapshots and generation-scoped Core contracts
provides:
  - immutable bounded WorldSnapshot value records
  - deterministic PerceptionInput and PerceptionAssembler publication
  - explicit Unknown, ObservedAbsent, ObservedPresent, and memory expiry states
affects:
  - phase 7 plan 02 behavior state and objective proposals
  - phase 7 plans 03-05 combat, objectives, and team information
  - phase 8 live perception and differential acceptance
actuals:
  tasks: 4
  commits: 1
  verification: passed
  requirements-completed: []
  requirements-progress: [PAR-02]
  production-commit: 56dbe42
---

# Plan 07-01 Summary

Implemented the SDK-free observation boundary above the Phase 6 navigation
contracts. `WorldSnapshot` now owns fixed-capacity arrays for actor, entity,
audible-event, and actor-memory records and exposes only const accessors.
`PerceptionInput` copies adapter-provided values into bounded input storage;
`PerceptionAssembler` validates frame ordering, normalizes confidence, rejects
stale or non-finite observations, and publishes transactionally without
retaining adapter storage.

Actor and entity generation stamps are part of every stable key. Omitted
contacts remain `Unknown`; explicit non-present contacts are sanitized and
cannot carry a confirmed position. Actor memory is separate from current
visibility, expires at a configured tick bound, and is cleared when a slot is
reused by another actor generation or when map/round identity changes.
Navigation revision identity is carried in the snapshot metadata for future
AstraNav-compatible overlays without adding a Nav write path.

## Files

- `include/astrabot/world/world_snapshot.hpp`
- `src/core/world/world_snapshot.cpp`
- `include/astrabot/perception/perception.hpp`
- `src/core/perception/perception.cpp`
- `tests/world_snapshot_tests.cpp`
- `tests/perception_tests.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- TDD RED observed: both new tests failed before the production headers
  existed; Debian GCC14 C++14 then passed both tests after implementation.
- Windows x86 portable CTest: 22/22 passed.
- Windows x86 Metamod CTest: 29/29 passed.
- Debian Linux x86 portable CTest: 22/22 passed.
- Debian Linux x86 Metamod CTest: 29/29 passed.
- Phase 6 regression harness: 9/9 checks passed.
- Python regression suite: 6/6 tests passed.
- Source manifest: 90 entries and 83 C/C++ files accepted.
- PE artifact: x86 with 6 exact exports.
- ELF artifact: x86 with 6 exact exports.
- C++ format checks: no leading-space indentation, trailing whitespace, or
  lines over the 120-column recommended limit in the six new C++ files;
  UTF-8 without BOM and LF line endings confirmed.
- `git diff --cached --check`: passed.
- FocalSpan: final index fresh and ready after update; queried the new
  WorldSnapshot and PerceptionAssembler contracts.
- code-review-graph: architecture/community inspection and staged incremental
  update completed. Its 84 test-gap result is a static graph inference for
  staged symbols, not a replacement for the 22/29 CTest evidence.

## Issues Encountered

The existing Windows CMake cache used MSVC 14.29 while the installed VS 2026
Developer Prompt did not populate its standard include/lib variables. The
build was completed after explicitly supplying the existing 14.29 and
Windows SDK 10.0.26100.0 x86 paths; no repository or build-cache source
configuration was changed.

Live HLDS/ReHLDS visual or sound acceptance, combat, multi-Bot stability, and
full CSBot parity remain open for Phase 8 as required by the project boundary.
