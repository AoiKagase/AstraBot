---
phase: 4
plan: 03
subsystem: csbot-actor-commands
tags: [compatibility, actor-commands, quota, native-guard, generation, x86]
requires:
  - plan: 04-02
provides:
  - BotConfiguration authorization for desired state and quota
  - generation-safe bot_add, bot_kick, and bot_kill runtime routing
  - managed actor name/handle tracking with collision rejection
affects:
  - phase 5 read-only Nav loading
  - later CSBot behavior parity
actuals:
  tasks: 5
  commits: 0
  verification: passed
requirements-completed: [COMP-01, COMP-02, COMP-03, TEST-02]
---

# Plan 04-03 Summary

Connected the compatibility surface to the Phase 3 actor lifecycle. Desired
CVar state is now owned by `BotConfiguration`, which authorizes add requests
without mutating quota or actor state. `PluginRuntime` executes parsed
commands on the main thread: bot additions select a profile and create a
FakeClient only after native-guard, enable/stop, quota, and name-collision
checks; bot kicks use the existing generation-safe removal path; bot kills
use the public GameDLL `pfnClientKill` callback while retaining the actor.

Managed handles and profile names are stored in fixed arrays. External
disconnects, successful removals, map deactivation, and guard reset clear the
corresponding ownership records. Failed create/remove/kill operations do not
advance managed ownership state.

## Files

- `include/astrabot/compat/bot_configuration.hpp`
- `src/core/compat/bot_configuration.cpp`
- `include/astrabot/metamod/compat_surface.hpp`
- `src/adapter/metamod/compat_surface.cpp`
- `include/astrabot/metamod/fake_client_manager.hpp`
- `src/adapter/metamod/fake_client_manager.cpp`
- `src/adapter/metamod/plugin_runtime.hpp`
- `src/adapter/metamod/plugin_runtime.cpp`
- `tests/bot_configuration_tests.cpp`
- `tests/compat_actor_command_tests.cpp`
- `docs/source-manifest.json`

## Verification

- RED observed before implementation: `bot_configuration.hpp` was absent.
- Linux x86 portable CTest: 10/10 passed.
- Linux x86 Metamod CTest: 17/17 passed.
- Windows x86 portable CTest: 10/10 passed.
- Windows x86 Metamod CTest: 17/17 passed.
- Python provenance/artifact tests: 6/6 passed.
- Source manifest: 54 entries, 49 C/C++ files.
- PE and ELF release artifacts: x86 with six exact exports each.
- FocalSpan and CRG indexes refreshed against the current compatibility
  implementation.

## Scope boundary

This slice proves offline configuration and actor-operation contracts only.
It does not claim live HLDS/ReHLDS loading, locomotion, combat, objectives,
legacy `.nav` loading, or complete CSBot/ZBot behavioral parity.
