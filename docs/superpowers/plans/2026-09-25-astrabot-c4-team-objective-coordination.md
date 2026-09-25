# C4 and Team Route Coordination Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` for Native execution or `superpowers:subagent-driven-development` for Subagent-driven execution. Complete each task in order and preserve the review gates below.

**Goal:** Assign unique useful team routes, recover dropped C4 with the nearest reachable teammate Bot, and complete C4 plant and defuse actions from observed game state.

**Architecture:** Add an SDK-free round-scoped coordinator for C4 ownership and roaming claims. Metamod supplies bounded public actor and bomb-entity observations, while existing NAV and `ActionAdapter` paths execute the selected assignment. Assignments carry actor, map, round, entity, and generation identities so stale actors or objectives cannot retain ownership.

**Tech Stack:** C++17, existing SDK-free Core, Metamod-P public callbacks, existing NAV query/controller, CMake, and source manifest validation.

**Spec:** `docs/superpowers/specs/2026-09-25-astrabot-c4-team-objective-coordination-design.md`

## Global Constraints

- Only one living managed Bot owns each dropped-C4 pickup or planted-C4 defuse assignment.
- Pick the nearest eligible managed Bot by reachable NAV route cost; ties use stable `ActorKey` order. Use geometric distance only when NAV is unavailable and record the fallback.
- Reserve roaming targets and first corridor links by map, round, team, and actor generation; release claims on completion or invalidation.
- Core remains SDK-free; Metamod alone classifies public edicts and writes public movement/action inputs.
- Unsupported dropped-bomb entity forms remain `UNAVAILABLE`; never infer a ground truth from a model or classname that was not validated against the configured server.
- Preserve Compatibility mode boundaries, existing NAV search limits, profiling controls, and unrelated worktree artifacts.
- Keep offline behavior results, deployed DLL identity, and live C4 events as separate evidence gates.

## Review Focus

- Two teammates request the same oldest visit Area or first route link.
- A dropped C4 has no NAV route, an unknown entity representation, or disappears before pickup.
- Two teammates are equidistant from a dropped or planted bomb.
- An actor dies, disconnects, reuses a slot, changes team, or the round/map changes while holding a reservation.
- A Bot enters/exits the plant or defuse region while action buttons are held.

## Task 1: Add the SDK-free round objective assignment model

**Files:**

- Create: `include/astrabot/team/round_objective_coordinator.hpp`
- Create: `src/core/team/round_objective_coordinator.cpp`
- Create: `tests/round_objective_coordinator_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/source-manifest.json`

**Interface:** Define `kMaximumTeamObjectiveActors = 32` and these SDK-free types in `round_objective_coordinator.hpp`:

```cpp
enum class TeamObjectiveKind : std::uint8_t
{
	None,
	RetrieveDroppedC4,
	PlantC4,
	DefuseC4
};

enum class TeamObjectiveResult : std::uint8_t
{
	Assigned,
	NoObjective,
	InvalidInput
};

struct TeamObjectiveActorObservation
{
	world::ActorKey actor;
	objectives::TeamRole team;
	bool alive;
	bool carryingC4;
	bool positionAvailable;
	world::WorldVector position;
	nav::AreaId currentArea;
};

struct TeamBombTargetObservation
{
	bool available;
	world::EntityKey entity;
	world::WorldVector position;
	nav::AreaId area;
};

struct TeamObjectiveInput
{
	std::uint32_t mapGeneration;
	std::uint32_t roundGeneration;
	std::size_t actorCount;
	TeamObjectiveActorObservation actors[kMaximumTeamObjectiveActors];
	TeamBombTargetObservation droppedC4;
	TeamBombTargetObservation plantedC4;
};

struct TeamObjectiveAssignment
{
	world::ActorKey actor;
	TeamObjectiveKind kind;
	world::EntityKey targetEntity;
	world::WorldVector targetPosition;
	nav::AreaId targetArea;
	std::uint32_t generation;
};

struct TeamObjectiveAssignmentSet
{
	std::size_t count;
	TeamObjectiveAssignment assignments[kMaximumTeamObjectiveActors];
};
```

`RoundObjectiveCoordinator::assign(const TeamObjectiveInput &, const nav::NavSnapshot &, TeamObjectiveAssignmentSet *)` returns `TeamObjectiveResult`; it emits at most one objective assignment per living eligible actor and one owner per bomb objective. `reset(mapGeneration, roundGeneration)` releases all old ownership. No SDK types enter these files.

- [ ] **Step 1: Add failing assignment cases.** In `tests/round_objective_coordinator_tests.cpp`, cover the nearest reachable living Terrorist for dropped C4, nearest reachable CT for planted C4, deterministic `ActorKey` ties, no assignment to dead/wrong-team actors, and one owner per objective.
- [ ] **Step 2: Register the test and source.** Add `astrabot_round_objective_coordinator` to `CMakeLists.txt`, add the new Core source to the Core target, and add both new source paths to `docs/source-manifest.json`.
- [ ] **Step 3: Run the focused test and confirm the expected failure.** Build and run only `astrabot_round_objective_coordinator`; the missing coordinator behavior must fail the new assertions.
- [ ] **Step 4: Implement bounded route-cost assignment.** Use the existing NAV query to compare reachable path costs from each candidate actor's current Area to the bomb target Area. Sort equal costs by actor slot then actor generation. Use bounded geometric distance only when NAV is unavailable; return no assignment when all NAV paths are confirmed unreachable.
- [ ] **Step 5: Preserve assignments by identity.** Retain an assignment while map generation, round generation, actor generation, team, objective entity identity, and objective state remain valid. Release on death, disconnect, objective completion, actor reuse, map/round change, or explicit path failure.
- [ ] **Step 6: Re-run the focused coordinator test.** Confirm all assignment and lifecycle cases pass, then build the Core target with warnings treated as errors.

## Task 2: Reserve distinct roaming goals and initial links

**Files:**

- Modify: `include/astrabot/runtime/nav_roam_controller.hpp`
- Modify: `src/core/runtime/nav_roam_controller.cpp`
- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Modify: `tests/nav_roam_controller_tests.cpp`

**Interface:** Define `NavRoamReservationBoard` in `nav_roam_controller.hpp`, keyed by map generation, round generation, raw validated team number, and generation-safe `ActorId`. Its methods are `reset(std::uint32_t mapGeneration, std::uint32_t roundGeneration)`, `goalReservedByOther(std::uint8_t team, ActorId actor, nav::AreaId goalArea) const`, `firstLinkReservedByOther(std::uint8_t team, ActorId actor, nav::AreaId fromArea, nav::AreaId toArea) const`, `reserve(std::uint8_t team, ActorId actor, nav::AreaId goalArea, nav::AreaId fromArea, nav::AreaId toArea)`, and `release(ActorId actor)`. `NavRoamController::setReservationBoard(NavRoamReservationBoard *)` connects each per-Bot controller to the shared board. `NavRoamDecision` reports `reservationFallback` when a duplicate claim is unavoidable. `PluginRuntime` resets the shared board when map or round generation changes; repeated same-generation reset calls retain claims. A claim contains one goal Area and the first directed link of its successfully selected corridor.

- [ ] **Step 1: Add failing multi-controller route cases.** Give two controllers the same visit history and NAV snapshot; assert they select different feasible goals and avoid the same first link when another feasible route exists. Assert a shared route is allowed when no alternative exists.
- [ ] **Step 2: Add claim lifecycle cases.** Assert map/round changes, actor-generation changes, reset, and path failure release the prior claim and permit another controller to reserve it.
- [ ] **Step 3: Run the focused NAV controller test and confirm the expected failure.** Build `astrabot_nav_roam_controller` and run only its CTest registration; the duplicate-goal case must fail first.
- [ ] **Step 4: Implement the shared reservation board.** Store at most one claim per valid `ActorKey` and team. Query claims while evaluating eligible Compatibility roam goals, prefer unclaimed target Areas and first links, and publish the selected route claim after the existing bounded corridor search succeeds.
- [ ] **Step 5: Bound route evaluation and retain goals.** Evaluate no more than eight unclaimed candidates per goal assignment; cache the winning route claim until arrival, path failure, or invalidation. Preserve oldest-visited ordering among candidates with equal reservation cost.
- [ ] **Step 6: Re-run NAV controller coverage.** Confirm independent controllers get distinct feasible routes, release stale claims, and retain a shared fallback only when no distinct feasible route exists.

## Task 3: Observe dropped C4 through validated public entities

**Files:**

- Modify: `include/astrabot/compat/observation.hpp`
- Modify: `src/adapter/metamod/observation_adapter.hpp`
- Modify: `src/adapter/metamod/observation_adapter.cpp`
- Modify: `tests/observation_adapter_tests.cpp`

**Interface:** Add an availability-qualified dropped-bomb observation containing a validated `world::EntityKey` and world position. The adapter accepts the public entity classname/model combinations verified for the configured ReGameDLL-CS server and otherwise reports `Unavailable`.

- [ ] **Step 1: Verify public dropped-C4 entity forms.** Check the configured public ReGameDLL-CS behavior and a fresh server log/runtime observation to identify the supported dropped entity classname/model. Keep unsupported forms unavailable.
- [ ] **Step 2: Add failing adapter cases.** Cover each verified dropped-C4 representation, entity reuse, freed entities, invalid positions, and an unrecognized entity that must remain unavailable.
- [ ] **Step 3: Run the focused observation-adapter test and confirm the expected failure.** Build and run only the existing `astrabot_observation_adapter` CTest registration.
- [ ] **Step 4: Implement bounded observation.** Scan at most the existing entity limit, validate classname/model and finite origin, then emit the stable entity identity and position. Do not treat planted `grenade` entities or arbitrary `weaponbox` models as dropped C4 without the verified discriminator.
- [ ] **Step 5: Re-run adapter coverage.** Confirm recognized, unavailable, invalid, and stale-entity cases behave as specified.

## Task 4: Connect team assignments to bomb and roam targets

**Files:**

- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Modify: `tests/compat_actor_command_tests.cpp`
- Modify: `tests/p076_performance_contract.py` only if a current contract must be updated to reflect shared team reservations.

**Interface:** Each managed-bot full update builds one bounded roster snapshot, gathers carrying/dropped/planted C4 observations, calls `RoundObjectiveCoordinator::assign`, and applies the returned actor-specific target before the per-actor roam controller chooses a corridor. Cache by map generation, round generation, objective entity identity, actor generation, and assignment generation.

- [ ] **Step 1: Add failing production-like cases.** In `tests/compat_actor_command_tests.cpp`, arrange three T-side managed actors and a dropped C4; assert only the lowest NAV-cost actor receives `RetrieveC4`, and possession observation transfers it to `PlantC4`. Add a planted-bomb case where only the nearest reachable CT receives `DefuseC4`.
- [ ] **Step 2: Run the focused runtime test and confirm the expected failure.** Build and run only `astrabot_compat_actor_command`; the single-owner assertion must fail under the current per-actor objective flow.
- [ ] **Step 3: Gather one bounded roster snapshot per full update.** Include living state, effective TeamInfo-derived team, actor generation, origin, current NAV Area, C4 possession, and objective entity identities. Skip unknown-team, dead, stale, or unavailable observations.
- [ ] **Step 4: Apply stable assignments.** Route the dropped-C4 owner to the entity, the carrier to the selected shortest reachable BombTarget, the defuser to the planted bomb, and other Bots to reserved roaming goals. Retain assignments until completion, invalidation, or a typed path failure.
- [ ] **Step 5: Add profile-only assignment diagnostics.** Correlate actor/frame, objective state and entity, assigned role, route cost, target Area, first link, cache/reassignment reason, and fallback reason.
- [ ] **Step 6: Re-run the focused runtime test and performance contracts.** Confirm only assigned actors receive objective goals and candidate/path evaluation remains bounded and cached.

## Task 5: Hold Plant/Defuse input until observed completion

**Files:**

- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Modify: `src/adapter/metamod/action_adapter.cpp`
- Modify: `src/adapter/metamod/action_adapter.hpp` only if the action result needs an explicit start/continue/complete phase.
- Modify: `tests/action_adapter_tests.cpp`
- Modify: `tests/compat_actor_command_tests.cpp`

**Interface:** Only the actor assigned `PlantC4` may emit Plant; only the actor assigned `DefuseC4` may emit Defuse. Plant selects `weapon_c4` once on action entry, then holds `IN_ATTACK` while stopped inside the validated plant region. Defuse holds `IN_USE` while stopped inside the validated defuse region. Both stop on observed completion or assignment invalidation.

- [ ] **Step 1: Add failing action lifecycle cases.** Assert unassigned teammates emit no objective input, Plant selection occurs once on entry, Plant attack and Defuse use remain held while in range, and both actions stop when the objective completes or the actor leaves/loses its assignment.
- [ ] **Step 2: Run focused action and command-boundary tests and confirm the expected failure.** Build and run only `astrabot_action_adapter` and `astrabot_compat_actor_command`.
- [ ] **Step 3: Connect actor-specific action decisions.** Gate the existing `decideManagedBotAction` objective branch on the coordinator assignment and validated region. Keep the current 96-unit bound unless the adapter supplies validated trigger bounds; do not use an unchecked site origin as an arbitrary plant volume.
- [ ] **Step 4: Make action entry idempotent.** Issue `use weapon_c4` once per action generation; retain attack/use through subsequent commands; clear the action generation when the bomb state, assigned actor, region, map, or round changes.
- [ ] **Step 5: Re-run action and command-boundary tests.** Confirm one actor holds the right input, other teammates retain locomotion, and completion clears the action.

## Task 6: Complete build and live evidence handoff

**Files:**

- Modify: `docs/source-manifest.json` for every new production or test source file.
- Modify: `CMakeLists.txt` for every new Core module and focused test executable.
- Update only the relevant Phase 8 UAT/evidence entry after a fresh live interval.

- [ ] **Step 1: Validate the source manifest delta.** Run the existing checker and record its baseline failure on pre-existing unlisted project sources. Confirm only this plan's new header, source, and test are each listed exactly once; do not add unrelated source-manifest entries in this objective change.
- [ ] **Step 2: Reconfigure and build the full Windows x86 target set.** Initialize `VsDevCmd.bat -arch=x86 -host_arch=x86` in the same command process as the build, confirm `where cl` resolves to Hostx86/x86, then build `build-action-adapter-x86-1451` completely.
- [ ] **Step 3: Run the full CTest suite.** Run CTest only after the complete build and record all results; do not infer live acceptance from CTest.
- [ ] **Step 4: Refresh FocalSpan and code-review-graph.** Update the FocalSpan index, run a relevant query, refresh the graph, and inspect impact and affected flows for the changed contracts.
- [ ] **Step 5: Inspect the final diff and preserve scope.** Check the new and modified files, confirm unrelated untracked artifacts remain untouched, and do not stage or commit without a separate request.
- [ ] **Step 6: Run live acceptance only with a matching artifact.** Verify source/build/deployed DLL SHA-256, capture a fresh offset-scoped qconsole log, and require physical NAV progress, dropped-C4 possession, `Planted_The_Bomb`, and `Defused_The_Bomb`. Leave any unobserved gate explicitly pending.

---
