# P00 Differential Trace Contract

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
