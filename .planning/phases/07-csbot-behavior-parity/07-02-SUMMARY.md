---
phase: 7
plan: 02
subsystem: behavior-objectives
tags: [behavior-state, objective-proposal, generation, recovery, sdk-free, x86]
requires:
  - plan: 07-01
    provides: immutable WorldSnapshot and bounded perception observations
provides:
  - actor-scoped generation-aware behavior state machine
  - deterministic objective proposal ownership, expiry, and selection
  - explicit recovery and transition reason results without action dispatch
affects:
  - phase 7 plan 03 weapon and combat intents
  - phase 7 plan 04 scenario objectives
  - phase 7 plan 05 team reports and communication
  - phase 8 live behavior parity acceptance
actuals:
  tasks: 4
  commits: 1
  verification: passed
  requirements-completed: []
  requirements-progress: [PAR-02, PAR-04, PAR-05]
  production-commit: 549ba95
---

# Plan 07-02 Summary

Implemented the actor-scoped behavior layer above the immutable WorldSnapshot.
`BehaviorStateMachine` accepts only a matching observer/frame identity and
transitions deterministically among Initial, Roam, Seek, Engage, Retreat,
Dead, and Recovering. Confirmed visible hostile contacts can create an Engage
target; only a target previously established by a confirmed observation can
enter bounded Seek memory. Unknown or unavailable life/round information
enters recovery and never creates an objective-completion claim.

`ObjectiveProposal` and `ObjectiveProposalSet` are separate from runtime
commands and completion feedback. Proposals are actor-owned, generation and
frame stamped, fixed-capacity, finite-lifetime values. Selection uses explicit
priority, earliest expiry, newest issue tick, objective, and target tie-breaks;
stale round/map proposals are rejected as stale rather than silently reused.

## Files

- `include/astrabot/behavior/behavior_state.hpp`
- `src/core/behavior/behavior_state.cpp`
- `include/astrabot/behavior/objective_proposal.hpp`
- `src/core/behavior/objective_proposal.cpp`
- `tests/behavior_state_tests.cpp`
- `tests/objective_proposal_tests.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- TDD RED observed: both new tests failed before behavior headers existed;
  focused Debian GCC14 C++14 builds then passed after implementation.
- Windows x86 portable CTest: 24/24 passed.
- Windows x86 Metamod CTest: 31/31 passed.
- Debian Linux x86 portable CTest: 24/24 passed.
- Debian Linux x86 Metamod CTest: 31/31 passed.
- Phase 6 regression harness: 9/9 checks passed.
- Python regression suite: 6/6 tests passed.
- Source manifest: 96 entries and 89 C/C++ files accepted.
- PE artifact: x86 with 6 exact exports.
- ELF artifact: x86 with 6 exact exports.
- Behavior Core SDK boundary scan: no Metamod/HLSDK headers, `edict_t`, or
  engine entity pointers found.
- C++ format checks: no leading-space indentation, trailing whitespace, or
  lines over the 120-column recommended limit in the six new C++ files;
  UTF-8 without BOM and LF line endings confirmed.
- `git diff --cached --check`: passed before production commit.
- FocalSpan: final index fresh and ready; queried behavior state, generation,
  recovery, and objective proposal contracts.
- code-review-graph: full architecture build plus staged incremental review
  update completed. Its 56 test-gap result is static graph inference and is
  superseded for this plan by the focused tests and 24/31 CTest runs.

## Issues Encountered

The first implementation compile caught a missing Perception include in the
behavior test and a namespace closure error in objective proposal source;
both were corrected before the focused GREEN run. The Windows cache again
required explicit MSVC 14.29 and Windows SDK 10.0.26100.0 x86 include/lib
paths because the installed VS 2026 prompt did not populate those variables.

Live HLDS/ReHLDS behavior, combat, multi-Bot stability, and full CSBot parity
remain open for Phase 8. This plan emits proposals only; it does not claim
movement, weapon, dispatch, or objective completion success.
