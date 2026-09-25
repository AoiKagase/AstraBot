# Managed Bot Freeze Recovery Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` for Native execution or `superpowers:subagent-driven-development` for Subagent-driven execution. Complete each task in order and preserve the review gates below.

**Goal:** Let a managed Bot resume movement when the real round freeze has expired, even if ReGameDLL leaves its public `maxspeed` value at `1.0`.

**Architecture:** Keep the movement gate SDK-free and feed it an explicit `roundFreezeActive` observation. The Metamod runtime will track the public round-start signal and `mp_freezetime`, while `FL_FROZEN` remains an independent control-freeze observation. Restore stale max speed only for live movement dispatch.

**Tech Stack:** C++17, Metamod public user-message and engine callbacks, CMake, existing Windows x86 NMake build.

**Spec:** `docs/superpowers/specs/2026-09-25-astrabot-c4-team-objective-coordination-design.md`

## Global Constraints

- Treat public `FL_FROZEN` and the tracked freeze interval as freeze evidence; a stale `maxspeed <= 1.0` value alone must not hold a Bot indefinitely.
- Preserve the existing round-freeze button policy and P02 command timing, including `msec`.
- Keep Plant/Defuse action buttons separate from locomotion freeze handling.
- Use only public Metamod/HLSDK/GameDLL boundaries; do not read private GameDLL state.
- Do not stage or modify unrelated dirty and untracked workspace artifacts.
- Keep offline behavior checks, deployed DLL identity, and live HLDS/ReHLDS evidence as separate gates.

## Review Focus

- `maxspeed=1.0` during an active freeze still neutralizes movement and attack.
- `maxspeed=1.0` after the tracked freeze interval no longer suppresses movement.
- Explicit `FL_FROZEN` remains authoritative even after the timer expires.
- Missing or invalid `mp_freezetime`, map startup, duplicate round messages, and lifecycle resets cannot leave stale freeze state active indefinitely.
- Plant/Defuse commands retain their required buttons and stationary movement behavior.

## Task 1: Make the movement gate consume explicit freeze state

**Files:**

- Modify: `include/astrabot/metamod/movement_execution_gate.hpp`
- Modify: `src/adapter/metamod/movement_execution_gate.cpp`
- Modify: `tests/movement_execution_gate_tests.cpp`

**Interface:** Add `bool roundFreezeActive` to `MovementExecutionObservation`. `MovementExecutionGate::evaluate` uses `explicitFrozen` for `ControlFrozen`, then `roundFreezeActive` for `RoundFreeze`; `maxSpeed` remains an observation for the runtime but no longer independently selects `RoundFreeze`.

- [ ] **Step 1: Add the stale-speed regression case.** In `tests/movement_execution_gate_tests.cpp`, start from `observation()`, set `roundFreezeActive=false`, `maxSpeedAvailable=true`, and `maxSpeed=1.0f`; assert that the result is `Live` and preserves forward movement, attack, and `msec`.
- [ ] **Step 2: Run the focused gate executable and confirm the expected failure.** Run `cmake --build build-action-adapter-x86-1451 --config Debug --target astrabot_movement_execution_gate`, then `ctest --test-dir build-action-adapter-x86-1451 -C Debug -R '^astrabot_movement_execution_gate$' --output-on-failure`. The stale-speed assertion must fail under the current gate.
- [ ] **Step 3: Add the explicit round-freeze input.** Extend `MovementExecutionObservation` and change `evaluate` so only `roundFreezeActive` enters `RoundFreeze`; retain existing movement, attack, duck, jump, use, and template invalidation semantics for each phase.
- [ ] **Step 4: Cover active freeze and explicit freeze precedence.** Assert that `roundFreezeActive=true` with `maxSpeed=240.0f` still gives `RoundFreeze`, and `explicitFrozen=true` gives `ControlFrozen` regardless of speed or timer.
- [ ] **Step 5: Re-run the focused gate executable.** The gate tests must pass with warnings treated as errors.

## Task 2: Track round-start time from a public signal

**Files:**

- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Modify: `tests/compat_actor_command_tests.cpp`

**Interface:** PluginRuntime stores the active `LifecycleSession::roundGeneration()`, the scheduled round-start time, and the computed freeze-until time. `Snapshot` exposes `roundFreezeActive` for the existing diagnostic/test surface. A public `pfnAlertMessage` hook formats bounded `at_logged` messages and passes recognized round lifecycle strings to `PluginRuntime::onRoundLifecycleMessage(const char *)`. `Game_Commencing` and `Restart_Round_(N_seconds)` start the tracked interval; the public `Round_Start` log marks freeze completion. `lifecycle_.beginRound()` runs once per accepted round-start message.

- [ ] **Step 1: Confirm the public log boundary and event timing.** Check the pinned ReGameDLL-CS `b0889847` `UTIL_LogPrintf` implementation and `CHalfLifeMultiplay::OnRoundFreezeEnd`. Confirm the hook receives `at_logged` with the formatted `World triggered "Round_Start"` message; confirm `Restart_Round_(N_seconds)` includes the public restart delay. Do not treat `HLTV` health/FOV messages or every `RoundTime` sync as a new round.
- [ ] **Step 2: Add the lifecycle regression cases.** Extend `tests/compat_actor_command_tests.cpp` to request the public engine hook table and assert `pfnAlertMessage` is registered. Send `Game_Commencing`, `Restart_Round_(5_seconds)`, `Round_Start`, duplicate `Round_Start`, and unrelated log messages through that callback. Assert only accepted round-start messages advance the snapshot generation; assert `snapshot().roundFreezeActive` begins for the scheduled interval and clears on `Round_Start` or its bounded timeout.
- [ ] **Step 3: Run the focused runtime test and confirm the expected failure.** Run `cmake --build build-action-adapter-x86-1451 --config Debug --target astrabot_compat_actor_command`, then `ctest --test-dir build-action-adapter-x86-1451 -C Debug -R '^astrabot_compat_actor_command$' --output-on-failure`. The new lifecycle assertion must fail before the alert hook is connected.
- [ ] **Step 4: Connect the public alert hook.** Add `HookAlertMessage(ALERT_TYPE, char *, ...)` to the requested engine function table and add `roundFreezeActive` to `Snapshot`. Format only bounded logged messages into a fixed buffer, call `PluginRuntime::onRoundLifecycleMessage`, then return `MRES_IGNORED` so Metamod forwards the original alert unchanged.
- [ ] **Step 5: Compute a bounded freeze interval.** On `Game_Commencing` use the current engine time as the initial start. Parse the integer delay from `Restart_Round_(N_seconds)`, schedule the round start at `globals_->time + N`, and set freeze-until to that time plus public `mp_freezetime`. `Round_Start` clears the active freeze immediately; the timer remains a lost-log fallback. Invalid or unavailable duration never creates an indefinite freeze, and `FL_FROZEN` still independently neutralizes that player.
- [ ] **Step 6: Re-run the focused runtime test.** Verify one generation advance per accepted start, duplicate and unrelated logs do not change state, and the fallback interval expires at the expected absolute time.

## Task 3: Recover stale speed only for live movement

**Files:**

- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Modify: `tests/compat_actor_command_tests.cpp`
- Modify: `docs/source-manifest.json` only if a new production source file is introduced.

**Interface:** `executeManagedBotCommand` sets `gateObservation.roundFreezeActive` from the tracked interval. Track `ActionDispatch::stopMovement` alongside each command template. When the gate returns `Live`, the freeze interval has expired, `FL_FROZEN` is false, and the public speed remains in `(0, 1]`, restore `kDefaultManagedBotMaxSpeed` through the public entity field and `pfnSetClientMaxspeed` only when the pending action does not intentionally stop movement.

- [ ] **Step 1: Add the action-stop speed regression.** In the existing planted-bomb CT runtime case, set `maxspeed=0.0f` before the Defuse command; assert the Bot keeps `IN_USE`, analog movement stays stopped, and neither the public field nor `pfnSetClientMaxspeed` is changed.
- [ ] **Step 2: Run the focused runtime test and confirm the expected failure.** Build `astrabot_compat_actor_command` and run its CTest registration. The current live gate restores zero max speed during the Defuse stop and must fail this assertion.
- [ ] **Step 3: Track the action stop with the command template.** Add `managedBotActionStopsMovement_[index]`; set it from `ActionDispatch::stopMovement` in both full NAV updates and neutral-command preparation, and clear it with the command template and actor lifecycle.
- [ ] **Step 4: Re-run the focused test.** The Defuse speed-preservation assertion must pass after the gate declines restoration for a stop-action template.
- [ ] **Step 5: Add the stale-speed live regression.** With no Plant/Defuse target and `maxspeed=1.0f`, issue the next managed live command after `Round_Start`; assert command dispatch, `maxspeed==240.0f`, and one `pfnSetClientMaxspeed` call. The fixture has no NAV file, so the live acceptance gate remains responsible for proving physical route progress.
- [ ] **Step 6: Run the focused runtime test and confirm the expected failure.** The stale-speed assertion must fail before the restore branch changes.
- [ ] **Step 7: Restore stale speed at the existing dispatch boundary.** Populate `roundFreezeActive` from the tracked interval, preserve the explicit-frozen branch, and restore speed only when the gate selects `Live`, the pending action is not a stop-action, and max speed is unavailable, nonpositive, or remains in `(0, 1]`.
- [ ] **Step 8: Add profile-only evidence.** When profiling is enabled, log round generation, freeze deadline decision, gate phase, prior speed, restored speed, action-stop flag, movement input, and dispatch result for the same actor and frame.
- [ ] **Step 9: Re-run the focused test and build the full x86 target set.** Run the full `cmake --build build-action-adapter-x86-1451 --config Debug` and full CTest suite in the initialized x86 developer environment.

## Completion Gate

- Offline gate and runtime tests pass, and the complete Windows x86 build succeeds.
- The live gate requires a fresh server interval, a surviving HLDS/ReHLDS process, and a deployed DLL SHA-256 that matches the build. Confirm movement through changing origin/velocity and corridor progress after the freeze timer; a nonzero input log alone is insufficient.

---
