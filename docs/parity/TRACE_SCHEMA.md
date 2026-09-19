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
