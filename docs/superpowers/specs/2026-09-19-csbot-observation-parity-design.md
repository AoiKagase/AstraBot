# CSBot Observation Parity Design

**Date:** 2026-09-19  
**Phase:** P04 — GameDLL Private-State / Observation Parity  
**Status:** Design approved for specification; implementation not started

## Goal

Establish a small, SDK-free Compatibility Observation boundary that records
what AstraBot can obtain about a player, weapon, objective, world entity, and
trace at decision time, together with the value's semantic identity,
provenance, freshness, lifecycle identity, and reliability. The boundary must
make unavailable information explicit instead of allowing a default value to
masquerade as authoritative state.

The result is an auditable observation inventory and an adapter-to-Core test
surface. It is not a CSBot state-machine implementation and does not change
combat, aim, weapon selection, objective decisions, navigation, buying, P02
timing semantics, or P03 RNG infrastructure.

## Frozen inputs and constraints

- AstraBot P04 start: `ebee3b461130ad65fdbbe9bdc2139389372c8ada`.
- ReGameDLL-CS observation reference: `b0889847fe6d03898be88acc9e366660efb40ab5`.
- ReGameDLL-CS is a behavioral/source oracle only. Its private classes,
  private symbols, ReAPI, and implementation code are not copied or linked.
- The runtime boundary remains public Metamod-P, HLSDK Engine, GameDLL, and
  AstraBot-owned Core contracts.
- Existing dirty and untracked worktree content is preserved. P04 stages only
  its own files.
- `WorldSnapshot`, `WeaponInventory`, objective models, lifecycle generation,
  and the P02/P03 contracts remain reusable and are not replaced wholesale.
- Unknown and unavailable values must retain validity and quality metadata;
  `0`, `false`, an empty string, or a default FOV must not be presented as an
  observed value without explicit evidence.

## Current evidence

The current adapter builds a public-edict `WorldSnapshot` in
`PluginRuntime::buildManagedWorldSnapshot`. It reads public position,
velocity, life, spectator, and team-related fields, while team fallback is
maintained by the join controller. `decideManagedBotAction` currently creates
a synthetic rifle `WeaponRecord`; it does not read GameDLL-private active
weapon, clip, reserve ammo, accuracy, reload timers, or zoom state. Objective
handling uses public proxies such as the C4 weapon bit, bomb entity
classification, `dmgtime`, and bomb-target entities. The existing parity
matrix correctly classifies these as partial, inferred, synthetic, or
unavailable rather than MATCH.

The CRG index was built at an older SHA and currently provides no useful
cross-community or semantic result for this boundary. Direct current source,
the pinned reference checkout, CMake/tests, and FocalSpan are therefore the
authoritative design inputs.

## Alternatives considered

### A. Add metadata directly to every existing domain model

This would put transport provenance and acquisition quality into
`WorldSnapshot`, `WeaponInventory`, and objective state directly. It is
initially small, but it couples decision-domain objects to adapter concerns,
makes lifecycle invalidation inconsistent, and encourages synthetic weapon
records to look like native observations.

### B. Add a dedicated Compatibility Observation boundary — selected

An adapter creates typed observation records with explicit metadata. Existing
domain models can consume a projected snapshot where appropriate, while the
original observation remains available for trace, inventory, and capability
decisions. This keeps the public/private boundary visible and permits fixture
injection without making Core depend on SDK headers.

### C. Use an untyped semantic event bag

An event bag would be easy to log but would force consumers and tests to parse
strings, weakens type and lifecycle guarantees, and makes `UNKNOWN` versus
valid zero difficult to enforce. It is unsuitable as the primary boundary.

## Architecture

```text
Metamod / public Engine / public GameDLL messages
                    |
                    v
       metamod::ObservationAdapter
                    |
                    v
       compat::CompatibilityObservation
       (typed values + provenance + capability)
          |             |              |
          v             v              v
   WorldSnapshot   Weapon view    Objective view
          |
          v
      existing decision consumers
```

### Core contract

The implementation will add a focused SDK-free contract under
`astrabot::compat`:

- `ObservationQuality`: `Exact`, `ExactEngineApi`, `ExactGameApi`, `Delayed`,
  `Inferred`, `Approximated`, `Unavailable`, `NotYetImplemented`,
  `NotRequired`, and `Unknown`.
- `ObservationFreshness`: `SameTick`, `CurrentFullUpdate`,
  `EventDrivenCached`, and `Stale`.
- `ObservationSource`: public edict, public engine API, public GameDLL/message,
  adapter cache, fixture, synthetic, or none.
- `ObservationContext`: semantic ID, actor key, frame identity, command /
  upkeep / full-update sequence, source, freshness, quality, and bounded delay
  in ticks.
- `ObservationValue<T>`: a typed value with `valid` state and its context.
  Invalid values remain queryable for their quality and source but cannot be
  mistaken for valid values.
- `CompatibilityObservation`: grouped player, active-weapon, objective,
  entity, and trace observations for one actor and one frame. Capabilities are
  represented as a small typed availability result per field/group, not as a
  general plugin-wide capability framework.

Semantic IDs are stable names such as `OBS-PLAYER-FOV`,
`OBS-PLAYER-ACTIVE-WEAPON`, `OBS-WEAPON-ACCURACY`,
`OBS-WEAPON-NEXT-PRIMARY`, `OBS-OBJECTIVE-BOMB-PLANTED`, and
`OBS-TRACE-LINE`. Source line numbers are supporting evidence only.

### Adapter contract

`metamod::ObservationAdapter` is the only new production path that collects
P04 observations from native-facing state. It may use public `edict_t` fields,
public engine callbacks such as trace/entity lookup, and public message data
already received by `PluginRuntime`. It must not add `get_pdata_*`,
`pvPrivateData` reads, private ReGameDLL APIs, ReAPI, or hard-coded private
offsets.

The first integration routes the existing world/objective sensor inputs
through this boundary without changing their decision policy. Synthetic
weapon data remains explicitly synthetic until a public, portable source is
proven. Private active weapon, clip/reserve, accuracy, reload timers, weapon
flags, silencer, burst, and internal zoom state are recorded as
`Unavailable` or `NotYetImplemented`, never as guessed values.

Public player fields such as health, armor when present, team, life state,
origin, velocity, view angles, `pev->fov`, posture/ground/water flags,
movement flags, max speed, buttons, old buttons, solid, movetype, bounds,
class/model, and public weapon bits are individually mapped. A public field
is not automatically `MATCH`: timing, lifecycle, reference consumer, and
value semantics must also be documented.

Objective records distinguish direct public entity facts from inferred
scenario meaning. C4 possession, dropped/planted entity, position, bomb
timer proxy, defuse/kit, bomb zone, hostage, rescue, and VIP rows each retain
their own status. No objective decision is changed in P04.

Trace records preserve the requested start/end, ignore semantics, flags, hit
entity, hit position, and fraction when the public Engine trace API supplies
them. Visibility remains an observation inventory item; P04 does not implement
CSBot FOV/visibility parity.

### Freshness and lifecycle

Every observation carries the frame identity and actor generation already used
by the current world/lifecycle models. Event/message values carry their source
sequence and bounded age. A new map, round, spawn generation, death, or slot
reuse invalidates cached actor observations. A stale record remains
diagnosable but is not returned as current.

The adapter receives timing context from the already-existing runtime event;
it does not alter `BotTimingScheduler`, 30/10 Hz cadence, deadline rebasing,
`msec`, command persistence, or command sequence allocation.

### Trace integration

Observation tracing is an optional, bounded sink disabled by default. A record
contains:

```text
obs=<semantic-id>
actor=<slot:generation>
value=<typed/redacted value>
quality=<quality>
source=<source>
freshness=<freshness>
frame=<map:round:tick>
timing=<command:upkeep:full-update>
delay_ticks=<integer>
```

The sink is independent from the P03 RNG trace sink. Observation collection
must not request RNG values, reorder RNG calls, or create scheduler events.

## Inventory and classification

`docs/parity/OBSERVATION_MATRIX.md` becomes the P04 source of truth. Each row
contains:

| Field | Meaning |
|---|---|
| Semantic ID | Stable observation identity |
| Category | Player, weapon, objective, entity, trace, profile, or scenario |
| Reference member/API | Pinned ReGameDLL consumer and source location |
| ReGameDLL consumer | Why CSBot reads the state |
| Astra source | Adapter field/API/cache or explicit absence |
| Status | One of the ten P04 quality classes |
| Delay | Same tick, full update, event/message delay, or stale |
| Risk | Decision and parity impact |
| Notes | Lifecycle, capability, fallback, and evidence limits |

Inventory work begins at CSBot execution paths for player, weapon, vision,
GameState/objective, and engine/entity code, then follows transitive reads
that influence those paths. The minimum rows include player life/team/pose/
movement/FOV, active weapon/ammo/reload/timers/accuracy/zoom, C4 and scenario
state, trace/visibility, and hostage/VIP/rescue availability.

Existing raw access is classified as `acceptable boundary`, `legacy direct
access`, `needs migration`, or `not parity related`. P04 migrates only
parity-critical access that can be moved without introducing a large
abstraction or changing behavior; the audit itself records the rest.

## Tests and fixture

The test target will cover these behaviors with real Core types:

1. Exact public player state passes through the boundary unchanged.
2. Existing public weapon fields (ID/clip/reload where actually available)
   retain their value and metadata.
3. Missing values remain invalid and unavailable rather than becoming zero or
   false.
4. Delayed message/event observations retain freshness and delay metadata.
5. Two actors have isolated observations and actor generations.
6. Spawn/death/respawn and slot reuse cannot consume stale state.
7. P02 timing context can be attached without changing scheduler behavior.
8. Enhanced/optional collection cannot mutate the Compatibility observation.

At least one production-level fixture calls the Metamod adapter boundary with
fake public engine/edict state and verifies the resulting Core observation.
The fixture will use a safely reproducible public observation such as player
life/FOV/position or objective entity state; it will not fake private weapon
state and call that exact parity.

## Documentation deliverables

P04 updates:

- `docs/parity/STATUS.md`
- `docs/parity/PARITY_MATRIX.md`
- `docs/parity/SOURCE_MAP.md`
- `docs/parity/OBSERVATION_MATRIX.md`
- `docs/parity/KNOWN_DEVIATIONS.md`
- `docs/parity/TRACE_SCHEMA.md`
- new `docs/parity/OBSERVATION_MODEL.md`

The documents will report counts by the ten required classifications and will
keep `IMPLEMENTED_UNVERIFIED` separate from `MATCH`. FOV, accuracy, reload,
next attack timers, ammo, C4, and scenario state receive explicit high-risk
rows. The final result is `PARTIAL` when critical observations remain
unavailable or unverified; no P05 state-machine work is included.

## Validation gates

Implementation will use the existing Windows x86 HostX86/x86 NMake
environment and preserve the source/build/deployed artifact identity checks.
The P04 gates are configure, complete Debug build, focused observation tests,
full CTest, Phase 8 PowerShell checks, FocalSpan refresh, and staged diff
inspection. Python/PE and live HLDS/ReHLDS acceptance remain separate gates
and are reported as unverified when unavailable; offline tests do not promote
live acceptance.

The final report must separately state P02 timing regression, P03 RNG
regression, live acceptance, remaining dirty files, commit identity, and P05
readiness.

