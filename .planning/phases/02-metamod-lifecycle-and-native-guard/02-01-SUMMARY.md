---
phase: 02-metamod-lifecycle-and-native-guard
plan: 01
subsystem: metamod-lifecycle
tags: [metamod-p, hl_sdk, c++14, x86, windows, linux, hook-table]
requires:
  - phase: 01-foundation-and-abi
    provides: pinned SDK include boundary and six-export ABI contract
provides:
  - adapter-owned PluginRuntime lifecycle context
  - Metamod pre-hook table for five lifecycle callbacks
  - cross-platform hook-table contract test
affects:
  - 02-02
  - 02-03
actuals:
  tasks: 3
  commits: 0
  verification: passed
requirements-completed: [LIFE-01, TEST-02]
---

# Plan 02-01 Summary

Implemented the first lifecycle slice without creating Bots or touching
private ReGameDLL symbols. `Meta_Attach` now owns an explicit adapter context,
publishes the public `GetEntityAPI2` and `GetEngineFunctions` providers, and
the entity API provider publishes `ClientDisconnect`, `ClientPutInServer`,
`ServerActivate`, `ServerDeactivate`, and `StartFrame` hooks. Detach clears
all stored Metamod/global pointers before returning.

## Files

- `src/adapter/metamod/plugin_runtime.hpp`
- `src/adapter/metamod/plugin_runtime.cpp`
- `src/adapter/metamod/plugin_exports.cpp`
- `tests/metamod_hook_table_tests.cpp`
- `CMakeLists.txt`

## Verification

- RED observed before implementation: the hook-table test reported
  `GetEntityAPI2 hook provider` as missing.
- Windows x86 Debug: `astrabot_metamod_abi` and
  `astrabot_metamod_hooks` passed, 2/2.
- Debian WSL Linux x86 Debug: `astrabot_metamod_abi` and
  `astrabot_metamod_hooks` passed, 2/2.
- FocalSpan updated after the source change and returned the new
  `PluginRuntime`/export context.
- CRG current-HEAD update found the export integration points; its C++
  test-gap report is retained for final review because untracked files are
  not included in its changed-file set until staged.

## Decisions

- Lifecycle callbacks use a single `PluginRuntime` owner rather than global
  state spread across each export.
- `GetEngineFunctions` remains empty until an engine hook is implemented;
  native command/CVar guarding belongs to Plan 02-03.
- `GiveFnptrsToDll` records non-owning pointers only and performs no engine
  call.

## Next readiness

Plan 02-02 can add deterministic SDK-free generation state and route the
existing callbacks to value observations without changing the Metamod ABI.
