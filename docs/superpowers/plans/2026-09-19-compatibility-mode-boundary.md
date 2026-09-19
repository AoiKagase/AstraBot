# Compatibility / Enhanced Mode Boundary Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` for inline execution or `superpowers:subagent-driven-development` for delegated execution. Execute only this P01 plan.

**Goal:** Add an explicit, observable Compatibility/Enhanced runtime mode and prove that documented Astra-only decision capabilities are disabled in Compatibility Mode without claiming CSBot parity.

**Architecture:** Extend the existing `CvarState` → `BotConfiguration` → `CompatibilitySurface` configuration path with one `RuntimeMode` value and the `astrabot_mode` server CVar. Derive a pure `RuntimeModePolicy` from that value, expose it in `PluginRuntime::Snapshot`, and keep current Nav/Combat/Objective controllers as shared baseline candidates. P01 adds no new intelligence and does not change timing, RNG, private-state, NAV, combat, or objective behavior.

**Tech Stack:** C++14, CMake/NMake, Windows x86 MSVC, existing portable C++ test executables, Metamod/HLSDK adapter test harness, Markdown parity artifacts.

**Spec:** `docs/superpowers/specs/2026-09-19-compatibility-mode-boundary-design.md`

## Global Constraints

- Execute P01 only; do not start P02 or any behavioral parity phase.
- Preserve all existing dirty-worktree changes and stage explicit P01 paths only.
- Keep `RuntimeMode::Compatibility` as the default.
- Do not classify any existing behavior as `MATCH` merely because a mode boundary exists.
- Do not copy ReGameDLL source code into AstraBot.
- Compatibility Mode must bypass enhanced capability paths before they can mutate state, consume RNG, or alter scheduling.
- Rebuild and test with the cached Windows x86 NMake/MSVC environment.

## Current files and responsibilities

| File | Planned responsibility |
|---|---|
| `include/astrabot/compat/cvar_state.hpp` | Add `RuntimeMode` and `CvarSnapshot::mode` |
| `src/core/compat/cvar_state.cpp` | Parse `astrabot_mode`, preserve state on invalid values |
| `include/astrabot/compat/runtime_mode_policy.hpp` | Define pure capability policy derived from `RuntimeMode` |
| `src/core/compat/runtime_mode_policy.cpp` | Implement policy predicates |
| `CMakeLists.txt` | Add policy source and policy test target |
| `tests/compat_cvar_state_tests.cpp` | Test default, valid mode changes, invalid-value immutability |
| `tests/runtime_mode_policy_tests.cpp` | Test all enhanced capabilities in both modes |
| `src/adapter/metamod/plugin_runtime.hpp` | Expose mode in `PluginRuntime::Snapshot` |
| `src/adapter/metamod/plugin_runtime.cpp` | Register/sync `astrabot_mode`, expose mode, log diagnostics |
| `tests/metamod_hook_table_tests.cpp` | Verify active runtime defaults to observable Compatibility Mode |
| `docs/parity/SOURCE_MAP.md` | Record configuration-to-runtime boundary |
| `docs/parity/PARITY_MATRIX.md` | Record mode-boundary status without MATCH promotion |
| `docs/parity/KNOWN_DEVIATIONS.md` | Replace the unestablished-mode finding with P01 evidence and remaining gaps |
| `docs/parity/STATUS.md` | Mark P01 complete and P02 pending only after all gates pass |

### Task 1: Add failing mode configuration tests

**Files:**

- Modify: `tests/compat_cvar_state_tests.cpp`
- Create: `tests/runtime_mode_policy_tests.cpp`
- Modify: `CMakeLists.txt` only if the new test target must be registered in this task

**Interfaces:**

- Consumes: the planned `astrabot::compat::RuntimeMode` and `RuntimeModePolicy` names from the approved spec.
- Produces: executable assertions that later implementation must satisfy.

- [ ] **Step 1: Extend the existing CVar test with the desired mode contract.**

  Add assertions for:

  - a default `CvarState` snapshot has `RuntimeMode::Compatibility`;
  - `setString("astrabot_mode", "enhanced")` returns `Updated` and changes the snapshot;
  - `setString("astrabot_mode", "COMPATIBILITY")` returns `Updated` and restores Compatibility;
  - an invalid mode returns `InvalidValue` and leaves the prior mode unchanged.

- [ ] **Step 2: Add the policy isolation test.**

  Test a `RuntimeModePolicy` constructed from each mode. Assert that every documented capability is false for Compatibility and true for Enhanced:

  ```cpp
  check(!compatibility.allowsEnhancedDecisionOverrides());
  check(!compatibility.allowsAdaptiveRouteWeighting());
  check(!compatibility.allowsOpponentProfileDecisionChanges());
  check(!compatibility.allowsTacticalTeamOverrides());
  check(!compatibility.allowsLearningSideEffects());
  ```

- [ ] **Step 3: Register the policy test target if needed and run the focused tests.**

  Run inside the existing x86 developer environment:

  ```text
  cmake --build build-action-adapter-x86-1451 --config Debug --target astrabot_compat_cvar_state
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_compat_cvar_state"
  cmake --build build-action-adapter-x86-1451 --config Debug --target astrabot_runtime_mode_policy
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_runtime_mode_policy"
  ```

  Expected RED result: the new mode type, parser, policy, or target is not yet implemented. Fix only test/target registration errors until the failure is specifically about the missing mode behavior.

### Task 2: Implement the pure mode value and policy

**Files:**

- Modify: `include/astrabot/compat/cvar_state.hpp`
- Modify: `src/core/compat/cvar_state.cpp`
- Create: `include/astrabot/compat/runtime_mode_policy.hpp`
- Create: `src/core/compat/runtime_mode_policy.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

- Consumes: failing tests from Task 1.
- Produces: `RuntimeMode`, `CvarSnapshot::mode`, `CvarState::setString("astrabot_mode", ...)`, and pure `RuntimeModePolicy` predicates.

- [ ] **Step 1: Add the mode enum and snapshot field.**

  Keep the enum in `astrabot::compat` with exactly two values: `Compatibility` and `Enhanced`. Initialize `CvarState` to Compatibility without changing existing CVar defaults.

- [ ] **Step 2: Implement strict, case-insensitive mode parsing.**

  Accept only `compatibility` and `enhanced` (case-insensitive). Return `Unknown` for an unrelated CVar and `InvalidValue` for a known mode with another value. Do not mutate `state_.mode` on invalid input.

- [ ] **Step 3: Implement the pure policy.**

  Give `RuntimeModePolicy` a constructor taking `RuntimeMode` and five const predicates matching the test names. Compatibility returns false for all five; Enhanced returns true for all five. Do not add RNG, caching, or controller calls to this policy.

- [ ] **Step 4: Add the source and executable to CMake.**

  Add the policy source to `astrabot_core`, create `astrabot_runtime_mode_policy` from `tests/runtime_mode_policy_tests.cpp`, link it to `astrabot_core`, apply existing warnings, and register it with CTest.

- [ ] **Step 5: Run the focused tests GREEN.**

  Re-run both focused targets and confirm the exact new behavior passes. Then run the existing `astrabot_compat_cvar_state` and `astrabot_bot_configuration` tests to catch aggregate/snapshot compatibility regressions.

### Task 3: Integrate mode into the Metamod runtime boundary

**Files:**

- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Modify: `tests/metamod_hook_table_tests.cpp`

**Interfaces:**

- Consumes: `CvarSnapshot::mode` and `RuntimeModePolicy` from Task 2.
- Produces: `PluginRuntime::Snapshot::mode`, registered/synchronized `astrabot_mode`, and mode diagnostics.

- [ ] **Step 1: Add the integration assertion before production changes.**

  Extend the active-map snapshot checks to assert the default mode is Compatibility. If the harness can set the CVar without changing unrelated fake-engine behavior, add a second assertion that setting `astrabot_mode` to `enhanced` is observable in the next snapshot; otherwise keep that transition covered by the Task 1 unit test and document the adapter limitation.

- [ ] **Step 2: Register the server CVar.**

  Add a static `cvar_t` for `astrabot_mode` with default `compatibility`. Include it in the existing compatibility registration list without changing native `bot_*` CVar ownership/restore behavior.

- [ ] **Step 3: Synchronize the mode through the existing surface.**

  In `synchronizeCompatibilityCvars`, read `astrabot_mode` with the existing engine CVar string API and forward it through `CompatibilitySurface::setString`. Missing engine pointers must preserve the current mode and existing safety behavior.

- [ ] **Step 4: Expose and log the mode.**

  Add `compat::RuntimeMode mode` to `PluginRuntime::Snapshot`, populate it from `compatibilitySurface_.configuration()`, and include a stable `mode=compatibility|enhanced` value in the map activation and compatibility-command diagnostics. Do not change movement/combat/objective decisions in this task.

- [ ] **Step 5: Run the integration test RED-to-GREEN cycle.**

  First run the modified `astrabot_metamod_hooks` test and confirm it fails only because Snapshot has no mode. Implement the smallest adapter changes, then re-run the target and existing `astrabot_compat_surface`, `astrabot_fake_client_isolation`, and `astrabot_compat_actor_command` tests.

### Task 4: Record the boundary and verify the full P01 gate

**Files:**

- Modify: `docs/parity/SOURCE_MAP.md`
- Modify: `docs/parity/PARITY_MATRIX.md`
- Modify: `docs/parity/KNOWN_DEVIATIONS.md`
- Modify: `docs/parity/STATUS.md`

**Interfaces:**

- Consumes: implemented mode/config/policy/runtime diagnostics and test evidence from Tasks 1-3.
- Produces: P01 audit trail; no new gameplay behavior.

- [ ] **Step 1: Update the source map.**

  Map `CvarState`, `BotConfiguration`, `CompatibilitySurface`, `RuntimeModePolicy`, and `PluginRuntime::Snapshot` to the P01 boundary. State explicitly that baseline candidate controllers remain unproven and shared.

- [ ] **Step 2: Update the parity matrix and known deviations.**

  Change the mode-boundary row from unestablished to `IMPLEMENTED_UNVERIFIED` only if the runtime snapshot and isolation test both pass. Keep timing, RNG, private-state, state-machine, NAV, combat, objective, and live rows unchanged.

- [ ] **Step 3: Run the full x86 configure/build/test gate.**

  Use the cached HostX86/x86 environment:

  ```text
  cmake -S . -B build-action-adapter-x86-1451
  cmake --build build-action-adapter-x86-1451 --config Debug
  ctest --test-dir build-action-adapter-x86-1451 -C Debug --output-on-failure
  ```

  Also run the existing Phase 8 PowerShell fixture tests. Do not start a live HLDS run for P01.

- [ ] **Step 4: Refresh FocalSpan and inspect the final diff.**

  Run `focalspan update --root .`, verify `index_fresh=true`, run a follow-up mode-boundary query, and inspect `git diff --check`. Stage only P01 source/test/doc paths; do not stage `.focalspan/`, `.planning/`, or unrelated dirty files.

- [ ] **Step 5: Commit P01 as one focused change.**

  Commit only the explicit P01 paths with:

  ```text
  feat(parity): isolate CSBot compatibility mode Astra enhancements
  ```

  Verify the commit SHA and worktree status. Stop after P01; do not begin P02.
