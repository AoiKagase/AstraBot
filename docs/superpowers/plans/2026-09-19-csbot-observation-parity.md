# CSBot Observation Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an SDK-free Compatibility Observation boundary that preserves typed player, weapon, objective, entity, and trace observations with explicit quality, provenance, freshness, lifecycle, and timing metadata.

**Architecture:** A new `astrabot::compat` Core contract owns typed observation values and metadata. A Metamod-only `ObservationAdapter` reads public `edict_t`, Engine, and already-captured public GameDLL/message state and projects it into that contract. Existing `WorldSnapshot` and objective sensor paths consume the adapter output without changing Combat, State Machine, NAV, P02 timing, or P03 RNG behavior.

**Tech Stack:** C++14, CMake 3.20+, public Metamod-P/HLSDK headers only in adapter targets, portable Core unit tests, Windows x86 HostX86/x86 NMake Debug build, CTest, PowerShell Phase 8 checks, and FocalSpan.

**Spec:** `docs/superpowers/specs/2026-09-19-csbot-observation-parity-design.md`

## Global Constraints

- ReGameDLL-CS reference commit: `b0889847fe6d03898be88acc9e366660efb40ab5`.
- AstraBot P04 start commit: `ebee3b461130ad65fdbbe9bdc2139389372c8ada`.
- Core remains free of Metamod-P, HLSDK, ReAPI, ReGameDLL private headers, and private pdata offsets.
- Public-native observation collection is limited to public `edict_t`, public Engine callbacks, and public GameDLL/message data already received by `PluginRuntime`.
- Unknown and unavailable values retain `valid=false` and explicit quality; zero, false, empty, or default values are never used as implicit observed values.
- Existing `WorldSnapshot`, `WeaponInventory`, objective models, lifecycle generation, P02 scheduler semantics, and P03 RNG source/trace contracts remain behaviorally unchanged.
- Do not implement CSBot State Machine, Combat/Aim/weapon-selection changes, objective decisions, NAV changes, buying, or P05 work.
- Preserve all existing tracked/untracked worktree content; stage explicit P04 paths only.
- Keep offline build/test evidence, PE/Python verification, and live HLDS/ReHLDS acceptance as separate gates.

## Review Focus

- A valid zero-valued field must remain distinguishable from an unavailable field; Task 1 tests `valid`, quality, and trace metadata for zero/false values.
- A delayed TeamInfo/FOV/objective event must not appear current; Task 1 tests freshness and delay, and Task 2 tests adapter context propagation.
- A reused slot must not expose the previous actor's observation; Task 1 tests actor-generation isolation and Task 3 tests runtime lifecycle reset.
- Private weapon state must not be synthesized as exact state; Task 2 tests unavailable active weapon/timer/accuracy fields while public fields remain exact where supported.
- Enhanced collection must not mutate Compatibility observations or timing/RNG contracts; Task 4 adds a regression test and runs the existing timing/RNG suites.

---

### Task 1: Add the SDK-free observation model and Core tests

**Files:**
- Create: `include/astrabot/compat/observation.hpp`
- Create: `src/core/compat/observation.cpp`
- Create: `tests/observation_model_tests.cpp`
- Modify: `CMakeLists.txt:21-52,69-80`

**Interfaces:**
- Consumes: `astrabot::world::ActorKey`, `FrameIdentity`, `WorldVector`, and existing `combat::ReloadState`.
- Produces: `ObservationQuality`, `ObservationFreshness`, `ObservationSource`, `ObservationValueKind`, `ObservationTimingContext`, `ObservationContext`, `ObservationValue<T>`, `PlayerObservation`, `WeaponObservation`, `ObjectiveObservation`, `CompatibilityObservation`, `ObservationTraceRecord`, and `IObservationTraceSink`.

- [ ] **Step 1: Write the failing model tests and register their target.**

  Add `tests/observation_model_tests.cpp` with a local `check(bool, const char *)` helper and these tests:

  ```cpp
  bool testQualityAndFreshnessAreExplicit();
  bool testZeroAndFalseRemainValidWhenObserved();
  bool testUnavailableValueIsNotAvailable();
  bool testDelayedValueRetainsDelayAndSource();
  bool testActorGenerationPreventsCrossBotReuse();
  bool testLifecycleContextRejectsStaleFrame();
  bool testTimingContextIsAttachedWithoutSchedulerMutation();
  bool testTraceSinkReceivesSemanticMetadata();
  ```

  Register `astrabot_observation_model` under `ASTRABOT_BUILD_TESTS`, link it to `astrabot_core`, add the public include directory, apply the existing warnings function, and register it with CTest.

- [ ] **Step 2: Run the model target and confirm the intended RED failure.**

  Run:

  ```powershell
  cmake --build build-action-adapter-x86-1451 --target astrabot_observation_model --config Debug
  ```

  Expected: compilation fails because `astrabot/compat/observation.hpp` and its model API do not exist. Do not implement production code before recording this missing-API failure.

- [ ] **Step 3: Implement the minimal Core observation contract.**

  Define the ten required quality values exactly as `Exact`, `ExactEngineApi`, `ExactGameApi`, `Delayed`, `Inferred`, `Approximated`, `Unavailable`, `NotYetImplemented`, `NotRequired`, and `Unknown`. Define freshness as `SameTick`, `CurrentFullUpdate`, `EventDrivenCached`, and `Stale`, and source as `PublicEdict`, `PublicEngineApi`, `PublicGameDll`, `AdapterCache`, `Fixture`, `Synthetic`, and `None`.

  Use these concrete structures:

  ```cpp
  struct ObservationTimingContext {
      std::uint32_t commandSequence;
      std::uint32_t upkeepSequence;
      std::uint32_t fullUpdateSequence;
  };

  struct ObservationContext {
      const char *semanticId;
      world::ActorKey actor;
      world::FrameIdentity frame;
      ObservationQuality quality;
      ObservationFreshness freshness;
      ObservationSource source;
      std::uint32_t delayTicks;
      ObservationTimingContext timing;
      bool isValid() const;
  };

  template <typename T>
  struct ObservationValue {
      T value;
      bool valid;
      ObservationContext context;
      bool isAvailable() const;
  };
  ```

  Add typed player fields for health, armor, team, dead state, origin, velocity, view angles, FOV, flags, ground/water state, max speed, buttons, old buttons, solid, movetype, and bounds. Add typed weapon fields for active weapon ID, clip, reserve, reload, next primary/secondary, accuracy, silencer, burst, and zoom. Add typed objective fields for C4 possession, bomb entity/planted state, bomb position/timer, defusing, kit, bomb zone, hostage, rescue, and VIP. Keep private-only fields invalid with an explicit context.

  Add `ObservationTraceRecord` with `ObservationValueKind`, scalar/vector slots, and the full `ObservationContext`; add `IObservationTraceSink::record(const ObservationTraceRecord &)`. Implement `isValid()` and string conversion helpers in `src/core/compat/observation.cpp`; keep the value template inline and SDK-free.

- [ ] **Step 4: Run the focused model tests and verify GREEN.**

  Run:

  ```powershell
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R astrabot_observation_model --output-on-failure
  ```

  Expected: all eight model tests pass, including observed zero/false, unavailable state, actor generation, stale frame, timing context, and trace metadata.

- [ ] **Step 5: Run the existing Core regression set.**

  Run:

  ```powershell
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_(world_snapshot|perception|weapon_state|objective_state|runtime_timing|compatibility_rng)$" --output-on-failure
  ```

  Expected: all selected pre-P04 tests pass with no timing or RNG changes.

### Task 2: Implement the public Metamod observation adapter and production fixture

**Files:**
- Create: `src/adapter/metamod/observation_adapter.hpp`
- Create: `src/adapter/metamod/observation_adapter.cpp`
- Create: `tests/observation_adapter_tests.cpp`
- Modify: `CMakeLists.txt:488-500,548-561,591-603,628-640`

**Interfaces:**
- Consumes: Core observation types from Task 1, public `edict_t`, `enginefuncs_t`, `globalvars_t`, and the adapter's existing lifecycle/frame context.
- Produces: `metamod::ObservationAdapter`, `ObservationAdapterResult`, and a public-native fixture path that returns `CompatibilityObservation`.

- [ ] **Step 1: Write the failing adapter/fixture tests.**

  Add a fake public entity table and these tests:

  ```cpp
  bool testPublicPlayerFieldsAreExactAtCollectionTick();
  bool testPrivateWeaponFieldsAreUnavailableNotSyntheticExact();
  bool testObjectiveProxyKeepsInferredQuality();
  bool testAdapterPreservesActorAndFrameIdentity();
  bool testAdapterHandlesMissingEnginePointers();
  ```

  Set `edict_t::v.health`, `armorvalue`, `team`, `deadflag`, `origin`, `velocity`, `v_angle`, `fov`, `flags`, `movetype`, `solid`, `mins`, `maxs`, `button`, `oldbuttons`, and `weapons` in the fixture. Verify public values carry `PublicEdict` and `SameTick`; verify active weapon private fields carry `valid=false` and `Unavailable` or `NotYetImplemented`; verify C4 bit/entity proxies carry `Inferred` and the correct source. Do not include `pvPrivateData` in the fixture.

  Register an `astrabot_observation_adapter` target containing the test and the new adapter source, with the same include directories and `astrabot_core` linkage used by other Metamod adapter tests.

- [ ] **Step 2: Run the adapter target and confirm RED.**

  Run:

  ```powershell
  cmake --build build-action-adapter-x86-1451 --target astrabot_observation_adapter --config Debug
  ```

  Expected: compilation fails because `ObservationAdapter` and its collection result do not exist.

- [ ] **Step 3: Implement the adapter with a narrow public-input contract.**

  Define:

  ```cpp
  enum class ObservationAdapterResult {
      Accepted,
      InvalidArgument,
      EngineUnavailable,
      InvalidEntity
  };

  class ObservationAdapter {
  public:
      ObservationAdapter();
      void configure(enginefuncs_t *engineFunctions, globalvars_t *globals);
      ObservationAdapterResult collectActor(
          edict_t *entity,
          const world::ActorKey &actor,
          const world::FrameIdentity &frame,
          const compat::ObservationTimingContext &timing,
          compat::CompatibilityObservation *observation) const;
      void setTraceSink(compat::IObservationTraceSink *sink);
  };
  ```

  Read only public `entity->v` fields and public Engine callbacks already present in the adapter. Attach stable IDs such as `OBS-PLAYER-HEALTH`, `OBS-PLAYER-FOV`, `OBS-PLAYER-ACTIVE-WEAPON`, `OBS-WEAPON-ACCURACY`, and `OBS-OBJECTIVE-C4-POSSESSION`. Emit bounded trace records only when a sink is configured. Do not read private data, add offsets, call ReAPI, or call RNG/timing APIs.

- [ ] **Step 4: Run the focused adapter fixture and verify GREEN.**

  Run:

  ```powershell
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R astrabot_observation_adapter --output-on-failure
  ```

  Expected: the production adapter fixture passes public field, unavailable private field, inferred objective, identity, missing-pointer, and trace assertions.

- [ ] **Step 5: Run the adapter target's existing regression tests.**

  Run:

  ```powershell
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_(metamod_hooks|fake_client_isolation|compat_actor_command|action_adapter)$" --output-on-failure
  ```

  Expected: existing hook, lifecycle, action translation, and command tests remain green.

### Task 3: Route current world/objective sensing through the adapter without behavior changes

**Files:**
- Modify: `src/adapter/metamod/plugin_runtime.hpp:4-20,129-181,206-310`
- Modify: `src/adapter/metamod/plugin_runtime.cpp:1816-2072,2074-2288`
- Modify: `CMakeLists.txt:489-500,548-561,591-603,628-640`
- Modify: `tests/compat_actor_command_tests.cpp` only for observations-specific regression assertions

**Interfaces:**
- Consumes: `ObservationAdapter::collectActor`, `CompatibilityObservation`, existing `LifecycleToken`, `WorldSnapshot`, objective scanner, and current command timing context.
- Produces: the same `WorldSnapshot`, `ActionProposal`, and objective target behavior as before, with observation collection and quality available at the adapter boundary.

- [ ] **Step 1: Add a regression assertion before integration.**

  Extend the existing fake-entity command fixture with a test that records the current public player position/team/FOV and asserts the current action-sensor result still dispatches the same movement/action boundary. Keep the test independent of private weapon state and do not alter the existing expected button or client-command assertions.

- [ ] **Step 2: Run the regression test to establish the baseline.**

  Run:

  ```powershell
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R astrabot_compat_actor_command --output-on-failure
  ```

  Expected: the existing fixture passes before the adapter is wired into `PluginRuntime`.

- [ ] **Step 3: Add the adapter to `PluginRuntime` and configure it with existing engine pointers.**

  Include `observation_adapter.hpp`, add one `ObservationAdapter observationAdapter_` member, initialize it in the constructor, and configure it from `giveEnginePointers` without changing the scheduler, lifecycle, or hook order. Build a `ObservationTimingContext` from the current command sequence when collecting inside a command; use zero for upkeep/full-update sequence fields because `BotTimingScheduler` does not expose event counters and P04 must not change its semantics.

- [ ] **Step 4: Replace direct public player reads in `buildManagedWorldSnapshot`.**

  For each eligible entity, call `ObservationAdapter::collectActor` with the current actor key, frame identity, and timing context. Populate the existing `ActorObservation` from the adapter's public player view. Preserve the existing eligibility checks, effective-team fallback, nearest-hostile selection, `PerceptionAssembler` validation, and return behavior. The adapter's metadata is diagnostic/provenance state; it must not change hostile selection or confidence policy.

- [ ] **Step 5: Route objective entity facts through the same boundary.**

  Keep the existing bomb-target and planted-C4 scan and path selection unchanged. Wrap public facts such as C4 possession, classname/model, bomb position, and `dmgtime` in objective observations with `Inferred` or `ExactEngineApi` as appropriate. Keep hostage/VIP/rescue fields unavailable unless a public source is actually present. Do not change `buildManagedObjectiveTarget`'s team, bomb-site, or planted-bomb decisions.

- [ ] **Step 6: Keep the synthetic weapon path explicitly marked.**

  In `decideManagedBotAction`, preserve the existing `WeaponRecord` values and combat call. Add an observation record or adapter diagnostic that marks active weapon, accuracy, reload timers, ammo, silencer, burst, and zoom as unavailable/private where no public source exists; do not promote the synthetic rifle to exact observation and do not alter `CombatController`.

- [ ] **Step 7: Build and run the focused integration tests.**

  Run:

  ```powershell
  cmake --build build-action-adapter-x86-1451 --target astrabot_compat_actor_command --config Debug
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_(observation_adapter|compat_actor_command|action_adapter)$" --output-on-failure
  ```

  Expected: the adapter fixture and existing action boundary remain green, with no changed button, client-command, timing, or objective decision assertions.

- [ ] **Step 8: Run the lifecycle isolation regression.**

  Run:

  ```powershell
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_(lifecycle_generation|fake_client_isolation|actor_registry)$" --output-on-failure
  ```

  Expected: slot generation, disconnect/reuse, FakeClient isolation, and lifecycle tests pass; no observation from a prior actor is reused.

### Task 4: Add bounded observation trace and enhanced/compatibility isolation tests

**Files:**
- Modify: `include/astrabot/compat/observation.hpp`
- Modify: `src/core/compat/observation.cpp`
- Modify: `src/adapter/metamod/observation_adapter.cpp`
- Modify: `tests/observation_model_tests.cpp`
- Create: `tests/observation_trace_tests.cpp`
- Modify: `CMakeLists.txt:69-80,488-500`

**Interfaces:**
- Consumes: Task 1 metadata and Task 2 adapter trace sink.
- Produces: optional bounded observation trace records independent from `IRandomTraceSink` and the timing scheduler.

- [ ] **Step 1: Write the failing isolation and trace tests.**

  Add tests that attach a `TraceCollector`, collect actor A then actor B, and assert semantic IDs, actors, frames, quality, source, freshness, and timing are preserved independently. Add a test that a separate Enhanced observation instance cannot mutate a Compatibility record or increment the P03 RNG source position.

- [ ] **Step 2: Run the trace target and confirm RED.**

  Register `astrabot_observation_trace` with the Core/adapter sources, then run:

  ```powershell
  cmake --build build-action-adapter-x86-1451 --target astrabot_observation_trace --config Debug
  ```

  Expected: the new sink/target APIs fail to compile before implementation.

- [ ] **Step 3: Implement bounded trace emission.**

  Emit at most one trace record per collected semantic field when a sink is installed. Leave the sink null by default. Do not format unbounded strings, write files from the adapter, call `RANDOM_*`, or add scheduler events. Use zero sequence fields only when the corresponding runtime event is unavailable and document that zero means “no context,” not a new event.

- [ ] **Step 4: Run trace, timing, and RNG tests.**

  Run:

  ```powershell
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_(observation_trace|runtime_timing|compatibility_rng|engine_random_source)$" --output-on-failure
  ```

  Expected: trace isolation passes and all P02/P03 tests remain unchanged and green.

### Task 5: Build the source-grounded inventory and parity documentation

**Files:**
- Create: `docs/parity/OBSERVATION_MODEL.md`
- Modify: `docs/parity/OBSERVATION_MATRIX.md`
- Modify: `docs/parity/SOURCE_MAP.md`
- Modify: `docs/parity/PARITY_MATRIX.md`
- Modify: `docs/parity/TRACE_SCHEMA.md`
- Modify: `docs/parity/KNOWN_DEVIATIONS.md`
- Modify: `docs/parity/STATUS.md`

**Interfaces:**
- Consumes: the pinned ReGameDLL source at `H:/sourcecode/003.Game/amxmodx/ReGameDLL_CS`, current AstraBot source, Task 1-4 observation IDs/metadata, and current P02/P03 documents.
- Produces: an auditable P04 inventory and status report that does not claim MATCH without source, timing, value, and lifecycle proof.

- [ ] **Step 1: Inventory reference consumers from the pinned commit.**

  Inspect the source at the pinned commit with targeted commands for `cs_bot.cpp`, `cs_bot_vision.cpp`, `cs_bot_weapon.cpp`, `cs_gamestate.cpp`, `cs_bot_manager.cpp`, `cs_bot_init.cpp`, scenario/hostage/VIP files, and public Engine trace use. Record each semantic observation ID, reference member/API, consumer, and whether the state is player, weapon, objective, entity, trace, profile, or scenario data.

- [ ] **Step 2: Audit current Astra raw access.**

  Run targeted `rg` searches for `pvPrivateData`, pdata helpers, `entity->v`, `pfnTraceLine`, `pfnFindEntityByString`, message handlers, weapon proxies, and objective scans. Classify every relevant access as `acceptable boundary`, `legacy direct access`, `needs migration`, or `not parity related`; do not migrate unrelated lifecycle or FakeClient code.

- [ ] **Step 3: Write `OBSERVATION_MODEL.md`.**

  Document the data flow, quality/freshness/source semantics, actor/frame lifecycle, timing context, trace sink, capability handling, Compatibility versus Enhanced isolation, and the explicit rule that unavailable values cannot become defaults.

- [ ] **Step 4: Expand the matrix with all required high-risk rows.**

  Add rows for player health/armor/team/life/origin/velocity/view/recoil/FOV/duck/ground/water/ladder/max speed/buttons/current weapon/ammo/zones/kit/shield/VIP/C4/hostage; weapon ID/clip/reserve/reload/next attack/accuracy/flags/silencer/burst/zoom/recoil/deploy; bomb/objective/world state; trace/visibility/entity properties; and hostage/VIP/rescue. Each row includes semantic ID, reference source, consumer, Astra source, status, delay, risk, and notes.

- [ ] **Step 5: Update status and deviations without promoting parity.**

  Record the P04 start/end commits, classification counts derived from the matrix, focused/full test evidence, FocalSpan status, Python/PE and live acceptance gates, P02/P03 regression results, remaining unavailable private state, and `PARTIAL` when critical observations remain unverified. State explicitly that P05 State Machine parity is not started.

- [ ] **Step 6: Check documentation consistency.**

  Run:

  ```powershell
  rg -n "MATCH|IMPLEMENTED_UNVERIFIED|UNAVAILABLE|NOT_YET_IMPLEMENTED|OBS-[A-Z0-9-]+|P05" docs/parity
  git diff --check
  ```

  Expected: every MATCH row has evidence, all unavailable rows are explicit, IDs are stable semantic names, and no document says P05 is complete.

### Task 6: Full validation, artifact identity, and P04 handoff

**Files:**
- Modify only the P04 implementation/test/documentation files from Tasks 1-5.
- Do not modify `.focalspan/`, `.focalspan.json`, `.serena/`, generated DLL/object files, or unrelated untracked files.

**Interfaces:**
- Consumes: completed Core/adapter implementation, documentation matrix, existing Phase 8 checks, and the Windows x86 NMake toolchain.
- Produces: verified P04 result report and an explicit `PASS`, `PARTIAL`, or `BLOCKED` status without entering P05.

- [ ] **Step 1: Reinitialize the matching x86 developer environment.**

  In one `cmd.exe` process, run:

  ```cmd
  call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x86
  where cl
  ```

  Expected: `where cl` resolves to the cached HostX86/x86 compiler required by `build-action-adapter-x86-1451`.

- [ ] **Step 2: Configure the current source without changing the cache architecture.**

  Reconfigure the existing x86 NMake Debug build using the repository's recorded CMake options and `ASTRABOT_METAMOD_SDK_ROOT`. Confirm the cache remains 32-bit and no ad-hoc Windows SDK include path is added.

- [ ] **Step 3: Build the complete Debug target set.**

  Run:

  ```cmd
  cmake --build build-action-adapter-x86-1451 --config Debug
  ```

  Expected: Core, Metamod plugin, all focused tests, and all existing CTest executables build from the current source.

- [ ] **Step 4: Run focused and full CTest.**

  Run:

  ```cmd
  ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_(observation_model|observation_adapter|observation_trace|compat_actor_command|action_adapter|runtime_timing|compatibility_rng|engine_random_source|lifecycle_generation|fake_client_isolation|actor_registry)" --output-on-failure
  ctest --test-dir build-action-adapter-x86-1451 -C Debug --output-on-failure
  ```

  Expected: all focused observations and the complete suite pass. Any Not Run, environment, or pre-existing failure is named separately rather than counted as PASS.

- [ ] **Step 5: Run Phase 8 PowerShell checks without claiming live parity.**

  Run the repository's current Phase 8 fixture/self-test commands, including `phase8_objective_gate_test.ps1`, `phase8_action_adapter_boundary_test.ps1`, and the current artifact/manifest checks. Record results separately from CTest and do not convert fixture success into live HLDS/ReHLDS acceptance.

- [ ] **Step 6: Refresh FocalSpan and inspect graph limitations.**

  Run:

  ```powershell
  focalspan update --root .
  focalspan status --json
  focalspan -- "How do the final ObservationAdapter and CompatibilityObservation paths connect to WorldSnapshot, objective sensing, tests, and trace without touching timing or RNG?"
  ```

  Record FocalSpan freshness and state any CRG index SHA mismatch or graph gap; current source/build/runtime evidence remains authoritative.

- [ ] **Step 7: Verify artifact identity and staged scope.**

  Compare current source/build/deployed DLL SHA-256 and PE export checks using the repository's existing verification scripts where the Python environment can run. Stage explicit P04 files only, then run:

  ```powershell
  git diff --cached --check
  git diff --cached --stat
  git status --short
  ```

  Expected: no unrelated untracked artifact is staged, and source/build/deployed identity is reported separately from tests.

- [ ] **Step 8: Write the final P04 report and stop.**

  Report the required sections: result, reference/start/end, inventory counts, architecture, high-risk observations, production fixture, tests, validation, P02/P03 regression, parity changes, known deviations, changed files, commit, remaining dirty files, P05 readiness, and blockers. Use `PARTIAL` when private weapon/objective/visibility observations remain unavailable or unverified. Do not start P05.

