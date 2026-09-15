---
phase: 5
plan: 01
subsystem: nav-model
tags: [nav, sdk-free, validation, bounded-model, x86]
requires:
  - phase: 4
provides:
  - bounded format-neutral NavDocument value model
  - geometry, reference, identity, and duplicate validation results
affects:
  - 05-02 legacy .nav reader
  - 05-03 immutable Nav snapshot
actuals:
  tasks: 3
  commits: 0
  verification: passed
requirements-completed: []
requirements-progress: [NAV-01, NAV-02, NAV-04, TEST-02]
---

# Plan 05-01 Summary

Added the first read-only Nav boundary as an SDK-free value model. The model
contains stable area IDs, extents and corner heights, directed connections,
hiding spots, approach records, encounter spot orders, places, and source
identity. File layout and ReGameDLL types are not part of the public Core
contract.

All model operations use explicit results and named limits. Area geometry must
be finite and non-degenerate; IDs and references must be valid and unique;
per-area vectors and document counts are bounded. Full-document validation
resolves directed area references and globally unique hiding spot identities.
The model uses transactional caller-owned construction so the future reader
can validate a candidate before publication.

## Files

- `include/astrabot/nav/nav_model.hpp`
- `src/core/nav/nav_model.cpp`
- `tests/nav_model_tests.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- RED observed before implementation: `nav_model.hpp` was absent.
- Debian WSL Linux x86 Nav model CTest: 1/1 passed.
- Windows x86 portable Nav model CTest: 1/1 passed.
- Source manifest: 57 entries, 52 C/C++ files.
- C/C++ format checks pass for current project sources.
- FocalSpan and CRG refreshed against the current staged source.

## Scope boundary

This plan does not read or write `.nav` files, publish a runtime snapshot,
perform map fingerprint checks, generate Nav, or add AstraNav authoring.
Those concerns are isolated to 05-02 through 05-04 and later milestones.
