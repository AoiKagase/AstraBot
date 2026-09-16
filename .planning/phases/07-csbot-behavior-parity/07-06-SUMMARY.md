---
phase: 7
plan: 06
subsystem: scenario-replay-verification
tags: [scenario, replay, fixtures, provenance, uncertainty, offline, phase8-boundary]
requires:
  - plan: 07-05
    provides: actor/team-scoped reports and radio intents
provides:
  - provenance-labeled deterministic Phase 7 contract fixtures
  - malformed, stale, unknown, and forbidden-claim rejection harness
  - explicit offline-complete versus live/differential-pending tracker state
affects:
  - phase 8 differential and real-server acceptance
actuals:
  tasks: 4
  commits: 1
  verification: passed
  requirements-completed: []
  requirements-progress: [PAR-02, PAR-03, PAR-04, PAR-05]
  production-commit: f023a93
---

# Plan 07-06 Summary

Added three source-independent, versioned scenario fixtures for visibility and
sound uncertainty, round/objective recovery, and combat intent boundaries.
Each trace carries map/round/tick and actor-generation identity plus only
bounded observations and expected contract outcomes. Provenance explicitly
identifies the fixtures as synthetic behavior-only traces; no reference
source, private symbol, hidden enemy coordinate, delivery result, or engine
success claim is included.

`tests/phase7_scenarios.py` validates the fixture schema, finite and bounded
values, monotonic frames, Unknown versus observed state, and deterministic
expected outcomes. Its self-checks reject incomplete, stale, Unknown-with-
position, and forbidden engine-success claims. The harness is intentionally a
contract/replay gate; it does not label offline results as live CSBot parity.

PAR-02 through PAR-05 now document that their Phase 7 offline contracts and
fixtures are complete while their live parity remains pending. PAR-06,
TEST-03, and TEST-04 remain pending for Phase 8 as required.

## Files

- `tests/phase7_scenarios.py`
- `tests/fixtures/phase7/README.md`
- `tests/fixtures/phase7/visibility-uncertainty.json`
- `tests/fixtures/phase7/objective-recovery.json`
- `tests/fixtures/phase7/combat-boundary.json`
- `docs/source-manifest.json`
- `.planning/REQUIREMENTS.md`

## Verification

- TDD/negative gate: malformed and incomplete fixture rejection observed;
  final harness passed with 7 checks.
- Windows x86 portable CTest: 30/30 passed.
- Windows x86 Metamod CTest: 37/37 passed.
- Debian Linux x86 portable CTest: 30/30 passed.
- Debian Linux x86 Metamod CTest: 37/37 passed.
- Phase 6 regression harness: 9/9 checks passed.
- Python regression suite: 6/6 tests passed.
- Source manifest: 114 entries and 107 C/C++ files accepted.
- PE artifact: x86 with 6 exact exports.
- ELF artifact: x86 with 6 exact exports.
- Fixture format and JSON parsing: passed through the Phase 7 harness and
  Python compile check.
- C++ source format／SDK boundary checks for the preceding behavior phases:
  passed; no Core SDK or engine dispatch coupling added by the harness.
- `git diff --cached --check`: passed before the fixture commit.
- FocalSpan: final index updated and ready; queried scenario/replay,
  provenance, Unknown-state rejection, and Phase 8 boundary contracts.
- code-review-graph: staged incremental impact update completed. Its 21
  static Python-symbol test-gap suggestions and risk 0.60 are advisory graph
  evidence; the harness and complete offline matrix are execution evidence.

## Issues Encountered

The first harness negative test revealed that Unknown actor positions were not
being rejected by the validator. `validate_position` now rejects coordinates
for all unconfirmed states, while remembered positions remain allowed only in
their separately labeled memory record. No production Core behavior was
changed for this verifier correction.

The final offline gate does not prove HLDS/ReHLDS movement, combat, radio,
objective, stability, differential, or multi-Bot acceptance. Those remain
Phase 8 live evidence, and TEST-03, TEST-04, and PAR-06 were intentionally not
marked complete.
