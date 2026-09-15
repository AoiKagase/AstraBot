# AstraBot CSBot-Compatible Metamod Plugin Design

**Date:** 2026-09-15
**Status:** Design revision pending written-spec review

## 1. Purpose

AstraBot is an independently implemented Counter-Strike 1.6 Bot that can replace the operational surface and gameplay behavior of the CSBot/ZBot implementation shipped by ReGameDLL-CS. The first target is a standalone Metamod-P plugin for unmodified ReGameDLL-CS, with Windows 32-bit and Linux 32-bit builds, existing `bot_*` operation compatibility, and read-only loading of existing `.nav` data. Nav creation, learning, editing, and the optimized `astranav` format are later AstraNav work, not part of the first CSBot parity target.

The project is a clean-room behavioral reimplementation. ReGameDLL-CS and Metamod-P are reference inputs for public behavior, ABI boundaries, file formats, and observable results. Their Bot source, internal classes, and implementation structure are not copied or linked into AstraBot.

## 2. Goals

- Reproduce the observable CSBot/ZBot lifecycle and gameplay behavior closely enough to replace it in normal server operation when a compatible existing `.nav` is available.
- Load without modifying or relinking ReGameDLL-CS.
- Use only Metamod-P, HLSDK, Engine, and GameDLL public boundaries at runtime; ReAPI and private DLL patches are not dependencies.
- Preserve the operational surface of existing `bot_*` commands/CVars, Bot profiles, `.nav` files, and server configuration.
- Keep a format-neutral Nav model and persistence boundary so a future AstraNav subsystem can add Nav creation, learning, editing, analysis, and an optimized `astranav` file without changing Bot decision logic.
- Keep Core behavior testable in deterministic offline simulations and replay tests.
- Produce and verify Windows x86 and Linux x86 artifacts separately from real-server acceptance.

### Initial CSBot parity boundary

The initial implementation accepts and loads existing compatible `.nav` files only. It does not generate Nav, learn new geometry, edit Nav, analyze a map into new Nav data, or write back `.nav`/`astranav`. If a compatible Nav file is missing or invalid, the plugin reports the condition and does not claim CSBot parity for that map. These capabilities belong to the later AstraNav extension.

## 3. Constraints and references

- Target game: GoldSrc Counter-Strike 1.6 with ReGameDLL-CS and Metamod-P.
- Runtime target: unmodified ReGameDLL-CS plus Metamod-P.
- Build targets: Windows 32-bit and Linux 32-bit.
- ReGameDLL-CS reference commit: `b0889847fe6d03898be88acc9e366660efb40ab5`.
- Metamod-P SDK reference commit: `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
- `C_CPP_REFACTOR_RULES.md` is mandatory for all AstraBot C/C++ source and tests: real tab indentation, LF, UTF-8 without BOM, trailing-newline and whitespace checks, explicit error handling, existing naming conventions, and verification proportional to the change.
- Runtime Engine/GameDLL calls are main-thread-only.
- SDK and GameDLL types remain inside adapters. Core contracts use value types, stable IDs, generation stamps, and explicit result values.
- The reference ReGameDLL-CS checkout currently contains unrelated local modifications. It is read-only input for this project and must not be changed.
- Distribution licensing is a release gate. The project must retain source-origin and license records and must not be described as “MPL-only” without a final license review.

## 4. Architecture

```text
Metamod-P hooks
    -> GoldSrc HostAdapter
    -> WorldSnapshot / GameEvent
    -> AstraBot Core
    -> BotCommand / CommandReceipt
    -> GoldSrc HostAdapter
    -> Engine / GameDLL / FakeClient
```

### 4.1 Core

The SDK-free Core is divided by responsibility:

- `BotRuntime`: Bot lifecycle, actor identity, Entity generation, map generation, and round generation.
- `WorldModel`: immutable frame values for players, objectives, weapons, sounds, rounds, and map state.
- `PerceptionSystem`: vision, sound, contacts, confidence, memory, and uncertainty.
- `ObjectivePlanner`: attack, defend, rotate, retake, save, escort, rescue, and scenario objectives.
- `BehaviorStateMachine`: high-level Bot state transitions.
- `NavigationSystem`: Nav model, queries, local movement, and recovery; future learning, editing, and persistence ports remain isolated behind AstraNav boundaries.
- `ActionSystem`: movement, view, firing, reload, weapon selection, purchase, bomb, hostage, and radio actions.
- `CompatibilityLayer`: command/CVar/profile contracts and compatibility diagnostics.

Core ports include `Clock`, `RandomSource`, `TracePort`, `EntityPort`, `FileStore`, and `CommandSink`. No port exposes `edict_t`, `Vector` from the SDK, private GameDLL classes, or an Engine function table.

### 4.2 Adapters

- `adapter/goldsrc`: translates Entity, FakeClient, Trace, physics, weapons, messages, file paths, and game events.
- `adapter/metamod`: implements plugin exports, hook tables, lifecycle, command interception, CVar access, logging, and dispatch.
- `tools`: validates existing Nav files, runs deterministic simulations, and replays traces. Nav generation and learning tools are deferred to AstraNav.

## 5. Runtime lifecycle

The adapter validates the Metamod interface in `Meta_Query`, creates the runtime in `Meta_Attach`, and releases it in `Meta_Detach`. `GiveFnptrsToDll` and the Engine/GameDLL API tables are treated as ABI boundaries and are validated before use.

`ServerActivate` creates a map session, loads and validates Nav data, establishes the map fingerprint, and initializes the Bot registry. `ClientPutInServer` and `ClientDisconnect` bind and retire slot-scoped identities. `StartFrame` captures one world snapshot, processes events, schedules Bot ticks, calls Core, validates commands, and dispatches only current commands. `CmdStart` and the required player hooks provide the input and post-dispatch feedback boundary. `ServerDeactivate` invalidates map state and releases all Entity references. Initial CSBot parity has no Nav write or learning flush path.

All observations, plans, commands, and receipts carry map, round, tick, actor, Entity generation, Nav revision, and sequence information. A stale or duplicate receipt cannot advance a route or state machine.

### 5.1 Native CSBot guard

ReGameDLL-CS contains a native CSBot manager. AstraBot must prevent double ownership:

- disable the native Bot path while AstraBot is active;
- expose the existing `bot_*` surface through the compatibility layer;
- mark AstraBot-created FakeClients with an independent identity;
- detect and report any native Bot that appears after suppression;
- fail closed rather than silently mixing native and AstraBot actors.

The exact suppress/override behavior is a Phase 1 runtime acceptance item because it depends on the actual Metamod hook order and CVar behavior. If the standard public boundary cannot guarantee isolation, the plugin reports the incompatibility and does not create managed Bots.

## 6. Nav model and initial legacy load

The format pipeline is:

```text
.nav
    -> LegacyNavReader
    -> format-neutral NavDocument
    -> NavNormalizer / NavEnricher
    -> immutable NavSnapshot
    -> NavQuery / PathPlanner
```

The initial `LegacyNavReader` supports legacy `.nav` versions 1 through 5 and is read-only. `NavDocument` contains no file-format-specific type, leaving room for a later `AstraNavCodec`. Loading is transactional and rejects invalid counts, overflow, invalid indices, non-finite coordinates, duplicate identity, and unsafe allocations without partially publishing the graph.

### 6.1 Area data

- stable Area ID, center, boundaries, corners, extent, and spatial index;
- floor/ceiling heights, floor normal, slope, support, and step-up information;
- standing and crouching hull clearance;
- Room, Corridor, Choke, Stair, Connector, and similar derived classification;
- Place/region identity;
- spawn, buy, bombsite, hostage, rescue, and other objective metadata;
- hiding, cover, peek, hold, sniper, anchor, entry, support, and fallback candidates;
- directional exposure and static visibility profiles;
- acoustic region and static sound propagation metadata.

### 6.2 Directed traversal data

Every traversal is an independently validated directed link. Reverse traversal is not inferred automatically.

```text
TraversalLink
    stableId
    fromArea
    toArea
    kind
    entryPortal
    exitPortal
    approachDirection
    clearance
    requiredPosture
    movementEnvelope
    baseTravelCost
```

Supported kinds include Walk, Crouch, StepUp, Jump, Drop, Ladder, Door, NarrowPassage, and LearnedTraversal. The movement envelope records the relevant launch/landing, clearance, slope, height, horizontal reach, damage risk, and input requirements. Runtime geometry and physics still validate the envelope before execution.

### 6.3 Future AstraNav and learned layers

The initial CSBot parity graph is loaded read-only and remains immutable. A future AstraNav subsystem may compose it with derived and learned data such as `FAST`, `SAFE`, `LOW_EXPOSURE`, `LOW_TRAFFIC`, `FLANK`, and `OBJECTIVE_FAST` policies.

Future persistent experience will be keyed by map and Nav revision and will keep human and Bot evidence separate. Area experience may include visits, team danger, encounters, deaths, grenades, sniper threat, push/retake success, and traffic. Traversal experience may include human/Bot attempts, successes, failures, time, damage, posture, and failure reason. This is outside the initial parity target.

Round/session overlays contain current traffic, reservations, temporary blockers, cooldowns, contextual danger, and current enemy beliefs. They are not facts about the static Nav and are not unconditionally written into the base graph.

## 7. Future AstraNav ballistics and Wallbang geometry

Wallbang is a later advanced-AI extension, not an initial CSBot parity gate. When AstraNav adds it, static geometry will be required, but a single wall-thickness field is insufficient. The optional `BallisticsGeometry` chunk in `astranav` stores representative penetration paths from tactical points and direction sectors:

```text
PenetrationPath
    sourceTacticalPoint
    directionSector
    targetAreaOrSector
    layers[]
    totalThickness
    entryPoint
    exitPoint
    materialProfile
    confidence
    mapFingerprint
```

Each layer records material/surface, thickness, entry/exit normals, and surface flags. Precomputation is bounded to cover, peek, sniper, objective, and likely-enemy sectors rather than every ray in the map.

At fire time, a future Core extension combines this static geometry with current weapon penetration, distance, angle, ammunition, enemy belief, friendly-fire risk, and fresh Trace results. Unknown geometry is not treated as penetrable. Dynamic enemy positions and hidden engine truth are never stored in Nav or used as a wallhack target. Wallbang outcomes are learned in an experience sidecar keyed by a stable geometry/profile identity.

## 8. Information boundaries

Nav may contain static visibility, acoustic reachability, cover geometry, and route risk. It must not contain current hidden enemy coordinates, current Visual/Sound memory, Bot personality, current role, current tactical plan, current path, Entity pointers, or unverified traversal claims.

Team communication may share explicit observations, tactical proposals, assignments, and objective status, but uncertainty must not be converted into omniscient certainty. Opponent profiles remain map-session or explicitly scoped experience data rather than raw Nav facts.

## 9. Failure and resource policy

The plugin fails closed for ABI mismatch, missing required hooks, corrupt or mismatched Nav, invalid Trace, stale generations, invalid commands, native Bot mixing, and unsafe future Wallbang information. Initial parity has no Nav write path, so a loaded source Nav remains untouched.

No C++ exception crosses a Metamod C ABI boundary. All queues, Bot ticks, Trace queries, path searches, learning candidates, logs, and Nav allocations have explicit upper bounds. Engine and GameDLL calls are never made from worker threads. Diagnostics include map/round/tick/actor/Entity generation/Nav revision, sequence, and failure reason.

## 10. Verification and acceptance

### 10.1 Portable Core

- Nav graph/query, legacy Codec, `astranav` schema, CRC, corruption, and bounded allocation tests;
- traversal envelope tests;
- deterministic perception, planning, combat, and replay tests;
- generation, Unknown, stale command, and resource-limit tests;
- Windows x86 and Linux x86 builds.

### 10.2 Metamod adapter

- plugin exports and ABI/calling convention;
- hook pre/post behavior and lifecycle;
- FakeClient create/join/remove/reuse;
- `bot_*` command/CVar/profile compatibility;
- native CSBot suppression;
- Release artifact export verification.

### 10.3 Fake host/integration

- map/round/Entity generation changes;
- multiple Bot isolation and scheduling;
- rejected Trace/Command/receipt paths;
- Nav load and immutable Nav publication;
- Tactical, Combat, TeamReport, and hidden-information boundaries.

### 10.4 Real server

On an unmodified ReGameDLL-CS plus Metamod-P installation, verify plugin loading, existing configuration migration, Bot management, read-only `.nav` load, locomotion, perception, combat, objectives, map changes, round restarts, disconnects, Entity reuse, 1v1, 2v2, and the configured multi-Bot count on both supported operating systems. Automatic Nav creation/learning and Wallbang are not initial parity gates.

Offline tests are evidence for their layer only. They do not replace real HLDS/ReHLDS movement, combat, stability, or multi-Bot acceptance. Each live result records the DLL, SDK, BSP, Nav, map, configuration, Bot count, tick trace, and failure details.

## 11. Implementation sequence

The roadmap should keep each item independently buildable and verifiable:

1. Project/toolchain, source manifest, and ABI contract.
2. Metamod plugin skeleton, lifecycle, hooks, and native Bot guard.
3. FakeClient registry and safe command/input dispatch.
4. `bot_*` command/CVar/profile compatibility.
5. Legacy `.nav` read-only Reader and immutable Nav model.
6. Area queries, directed traversal, and basic Walk/Crouch/Step/Jump/Drop/Ladder execution.
7. CSBot parity for perception, visual/sound memory, WorldModel, combat, weapons, objectives, state machines, radio/chatter, and round recovery.
8. Differential behavior tests, multi-Bot integration, and full cross-platform/live CSBot parity acceptance.
9. AstraNav: Nav generation, learning, editing, analysis, atomic persistence, and `astranav` Codec.
10. AstraNav-derived tactical/visibility/acoustic/ballistic chunks, Wallbang, adaptive routing, and advanced learning.

Each roadmap phase may be split into smaller GSD plans. A phase is complete only after its implementation, applicable offline verification, documentation, and required live acceptance are separately recorded.

## 12. Source-grounding record

Behavioral and design inputs inspected during brainstorming include:

- `ReGameDLL_CS/regamedll/dlls/bot` and its public Metamod/HLSDK-facing boundaries;
- `AstraBot_bk/docs/plans/phase-9-persistent-experience.md`;
- `AstraBot_bk/docs/plans/phase-10-adaptive-tactical-navigation.md`;
- `AstraBot_bk/docs/plans/phase-11-advanced-learning-and-traversal.md`;
- `AstraBot_bk/docs/plans/phase-12-zbot-compatibility.md`;
- `AstraBot_bk/src/core/p11_learning.hpp`, `experience.hpp`, `visual_memory.hpp`, `sound_memory.hpp`, and `team_reports.hpp`;
- `AstraBot_bk/src/nav/model`, `nav/query`, `nav/enrichment`, and `nav/local` contracts.

These files are behavioral comparators and design evidence only. They are not implementation inputs to copy into this project.
