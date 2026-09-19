---
phase: 8
plan: 02
subsystem: differential-replay
tags: [differential, replay, provenance, uncertainty, offline, phase8]
requires:
  - plan: 08-01
provides:
  - bounded source-independent differential trace schema
  - deterministic observable replay comparator
  - forbidden/invalid trace rejection coverage
affects:
  - 08-03 live-server acceptance
  - 08-04 final audit
actuals:
  tasks: 4
  verification: passed
  requirements-completed: []
  requirements-progress: [TEST-03]
production-commit: uncommitted-working-tree-checkpoint
---

# Plan 08-02 Summary

Added `tests/phase8_differential.py` and the versioned
`tests/fixtures/phase8/differential-observable.json` contract. The harness
validates pinned source identities, source-independent provenance, map/round/
tick/actor-generation alignment, finite bounded timing, explicit Unknown
states, and an allowlisted observable surface for lifecycle, behavior,
objective, perception, combat, and team radio values.

The replay comparator reports deterministic mismatch paths and never promotes
an intent to an engine receipt, delivered message, damage result, or objective
completion. It rejects missing reference provenance, hidden-state fields,
actor-generation drift, non-finite timing, unbounded traces, and candidate /
reference identity drift. The checked-in sample is intentionally synthetic and
prints `TEST-03 pending reference capture`.

## Files

- `tests/phase8_differential.py`
- `tests/fixtures/phase8/README.md`
- `tests/fixtures/phase8/differential-observable.json`
- `docs/source-manifest.json`
- `.planning/ROADMAP.md`
- `.planning/STATE.md`

## Verification

- Positive differential replay: passed twice with identical output.
- Six malformed/forbidden negative cases: rejected.
- Allowed observable mismatch: reported with deterministic path.
- Source manifest: 114 entries / 107 C/C++ files.
- Phase 7 scenarios: 7/7 passed.
- FocalSpan: updated and queried after the change.
- CRG: current HEAD matched `5a62a98`; source graph used for impact/navigation review.

## Issue fixed

The first run compared tick progression as if it were actor identity. The
validator was corrected to compare actor slot/generation separately and to
check tick monotonicity independently. The positive and negative suite then
passed.

## Boundary

The fixture is a synthetic contract sample, not a captured CSBot trace.
TEST-03 remains pending until a real pinned reference trace and corresponding
AstraBot trace are captured and reviewed. Live-server acceptance remains open.
