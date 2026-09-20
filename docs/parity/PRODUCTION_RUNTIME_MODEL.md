# P07.6 Production Runtime Model

Status: `PARTIAL` until a fresh HLDS/ReHLDS interval confirms the live gates.
This records production adapter wiring and the evidence boundary; it does not
start P08 Combat.

## Pipeline and ownership

```text
GameDLL/Engine public state
  -> StartFrame lifecycle + CVar sync
  -> FakeClient lifecycle/readiness
  -> P02 BotTimingScheduler (30 Hz command, 10 Hz full update)
  -> public ObservationAdapter / Compatibility belief
  -> Compatibility state update
  -> Goal producer (Compatibility Roam or observed public Objective)
  -> current NAV area / goal area
  -> typed path request/result and persistent corridor
  -> NavRoamDecision movement intent
  -> MovementExecutionGate
  -> command template / InputDispatcher
  -> public RunPlayerMove
```

`PluginRuntime::onStartFrame()` advances lifecycle and synchronizes controls.
`onStartFramePost()` processes join controllers and invokes managed movement.
`BotTimingScheduler` remains the owner of cadence and absolute deadlines; the
execution gate never stops Think, perception, state, or the 10 Hz full update.

## Freeze policy

The reference CGameRules source uses authoritative `IsFreezePeriod()` and
projects it through player `maxspeed=1` until `OnRoundFreezeEnd`. AstraBot
does not read private GameRules memory. The adapter uses public projections:

- `FL_FROZEN` is a hard control freeze: forward/side/up, buttons, and msec are
  zeroed before `RunPlayerMove`.
- finite public `edict_t::v.maxspeed` in `(0, 1]` is round freeze: movement and
  attack are removed, `IN_USE` remains explicit, and `IN_DUCK`/`IN_JUMP` are
  retained only when public `freezetime_duck`/`freezetime_jump` are positive.
- stale movement templates are invalidated while frozen. After thaw, a command
  deadline without a new full decision remains neutral; the next full update
  produces movement.

This keeps scheduler cadence and msec semantics intact. No spawn timer or local
freeze elapsed-time guess is used.

## Goal producer and NAV transaction

Before P08, the Compatibility Goal producer is bounded normal `Roam` when no
observed objective target exists. An observed public bomb/objective target is
an `Objective` Goal. Hidden enemy coordinates, enhanced profiles, tactical bomb
strategy, and adaptive route learning are not Goal inputs.

Each Full Update carries one actor/frame transaction. The decision records
`goalPresent`, `goalKind`, `goalArea`, `goalPosition`, `currentArea`,
`pathRequested`, `pathResult`, `pathSequence`, corridor length/cost, intent,
and movement dispatch outcome.

Failure reasons remain distinct: `NoGoal`, `GoalInvalid`, `CurrentAreaMissing`,
`GoalAreaMissing`, `PathSearchFailed`, `NavApplyRejected`, and
`MovementNotProduced`.

An active corridor is retained across temporary current-area loss. Map/round
identity changes, goal changes, path invalidation, or bounded stuck recovery
are the conditions that retire it.

## Profiling and cadence

Set `astrabot_profile 1` to enable one-second aggregate reports. Disabled
profiling has no timer/counter/logging behavior. Reports include each stage's
`calls/sec`, `total usec/sec`, average usec, and max usec, plus TraceLine/sec,
visibility candidates, body probes, path search success/failure/recompute
counters, and RunPlayerMove/sec.

The fixed stages are `StartFrame`, `RegistryLifecycle`, `Observation`,
`Vision`, `TraceLine`, `WorldPublish`, `Perception`, `RuntimeInput`,
`RuntimeFullUpdate`, `NavCurrentAreaLookup`, `PathSearch`, `PathRecompute`,
`NavMovement`, `MovementDispatch`, and `TraceSerialization`.

## Live A/B procedure

Pin the current x86 Debug/Release DLL SHA-256 and use fresh qconsole output.
Run separate intervals with the same map/config:

```text
A plugin off
B plugin on, Bot 0
C Bot 1, astrabot_profile 0
D Bot 1, astrabot_profile 1
E Bot 2, astrabot_profile 0
F Bot 4, astrabot_profile 0
```

Record server FPS, StartFrame usec, TraceLine/sec, pathSearch/sec,
pathRecompute/sec, and RunPlayerMove/sec. For one Bot, also record:

```text
spawn -> freeze no movement -> round start -> state update -> Goal
-> current area -> goal area -> path with at least 2 areas
-> movement intent -> command submitted -> RunPlayerMove
-> physical origin progress toward the Goal
```

Offline CTest, fixture checks, FocalSpan, and profiler reports do not prove
that live chain. `P08 NOT STARTED`.

## P07.6-PERF execution inventory

The production adapter has two StartFrame hooks. The pre hook performs the
lifecycle/CVar/native-control work; the post hook processes joins and invokes
managed movement. The per-actor runtime inventory is:

| Stage | Production location | Cadence |
|---|---|---|
| lifecycle/CVar/native guard | `PluginRuntime::onStartFrame` | per-frame |
| FakeClient join heartbeat | `processJoinControllers` / join dispatch | per-frame while joining |
| scheduler upkeep/command | `BotTimingScheduler::advance` | 30 Hz command gate |
| observation, candidate enumeration, Vision FOV/LOS/body probes | `buildManagedWorldSnapshot` / `ObservationAdapter::collectVisibility` | 10 Hz Full Update; cached on command ticks |
| WorldModel publish | `PerceptionAssembler::publish` | 10 Hz Full Update |
| Compatibility state and NAV decision | `updateManagedBotMovement` / `NavRoamController::update` | 10 Hz Full Update |
| current-area lookup and route selection | `NavRoamController` | on Full Update; route retained between updates |
| NAV movement intent | `NavRoamController` / locomotion | 10 Hz Full Update |
| public movement dispatch | `executeManagedBotCommand` / `InputDispatcher` | 30 Hz command gate |
| trace serialization | optional trace sinks | event-driven/disabled by default |

The pinned reference calls `CBot::BotThink` once per valid bot per server
frame, but gates `Upkeep` and the nested heavy `Update` at 30 Hz and 10 Hz.
Reference `CCSBot::IsVisible(CBasePlayer*)` preserves the chest/head/feet/
left/right probe order. AstraBot keeps that order and measures the actual
engine TraceLine calls rather than only the Perception consume path.

## P07.6-PERF static scaling findings

Before a live interval is available, the source-level scaling is explicit:

```text
each alive managed bot
  -> each connected alive non-spectator player
    -> five ordered body probes
      -> FOV check before LOS TraceLine
```

Therefore the current compatibility visibility path can scale as
`O(alive_bots * alive_players * body_probes)` per 10 Hz Full Update. It is not
run on every command tick: a command tick reuses the bot's published snapshot.
This is a measured candidate for live FPS impact, not a claim that Vision is
the root cause before the live counters are collected.

The confirmed redundant NAV work was different: the old objective sensor
enumerated every bomb-site NAV area and built a corridor for each site area on
every eligible call, even when the bot was not carrying C4 and could not use a
bomb-site goal. The production guard now performs that expensive branch only
for a Terrorist with a public carrying-C4 observation. Counter-Terrorists only
scan for a planted bomb; ordinary roam bots do neither. This preserves the
existing public-objective semantics and leaves normal Compatibility roam path
persistence unchanged.

## P07.6-PERF profiler and A/B controls

`astrabot_profile 1` enables one-second aggregate reports. The report includes
the fixed stage timing table plus alive-bot count, visibility candidates,
FOV checks, LOS checks, body probes, exact TraceLine calls, path request
success/failure/recompute counters, and RunPlayerMove count. No high-resolution
clock, counter update, or console output is performed by the stage scopes when
profiling is disabled.

The A* report additionally records `expanded`, `enqueues`, `reopens`,
`staleQueue`, `equalCostRepl`, `pathSearchUsec`, and `pathSearchMaxUsec` from
the actual bounded search. `PathSearch` timing is a leaf measurement; the
Full Update total remains an inclusive orchestration measure, while RuntimeInput
and NavMovement no longer wrap the whole StartFrame post hook.

An Objective ResourceLimit failure is cached by
`(mapGeneration, startArea, goalArea, routeType)` with bounded exponential
frame backoff. The cache is cleared on map/round/goal changes and successful
route selection. The A* search limits themselves are unchanged.

The default-off diagnostic CVars are `astrabot_perf_disable_vision`,
`astrabot_perf_disable_pathsearch`, and `astrabot_perf_disable_trace`. They are
temporary A/B controls only: vision bypasses the adapter scan, trace bypasses
engine LOS calls while retaining FOV/body accounting, and pathsearch
short-circuits route production with a typed `PathSearchFailed` diagnostic.
They are not compatibility behavior and are not a P08 dependency.

For live evidence, keep the same map/configuration and record each interval:

| Interval | Setup | Required output |
|---|---|---|
| A | plugin off | server FPS |
| B | plugin on, Bot 0 | FPS, StartFrame stage usec |
| C | 1 alive Bot | FPS, alive_bots, Vision/TraceLine/path counters |
| D | 2 alive Bots | same counters |
| E | 4 alive Bots | same counters |
| F | 4 alive, vision disabled | same counters |
| G | 4 alive, trace disabled | same counters |
| H | 4 alive, pathsearch disabled | same counters |

Run each interval for 30 seconds, save the one-second summaries, kill one Bot,
repeat the interval, and continue until all managed Bots are dead. No live FPS
or scaling value is filled in by offline tests; P07.6 remains `PARTIAL` until
fresh HLDS/ReHLDS output identifies the dominant stage.
