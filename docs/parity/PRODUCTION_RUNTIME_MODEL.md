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
