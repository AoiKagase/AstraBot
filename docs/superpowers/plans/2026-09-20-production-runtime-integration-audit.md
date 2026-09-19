# P07.6 Production Runtime Integration Audit & Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the P01-P07 Compatibility Core runtime chain observable and correct on the production adapter from authoritative freeze gating through Goal/NAV selection and public `RunPlayerMove`, while preserving P02 timing and leaving P08 Combat work unopened.

**Architecture:** Keep Core SDK-free. Add a small adapter-owned execution gate that consumes public entvars/CVar observations, and apply it only at command execution so Think, perception, state, and full-update cadence continue during freeze. Extend the existing `NavRoamDecision` with typed Goal/path transaction evidence and retain valid routes across temporary area loss. Add bounded, opt-in aggregate counters at the adapter boundary; no per-frame logging.

**Tech Stack:** C++14, CMake, Metamod-P/HLSDK public adapter headers, portable CTest, PowerShell phase-8 fixture checks, FocalSpan, code-review-graph.

**Spec:** User-provided P07.6 request in `C:\Users\SandS\.codex\attachments\eca8f29c-5c11-414d-b05d-7ea40237302f\貼り付けたテキスト.txt`.

## Global Constraints

- P08 Combat/Aim/Weapon must not be started.
- P02 `30Hz/10Hz`, absolute deadlines, no catch-up, and `msec` semantics must not change.
- Core remains independent of HLSDK, Metamod-P, ReGameDLL private symbols, ReAPI, `edict_t`, and engine/GameDLL function tables.
- Authoritative freeze evidence must come from public runtime projections (`FL_FROZEN` and GameDLL-maintained `edict_t::v.maxspeed`) or remain Unknown; no local elapsed-time guess is allowed.
- Existing dirty/untracked files must be preserved; stage only P07.6 paths.
- Offline tests, live HLDS/ReHLDS evidence, and Finish acceptance remain separate gates.

## Review Focus

- A command template generated before freeze must not replay after thaw; the test belongs to the execution-gate task.
- A temporary current-area miss during an active route must not erase the route; the test belongs to the NAV transaction task.
- A missing Goal must not be reported as A* failure; the typed failure-reason test belongs to the NAV transaction task.
- Profiling disabled must have no behavior effect and must not emit per-frame logs; the profiler task owns this test.
- Multiple managed Bots must keep actor/generation/frame counters isolated; the execution and transaction integration tests cover this.

---

### Task 1: Authoritative freeze execution gate

**Files:**
- Create: `include/astrabot/metamod/movement_execution_gate.hpp`
- Create: `src/adapter/metamod/movement_execution_gate.cpp`
- Create: `tests/movement_execution_gate_tests.cpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp:1707-1789,2488-2569`
- Modify: `src/adapter/metamod/plugin_runtime.hpp` managed-bot execution state arrays
- Modify: `CMakeLists.txt` adapter test registration
- Modify: `tests/compat_actor_command_tests.cpp` fixture thaw/maxspeed evidence

**Interfaces:**
- Consumes: `edict_t::v.flags`, `edict_t::v.maxspeed`, `IN_*` buttons, and optional `freezetime_duck`/`freezetime_jump` CVar values supplied by the adapter.
- Produces: `MovementExecutionGate::evaluate(const MovementExecutionObservation&)` returning `MovementExecutionDecision { phase, forward, side, up, buttons, msec, staleTemplate }`.

- [ ] **Step 1: Write the failing tests**

  Add tests for `FL_FROZEN`, authoritative `maxspeed <= 1`, configured duck/jump allowance, attack suppression, stale-template invalidation, and live-state pass-through.

- [ ] **Step 2: Run the focused test and observe the expected failure**

  Run: `cmake --build build-action-adapter-x86-1451 --config Debug --target astrabot_movement_execution_gate`

  Expected: the new target is absent or the test does not compile because the gate interface is not yet implemented.

- [ ] **Step 3: Implement the minimal gate**

  Treat `FL_FROZEN` as a hard neutral execution state. Treat finite `maxspeed <= 1.0f` as the public GameDLL freeze projection, without a local timer. Zero forward/side/up and msec in that state, suppress `IN_ATTACK`, and preserve `IN_DUCK`/`IN_JUMP` only when their corresponding public CVar allowance is positive. Keep `IN_USE` policy explicit and testable. Mark the command template stale whenever a freeze-neutral command is dispatched.

- [ ] **Step 4: Integrate at execution only**

  Keep `BotTimingScheduler::advance`, full-update state/perception/NAV work, and `RunPlayerMove` cadence unchanged. Apply the decision immediately before enqueue/dispatch; do not restore `maxspeed` while the gate classifies the actor as frozen. After thaw, invalidate the old template so the next normal full decision creates movement; a command deadline before that full update must dispatch neutral input.

- [ ] **Step 5: Run the red-green test cycle and focused adapter regression**

  Run: `cmake --build build-action-adapter-x86-1451 --config Debug --target astrabot_movement_execution_gate astrabot_compat_actor_command`

  Expected: the new gate cases pass and the existing actor-command freeze/cadence cases remain green.

- [ ] **Step 6: Commit the focused change**

  ```powershell
  git add -- include/astrabot/metamod/movement_execution_gate.hpp src/adapter/metamod/movement_execution_gate.cpp src/adapter/metamod/plugin_runtime.cpp src/adapter/metamod/plugin_runtime.hpp tests/movement_execution_gate_tests.cpp tests/compat_actor_command_tests.cpp CMakeLists.txt
  git commit -m "fix: gate managed movement during authoritative freeze"
  ```

### Task 2: Production Goal/NAV transaction and typed rejection evidence

**Files:**
- Modify: `include/astrabot/runtime/nav_roam_controller.hpp`
- Modify: `src/core/runtime/nav_roam_controller.cpp`
- Modify: `tests/nav_roam_controller_tests.cpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp:1619-1813,2818-2902`
- Modify: `include/astrabot/metamod/plugin_runtime.hpp`
- Modify: `tests/runtime_movement_integration_contract.py`

**Interfaces:**
- Consumes: existing `NavRoamObservation`, immutable `NavSnapshot`, actor/frame identity, and public movement readiness.
- Produces: `NavRoamDecision` fields `goalPresent`, `goalKind`, `goalArea`, `goalPosition`, `pathRequested`, `pathResult`, and `NavFailureReason` with values `NoGoal`, `GoalInvalid`, `CurrentAreaMissing`, `GoalAreaMissing`, `PathSearchFailed`, `NavApplyRejected`, `MovementNotProduced`, and `None`.

- [ ] **Step 1: Add failing NAV contract tests**

  Pin that a normal compatibility roam creates a Goal before a route request, a missing current area is distinct from path search failure, a rejected/invalid goal is distinct from both, unchanged routes retain `pathSequence`, and temporary current-area loss does not reset an active route.

- [ ] **Step 2: Run `astrabot_nav_roam_controller` and observe the failures**

  Run: `ctest --test-dir build-action-adapter-x86-1451 -C Debug -R astrabot_nav_roam_controller --output-on-failure`

  Expected: the new assertions fail because the decision has no typed Goal/failure transaction fields and the temporary-loss path currently calls `resetRoute()`.

- [ ] **Step 3: Implement typed transaction state**

  Set `CurrentAreaMissing` before recovery/temporary-loss handling, preserve `activeCorridor_` and its `pathSequence` when a route exists, and only clear it on map/round change, goal change, invalidation, or bounded stuck failure. In `selectRoamRoute`, mark the selected destination as a Compatibility `Roam` Goal before A* and mark `PathSearchFailed` only after a corridor request actually fails. Keep objective targets as observed public objective Goals; do not add hidden enemy coordinates or tactical strategy.

- [ ] **Step 4: Add one production-like chain assertion**

  Extend the existing runtime contract to require the source path `NavRoamDecision -> ActionAdapter -> managedBotCommandTemplates_ -> executeManagedBotCommand -> inputDispatcher_.dispatchNext`, while checking actor/frame/map/round identity at each boundary. Add a fixture assertion that `goal_present`, `current_area`, `goal_area`, `path_request`, `path_result`, route length, and movement intent are present in the diagnostic transaction.

- [ ] **Step 5: Integrate bounded NAV rejection logging**

  Extend the existing sampled `movement diagnostic` record with the typed failure reason and transaction fields. Keep the existing per-round/sample limit; do not emit every frame.

- [ ] **Step 6: Run focused tests and commit**

  Run: `cmake --build build-action-adapter-x86-1451 --config Debug --target astrabot_nav_roam_controller astrabot_compat_actor_command && ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_nav_roam_controller|astrabot_compat_actor_command" --output-on-failure`

  ```powershell
  git add -- include/astrabot/runtime/nav_roam_controller.hpp src/core/runtime/nav_roam_controller.cpp tests/nav_roam_controller_tests.cpp src/adapter/metamod/plugin_runtime.cpp include/astrabot/metamod/plugin_runtime.hpp tests/runtime_movement_integration_contract.py
  git commit -m "feat: trace production goal and navigation transactions"
  ```

### Task 3: Opt-in runtime profiler and cadence counters

**Files:**
- Create: `include/astrabot/metamod/runtime_profiler.hpp`
- Create: `src/adapter/metamod/runtime_profiler.cpp`
- Create: `tests/runtime_profiler_tests.cpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp:949-981,1407-1813,1816-1991`
- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/observation_adapter.hpp`
- Modify: `src/adapter/metamod/observation_adapter.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: stage begin/end timestamps, TraceLine calls, visible candidates/body probes, path request/search results, and `RunPlayerMove` receipts.
- Produces: bounded per-stage `calls/sec`, `total usec/sec`, `average usec`, `max usec`; per-bot `path_search_count/sec`, `path_search_success/sec`, `path_search_failure/sec`, `path_recompute_count/sec`; one aggregate line approximately once per second when `astrabot_profile 1` is set.

- [ ] **Step 1: Write failing profiler tests**

  Assert disabled profiling does not alter decisions or counters exposed to the runtime, enabled profiling aggregates two samples correctly, counters reset at the one-second report boundary, and TraceLine/candidate/body-probe counters are separate.

- [ ] **Step 2: Run the new target and observe the expected failure**

  Run: `cmake --build build-action-adapter-x86-1451 --config Debug --target astrabot_runtime_profiler`

  Expected: target/interface is absent before implementation.

- [ ] **Step 3: Implement bounded aggregation**

  Use `std::chrono::steady_clock` only inside the opt-in profiler. Keep counters in fixed arrays; no allocation and no console output while disabled. Report once per elapsed second through the existing plugin console callback. Count TraceLine calls at `ObservationAdapter::collectVisibility`, visibility candidates/body probes at the same sampling boundary, and route searches/recomputes from `NavRoamDecision`.

- [ ] **Step 4: Add the explicit profile CVar and A/B output**

  Register `astrabot_profile` as an adapter CVar, synchronize it each StartFrame, and log only aggregate snapshots. The report must include plugin overhead, per-Bot incremental totals, `RunPlayerMove/sec`, and the named heavy stages. Preserve existing `astrabot_mode` behavior.

- [ ] **Step 5: Run profiler tests and focused runtime tests**

  Run: `cmake --build build-action-adapter-x86-1451 --config Debug --target astrabot_runtime_profiler astrabot_compat_actor_command && ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_runtime_profiler|astrabot_compat_actor_command" --output-on-failure`

- [ ] **Step 6: Commit profiler changes**

  ```powershell
  git add -- include/astrabot/metamod/runtime_profiler.hpp src/adapter/metamod/runtime_profiler.cpp tests/runtime_profiler_tests.cpp src/adapter/metamod/plugin_runtime.cpp src/adapter/metamod/plugin_runtime.hpp src/adapter/metamod/observation_adapter.hpp src/adapter/metamod/observation_adapter.cpp CMakeLists.txt
  git commit -m "feat: add opt-in production runtime profiling"
  ```

### Task 4: Parity documentation, live procedure, and final verification

**Files:**
- Create: `docs/parity/PRODUCTION_RUNTIME_MODEL.md`
- Modify: `docs/parity/STATUS.md`
- Modify: `docs/parity/PARITY_MATRIX.md`
- Modify: `docs/parity/KNOWN_DEVIATIONS.md`
- Modify: `docs/parity/TRACE_SCHEMA.md`
- Modify: `docs/parity/NAVIGATION_MODEL.md`
- Modify: `docs/parity/TIMING_MODEL.md`
- Modify: `.planning/ROADMAP.md`
- Modify: `.planning/STATE.md`

**Interfaces:**
- Consumes: current source behavior, pinned ReGameDLL-CS evidence, profiler output schema, and test results.
- Produces: an explicit P07.6 audit record and user-runnable A/B procedure for plugin-off, Bot 0/1/2/4, debug/profile off/on, FPS, StartFrame usec, TraceLine/sec, path searches/recomputes/sec, and RunPlayerMove/sec.

- [ ] **Step 1: Write documentation checks first**

  Extend the existing phase fixture/documentation checks to require the freeze policy, Goal producer, typed NAV rejection reasons, profiler fields, and an explicit `P08未開始` marker.

- [ ] **Step 2: Run the documentation checks before editing docs**

  Run: `rtk powershell -NoProfile -ExecutionPolicy Bypass -File tests/phase8_action_adapter_boundary_test.ps1` and the new P07.6 documentation check.

  Expected: the checks fail because the new production runtime model and fields are absent.

- [ ] **Step 3: Update parity and GSD records**

  Record that freeze state is sourced from public GameDLL projections, scheduler cadence remains P02-compatible, normal Compatibility roam is the pre-P08 Goal producer, current-area/path failures are distinct, live measurements remain unrun until the user executes them, and P08 is not started.

- [ ] **Step 4: Run the required verification matrix**

  Initialize the matching x86 developer environment, then run `cmake --build build-action-adapter-x86-1451 --config Debug` followed by `ctest --test-dir build-action-adapter-x86-1451 -C Debug --output-on-failure`. Run the Phase 8 fixture checks, focused runtime/NAV tests, `git diff --check`, `focalspan update --root .`, `focalspan status --json`, and a follow-up FocalSpan query. Do not replace missing live HLDS/ReHLDS evidence with CTest.

- [ ] **Step 5: Review current diff and commit documentation**

  Stage only the listed P07.6 files and run `git diff --cached --check`.

  ```powershell
  git add -- docs/parity/PRODUCTION_RUNTIME_MODEL.md docs/parity/STATUS.md docs/parity/PARITY_MATRIX.md docs/parity/KNOWN_DEVIATIONS.md docs/parity/TRACE_SCHEMA.md docs/parity/NAVIGATION_MODEL.md docs/parity/TIMING_MODEL.md .planning/ROADMAP.md .planning/STATE.md
  git commit -m "docs: record P07.6 production runtime audit"
  ```

## Self-review

- Freeze requirements are covered by Task 1 without changing scheduler timing.
- Goal/NAV, current-area lifecycle, route persistence, rejection reasons, managed actor/frame identity, and one-chain transaction evidence are covered by Task 2.
- StartFrame overhead, TraceLine cadence, path counters, debug/profile A/B support, and disabled behavior invariance are covered by Task 3.
- Live acceptance procedure, parity status, P02-P07 regression boundary, and P08 prohibition are covered by Task 4.
- Full live FPS/HLDS values cannot be produced by offline execution; the plan records them as a separate user-run gate instead of inventing thresholds or evidence.

