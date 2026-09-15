---
phase: 5
plan: 03
subsystem: nav-snapshot-fingerprint
tags: [nav, snapshot, fingerprint, revision, x86]
requires:
  - plan: 05-02
provides:
  - immutable const NavSnapshot handles with explicit map generation and revision
  - transactional NavSnapshotPublisher invalidation and replacement
  - bounded SDK-free map/source fingerprint construction and comparison
affects:
  - 05-04 Metamod Nav loader diagnostics
  - future AstraNav codec and derived-data boundaries
actuals:
  tasks: 3
  commits: 0
  verification: passed
requirements-completed: []
requirements-progress: [NAV-02, NAV-03, NAV-04, TEST-02]
---

# Plan 05-03 Summary

Added `NavSnapshotPublisher`, which validates a complete `NavDocument`, copies
it behind a `shared_ptr<const NavDocument>`, and publishes only the immutable
handle. Invalid or null replacements preserve the previous document and
revision. Map invalidation clears the current handle and advances the
explicit revision; old handles remain read-only but are no longer current.

Added a Metamod-side but SDK-free `MapFingerprint` value boundary. Map names
are bounded, trimmed, normalized to ASCII lowercase, and have a trailing
`.bsp` suffix removed. Comparisons return distinct map-name, BSP-size, and Nav
source-hash mismatch results without reading or writing files.

## Files

- `include/astrabot/nav/nav_snapshot.hpp`
- `src/core/nav/nav_snapshot.cpp`
- `include/astrabot/metamod/map_fingerprint.hpp`
- `src/adapter/metamod/map_fingerprint.cpp`
- `tests/nav_snapshot_tests.cpp`
- `tests/map_fingerprint_tests.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- RED observed before implementation: CMake could not find the new snapshot
  and fingerprint source files.
- Windows x86 portable CTest: 14/14 passed.
- Windows x86 Metamod CTest: 21/21 passed.
- Debian WSL Linux x86 portable CTest: 14/14 passed.
- Debian WSL Linux x86 Metamod CTest: 21/21 passed.
- Source manifest: 66 entries, 61 C/C++ files.
- C++ diff whitespace, CR, BOM, and SDK-boundary checks passed.
- FocalSpan refreshed with 119 files, 1037 symbols, and a fresh index.

## Scope boundary

This plan does not load files from the Metamod lifecycle or publish runtime
diagnostics. It does not write legacy `.nav` data, generate AstraNav files,
compare hidden world state, or mix actor state into Nav identity. Those
concerns remain isolated to 05-04 and future milestones.
