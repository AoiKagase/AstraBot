# Differential Trace Schema Target

P11 may refine the exact serialization format. Keep it line-oriented and deterministic enough for diff tools. JSON Lines is preferred.

Every record should contain a stable sequence number and a simulation/game timestamp. Avoid pointer addresses and process-specific IDs.

## Required record classes

### `think`

- bot stable id
- game time
- command tick index
- full-think index if applicable
- current task
- current primary state
- attacking overlay flag/state
- disposition/morale if behaviorally relevant
- current enemy stable id or none
- current NAV area id
- selected route/goal identifiers

### `observation`

Only fields that can influence compatibility decisions:

- self origin/velocity/view/FOV/posture
- health/armor/team
- active weapon/ammo/reload state
- bomb/objective state
- visible enemy set and relevant visibility masks
- heard/recent noise identity/location/type
- remembered last-known enemy data
- relevant teammate/hostage/objective facts

### `rng`

- sequence number
- compatibility call-site id
- requested range/type
- produced value

### `transition`

- old state
- new state
- task before/after
- reason id if Astra has one (do not require the reference to have the same internal reason object)
- important side-effect flags

### `command`

- forwardmove
- sidemove
- upmove
- buttons
- view angles
- command msec/timing
- selected weapon or command if applicable

### `path`

- source area
- target area/position
- route type
- ordered area ids
- selected ladder/jump transitions
- total/reference cost if stable enough to compare

## Comparison policy

- Exact compare enums, ids, buttons, state transitions and random sequence order.
- Floating-point epsilon is allowed only for engine/math representation noise. Document each epsilon field.
- Do not compare pointer values.
- Normalize entity identity to stable indices/serials.
- Keep traces compact. Trace only compatibility-relevant state, not every internal member.
