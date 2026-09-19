# AstraBot P02 CSBot Runtime Timing Parity

## Status

Architectural design approved by the user on 2026-09-19. This design covers
P02 only: runtime timing, scheduler lifecycle, command cadence, command
persistence, and command `msec` calculation. It does not authorize P03 or any
changes to RNG, navigation, combat, state-machine, or objective decisions.

## Goal

Make AstraBot Compatibility Mode expose the same observable runtime cadence as
the pinned ReGameDLL-CS CSBot implementation while keeping the scheduler
independent from game decisions. The scheduler answers only **when** a timing
stage is due. The adapter and existing controllers continue to decide **what**
command data is produced.

## Reference evidence

The comparison target is the clean-room behavioral observation of
ReGameDLL-CS commit `b0889847fe6d03898be88acc9e366660efb40ab5` in the local
read-only checkout at
`H:/sourcecode/003.Game/amxmodx/ReGameDLL_CS`.

The relevant source facts are:

- `regamedll/game_shared/bot/bot.h` defines the command interval as
  `1.0 / 30.0` and the full-think interval as `1.0 / 10.0`.
- `regamedll/game_shared/bot/bot.cpp::Spawn` initializes both next deadlines
  to spawn time plus their interval, initializes the previous-command time to
  spawn time, and clears the command state.
- `regamedll/game_shared/bot/bot.cpp::BotThink` first checks the command
  deadline. Once due, it assigns `next = current_time + interval`, runs
  `Upkeep`, checks the full-think deadline, assigns its next deadline using
  `current_time + interval`, then runs `ResetCommand`, `Update`, and finally
  `ExecuteCommand`.
- The nested full-think check means a full update cannot run unless the 30Hz
  command gate is also due.
- `ExecuteCommand` computes `msec` from current time minus the previous command
  time, truncates the result to an integer, clamps only values above 255, saves
  the current time, and submits one `PLAYER_RUN_MOVE` command.
- `ResetCommand` clears movement and button state. It is not called after every
  command execution; therefore command state produced by a full update remains
  available to later 30Hz executions until the next full-update reset.
- `CBotManager::StartFrame` visits each valid bot once per server frame and
  calls `BotThink`; it does not perform a fixed-timestep catch-up loop.

The reference checkout is an observation input only. No ReGameDLL source or
private implementation is copied into AstraBot.

## Design

### 1. Timing source of truth

Add one `runtime::BotTimingScheduler` under `include/astrabot/runtime` and
`src/core/runtime`. It owns only:

- command and full-update intervals;
- next command and next full-update deadlines;
- initialization/reset state;
- previous command timestamp;
- due decisions and timing lifecycle events;
- reference-compatible command `msec` calculation.

It must not own or inspect:

- buttons, movement, view angles, weapons, or tactical data;
- Combat, Navigation, State Machine, or objective decisions;
- edicts, GameDLL state, engine callbacks, or mode-specific planners.

The public interface will accept explicit `float` timestamps so tests do not
wait on wall-clock time. The scheduler will use the reference's float timing
domain and absolute deadlines. The intended observable operations are:

```text
reset(spawn_time)
advance(current_time) -> ordered timing events
command_msec(current_time) -> uint8_t
next_command_deadline()
next_full_update_deadline()
```

`advance` emits no events when the command deadline is not due. When it is
due, it updates the command deadline to `current_time + command_interval`,
emits `Upkeep`, then evaluates the full-update deadline. If full update is due,
it updates that deadline to `current_time + full_interval` and emits
`CommandReset` followed by `FullUpdate`. It then emits `CommandExecute`.

This event list is a scheduling contract, not a decision API. The caller owns
the callbacks and command data associated with each event.

### 2. Long-frame and rate semantics

The implementation will use `>=` due checks and `next = now + interval`, not
`next += interval`. A delayed frame therefore produces at most one command
execution and at most one full update for that bot. Missed intervals are not
replayed, and the next deadline is rebased from the delayed frame's current
time. The new deterministic tests will assert both the event count and the
post-delay deadline values.

### 3. Command lifecycle integration

`PluginRuntime` will retain one scheduler state and one persisted command state
per managed bot. The existing movement/action generation remains in the
adapter boundary and is moved into the scheduler callbacks without changing
its decision algorithms:

```text
StartFrame / StartFramePost
  -> join/lifecycle handling already required for fake-client entry
  -> BotTimingScheduler::advance(current_time)
     -> Upkeep callback
     -> CommandReset callback: clear persisted movement/button command state
     -> FullUpdate callback: run the existing baseline command-generation path
     -> CommandExecute callback: submit the persisted command once
```

The scheduler starts for a bot when the managed fake client reaches the
post-join runtime boundary. Joining heartbeats remain part of the existing
join controller contract until that boundary and are not treated as CSBot
decision ticks. Dead/not-ready/frozen handling remains adapter lifecycle
behavior, but it is executed at the scheduled command boundary rather than
creating an additional per-frame command cadence.

The persisted command is the adapter's command data, not scheduler state. A
full update clears it before rebuilding it. A later command execution reuses
the rebuilt movement/buttons until the next full update. The execution path
updates only the per-execution `msec` and submits the command through the one
existing `InputDispatcher` / `pfnRunPlayerMove` path.

The refactor must preserve existing public action translation and physics
feedback contracts. It must not add a new NAV, Combat, or State Machine
algorithm.

### 4. `msec` contract

Scheduled command execution will use the scheduler's previous-command clock:

```text
integer_msec = int((current_time - previous_command_time) * 1000)
if integer_msec > 255: integer_msec = 255
previous_command_time = current_time
```

The conversion truncates toward zero, performs no half-up rounding, and has no
new lower-bound clamp. The spawn timestamp initializes the first command's
previous-command time. Frozen execution may submit zero `msec` as an adapter
condition, matching the reference's frozen command path, while still
advancing the previous-command timestamp once for that execution.

The current `globalvars_t::frametime`-based rounding helper will not be used
for scheduled Compatibility Mode commands. Any remaining use for non-CSBot
join/lifecycle heartbeats must be kept separate and documented as such.

### 5. Compatibility / Enhanced isolation

The scheduler has no `RuntimeMode` input and therefore cannot vary its timing
state by mode. Compatibility and Enhanced callers will receive identical event
sequences and deadlines for identical timestamp inputs. Enhanced-only work, if
present at the adapter boundary, runs after the baseline timing event is
selected and cannot mutate the scheduler's deadlines, previous-command clock,
or command cadence.

### 6. Diagnostics and trace fields

The scheduler test surface will expose ordered event records containing the
explicit timestamp and sequence information needed to diagnose timing:

- frame/current timestamp;
- command due and executed;
- upkeep executed;
- full-update due and executed;
- command reset;
- command execution sequence;
- command `msec`.

Production diagnostics will remain bounded and use the existing adapter
diagnostic path. The parity trace documentation will describe the fields
without enabling unbounded per-frame logging by default.

## Test design

Add a deterministic `astrabot_runtime_timing` CTest target covering the real
`BotTimingScheduler` implementation. Tests will use explicit timestamps and
will not call `sleep` or depend on wall-clock time.

The test cases are:

1. **Nominal cadence:** command, upkeep, full-update, reset, and execute event
   order at 30Hz/10Hz boundaries.
2. **High FPS:** short frame intervals do not cause over-execution.
3. **Low FPS:** command and full-update counts follow nested reference gates.
4. **Irregular frames:** explicit jittered timestamps preserve the reference
   event order and counts.
5. **Same-frame ordering:** a frame due for both cadences emits
   `Upkeep -> CommandReset -> FullUpdate -> CommandExecute`.
6. **First update:** spawn reset, first command deadline, first full-update
   deadline, and first command execution are independently asserted.
7. **Long frame:** one delayed execution is emitted, no catch-up loop occurs,
   and both deadlines rebase from the delayed timestamp.
8. **Mode isolation:** identical timestamp fixtures produce identical scheduler
   traces for Compatibility and Enhanced callers.
9. **Independent `msec` fixture:** normal, short, long, first-command,
   truncation, and 255-clamp cases are asserted without deriving expectations
   through production helpers.
10. **Command persistence:** a test pipeline records a command marker generated
    at Full Update, verifies it is reused by the next two CommandExecute
    events, and verifies the next CommandReset removes it before rebuilding.
11. **Frozen execution lifecycle:** adapter-level command reset/zero-msec
    behavior is covered without adding freeze logic to the scheduler.

Existing actor-command coverage will be extended only where needed to prove
that the scheduled persisted command reaches the existing engine submission
boundary. Existing tests will not be deleted or skipped.

## Documentation changes

P02 implementation will add:

- `docs/parity/TIMING_MODEL.md` describing the reference evidence and Astra
  scheduler contract;
- P02 status/evidence updates in `docs/parity/STATUS.md`;
- Runtime rows and evidence in `docs/parity/PARITY_MATRIX.md`;
- timing symbol mappings in `docs/parity/SOURCE_MAP.md`;
- resolved timing deviations in `docs/parity/KNOWN_DEVIATIONS.md`;
- timing event fields in `docs/parity/TRACE_SCHEMA.md`.

The documents will distinguish offline deterministic evidence from live
HLDS/ReHLDS acceptance. P02 success will not promote combat, NAV, RNG, state,
or full CSBot parity to `MATCH`.

## Verification and change hygiene

Before implementation, this design is self-reviewed for scope, type names,
ordering, long-frame semantics, and test coverage. After implementation:

1. run the smallest focused CTest target;
2. configure the existing x86 NMake build if required;
3. build the complete `build-action-adapter-x86-1451` Debug target in the
   matching VS HostX86/x86 environment;
4. run the full CTest suite;
5. run the Phase 8 PowerShell checks;
6. run the PE/Python verifier separately and report it as environment-
   unverified if `py -3` cannot create its process;
7. run `focalspan update --root .`, confirm `index_fresh=true`, and query the
   final timing contract;
8. inspect the final diff and run `git diff --cached --check`.

Only explicit P02 paths will be staged. Existing untracked build artifacts,
`.focalspan.json`, `.serena`, and other dirty-worktree files will remain
untouched.
