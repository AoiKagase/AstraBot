---
phase: 5
plan: 02
subsystem: legacy-nav-reader
tags: [nav, binary-reader, versioned-format, transactional-load, x86]
requires:
  - plan: 05-01
provides:
  - bounded little-endian legacy .nav reader for versions 1 through 5
  - explicit read, corruption, and resource-limit results
  - transactional normalization into NavDocument
affects:
  - 05-03 immutable Nav snapshot and fingerprint
  - 05-04 Metamod file/diagnostic boundary
actuals:
  tasks: 3
  commits: 0
  verification: passed
requirements-completed: []
requirements-progress: [NAV-01, NAV-02, NAV-04, TEST-02]
---

# Plan 05-02 Summary

Added `LegacyNavReader`, a Core-only checked byte cursor for the pinned
ReGameDLL-CS legacy navigation layout. It accepts versions 1 through 5,
decodes little-endian scalar fields, normalizes v1 hiding spots to stable
cover-marked records, reads v2+ hiding spot objects, consumes the v1/v2 old
encounter path layout, retains v3+ encounter IDs and directions, and handles
v4 BSP size and v5 places/area place entries.

The reader uses a bounded file size and area count, rejects section count
overflow before reserve/iteration, distinguishes truncated data from resource
limits and invalid geometry/place data, rejects trailing bytes, computes a
bounded source hash, and only replaces the caller's document after complete
validation. Input bytes remain unchanged and failed reads preserve the prior
document.

## Files

- `include/astrabot/nav/legacy_nav_reader.hpp`
- `src/core/nav/legacy_nav_reader.cpp`
- `include/astrabot/nav/nav_model.hpp`
- `src/core/nav/nav_model.cpp`
- `tests/legacy_nav_reader_tests.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- RED observed before implementation: `legacy_nav_reader.hpp` was absent.
- Debian WSL Linux x86 Reader CTest: 1/1 passed.
- Windows x86 portable Reader CTest: 1/1 passed.
- Linux x86 portable CTest: 12/12 passed.
- Linux x86 Metamod CTest: 19/19 passed.
- Windows x86 portable CTest: 12/12 passed.
- Windows x86 Metamod CTest: 19/19 passed.
- Source manifest: 60 entries, 55 C/C++ files.
- FocalSpan and CRG refreshed against the current Reader source.

## Scope boundary

This plan does not publish a runtime snapshot, compare map/BSP fingerprints,
or load files from the Metamod lifecycle. It never writes legacy `.nav` data;
those concerns are isolated to 05-03/05-04.
