---
phase: 7
plan: 05
subsystem: team-communication
tags: [team-report, radio, chatter, assignment, uncertainty, generation, sdk-free, x86]
requires:
  - plan: 07-04
    provides: actor/team-scoped round objective observations
provides:
  - bounded team report source, confidence, age, expiry, and generation contracts
  - actor/team-scoped radio and chatter send intents
  - deterministic report selection and radio cooldown boundaries
affects:
  - phase 7 plan 06 behavior scenario replay
  - phase 8 live radio and communication parity acceptance
actuals:
  tasks: 4
  commits: 1
  verification: passed
  requirements-completed: []
  requirements-progress: [PAR-05]
  production-commit: 2bbefc1
---

# Plan 07-05 Summary

Implemented actor/team-scoped communication contracts. `TeamReport` records a
reporter actor and team, source frame, report category, subject generation,
position, confidence, age, and expiry. `TeamReportBoard` has fixed capacity,
same-team ownership, stale-frame rejection, and slot generation watermarks so
old reporter generations cannot be selected after slot reuse. Reports remain
observations: `isConfirmedFact()` is always false and Unknown reports are not
promoted to certainty.

`RadioController` maps permitted reports to bounded Contact, NeedBackup, Bomb,
Hostage, Round, or Chatter send intents. It enforces source team, reporter
generation, report age, confidence, expiry, recipient bounds, and cooldown.
`RadioIntent` contains no delivered-message or gameplay-success state;
delivery and engine message calls remain adapter-owned.

## Files

- `include/astrabot/team/team_report.hpp`
- `src/core/team/team_report.cpp`
- `include/astrabot/team/radio_intent.hpp`
- `src/core/team/radio_intent.cpp`
- `tests/team_report_tests.cpp`
- `tests/radio_intent_tests.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- TDD RED observed: both new tests failed before team/radio headers existed;
  focused Debian GCC14 C++14 builds then passed after implementation.
- Windows x86 portable CTest: 30/30 passed.
- Windows x86 Metamod CTest: 37/37 passed.
- Debian Linux x86 portable CTest: 30/30 passed.
- Debian Linux x86 Metamod CTest: 37/37 passed.
- Phase 6 regression harness: 9/9 checks passed.
- Python regression suite: 6/6 tests passed.
- Source manifest: 114 entries and 107 C/C++ files accepted.
- PE artifact: x86 with 6 exact exports.
- ELF artifact: x86 with 6 exact exports.
- Team/radio Core SDK boundary scan: no Metamod/HLSDK headers, engine
  message calls, hidden enemy coordinates, or dispatch receipt claims found.
- C++ format checks: no leading-space indentation, trailing whitespace, or
  lines over the 120-column recommended limit in the six new C++ files;
  UTF-8 without BOM and LF line endings confirmed.
- `git diff --cached --check`: passed before production commit.
- FocalSpan: final index fresh and ready; queried team report, uncertainty,
  radio intent, recipient, and cooldown contracts.
- code-review-graph: architecture inspection and staged incremental update
  completed. Its 49 static test-gap suggestions are graph evidence only;
  focused tests and 30/37 CTest runs provide execution evidence.

## Issues Encountered

The first CMake Linux run used an older team-report test binary while the
working source had already changed. Rebuilding the explicit m32 target
reproduced the remaining failure and showed x87 exact-float comparison; the
test now uses a bounded tolerance, with temporary diagnostics removed. No
production behavior was changed for that platform difference.

Live HLDS/ReHLDS radio, chatter, team-information, stability, and multi-Bot
parity remain open for Phase 8. These contracts emit observations and send
intents only, not delivered messages or gameplay success.
