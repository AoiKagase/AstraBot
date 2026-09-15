---
phase: 02-metamod-lifecycle-and-native-guard
plan: 03
subsystem: native-bot-guard
tags: [native-csbot, cvar, fakeclient, metamod-p, x86, visibility]
requires:
  - plan: 02-02
    provides: lifecycle callback wiring, slot bounds, generation diagnostics
provides:
  - SDK-free native ownership decision layer
  - public bot CVar suppression and FakeClient scan
  - exact six-export PE/ELF artifact boundary
affects:
  - phase-3-fakeclient-input-dispatch
  - phase-4-csbot-compatibility-surface
actuals:
  tasks: 5
  commits: 0
  verification: passed
requirements-completed: [LIFE-03, TEST-02]
---

# Plan 02-03 Summary

Implemented `NativeBotGuard` without linking or including ReGameDLL-CS
internals. The adapter uses public `bot_enable`/`bot_quota` getter/setter
functions, blocks the pinned native bot command registration names through
`pfnAddServerCommand`, scans only the bounded client edict range for
`FL_FAKECLIENT`, and fails closed for missing boundary capability, active
native controls, invalid observations, or unmanaged fake clients. Original
public CVar values are restored during detach after AstraBot ownership is
disabled.

## Files

- `include/astrabot/metamod/native_bot_guard.hpp`
- `src/adapter/metamod/native_bot_guard.cpp`
- `src/adapter/metamod/plugin_runtime.hpp`
- `src/adapter/metamod/plugin_runtime.cpp`
- `tests/native_bot_guard_tests.cpp`
- `tests/metamod_hook_table_tests.cpp`
- `include/astrabot/metamod/abi_contract.hpp`
- `src/adapter/metamod/plugin_exports.cpp`
- `docs/source-manifest.json`
- `CMakeLists.txt`

## Verification

- RED observed before implementation: the native guard test could not
  configure because `native_bot_guard.cpp` was absent.
- Windows x86 Debug: full CTest passed 5/5; PE verifier reported x86 and six
  exact exports.
- Debian WSL Linux x86 Debug: full CTest passed 5/5; ELF verifier reported
  x86 and six exact exports.
- Source manifest checker passed with 20 entries and 15 C/C++ files.
- FocalSpan dependency indexes for ReGameDLL-CS and Metamod-P are available;
  AstraBot index was refreshed after the implementation.

## Decisions

- Native suppression is based on public controls and observation, not a
  private `TheBots`/`CCSBotManager` pointer or a copied bot header.
- Linux target visibility is hidden for internal C++ symbols and
  `GiveFnptrsToDll` is exported with the SDK-compatible conditional form so
  Windows `.def` and Linux ELF each expose exactly six names.
- An unmanaged fake client disables managed creation; this phase has no
  managed ownership slots yet, leaving that registry to Phase 3.

## Remaining boundary

This is offline adapter evidence. Loading the DLL into an actual unmodified
HLDS/ReHLDS instance and proving native-spawn race behavior remain live
acceptance items, not claims of this summary.
