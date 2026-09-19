# P02 Runtime Timing Model

## Comparison target

Reference: ReGameDLL-CS commit `b0889847fe6d03898be88acc9e366660efb40ab5`.
The local read-only checkout is
`H:/sourcecode/003.Game/amxmodx/ReGameDLL_CS`.

Relevant reference symbols:

- `game_shared/bot/bot.h`: `g_flBotCommandInterval = 1.0 / 30.0` and
  `g_flBotFullThinkInterval = 1.0 / 10.0`.
- `game_shared/bot/bot.cpp::Spawn`: initializes both deadlines to spawn time
  plus their interval, initializes previous-command time to spawn time, and
  clears command state.
- `game_shared/bot/bot.cpp::BotThink`: checks the command deadline, rebases it
  to `now + command_interval`, runs `Upkeep`, then checks the nested full
  deadline. A full due tick rebases that deadline to `now + full_interval`,
  runs `ResetCommand`, runs `Update`, and then runs `ExecuteCommand`.
- `game_shared/bot/bot.cpp::ExecuteCommand`: calculates `msec` from the
  previous command timestamp, truncates to integer milliseconds, clamps only
  above 255, saves the current time, and submits `PLAYER_RUN_MOVE`.
- `game_shared/bot/bot.cpp::ResetCommand`: clears movement and buttons; it is
  not called after every command execution.
- `game_shared/bot/bot_manager.cpp::StartFrame`: calls `BotThink` once per
  valid bot per server frame; it does not replay missed ticks.

ReGameDLL source is an observation oracle only. No private implementation is
copied into AstraBot.

## AstraBot Compatibility Mode contract

`runtime::BotTimingScheduler` is the single timing source of truth. It owns
only deadlines, cadence state, previous-command time, and ordered timing
events. It does not receive RuntimeMode, edicts, buttons, weapons, NAV,
Combat, State Machine, or tactical data.

For one bot and one observed server timestamp, the contract is:

```text
if command deadline is not due:
    no event
else:
    command deadline = now + 1/30
    Upkeep
    if full deadline is due:
        full deadline = now + 1/10
        CommandReset
        FullUpdate
    CommandExecute
```

The full gate is nested under the command gate. Deadlines use `>=` and
`next = now + interval`; AstraBot does not use `next += interval` and does not
run a catch-up loop. A delayed frame therefore produces at most one command
execution and one full update, then rebases both deadlines from that delayed
timestamp.

## Command state and adapter boundary

`PluginRuntime` holds one scheduler and one command template per managed bot.
Full Update clears and rebuilds the template. Command-only ticks materialize
the same movement/button/view state again until the next Full Update. The
adapter refreshes command sequence and issue frame for every materialization;
the queue therefore never receives the same sequence twice.

Joining heartbeats remain on the JoinController lifecycle path. The scheduler
starts once when the actor reaches the joined post-spawn runtime boundary.
The first scheduler frame only establishes the deadlines; the first command
is emitted after the 30Hz interval.

Frozen execution is an adapter boundary: it clears buttons and movement and
submits zero `msec`. Normal scheduled commands use the scheduler's previous
command clock. The current public command validity contract rejects zero
`msec`, so frozen zero-msec submission uses the engine `pfnRunPlayerMove`
boundary directly.

## `msec`

For each normal command execution:

```text
integer_msec = int((now - previous_command_time) * 1000)
if integer_msec > 255:
    integer_msec = 255
previous_command_time = now
```

The conversion truncates toward zero. There is no half-up rounding and no
artificial minimum. The spawn/post-spawn reset timestamp is used for the first
command.

## Offline evidence

`astrabot_runtime_timing` covers nominal, high-FPS, low-FPS, irregular,
first-think, long-frame/no-catch-up, event ordering, independent `msec`,
command-template persistence, per-execution sequence refresh, and external
Compatibility/Enhanced cadence isolation.

`astrabot_compat_actor_command` covers the public adapter boundary, first
post-join cadence, objective action preservation, dead/neutral execution,
frozen zero-msec/button clearing, and post-frozen sequence refresh.

These are deterministic offline gates. They do not claim live HLDS/ReHLDS,
full CSBot, Combat, NAV, RNG, private-state, or autonomous gameplay parity.

## P03 RNG timing association

P03 adds timing fields to the optional RNG trace context only. It does not
modify `BotTimingScheduler`, its 30Hz/10Hz cadence, absolute deadline rebasing,
no-catch-up behavior, command persistence, or timestamp-derived `msec` logic.
When a future Compatibility callsite is reached inside an existing scheduler
event, the caller may attach the already-observed command/upkeep/full-update
sequence values to the RNG record. A zero value means that no timing context
was available; it is not a new scheduler event.
# P07.6 timing boundary

P07.6 keeps the scheduler contract unchanged. Freeze is an execution gate after
decision production: full updates continue, stale movement is invalidated
rather than replayed after thaw, and `astrabot_profile` reports aggregate
stage/counter windows once per second while disabled by default.
