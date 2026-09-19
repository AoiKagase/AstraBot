# P00 Differential Trace Contract

## P04 observation trace extension

Observation records are bounded and optional. They are independent from the
P03 RNG trace sequence and do not create scheduler events.

| Field | Required meaning |
|---|---|
| `sequence` | Monotonic sequence local to one ObservationAdapter instance |
| `obs` | Stable semantic observation ID, for example `OBS-PLAYER-FOV` |
| `actor` | `slot:generation` actor identity |
| `value` | Typed scalar/vector observation value |
| `quality` | One of the ten P04 quality classifications |
| `source` | Public edict/Engine/GameDLL/cache/fixture/synthetic/none |
| `freshness` | Same tick, current full update, event-driven cached, or stale |
| `frame` | `map_generation:round_generation:tick` |
| `timing` | `command:upkeep:full_update`; zero means unavailable context |
| `delay_ticks` | Explicit message/cache delay |

The production sink is null by default. Trace collection must not read pdata,
write unbounded files, request RNG values, or alter `BotTimingScheduler`.

The existing root `docs/TRACE_SCHEMA.md` is retained as the general contract.
This parity copy records the P00 minimum needed to compare the pinned reference
and AstraBot without pointer or process-specific identifiers.

Use deterministic JSON Lines. Every record carries `sequence`, `mapGeneration`,
`roundGeneration`, `tick`, and stable actor slot/generation IDs.

| Record | Required fields |
|---|---|
| `think` | command tick, full-think tick, task/state, attack-overlay state, enemy ID, NAV area, route ID |
| `observation` | self origin/velocity/view/FOV/posture, life/health/team, weapon/ammo/reload, objective, visible enemies/body parts, noise and remembered contacts |
| `rng` | call sequence, compatibility call-site ID, range/type, produced value |
| `transition` | old/new state, task before/after, reason, side-effect flags |
| `command` | forward/side/up move, buttons, view angles, msec, weapon/command action |
| `path` | source/target area, route type, ordered areas, ladder/jump transitions, stable cost |

Comparison rules:

- Compare enums, IDs, button flags, state transitions, RNG sequence, and command
  order exactly.
- Use an explicitly recorded epsilon only for engine floating-point noise.
- Never compare pointers; normalize entity identity to slot and generation.
- A synthetic fixture, CTest result, CRG result, or FocalSpan result is not a
  reference trace and cannot close live parity.

P00 result: the schema and negative-input differential checks exist, but no
pinned CSBot reference trace or production RNG tape is present.

## P02 timing fields

P02 adds a bounded runtime timing record for deterministic offline diagnostics.
Each timing record may contain:

- `timestamp`: explicit current game time;
- `frame_delta`: observed time since the preceding frame;
- `command_due`, `command_executed`: 30Hz gate and execution flags;
- `upkeep_executed`: frequent maintenance event flag;
- `full_update_due`, `full_update_executed`: nested 10Hz gate and update flags;
- `command_reset`: reset event flag;
- `command_sequence`: per-execution materialized command sequence;
- `command_msec`: timestamp-derived integer command duration.

The scheduler emits at most one ordered event sequence per bot per observed
frame. Diagnostics must remain bounded and must not enable unbounded per-frame
logging by default. These fields prove offline cadence and ordering only; they
are not a live reference trace or evidence of RNG, private-state, Combat, NAV,
or full CSBot parity.

## P03 RNG record

An optional `rng` record is emitted by the Compatibility RNG source only when
parity tracing is enabled. It is bounded and does not enable always-on verbose
logging.

| Field | Required meaning |
|---|---|
| `sequence` | Monotonic sequence from the shared Compatibility source |
| `site` | Semantic callsite ID, never a line-number-only ID |
| `type` | `float` or `long` |
| `min`, `max` | Exact type-preserving arguments forwarded to the source |
| `result` | Value returned by the engine adapter or scripted tape |
| `bot` / `entity` | Stable actor slot/generation, or explicit unknown for manager calls |
| `command_sequence` | Existing command timing context, or zero when unavailable |
| `upkeep_sequence` | Existing upkeep timing context, or zero when unavailable |
| `full_update_sequence` | Existing full-update timing context, or zero when unavailable |

The sequence counter belongs to the shared Compatibility source. Enhanced-source
calls are not emitted on this Compatibility stream. Synthetic traces, CTest,
FocalSpan, and graph output remain offline evidence, not pinned reference
production traces or live HLDS/ReHLDS acceptance.
