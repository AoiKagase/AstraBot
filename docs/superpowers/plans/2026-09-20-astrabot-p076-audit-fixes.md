# AstraBot P07.6 Audit Fixes Implementation Plan
> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (native execution recommended) or superpowers:subagent-driven-development (independent task execution with fresh review).

**Goal:** Close the confirmed F01-F08 P07.6 audit findings without changing P02 timing, Compatibility/Enhanced boundaries, or entering P08.

**Architecture:** Work in three bounded subsystems. First make effective team and runtime diagnostics authoritative. Then repair shared NAV geometry/follower/traversal contracts. Finally repair Compatibility Goal production and BombTarget registration/evaluation cache. Each subsystem exposes typed contracts and fixtures before production integration is changed.

**Tech Stack:** C++14, CMake/NMake HostX86 x86, Metamod-P/HLSDK public boundary, portable CTest, PowerShell fixture gates, Python source contracts, FocalSpan, pinned ReGameDLL-CS read-only comparison.

**Spec:** `docs/superpowers/specs/2026-09-20-astrabot-p076-audit-fixes-design.md`

## Global Constraints

- P08 Combat/Aim/Weapon remains out of scope.
- Preserve P02 30 Hz command / 10 Hz Full Update cadence, absolute deadlines, `msec`, and freeze gating.
- Preserve Compatibility versus Enhanced behavior boundaries; do not use actor-ID modulo, forced team spreading, or arbitrary randomization as Compatibility semantics.
- Preserve existing search-budget/corridor-capacity limits and avoid full-site/full-route searches every Full Update.
- Preserve the SDK-free public observation boundary; unavailable observations remain explicit unavailable values.
- Preserve unrelated dirty/untracked files and stage explicit paths only.
- Keep offline tests, source/build/deployed-DLL identity, and live ReHLDS acceptance as separate gates.

## Review Focus

- A stationary actor must not advance two corridor segments or steer outside the current portal segment.
- A sloped Area must return the interpolated surface Z at the query XY, not an averaged Z.
- A Jump-marked destination must not be rejected by ordinary step-height validation before Jump semantics are considered.
- Effective TeamInfo must be fresh and consistent across Objective, State, action, and diagnostics.
- A cache hit must not hide unregistered sites or make a profile window report zero registered BombTargets.

## File Map and Boundaries

### Observation contract

- Modify `src/adapter/metamod/observation_adapter.hpp/.cpp`: effective team observation helper and explicit availability semantics.
- Modify `src/adapter/metamod/plugin_runtime.hpp/.cpp`: consume effective team consistently and serialize a single per-command diagnostic record.
- Test `tests/observation_adapter_tests.cpp`, `tests/compat_actor_command_tests.cpp`, `tests/runtime_movement_integration_contract.py`, and `tests/p076_performance_contract.py`.

### NAV geometry and traversal

- Modify `include/astrabot/nav/nav_model.hpp`, `src/core/nav/nav_model.cpp`, `src/core/nav/legacy_nav_reader.cpp`: retain the existing two implicit corner Z values plus extent corner Z and validate finite geometry.
- Modify `src/core/nav/nav_query.cpp`: shared `surfaceZAt`, surface-aware closest point, and explicit 2D versus 3D nearest contracts.
- Modify `src/core/nav/locomotion.cpp`: Jump/Crouch precedence before ordinary step rejection.
- Modify `src/core/runtime/nav_roam_controller.cpp`: active-link transition ownership and safe traversal start.
- Modify `src/core/nav/jump_drop.cpp`: one-link envelope and explicit airborne/landing completion checks.
- Modify `src/core/nav/special_traversal.cpp` only if the shared active-link contract requires its entry/exit validation change.
- Test `tests/nav_query_tests.cpp`, `tests/locomotion_tests.cpp`, `tests/nav_roam_controller_tests.cpp`, `tests/jump_drop_tests.cpp`, `tests/special_traversal_tests.cpp`, and `tests/movement_execution_gate_tests.cpp`.

### Goal and objective selection

- Modify `include/astrabot/runtime/nav_roam_controller.hpp`, `src/core/runtime/nav_roam_controller.cpp`: introduce a Goal-selection boundary separate from adjacent-link corridor execution.
- Modify `include/astrabot/metamod/runtime_profiler.hpp`, `src/adapter/metamod/runtime_profiler.cpp`, and `src/adapter/metamod/plugin_runtime.cpp`: registered/evaluated/cache-hit/selected-site counters and per-site reasons.
- Modify `src/adapter/metamod/plugin_runtime.hpp/.cpp`: map/round/site registry lifecycle and actor-local selected-site cache.
- Test `tests/nav_roam_controller_tests.cpp`, `tests/runtime_profiler_tests.cpp`, `tests/nav_external_performance_tests.cpp`, `tests/compat_actor_command_tests.cpp`, and `tests/p076_performance_contract.py`.

## Task 1: Effective team and diagnostic command contract

**Files:**

- Modify: `include/astrabot/metamod/observation_adapter.hpp`
- Modify: `src/adapter/metamod/observation_adapter.cpp`
- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Test: `tests/observation_adapter_tests.cpp`, `tests/compat_actor_command_tests.cpp`, `tests/runtime_movement_integration_contract.py`

**Interfaces:**

- Produce `ObservationAdapter::effectiveTeam(const CompatibilityObservation&, std::uint8_t teamInfo, bool teamInfoFresh)` or an equivalent typed helper returning `ObservationValue<std::int32_t>` with `Available`/`Unavailable` status.
- `PluginRuntime::updateManagedBotCompatibilityState`, `buildManagedObjectiveTarget`, and `decideManagedBotAction` consume the same effective-team result.
- The diagnostic record carries actor, frame, effective team, raw edict team, TeamInfo team, goal source, goal area/position, active segment, intent target, command sequence, final buttons, and explicit availability.

- [ ] **Step 1: Write failing tests.** Add a fixture where `entity->v.team == 0` and fresh TeamInfo says Terrorist; assert Objective and state transition use team 1. Add a stale TeamInfo case; assert team remains unavailable rather than silently using stale data. Add a diagnostic contract asserting raw and effective team are distinct fields.
- [ ] **Step 2: Run tests to verify RED.** Run `astrabot_observation_adapter`, `astrabot_compat_actor_command`, and `py -3 tests/runtime_movement_integration_contract.py`. Expected: the fresh TeamInfo case either reports unavailable/raw team 0 or does not request PlantBomb, and the diagnostic field contract is absent.
- [ ] **Step 3: Implement the minimal effective-team helper.** Route only fresh managed TeamInfo through the helper; keep raw edict team as a separate observation. Replace duplicated team fallback branches in Objective, State, and action decisions with the helper. Do not alter scheduler cadence.
- [ ] **Step 4: Add the command-boundary diagnostic.** Serialize values captured from the same Full Update and final command, not retained route state alone. Keep output profile-gated and aggregate/one-second bounded.
- [ ] **Step 5: Run focused GREEN checks.** Rebuild the affected Metamod targets in `build-action-adapter-x86-1451` and run the three focused tests.
- [ ] **Step 6: Commit.** Commit only the observation/plugin/test paths with message `fix: make P07.6 team and command diagnostics authoritative`.

## Task 2: Surface-Z interpolation and nearest-query contracts

**Files:**

- Modify: `include/astrabot/nav/nav_model.hpp`, `src/core/nav/nav_model.cpp`, `src/core/nav/legacy_nav_reader.cpp`, `src/core/nav/nav_query.cpp`
- Test: `tests/nav_query_tests.cpp`, `tests/nav_model_tests.cpp`

**Interfaces:**

- Produce `float surfaceZAt(const NavArea&, float x, float y)` using the CSBot implicit corners: extent `lo.z`, `northEastZ`, `southWestZ`, extent `hi.z` with clamped normalized XY interpolation.
- `closestPointOnArea` and 3D nearest use `surfaceZAt`; retain a named rectangle-only distance helper for explicit XY callers.

- [ ] **Step 1: Write failing tests.** Add a sloped Area with four implicit corner heights and assert surface Z at center, NE, SW, and an out-of-extent clamped point. Add a nearest test with two overlapping XY Areas at different surface Z and assert the 3D query selects the standing surface, while the explicit XY query keeps its documented tie-break.
- [ ] **Step 2: Run `astrabot_nav_query` and `astrabot_nav_model` to verify RED.** Expected: center/edge Z uses the current average and the 3D-overlap fixture selects the wrong Area or returns the wrong point Z.
- [ ] **Step 3: Implement `surfaceZAt` and route all surface-aware callers through it.** Preserve NAV file byte layout and existing `northEastZ`/`southWestZ` fields; do not invent extra serialized floats. Update finite/degenerate validation.
- [ ] **Step 4: Run focused GREEN checks.** Run `astrabot_nav_query`, `astrabot_nav_model`, `astrabot_nav_loader`, and the external NAV fixture.
- [ ] **Step 5: Commit.** Commit only geometry/query/reader tests with message `fix: use interpolated NAV surface height`.

## Task 3: Follower arrival and corridor segment ownership

**Files:**

- Modify: `src/core/nav/nav_query.cpp`, `include/astrabot/nav/nav_query.hpp`
- Modify: `src/core/runtime/nav_roam_controller.cpp`
- Test: `tests/nav_query_tests.cpp`, `tests/nav_roam_controller_tests.cpp`

**Interfaces:**

- `NavPathFollower::update` returns `TargetReady`, `Advanced`, or `Reached` only after the current portal/segment gate is satisfied; `currentIndex()` remains the authoritative corridor index.
- `NavRoamController` records the follower result and never advances from a stationary Area merely because the next Area rectangle is nearby.

- [ ] **Step 1: Write failing tests.** Add the audit R3 A→B→C fixture. Call update twice without moving and assert the second call remains on B with the same portal target. Add a moved-through-portal case that advances exactly one index.
- [ ] **Step 2: Run `astrabot_nav_query` and `astrabot_nav_roam_controller` to verify RED.** Expected: the stationary second update advances to C.
- [ ] **Step 3: Implement segment/portal gating.** Use directed link direction and portal steering point; require current position to enter the current segment/portal or the final Area tolerance before advancing. Keep existing horizontal/vertical tolerances and no-progress detection separate.
- [ ] **Step 4: Run focused GREEN checks.** Run nav query, roam controller, locomotion, and external NAV tests.
- [ ] **Step 5: Commit.** Commit only follower/roam tests with message `fix: keep NAV corridor segment until portal arrival`.

## Task 4: Traversal link ownership and Jump/Drop ordering

**Files:**

- Modify: `src/core/nav/locomotion.cpp`
- Modify: `src/core/runtime/nav_roam_controller.cpp`
- Modify: `src/core/nav/jump_drop.cpp`
- Test: `tests/locomotion_tests.cpp`, `tests/nav_roam_controller_tests.cpp`, `tests/jump_drop_tests.cpp`, `tests/special_traversal_tests.cpp`

**Interfaces:**

- `NavRoamController` exposes one active `NavDirectedLink` at a time and starts traversal for that link only.
- `JumpDropController::start` receives a two-area corridor whose first/last Areas equal the envelope launch/landing Areas.
- `LocomotionController::buildIntent` gives `NavArea::kJump` precedence over ordinary step rejection.

- [ ] **Step 1: Write failing tests.** Add R5 with height delta 32 and `kJump`; assert Jump intent instead of `StepTooHigh`. Add R6 A→B Walk→C Jump; assert the second link produces Jump. Add a three-Area initial Jump case and assert the envelope contract does not reject it solely because the full goal corridor has a different terminal Area.
- [ ] **Step 2: Run jump/locomotion/roam tests to verify RED.** Expected: R5 returns `StepTooHigh`, R6 retains Walk or JumpDrop rejects the full corridor.
- [ ] **Step 3: Implement traversal ownership.** On follower advancement, resolve the new link’s `how`, destination attributes, and directed geometry; stop/restart only the relevant traversal controller. Keep ladder/special traversal explicit. Do not add stuck-random-jump behavior.
- [ ] **Step 4: Run focused GREEN checks.** Run locomotion, jump-drop, special traversal, roam controller, movement gate, and external NAV tests.
- [ ] **Step 5: Commit.** Commit only traversal implementation/tests with message `fix: honor active NAV traversal links`.

## Task 5: Progress window and terrain/clearance observation

**Files:**

- Modify: `include/astrabot/nav/locomotion.hpp`, `src/core/nav/locomotion.cpp`
- Modify: `include/astrabot/runtime/nav_roam_controller.hpp`, `src/core/runtime/nav_roam_controller.cpp`
- Modify: `src/adapter/metamod/observation_adapter.hpp/.cpp`, `src/adapter/metamod/plugin_runtime.cpp`
- Test: `tests/locomotion_tests.cpp`, `tests/nav_roam_controller_tests.cpp`, `tests/observation_adapter_tests.cpp`

**Interfaces:**

- `LocomotionObservation` carries explicit clearance availability and a bounded movement-progress sample.
- Progress classification distinguishes `Freeze`, `Airborne`, `Ladder`, `Recovery`, and `NoProgress`; unavailable clearance never becomes a guessed constant.

- [ ] **Step 1: Write failing tests.** Add R8 with 0.02-unit lateral jitter over the configured progress window and assert it remains `NoProgress`. Add airborne/ladder/recovery cases that do not trigger ordinary stuck recovery. Add unavailable-clearance coverage that remains explicit.
- [ ] **Step 2: Run focused tests to verify RED.** Expected: the current single-frame threshold clears or misclassifies the jitter/recovery cases.
- [ ] **Step 3: Implement the bounded progress window.** Retain a small fixed history per actor; compare forward progress along the active segment and use state gates before classifying stuck. Keep the existing recovery count/backoff bounds.
- [ ] **Step 4: Wire only available public terrain observations.** Do not fabricate SDK-private clearance; use `UNAVAILABLE` when the public boundary cannot supply it.
- [ ] **Step 5: Run GREEN checks.** Run locomotion, roam, observation, movement-gate, and P07.6 profiler tests.
- [ ] **Step 6: Commit.** Commit only progress/observation/test paths with message `fix: classify NAV progress without jitter escapes`.

## Task 6: Compatibility Goal selection boundary

**Files:**

- Modify: `include/astrabot/runtime/nav_roam_controller.hpp`, `src/core/runtime/nav_roam_controller.cpp`
- Modify: `include/astrabot/compat/random_source.hpp` only if the existing P03 boundary requires a typed request
- Test: `tests/nav_roam_controller_tests.cpp`, `tests/compatibility_rng_tests.cpp`, `tests/p076_performance_contract.py`

**Interfaces:**

- Produce a Goal-selection result containing `goalArea`, `goalPosition`, `goalSource`, `retentionCondition`, and `reselectReason`.
- Consume the existing Compatibility RNG boundary with an explicit semantic ID and actor/timing context; never derive selection from actor ID modulo.
- Corridor execution consumes a selected Goal and does not itself decide the scenario Goal by taking the first adjacent link.

- [ ] **Step 1: Read the pinned ReGameDLL-CS Goal call sites and add failing tests.** Cover initial Goal, unchanged Goal retention, GoalReached re-selection, invalid/stuck re-selection, and RNG request bounds/semantic order. Add R1/R2 graph fixtures showing the first-neighbor-only behavior.
- [ ] **Step 2: Run the focused tests to verify RED.** Expected: Compatibility selects Area 2 for every identical controller and does not emit a Goal-selection RNG request.
- [ ] **Step 3: Implement the smallest reference-shaped Goal selector.** Keep scenario-specific Combat/Aim/Weapon behavior out. Use the verified reference observable selection source and retention/reselection rules; then pass the selected Goal into existing corridor search.
- [ ] **Step 4: Run GREEN tests.** Run roam controller, compatibility RNG, runtime timing, and external NAV tests. Confirm path search counts remain bounded and no per-tick full-area scan is introduced.
- [ ] **Step 5: Commit.** Commit only Goal-selection/RNG/tests with message `fix: separate Compatibility Goal selection from route stepping`.

## Task 7: BombTarget registry and selection cache

**Files:**

- Modify: `src/adapter/metamod/plugin_runtime.hpp/.cpp`
- Modify: `include/astrabot/metamod/runtime_profiler.hpp`, `src/adapter/metamod/runtime_profiler.cpp`
- Test: `tests/compat_actor_command_tests.cpp`, `tests/runtime_profiler_tests.cpp`, `tests/nav_external_performance_tests.cpp`, `tests/p076_performance_contract.py`

**Interfaces:**

- Produce a map/round-scoped registry containing every recognized `func_bomb_target`/`info_bomb_target`, entity identity, site identity, bounds/center, candidate Areas, validity, and rejection reason.
- Produce selection counters: `registered_site_count`, `evaluated_this_window`, `cache_hit`, `selected_site_id`, and per-site candidate/rejection data.
- Actor-local cache stores selected site identity and selected reachable target; registry is shared only as immutable map/round data.

- [ ] **Step 1: Write failing tests.** Add two-site fixtures with same nearest Area, separate identities, one invalid site, map-generation reset, round reset, and cache-hit profile semantics. Assert profile does not report `bombSites=0` merely because selection was cached.
- [ ] **Step 2: Run focused tests to verify RED.** Expected: current one-nearest candidate and aggregate counters cannot distinguish registered sites from evaluated candidates/cache hits.
- [ ] **Step 3: Implement registry lifecycle and bounded evaluation.** Discover sites on map/round invalidation, retain identities, evaluate candidate Areas only on required selection/revalidation, and never re-run all site routes every Full Update.
- [ ] **Step 4: Update profile output.** Keep normal logging off; emit the one-second aggregate and per-site diagnostic only when `astrabot_profile` is enabled. Preserve current search budget and duplicate-search counters.
- [ ] **Step 5: Run GREEN tests.** Run objective, profiler, external NAV, compatibility actor, and P076 contract tests.
- [ ] **Step 6: Commit.** Commit only BombTarget registry/profiler/tests with message `fix: retain BombTarget registry across selection cache hits`.

## Task 8: Full verification and live handoff

**Files:**

- Verify only; modify documentation only if results require a factual update: `docs/parity/PRODUCTION_RUNTIME_MODEL.md`, `docs/parity/STATUS.md`, `tests/p076_production_runtime_docs.py`.

- [ ] **Step 1: Run FocalSpan update/status and a follow-up query covering the changed contracts.** Expected: fresh index and current source/build identity recorded.
- [ ] **Step 2: Initialize the exact VS HostX86/x86 environment and build the complete `build-action-adapter-x86-1451` directory.** Expected: build exit 0 and `where cl` resolves Hostx86/x86.
- [ ] **Step 3: Run `ctest --test-dir build-action-adapter-x86-1451 -C Debug --output-on-failure` and `ctest -N`.** Expected: total is not lower than the pre-change registration and zero Not Run/failed tests.
- [ ] **Step 4: Run external NAV, Phase 6/7/8 fixture, Python contract, and PE artifact gates separately.** Record environment-limited failures separately from source failures.
- [ ] **Step 5: Compare built and deployed DLL size/SHA-256 after a timestamped backup.** Do not claim deployment from copy exit status alone.
- [ ] **Step 6: Commit verification/documentation updates explicitly.** Use a phase-level message only after all required gates are fresh.
- [ ] **Step 7: Stop at P07.6.** Live acceptance remains distinct until a matching DLL is loaded, a fresh server interval runs, and the required A/B/C/performance evidence is captured.
