---
phase: 4
plan: 01
subsystem: csbot-compatibility-surface
tags: [compatibility, commands, cvars, metamod, native-guard]
requires:
  - phase: 3
provides:
  - SDK-free bot_* command registry and desired-state CVar model
  - AstraBot-owned Metamod command registration boundary
affects:
  - 04-02 profile catalog and loader
  - 04-03 actor command execution
actuals:
  tasks: 4
  commits: 0
  verification: passed
requirements-completed: [COMP-01, TEST-02]
---

# Plan 04-01 Summary

Implemented the public CSBot compatibility contract without importing native
ReGameDLL-CS bot classes or private symbols. Core command parsing and desired
CVar state are SDK-free and return explicit results for invalid input. The
Metamod surface registers only AstraBot-owned callbacks while the native guard
continues to block native or unknown bot ownership.

## Files

- `include/astrabot/compat/command_registry.hpp`
- `src/core/compat/command_registry.cpp`
- `include/astrabot/compat/cvar_state.hpp`
- `src/core/compat/cvar_state.cpp`
- `include/astrabot/metamod/compat_surface.hpp`
- `src/adapter/metamod/compat_surface.cpp`
- `src/adapter/metamod/plugin_runtime.cpp`
- `src/adapter/metamod/plugin_runtime.hpp`
- `tests/compat_command_registry_tests.cpp`
- `tests/compat_cvar_state_tests.cpp`
- `tests/compat_surface_tests.cpp`

## Verification

- RED observed before implementation: new compatibility headers were absent.
- Windows x86 Metamod CTest: 15/15 passed.
- Windows x86 portable CTest: 9/9 passed.
- Debian WSL Linux x86 Metamod CTest: 15/15 passed.
- Debian WSL Linux x86 portable CTest: 9/9 passed.
- Invalid command/CVar input and AstraBot/native registration ownership are
  covered by direct value tests.

## Scope boundary

The commands resolve to actions and desired state only. Actor creation,
profile selection during bot addition, navigation, and live HLDS/ReHLDS
acceptance remain later work.
