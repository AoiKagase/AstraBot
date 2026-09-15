---
phase: 03-fakeclient-and-input-dispatch
plan: 02
subsystem: command-queue-input-dispatch
tags: [command, receipt, runplayermove, actor-generation, sdk-free, x86]
requires:
  - plan: 03-01
    provides: ActorRegistry and public FakeClient manager
provides:
  - SDK-free BotCommand and CommandReceipt values
  - fixed-capacity actor-isolated CommandQueue
  - main-thread InputDispatcher for public RunPlayerMove
affects:
  - 03-03
actuals:
  tasks: 4
  commits: 0
  verification: passed
requirements-completed: [LIFE-02, LIFE-04, TEST-02]
---

# Plan 03-02 Summary

Added SDK-free `BotCommand`, `MovementInput`, `ViewAngles`, and
`CommandReceipt` values. `CommandQueue` is fixed-capacity and validates
current lifecycle/actor generations, finite and bounded movement values,
strictly increasing per-actor sequences, queue capacity, and actor-specific
FIFO consumption.

Added `InputDispatcher` as the only Phase 3 path to public
`pfnRunPlayerMove`. It binds validated actor/edict pairs, drains one actor's
commands, rechecks current map/round/slot generations before dispatch, and
returns explicit `Dispatched`, `NoCommand`, `StaleActor`,
`EngineUnavailable`, or `InvalidCommand` receipts.

## Files

- `include/astrabot/runtime/bot_command.hpp`
- `src/core/runtime/bot_command.cpp`
- `include/astrabot/runtime/command_queue.hpp`
- `src/core/runtime/command_queue.cpp`
- `include/astrabot/metamod/input_dispatcher.hpp`
- `src/adapter/metamod/input_dispatcher.cpp`
- `src/adapter/metamod/plugin_runtime.hpp`
- `src/adapter/metamod/plugin_runtime.cpp`
- `tests/command_queue_tests.cpp`
- `tests/input_dispatcher_tests.cpp`
- `docs/source-manifest.json`
- `CMakeLists.txt`

## Verification

- RED observed before implementation because the queue header was absent.
- Queue RED found stale commands were accepted after a round transition;
  adding current `LifecycleSession` token validation fixed it.
- Dispatcher RED initially expected a pre-round actor to remain current;
  the test was corrected to require a fresh actor/token after round
  invalidation.
- Windows x86 Debug: full Metamod CTest 9/9 and portable CTest 5/5 passed.
- Debian WSL Linux x86 Debug: full Metamod CTest 9/9 and portable CTest 5/5
  passed.
- PE and ELF artifact verifiers passed with x86 and six exact exports.
- Source manifest passed with 34 entries and 29 C/C++ files.

## Decisions

- Queue validates both the registry's actor record and the lifecycle
  session's current token; saved historical stamps are never authoritative.
- `InputDispatcher` does not own or expose SDK-free entity pointers to Core;
  the edict binding remains adapter-only.
- `RunPlayerMove` is not called when the actor is stale, unbound, the queue
  is empty, or the engine function is unavailable.

## Remaining verification

Phase 3 is not complete until Plan 03-03 adds the full multi-actor fake-host
integration and final phase verification. Live HLDS/ReHLDS movement remains
outside this offline checkpoint.
