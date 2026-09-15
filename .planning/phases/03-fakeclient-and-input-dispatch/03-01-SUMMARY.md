---
phase: 03-fakeclient-and-input-dispatch
plan: 01
subsystem: actor-registry-fakeclient
tags: [fakeclient, actor-generation, metamod-p, c++14, x86, windows, linux]
requires:
  - phase: 02-metamod-lifecycle-and-native-guard
    provides: lifecycle generations, public engine table, native gate
provides:
  - fixed-capacity SDK-free ActorRegistry
  - public-boundary FakeClientManager
  - PluginRuntime-owned actor/manager integration boundary
affects:
  - 03-02
  - 03-03
actuals:
  tasks: 4
  commits: 0
  verification: passed
requirements-completed: [LIFE-02, LIFE-04, TEST-02]
---

# Plan 03-01 Summary

Added a bounded `ActorRegistry` with explicit
`Reserved -> Joining -> Joined -> Removing -> Vacant` transitions. Actor
identity combines slot and actor generation, and current checks require the
stored lifecycle token to match. Slot reuse receives a new generation and
stale actors cannot address the reused slot.

Added `FakeClientManager` at the adapter boundary. It checks the Phase 2
managed-Bot gate, invokes public `pfnCreateFakeClient`, validates the slot,
reserves and joins the actor, calls public GameDLL `pfnClientPutInServer`, and
publishes ownership only after the join transition. Removal invalidates the
actor before public disconnect/kick cleanup and restores no hidden state.

## Files

- `include/astrabot/runtime/actor_registry.hpp`
- `src/core/runtime/actor_registry.cpp`
- `include/astrabot/metamod/fake_client_manager.hpp`
- `src/adapter/metamod/fake_client_manager.cpp`
- `src/adapter/metamod/plugin_runtime.hpp`
- `src/adapter/metamod/plugin_runtime.cpp`
- `tests/actor_registry_tests.cpp`
- `tests/fake_client_manager_tests.cpp`
- `docs/source-manifest.json`
- `CMakeLists.txt`

## Verification

- Actor registry RED observed before implementation because the requested
  header/API did not exist; Windows/Linux x86 tests then passed.
- FakeClient manager RED observed for slot reuse; using the lifecycle token's
  connected-state validity fixed the issue and the test passed.
- MSVC `/WX` and Linux `-Werror` caught unsafe CRT formatting and SDK
  `edict_t` byte-clearing; both were removed from the fixture/implementation.
- Manifest checker passed with 26 entries and 21 C/C++ files at the latest
  checkpoint.

## Decisions

- FakeClient manager uses public engine/GameDLL tables only and does not copy
  or include ReGameDLL bot implementation.
- Cleanup tracks whether the current create operation connected the lifecycle
  slot, preventing a failed create from disconnecting an existing actor.
- `PluginRuntime` now owns the registry and manager; command/RunPlayerMove
  dispatch remains the next plan.

## Remaining verification

Full Phase 3 verification is not complete. Command queue, RunPlayerMove
dispatch, multi-actor receipt tests, and live movement acceptance remain
pending in Plans 03-02/03.
