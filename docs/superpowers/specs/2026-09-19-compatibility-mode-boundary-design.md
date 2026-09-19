# AstraBot Compatibility / Enhanced Mode Boundary

## Status

P01 design approved by the user on 2026-09-19. This design is limited to
separating runtime mode selection and enhanced-decision capability checks. It
does not claim CSBot behavioral parity.

## Goal

Make the runtime mode explicit and observable while ensuring that Compatibility
Mode is a baseline-only route. Enhanced Mode may enable Astra-specific decision
modifiers later, but P01 does not add new intelligence or alter the current
baseline candidate behavior.

## Current flow

The current direct call path is:

```text
HookStartFrame
  -> PluginRuntime::onStartFrame
  -> CVar synchronization / lifecycle observation
HookStartFramePost
  -> PluginRuntime::onStartFramePost
  -> processJoinControllers
  -> updateManagedBotMovement
  -> buildManagedWorldSnapshot / buildManagedObjectiveTarget
  -> NavRoamController
  -> CombatController / RoundObjectivePlanner
  -> ActionAdapter
  -> CommandQueue / InputDispatcher
  -> pfnRunPlayerMove and GameDLL client command
```

`CvarState` currently stores only `bot_enable`, `bot_stop`, difficulty, quota,
and join-team settings. `PluginRuntime` owns the runtime controllers directly;
there is no mode field in `PluginRuntime::Snapshot` and no explicit enhanced
capability policy.

## Design

### 1. Extend the existing configuration machinery

Add one `RuntimeMode` enum to the existing compatibility configuration domain:

- `RuntimeMode::Compatibility` — default and test-oracle mode.
- `RuntimeMode::Enhanced` — explicit opt-in mode.

Store it in `compat::CvarSnapshot` and parse it through the existing
`CvarState`/`BotConfiguration`/`CompatibilitySurface` path. Register and sync
one server CVar, `astrabot_mode`, with the strings `compatibility` and
`enhanced`. Invalid values must leave the previous mode unchanged.

This avoids a second configuration system and keeps the mode value available to
both the command surface and the runtime orchestrator.

### 2. Centralize enhancement capability policy

Add a small pure policy value derived from `RuntimeMode`. It exposes capability
checks for the P00-documented Astra-only decision modifiers:

- enhanced decision overrides;
- adaptive route weighting;
- opponent-profile decision changes;
- tactical/team-director overrides;
- learning/adaptation side effects.

Every capability returns false in Compatibility Mode and true in Enhanced Mode.
The policy is not a new persistent configuration source. It is a deterministic
view of the selected mode, so disabled code is bypassed before it can consume
RNG, update caches, or change scheduling.

P01 does not classify any existing controller as `MATCH`. The existing
`NavRoamController`, `CombatController`, and `RoundObjectivePlanner` remain
baseline candidates shared by both modes; future enhanced overrides must enter
through the policy boundary rather than modifying those baseline outputs in
place.

### 3. Make mode observable

Expose the selected mode through `PluginRuntime::Snapshot` and include it in
runtime mode diagnostics at map activation and compatibility-command handling.
Trace/status consumers can therefore identify whether a record was produced in
Compatibility or Enhanced Mode.

### 4. Prove isolation with tests

Add unit coverage for default mode, valid/invalid mode parsing, policy values in
both modes, and no mutation after an invalid update. Extend the Metamod runtime
snapshot test to verify that the active runtime reports Compatibility Mode by
default. The isolation test must assert that every documented enhanced capability
is disabled in Compatibility Mode.

## Invariants

1. A fresh runtime starts in Compatibility Mode.
2. Compatibility Mode is the only mode used when no explicit CVar is supplied.
3. Invalid mode strings do not change the current mode.
4. Compatibility Mode does not call enhanced decision modifiers and does not
   consume enhanced RNG/cache state.
5. Enhanced Mode remains buildable and selectable, but P01 does not add new
   enhanced behavior.
6. Existing fake-client creation, movement dispatch, combat/objective action
   translation, and lifecycle behavior are not rewritten by P01.
7. No status is promoted to `MATCH` by adding the mode boundary.

## Explicit non-goals

- 30/10Hz timing parity.
- RNG source, seed, tape, or consumption-order parity.
- private GameDLL player/weapon state access.
- CSBot state-machine, perception, combat, NAV, economy, objective, or chatter
  parity.
- deletion or rewrite of Astra-specific modules.
- live HLDS/ReHLDS acceptance.
