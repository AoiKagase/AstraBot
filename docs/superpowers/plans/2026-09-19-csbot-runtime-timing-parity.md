# P02 CSBot Runtime Timing Parity — Implementation Plan

## Objective

Implement only P02: deterministic Compatibility Mode scheduling, 30Hz command
cadence, nested 10Hz full-update cadence, reference command lifecycle order,
command persistence, and reference-compatible `msec`. Do not change RNG, NAV,
Combat, State Machine, objectives, profiles, or P03+ behavior.

Reference evidence is the read-only checkout
`H:/sourcecode/003.Game/amxmodx/ReGameDLL_CS` at
`b0889847fe6d03898be88acc9e366660efb40ab5`:

- `game_shared/bot/bot.h:33-37`: 30Hz and 10Hz intervals.
- `game_shared/bot/bot.cpp:30-57`: Spawn initializes deadlines, previous time,
  and command state.
- `game_shared/bot/bot.cpp:65-82`: nested due checks and ordering.
- `game_shared/bot/bot.cpp:257-299`: command execution and timestamp save.
- `game_shared/bot/bot.cpp:343-364`: command reset and `msec` conversion.
- `game_shared/bot/bot_manager.cpp:137-209`: one BotThink call per valid bot
  per server frame, with no catch-up loop.
- `dlls/bot/cs_bot_update.cpp:31-35,187-221`: Upkeep and Update boundaries.

## Files and responsibilities

Create:

- `include/astrabot/runtime/bot_timing_scheduler.hpp`: timing-only API.
- `src/core/runtime/bot_timing_scheduler.cpp`: absolute-deadline behavior.
- `tests/runtime_timing_tests.cpp`: deterministic scheduler and persistence
  pipeline tests.
- `docs/parity/TIMING_MODEL.md`: P02 reference and Astra timing model.

Modify:

- `CMakeLists.txt`: core source and `astrabot_runtime_timing` target.
- `src/adapter/metamod/plugin_runtime.hpp/.cpp`: per-slot scheduler,
  command-template persistence, scheduled materialization, and adapter
  integration.
- `tests/compat_actor_command_tests.cpp`: only boundary assertions needed to
  prove cadence, sequence refresh, neutral/frozen dispatch, and existing join
  behavior.
- `docs/parity/STATUS.md`, `PARITY_MATRIX.md`, `SOURCE_MAP.md`,
  `KNOWN_DEVIATIONS.md`, and `TRACE_SCHEMA.md`: P02 evidence updates.

Do not stage `.focalspan/`, `.focalspan.json`, `.serena`, DLLs, object files,
profiles, or the roadmap zip.

## Task 1 — RED: add the first scheduler test

Add the `astrabot_runtime_timing` executable beside the existing portable CTest
targets in `CMakeLists.txt`, and create `tests/runtime_timing_tests.cpp` with
the existing `check(bool, const char *)` executable style. The test must include
the not-yet-existing scheduler header and assert:

- `reset(10.0f)` followed by `advance(10.0f)` emits no events;
- `advance(10.0333334f)` emits exactly
  `Upkeep`, `CommandReset`, `FullUpdate`, `CommandExecute`;
- the next deadlines are `10.0333334f + 1.0f / 30.0f` and
  `10.0333334f + 1.0f / 10.0f`.

Use explicit literals and no test-side scheduler implementation. Configure and
build only this target. The expected RED result is a missing-header or missing
implementation compile failure. If it passes, fix the test before writing
production code.

## Task 2 — GREEN: implement the timing-only scheduler

Create `include/astrabot/runtime/bot_timing_scheduler.hpp` with these concrete
C++14 types and operations:

```cpp
enum class BotTimingEvent { Upkeep, CommandReset, FullUpdate, CommandExecute };

struct BotTimingStep {
  std::array<BotTimingEvent, 4> events;
  std::uint8_t eventCount;
};

class BotTimingScheduler {
public:
  static constexpr float kCommandInterval = 1.0f / 30.0f;
  static constexpr float kFullUpdateInterval = 1.0f / 10.0f;
  BotTimingScheduler();
  void reset(float spawnTime);
  BotTimingStep advance(float currentTime);
  std::uint8_t consumeCommandMsec(float currentTime);
  bool initialized() const;
  float nextCommandDeadline() const;
  float nextFullUpdateDeadline() const;
};
```

The class owns only intervals, deadlines, initialization, previous command
timestamp, and timing events. It must not receive RuntimeMode, edicts, buttons,
movement, weapons, controllers, or any tactical data.

Implement `src/core/runtime/bot_timing_scheduler.cpp` as follows:

1. The default object is uninitialized; `advance` returns zero events until
   `reset` is called.
2. `reset(spawnTime)` sets each deadline to `spawnTime + interval` and the
   previous command timestamp to `spawnTime`.
3. `advance` uses `currentTime >= nextCommandDeadline_`. If false, emit
   nothing. If true, first set `nextCommandDeadline_ = currentTime +
   kCommandInterval`, then emit `Upkeep`.
4. Only after Upkeep, test `currentTime >= nextFullUpdateDeadline_`. If true,
   first set `nextFullUpdateDeadline_ = currentTime +
   kFullUpdateInterval`, then emit `CommandReset` and `FullUpdate`.
5. Emit `CommandExecute` after that nested branch.
6. Never loop over missed intervals and never use `next += interval`.
7. `consumeCommandMsec` computes
   `static_cast<int>((currentTime - previousCommandTime_) * 1000.0f)`, clamps
   only values above 255, converts to `std::uint8_t`, then stores current time.
   Do not add a lower-bound clamp or half-up rounding.

Add the source to `astrabot_core`, build the focused target, and run it GREEN.
This is the first production implementation after the observed RED failure.

## Task 3 — Complete deterministic scheduler coverage

Add one failing assertion group at a time, run the focused target, then make
the smallest implementation change while keeping earlier groups green.

### Required timing cases

Use literal timestamp arrays and literal expected event sequences:

1. Nominal 30Hz/10Hz cadence and exact same-frame ordering.
2. 1ms/2ms high-FPS increments with no early or over-execution.
3. 50ms/100ms/200ms low-FPS increments with nested full-update counts.
4. Irregular timestamps `[0.010f, 0.027f, 0.067f, 0.072f, 0.162f,
   0.173f]` relative to a reset timestamp.
5. After `reset(0.0f)`, `advance(0.140f)` emits one each of Upkeep, reset,
   FullUpdate, and execute; deadlines become `0.140f + interval`; a later
   `advance(0.141f)` emits nothing.
6. First-think behavior: reset has no immediate event, first command and first
   full update occur only at their due timestamps.

The long-frame case must fail if implementation uses a catch-up loop, advances
deadlines by `+= interval`, or checks the full gate independently.

### Independent `msec` fixture

Call `consumeCommandMsec` on a scheduler reset at `10.0f` and hand-derive
assertions for a normal interval, a short interval, a value that truncates
rather than rounds, an elapsed value above 255, zero elapsed time, and the
first command from spawn time. Two successive calls must prove that the
previous timestamp advances. Expected values must not be calculated through a
production helper.

### Command persistence pipeline

Add a test-only fixture outside the scheduler:

```cpp
struct CommandTemplateFixture {
  int marker;
  bool valid;
  std::vector<int> executed;
};
```

Process real scheduler events by clearing `valid` on CommandReset, assigning a
new marker on FullUpdate, and appending the current marker on CommandExecute.
Drive one full update, two command-only executions, and the next full update.
Assert the first marker is executed twice and the next marker is used only
after reset/rebuild. Also assert that repeated template data is materialized
with a new execution sequence each time; `CommandQueue::isSequenceAccepted`
rejects the same sequence twice.

Run two scheduler instances over the same timestamps, label one Compatibility
and one Enhanced externally, and assert equal events and deadlines. Do not add
mode state to the scheduler. Keep frozen behavior for Task 5; the scheduler
itself must remain timing-only.

## Task 4 — Integrate one scheduler and template per managed bot

In `plugin_runtime.hpp`, add arrays aligned with `managedBotSlots_`:

```cpp
std::array<runtime::BotTimingScheduler,
           NativeBotObservation::kClientSlotCount> managedBotTiming_;
std::array<runtime::BotCommand,
           NativeBotObservation::kClientSlotCount>
  managedBotCommandTemplates_;
std::array<bool, NativeBotObservation::kClientSlotCount>
  managedBotCommandTemplateValid_;
```

Declare helpers for resetting one scheduler, clearing one template, preparing
the existing baseline command without dispatch, and executing a materialized
copy. Initialize/reset them in the constructor, map activation/deactivation,
movement reset, and managed-bot clear paths.

Start the scheduler exactly once when the managed actor crosses from joining to
the joined post-spawn runtime boundary, using `globals_->time`. Keep joining
heartbeats on the existing JoinController path. Do not reset timing every
frame.

Refactor `updateManagedBotMovement` into the following adapter flow without
changing NAV, Combat, ActionAdapter, or physics algorithms:

```text
advance(current_time)
  Upkeep: timing boundary only; no new decision
  CommandReset: clear persisted command template
  FullUpdate: existing command-generation path writes template only
  CommandExecute: clone template, refresh sequence/frame/msec, dispatch once
```

Dead, not-ready, unavailable, and no-intent branches must write a valid
neutral template instead of dispatching immediately. A pre-first-full-update
bot uses a neutral template. Existing diagnostics and controller resets remain
in their existing branches.

At execution, use this exact materialization shape:

```cpp
runtime::BotCommand command = managedBotCommandTemplates_[index];
command.sequence = ++managedBotCommandSequences_[index];
command.issueFrame = adapterFrameCount_;
command.movement.msec = managedBotTiming_[index].consumeCommandMsec(
  globals_->time);
inputDispatcher_.enqueue(command);
inputDispatcher_.dispatchNext(handle.actor, adapterFrameCount_);
```

Handle queue/stale results through existing diagnostics. Never reuse a
dispatched sequence number. Keep action client commands at FullUpdate time;
do not repeat them merely because a command template is executed again.

Replace scheduled joined Compatibility Mode use of
`movementMilliseconds(globalvars_t::frametime)` with the scheduler's previous
command-time delta. If the helper remains for join-only heartbeats, keep that
path separate and document it as outside CSBot command parity.

Frozen execution is adapter behavior: clear the persisted command fields,
submit zero `msec`, and advance the scheduler only once. Do not put freeze or
button logic in `BotTimingScheduler`.

## Task 5 — Add adapter-boundary regression tests

Write the failing assertions in `tests/compat_actor_command_tests.cpp` before
changing integration code. With explicit `globals.time` values and the
existing fake-client engine fixture, prove:

- joined frames below 33ms produce no scheduled RunPlayerMove;
- a command-due frame produces exactly one submission;
- a frame due for both cadences still submits once;
- two command-only frames reuse movement/button fields from the last full
  update;
- the next full update rebuilds after reset;
- observed command sequences increase on every execution;
- frozen execution submits zero msec and no prior buttons;
- joining heartbeat behavior remains unchanged and is labeled outside the
  scheduler.

If the fixture cannot expose a command marker without NAV/Combat changes, keep
marker persistence in the real scheduler pipeline test and limit this adapter
test to cadence, sequence refresh, neutral/frozen submission, and the existing
public boundary.

## Task 6 — Update parity documentation

Create `docs/parity/TIMING_MODEL.md` with reference commit/source evidence,
intervals, nested event order, Spawn/first-think behavior, `next = now +
interval`, no catch-up, long-frame rebase, template persistence, per-execution
sequence materialization, truncation/255 clamp, and joining/frozen boundaries.

Update only P02-relevant rows in the existing parity docs:

- `STATUS.md`: P02 offline result and remaining live/private-state gates.
- `PARITY_MATRIX.md`: Runtime cadence/command status with evidence.
- `SOURCE_MAP.md`: mappings for BotThink, ExecuteCommand, ResetCommand, and
  ThrottledMsec.
- `KNOWN_DEVIATIONS.md`: resolved cadence difference and remaining exact gaps.
- `TRACE_SCHEMA.md`: bounded timestamp, delta, due/executed, reset, sequence,
  and msec fields.

Do not alter the public/private observation matrix or unrelated phase rows.
Do not claim full CSBot, Combat, NAV, RNG, or live parity.

## Task 7 — Verification and P02-only commit

Run the focused tests first:

```powershell
ctest --test-dir build-action-adapter-x86-1451 -C Debug --output-on-failure -R "astrabot_runtime_timing|astrabot_compat_actor_command"
```

Then initialize the required matching toolchain, reconfigure the existing NMake
cache, and run the full target set:

```cmd
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x86
where cl
cmake -S . -B build-action-adapter-x86-1451 -G "NMake Makefiles" -DASTRABOT_BUILD_METAMOD=ON -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON -DASTRABOT_METAMOD_SDK_ROOT=H:/sourcecode/003.Game/amxmodx/metamod-p
cmake --build build-action-adapter-x86-1451 --config Debug
ctest --test-dir build-action-adapter-x86-1451 -C Debug --output-on-failure
```

Confirm `where cl` resolves to cached `Hostx86\\x86\\cl.exe`. Run all Phase 8
PowerShell checks and the PE/Python verifier separately. If `py -3` cannot
create its process, report that verifier environment-unverified rather than
substituting CTest evidence.

Run `focalspan update --root .`, `focalspan status --json`, and a final query
for scheduler deadlines, event order, template persistence, and msec
materialization. Confirm `index_fresh=true`, inspect the final diff, and run
`git diff --check`.

Stage only the explicit P02 implementation, test, CMake, and parity-doc paths.
Run `git diff --cached --check` and inspect the staged stat before committing:

```powershell
git commit -m "fix(parity): align compatibility think and command timing with CSBot"
```

## Review focus

Tests must catch these mutations:

1. `next = now + interval` changed to `next += interval`.
2. A long frame replayed through a catch-up loop.
3. CommandReset moved before Upkeep or executed on every command tick.
4. A persisted command dispatched with the same sequence twice.
5. `msec` rounded or derived from frametime instead of truncating the
   previous-command timestamp delta and clamping only above 255.

The P02 implementation is complete only when these mutations are covered,
all verification gates have fresh evidence, and the commit contains no
pre-existing dirty-worktree path.
