# P04 Compatibility Observation Model

## Scope

P04 records what AstraBot can observe through the public Metamod-P, HLSDK,
Engine, and GameDLL/message boundary. It does not claim that a public field is
behaviorally equal to a ReGameDLL private member, and it does not change
Combat, State Machine, NAV, objective decisions, P02 timing, or P03 RNG.

Reference: ReGameDLL-CS commit `b0889847fe6d03898be88acc9e366660efb40ab5`.

## Data flow

```text
public edict_t / Engine callbacks / public GameDLL messages
                         |
                         v
        metamod::ObservationAdapter
                         |
                         v
        compat::CompatibilityObservation
        typed value + quality + source + freshness
          |                  |                 |
          v                  v                 v
     WorldSnapshot      objective view     trace sink
```

`ObservationAdapter` is implemented in
`src/adapter/metamod/observation_adapter.{hpp,cpp}`. The Core contract is in
`include/astrabot/compat/observation.hpp` and
`src/core/compat/observation.cpp`. `PluginRuntime` configures the adapter from
the existing Engine pointers and routes managed world/player and bomb facts
through it.

## Quality semantics

| Quality | Meaning in P04 |
|---|---|
| `EXACT` | Value and semantics match the reference at the same decision point; no P04 row currently qualifies solely from a public field. |
| `EXACT_ENGINE_API` | Value is read from a public Engine/edict field at the collection tick; higher-level CSBot equivalence remains separate. |
| `EXACT_GAME_API` | Value is received through a public GameDLL API; no current P04 row uses private GameDLL objects. |
| `DELAYED` | Correct value arrives through TeamInfo/message/cache after the reference decision point. |
| `INFERRED` | A public proxy or entity heuristic is used; it is not the private scenario state. |
| `APPROXIMATED` | A bounded approximation is intentionally used. |
| `UNAVAILABLE` | The current public boundary cannot obtain the value. |
| `NOT_YET_IMPLEMENTED` | A portable/public source is possible or required, but P04 has not implemented it. |
| `NOT_REQUIRED` | The reference consumer was not found on the required CSBot decision path. |
| `UNKNOWN` | Reference or Astra meaning is not sufficiently resolved to classify safely. |

`EXACT_ENGINE_API` is an acquisition classification, not a `MATCH` claim.
`MATCH` additionally requires reference consumer, value semantics, timing,
and lifecycle equivalence.

## Freshness, source, and lifecycle

Every `ObservationValue<T>` carries `ObservationContext`:

- stable semantic ID;
- `ActorKey` including slot generation;
- `FrameIdentity` including map, round, and tick;
- quality, source, freshness, and delay ticks;
- command/upkeep/full-update sequence context.

`SameTick`, `CurrentFullUpdate`, `EventDrivenCached`, and `Stale` are distinct.
Zero upkeep/full-update sequence means that the existing scheduler does not
expose that event counter; it does not create a synthetic scheduler event.

An invalid value retains its context and quality but has `valid=false` and is
not available to consumers. This prevents unknown weapon accuracy, reload,
timer, or scenario state from becoming numeric zero or boolean false. Actor
generation and frame identity prevent slot reuse, respawn, map, or round state
from being silently reused.

## ObservationAdapter boundary

The adapter reads public `edict_t` fields for health, armor, team, deadflag,
origin, velocity, view angles, FOV, flags, ground/water state, max speed,
buttons, old buttons, solid, movetype, and bounds. It records public C4 bits
as `INFERRED`, not exact possession state. It recognizes planted C4 only from
public classname/model/dmgtime facts and records planted state, position, and
timer as `INFERRED`.

The adapter does not read `pvPrivateData`, pdata offsets, ReAPI, ReGameDLL
private classes, weapon pointers, or private timers. Active weapon ID, clip,
reserve ammo, reload, next attack, accuracy, weapon flags, silencer, burst,
and zoom fields therefore remain `UNAVAILABLE`. The existing synthetic
`WeaponRecord` used by `decideManagedBotAction` is intentionally not promoted
to an exact observation.

The current bomb-site bounds and path selection remain in
`PluginRuntime::buildManagedObjectiveTarget`; this is a documented legacy
direct public-entity access and remains inferred. Hostage, rescue, VIP,
defusing, kit, round, freeze, and win-condition state remain unavailable.

## Trace contract

`IObservationTraceSink` is optional and null by default. When installed, the
adapter emits bounded records containing sequence, semantic ID, actor/frame,
quality, source, freshness, delay, timing context, and typed value. Sequence
numbers are local to an adapter instance and do not share state with another
adapter, the P03 RNG source, or the scheduler.

Example:

```text
sequence=7 obs=OBS-PLAYER-FOV actor=4:9 value=90
quality=EXACT_ENGINE_API source=public_edict freshness=same_tick
frame=3:7:44 timing=18:0:0 delay_ticks=0
```

Trace collection has no file I/O, does not request random values, and does not
create timing events.

## Raw access audit

| Access family | Current classification | P04 decision |
|---|---|---|
| `ObservationAdapter` public `entity->v` reads | acceptable boundary | centralize player/C4/planted-bomb observations here |
| `PluginRuntime` direct public eligibility/readiness reads | legacy direct access | retain where it controls lifecycle/readiness; matrix documents parity impact |
| `PluginRuntime` bomb-site bounds/NAV overlap | legacy direct access | retain as inferred objective sensor; no decision rewrite |
| `PluginRuntime` classname/model lookup | acceptable public entity boundary | planted-C4 facts pass through adapter; site scan remains direct public scan |
| `FakeClientManager` entity initialization | not parity observation | preserve lifecycle behavior |
| `JoinController` TeamInfo/message cache | acceptable delayed boundary | use TeamInfo/requested-team fallback; raw team zero is not treated as active team |
| `pdata`, `pvPrivateData`, hard-coded offsets | no current P04 production path | do not add or promote to required dependency |
| `WorldSnapshot`/`WeaponInventory` Core models | consumer/domain boundary | preserve; provenance is held by CompatibilityObservation |

## Capability rule

P04 uses per-field validity and quality rather than a broad capability
framework. A field may be available in one host configuration and unavailable
in another without changing the meaning of an unavailable value. No default
is authoritative merely because it is representable.

## Acceptance boundary

Focused and full offline tests can establish model/adapter contracts only.
They do not prove ReGameDLL private-state parity, live HLDS/ReHLDS behavior,
or production combat/C4 acceptance. With the rows in
`OBSERVATION_MATRIX.md`, P04 remains `PARTIAL` and P05 State Machine parity
does not start.
