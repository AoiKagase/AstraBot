# CSSDK Participation Rebuild Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the current direct FakeClient team/class command path with the old CSSDK-backed menu state machine so managed Bots leave the spectator camera and join normally.

**Architecture:** Copy the old CSSDK snapshot as a vendored, read-only SDK boundary. Port the old CStrike join-state and message semantics into a current-adapter controller that uses `runtime::ActorId`, `LifecycleToken`, and `edict_t` bindings. Remove the current one-shot `pendingTeamJoins_` flow while retaining current FakeClient ownership, actor registry, movement, and cleanup.

**Tech Stack:** C++17, CMake/NMake, Metamod-P/HLSDK public tables, vendored CSSDK headers, Windows x86 Debug DLL.

**Spec:** `docs/superpowers/specs/2026-09-17-cssdk-participation-rebuild-design.md`

## Global Constraints

- Copy `AstraBot_bk/third_party/cssdk` wholesale, preserving its license and provenance files.
- Do not copy or link ReGameDLL private bot implementation.
- Keep current actor/lifecycle ownership and public Metamod/GameDLL boundaries.
- Preserve unrelated dirty files and stage explicit paths only.
- Do not build or run test targets before the real-server acceptance gate; verify with static checks, DLL build, export inspection, and later live HLDS evidence.

## File Map

- Create: `third_party/cssdk/**` — copied CSSDK headers, sources, license, README, and provenance.
- Create: `include/astrabot/metamod/join_controller.hpp` — current-adapter join state interface.
- Create: `src/adapter/metamod/join_controller.cpp` — CSSDK/message-driven join state implementation.
- Modify: `src/adapter/metamod/fake_client_manager.cpp` — old CSSDK-compatible userinfo metadata.
- Modify: `src/adapter/metamod/plugin_runtime.hpp` — join-controller state and callbacks.
- Modify: `src/adapter/metamod/plugin_runtime.cpp` — replace direct pending join path and route menu/message events.
- Modify: `src/adapter/metamod/compat_surface.cpp` — preserve command registration names used by the controller.
- Modify: `CMakeLists.txt` — include CSSDK and join-controller source for `astrabot_mm`.
- Modify: `docs/source-manifest.json` — register copied source files when the manifest requires it.
- Create/modify: focused contract coverage for menu sequencing and team/class confirmation, without running test targets under the current gate.

## Task 1: Vendor the old CSSDK

**Files:**

- Create: `third_party/cssdk/**`
- Create: `third_party/cssdk/CSSDK-PROVENANCE.md`

- [ ] Copy the complete old tree with `Copy-Item -Recurse -Force` and verify file count against the old tree.
- [ ] Preserve `LICENSE`, `README.md`, and provenance text exactly.
- [ ] Run `git diff --check` and confirm no generated build files entered the vendor tree.

## Task 2: Define the current join-controller boundary

**Files:**

- Create: `include/astrabot/metamod/join_controller.hpp`
- Create: `src/adapter/metamod/join_controller.cpp`

**Interfaces:**

- Consumes: `runtime::ActorId`, `runtime::LifecycleToken`, bound `edict_t`, current frame number, and decoded menu/message events.
- Produces: `JoinAction` values (`NoOp`, `SendMenuSelect`, `Joined`, `Failed`, `Cancelled`) with a bounded selection value and failure reason.

- [ ] Port the old phase model: waiting for team menu, team selection, waiting for class menu, class selection, confirmation, joined, failed, cancelled.
- [ ] Port old retry and timeout limits, adapting old host tick types to `std::uint32_t` adapter frames.
- [ ] Keep the controller SDK-free except for message/event types; isolate CSSDK includes in the adapter implementation.
- [ ] Add a focused source contract that proves the controller cannot emit an unbounded retry loop.

## Task 3: Replace direct team/class dispatch

**Files:**

- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp`

- [ ] Remove `PendingTeamJoin`, `pendingTeamJoins_`, `assignBotTeam`, `processPendingTeamJoins`, and the direct `jointeam`/`joinclass` sequence.
- [ ] Start a `JoinController` when `bot_add`, `bot_add_t`, or `bot_add_ct` creates a managed actor.
- [ ] Route `ShowMenu`/`VGUIMenu` and text/byte message decoding into the controller.
- [ ] Dispatch controller `SendMenuSelect` actions through the existing public `pfnClientCommand` boundary using `menuselect` and the selected option.
- [ ] Confirm joined state from team/class feedback before reporting successful participation.
- [ ] Cancel and invalidate the controller on disconnect, map deactivate, actor generation change, timeout, or failed command dispatch.

## Task 4: Match old FakeClient metadata

**Files:**

- Modify: `src/adapter/metamod/fake_client_manager.cpp`

- [ ] Preserve current public callback order and actor reservation.
- [ ] Add the old join-compatible userinfo keys, including `_vgui_menus=0`, `_ah=0`, and `*bot=1`, plus the stable rate/update-rate metadata required by CSSDK.
- [ ] Keep cleanup idempotent and ensure metadata setup failures roll back the actor and FakeClient.

## Task 5: Wire the build and manifest

**Files:**

- Modify: `CMakeLists.txt`
- Modify: `docs/source-manifest.json`

- [ ] Add `third_party/cssdk` include paths only to the Metamod adapter target.
- [ ] Add `join_controller.cpp` to `astrabot_mm`; do not add CSSDK to Core targets.
- [ ] Run the source manifest checker statically and inspect `git diff --check`.

## Task 6: Build, deploy, and verify

- [ ] Initialize Visual Studio with `VsDevCmd.bat -arch=x86 -host_arch=x64 -vcvars_ver=14.29` to match the existing CMake cache.
- [ ] Build only `cmake --build build-metamod-x86-test --target astrabot_mm`.
- [ ] Verify PE x86 architecture and exact exports with `tools/verify_x86_artifact.py`.
- [ ] Back up the deployed DLL and copy the new artifact to `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll`.
- [ ] Verify source/deployed SHA-256 equality.
- [ ] Record live acceptance separately: `bot_enable 1`, `bot_quota 20`, `bot_add`, `bot_add_t`, `bot_add_ct`, normal team/class state, camera exit, and `sv_restart 1` recovery.
