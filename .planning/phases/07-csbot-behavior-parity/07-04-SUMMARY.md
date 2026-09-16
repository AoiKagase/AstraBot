---
phase: 7
plan: 04
subsystem: round-objectives
tags: [bomb, hostage, buy, round, objectives, feedback, generation, sdk-free, x86]
requires:
  - plan: 07-03
    provides: actor-scoped behavior and combat intent boundaries
provides:
  - scenario and round-phase observation contracts
  - actor-owned objective proposal and explicit completion feedback state
  - bomb, hostage, buy, attack, defend, plant, defuse, rescue, escort, and save planning
affects:
  - phase 7 plan 05 radio and team reports
  - phase 7 plan 06 behavior scenario replay
  - phase 8 live objective parity acceptance
actuals:
  tasks: 4
  commits: 1
  verification: passed
  requirements-completed: []
  requirements-progress: [PAR-04]
  production-commit: 4e214cc
---

# Plan 07-04 Summary

Implemented the SDK-free round-objective boundary. `ScenarioIdentity` carries
map, round, scenario-generation, scenario-kind, and team role. Bounded
`ScenarioObservation` records round phase, buy availability, and observed
bomb/hostage/round events. `RoundObjectivePlanner` consumes these values and
the actor-scoped behavior state to propose attack, defend, plant, defuse,
rescue, escort, buy, and save objectives with deterministic frame and team
ownership.

`ObjectiveState` stores only the actor's current proposal and accepts
completion only from an explicit, generation-matching
`ObjectiveCompletionFeedback`. BombDefused, BombExploded, HostageRescued, and
HostageKilled events alone produce no objective rather than claiming engine
completion. Unknown scenario or event availability enters recovery, while
stale actor, round, and event frames fail closed. Existing NavDocument/Nav
snapshots remain untouched and no action dispatch is emitted.

## Files

- `include/astrabot/objectives/objective_state.hpp`
- `src/core/objectives/objective_state.cpp`
- `include/astrabot/objectives/round_objectives.hpp`
- `src/core/objectives/round_objectives.cpp`
- `tests/objective_state_tests.cpp`
- `tests/round_objectives_tests.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- TDD RED observed: both new tests failed before objective headers existed;
  focused Debian GCC14 C++14 builds then passed after implementation.
- Windows x86 portable CTest: 28/28 passed.
- Windows x86 Metamod CTest: 35/35 passed.
- Debian Linux x86 portable CTest: 28/28 passed.
- Debian Linux x86 Metamod CTest: 35/35 passed.
- Phase 6 regression harness: 9/9 checks passed.
- Python regression suite: 6/6 tests passed.
- Source manifest: 108 entries and 101 C/C++ files accepted.
- PE artifact: x86 with 6 exact exports.
- ELF artifact: x86 with 6 exact exports.
- Objective Core SDK boundary scan: no Metamod/HLSDK headers, private
  GameDLL markers, hidden enemy data, or Nav-write path found.
- C++ format checks: no leading-space indentation, trailing whitespace, or
  lines over the 120-column recommended limit in the six new C++ files;
  UTF-8 without BOM and LF line endings confirmed.
- `git diff --cached --check`: passed before production commit.
- FocalSpan: final index fresh and ready; queried scenario, round, bomb,
  hostage, buy, and explicit feedback contracts.
- code-review-graph: architecture inspection and staged incremental update
  completed. Its 59 static test-gap suggestions are graph evidence only;
  focused tests and 28/35 CTest runs provide execution evidence.

## Issues Encountered

The initial ObjectiveState test helper omitted the nested scenario stamp and
the planner validated a proposal before attaching its actor owner; both were
corrected before the GREEN run. Unknown scenario input was then deliberately
relaxed to a well-formed recovery input while proposals and feedback still
require known scenario identity.

Live HLDS/ReHLDS bomb, hostage, buy, round, stability, and multi-Bot parity
remain open for Phase 8. The planner emits proposals only and does not claim
plant, defuse, rescue, purchase, round, or engine completion success.
