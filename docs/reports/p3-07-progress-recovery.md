# P3-07 — Dispatched progress and finite recovery

## Implemented contract

`Recovery` is an allocation-free, actor/goal-owned value. The host keeps it
outside `Walk`, binds replacement route generations, and feeds only matching
successful transport receipts with actual pre-dispatch position and rounded
command duration. Rejected transport/guards and deliberate neutral commands
do not accumulate stuck time. Unknown positions are never replaced by zero.

Ground Walk uses a 500 ms commanded-time window; crouch uses 1000 ms. The
detector measures displacement, signed progress toward the selected corridor
direction (before lateral steering), and path length versus net displacement.
The forward high-water point survives window resets; direction/target jitter
and repeated excursions cannot repeatedly refill the window. Fresh lateral
displacement can postpone detection during ordinary avoidance, but cannot
replenish replan attempts. Measured forward progress of four units can reset
the P3-07 budget. A new explicit goal resets it as well. Route replacement,
neutral waiting, and recovery-generated movement cannot do so.

Detected failed progress produces Wait, Sidestep, Reverse/realign, Replan,
then an abort if progress still fails on the replacement route. Each initial
recovery stage has a 250 ms deadline. Local decisions observe expiry at the
next scheduled tick (25 Hz target; one update per frame at lower FPS), while
dispatch guards reject a command whose actual end would exceed the stage
deadline. Thus no late queued command extends a recovery movement stage.

Side selection uses measured clearance, then stable actor-slot parity for an
exact tie. The selected side is retained for the attempt. Detours are at most
12 units at 40 units/s, with the existing 120 ms intent horizon, Motor and
transport guards. Fresh ground, swept hull, floor samples, current NAV area
and retained hull-safe extent are required. All issued queries, including
same-frame guards, share the existing 21-query limit. Missing support, stale
queries, insufficient budgets or unsafe extents produce neutral output until
the finite stage expires. A same-area route without a retained extent skips
detour movement and proceeds to bounded replan/abort.

Door/player waiting, posture transitions, Jump and Ladder retain their own
observations and finite timeouts. Generic recovery never emits jump/use spam
or replaces specialized airborne/ladder control. Typed terminal observations
map to DoorBlocked, PlayerBlocked, GeometryBlocked or TraversalFailed; missing
evidence remains Unknown. A detected stuck actor alone does not prove an edge
is blocked: its single automatic route query has no fabricated exclusion.
P3-04's separately established dynamic-blocker attempt contract is preserved.
Cancellation retires queued recovery commands and pending replans. A retired
Walk emits one terminal event; repeated observations cannot emit another.

## Offline evidence

- `tests/nav/recovery_tests.cpp`: stationary commanded movement, Walk/crouch
  windows, slow crouch progress, pauses/rejected commands, stale and wrong
  dispatch identities, small/large oscillation and target-direction reversal,
  useful lateral movement, finite stages, carried attempt budget, measured
  progress reset, terminal uniqueness and unknown-cause replan policy.
- `tests/nav/walk_tests.cpp`: safe deterministic side/reverse segments, actual
  query ordinals/count, missing floor, stale observations, airborne state,
  hull-extent failure, collision, exhausted query budget, typed causes and
  repeated terminal calls. Existing door/crouch/steering tests still apply.
- `tests/adapter/fake_client_tests.cpp`: independent command execution with
  frozen positions (permanent and released after 700 ms), actual dispatch
  stage durations, measured query counts <=21, one retry, final termination,
  new-goal reset and cancellation during Wait/Sidestep/pending Replan.
  Existing multi-actor, guard rejection, door/player, jump and ladder tests
  remain regression evidence; specialized ladder fall/timeout tests remain
  in `tests/nav/ladder_tests.cpp`.

The frozen host regression failed before implementation because Walk never
terminated. A reversing-direction regression also failed before retaining
the forward reference. Both now pass. These fixtures are synthetic; they do
not establish real CS/ReGameDLL NAV compatibility or live-server acceptance.

Measured from the start of each synthetic goal, in milliseconds:

| Frame | Detection | Permanent abort | Transient arrival | Side dispatch | Reverse dispatch |
| --- | --- | --- | --- | --- | --- |
| 8 ms | 568 | 1984 | 3424 | 240 | 240 |
| 16 ms | 576 | 2016 | 3424 | 224 | 224 |
| 100 ms | 700 | 2400 | 3800 | 100 | 100 |

Detection includes primitive entry, next-frame dispatch and local scheduling;
the configured detector window itself counts successful command duration.
Each host scenario produces one replacement route and a single final Walk
terminal event. Movement stops after termination.

## Verification and acceptance boundary

2026-09-06: Windows x86 Metamod Debug passed 44/44 CTest with warnings as
errors; Linux x86 Debug passed 38/38 under Debian WSL with GCC `-m32`.
Windows portable x86 Debug passed 36/36 (inspection tools disabled in that
fresh configuration). Windows x86 Release built with tests disabled; PE32/x86
and exactly six undecorated exports were verified: Meta_Query, Meta_Attach,
Meta_Detach, GetEntityAPI2, GetEngineFunctions and GiveFnptrsToDll.
The checked Metamod-P SDK SHA is
`7ec9b014f8c0a947a724644aebe34eb33706e44b`.

FocalSpan was ready/fresh before changes and provided the relevant context.
The code-review graph was consulted first but trails HEAD, so its results
were supplemented by the actual scoped diff/source and executable tests.

P3-08 and project-wide Finish remain separate work. Real Windows/Linux
HLDS/ReHLDS forced-stuck, transient obstruction, ladder timeout/fall and
map-change acceptance remain **Not yet validated**, to be run after explicit
project-wide Finish. Threshold tuning and real success rates are deferred.
