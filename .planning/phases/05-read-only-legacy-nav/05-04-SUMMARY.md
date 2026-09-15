---
phase: 5
plan: 04
subsystem: nav-loader-diagnostics
tags: [nav, metamod, read-only, diagnostics, lifecycle, x86]
requires:
  - plan: 05-03
provides:
  - bounded adapter-owned legacy .nav file loading
  - map-aware diagnostics and transactional immutable publication
  - map lifecycle invalidation and current-map load attempt
affects:
  - phase 5 verification and requirement traceability
  - phase 6 baseline locomotion
actuals:
  tasks: 4
  commits: 0
  verification: passed
requirements-completed: []
requirements-progress: [NAV-01, NAV-02, NAV-03, NAV-04, TEST-02]
---

# Plan 05-04 Summary

Added `NavLoader` as the adapter-owned, read-only file boundary. It bounds the
path and file size, reads at most one byte over the configured limit, maps all
Core reader results to explicit adapter diagnostics, and keeps the candidate
document local until map name, version-specific BSP metadata, and optional
source hash checks pass. Only then does it publish through
`NavSnapshotPublisher`; missing, corrupt, oversized, or mismatched input does
not replace an existing snapshot.

Added bounded `NavLoadDiagnostic` identity fields and exposed the loader,
diagnostic, and immutable snapshot through `PluginRuntime`. Map activation
invalidates the previous Nav generation and attempts the current map's
`maps/<map>.nav` through public engine metadata when available. Map
deactivation, detach, and reattach clear stale Nav state. No legacy `.nav`
write path, AstraNav authoring, private GameDLL symbol, or ReAPI dependency
was added.

## Files

- `include/astrabot/metamod/nav_loader.hpp`
- `src/adapter/metamod/nav_loader.cpp`
- `src/adapter/metamod/astrabot_mm.map`
- `src/adapter/metamod/plugin_runtime.hpp`
- `src/adapter/metamod/plugin_runtime.cpp`
- `tests/nav_loader_tests.cpp`
- `tests/nav_diagnostics_tests.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- RED observed before implementation: CMake could not find
  `src/adapter/metamod/nav_loader.cpp`.
- Windows x86 portable CTest: 16/16 passed.
- Windows x86 Metamod CTest: 23/23 passed.
- Debian WSL Linux x86 portable CTest: 16/16 passed.
- Debian WSL Linux x86 Metamod CTest: 23/23 passed.
- Source manifest: 71 entries, 65 C/C++ files.
- PE/ELF x86 artifact verification: six exact exports each.
- Linux linker version script keeps implementation symbols out of the public ABI.
- C++ diff whitespace, CR, BOM, and SDK-boundary checks passed.

## Scope boundary

Phase 5 remains read-only and offline. Real HLDS/ReHLDS map loading,
locomotion, combat, multi-Bot stability, and live acceptance remain later
verification layers. AstraNav generation/editing/serialization and adaptive
AI remain deferred milestones.
