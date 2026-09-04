# Phase 3 plan — Nav movement

## Goal

On the pinned live server, an operator command such as
`astrabot_goto <area-id>` resolves the Bot's current area, computes an observable
area corridor, builds portal/traversal constraints, and uses local steering plus
the Phase 1 motor to reach the goal.  Acceptance covers floor, doorway, stairs,
narrow passage, crouch, jump, ladder, player blocking, dynamic obstacle, stuck
detection and recovery.  Area-center chaining is not an accepted controller.

## Planned modules

```text
src/nav/corridor/*             area edges to portals/traversal constraints
src/nav/local/*                short-horizon steering and movement state
src/nav/runtime/*              grounded queries and dynamic overlay
src/adapter/cstrike/nav/*      ladder/entity/door/trace adaptation
src/debug/nav_command.*        goto/status/route debug surface
tests/nav/simulation/*         deterministic fake-query scenarios
tests/live/nav/*               map scenario manifest and evidence
```

## Commit-sized tasks

### P3-01 — Observable goto request and runtime route session

- **Goal:** connect a debug goal to Phase 2 route output without movement policy.
- **Files/modules:** debug command parser, route-session state/result trace.
- **Implementation outline:** authorize/validate Bot and area ID, snapshot map/
  overlay/experience, locate current area, run A*, publish corridor/cost/reason;
  cancel on agent/map generation change.
- **Dependencies:** Phase 1 complete; Phase 2 immutable queries complete.
- **Tests:** invalid caller/Bot/area, no route, same area, goal replacement,
  disconnect/map change and deterministic trace.
- **Acceptance:** live command prints current/goal IDs, ordered edges, costs and
  failure reason; it does not yet claim movement.
- **Risk:** debug command becoming public AMXX ABI.  Keep it operator-only and
  explicitly unstable in Phase 3.

### P3-02 — Corridor portal extraction and look-ahead target

- **Goal:** replace center chains with geometric corridor constraints.
- **Files/modules:** shared-edge/portal builder, corridor cursor, simulation tests.
- **Implementation outline:** derive overlap portal for each directed floor edge,
  shrink by Bot clearance, retain traversal annotations, compute deterministic
  look-ahead/string-pull target constrained inside portals; center fallback is an
  explicit diagnostic failure mode only.
- **Dependencies:** P3-01.
- **Tests:** straight/L/zigzag corridors, unequal/sloped areas, narrow portal,
  reversed edge, degeneracy, stable target under small position changes.
- **Acceptance:** route visualization distinguishes areas, portals and steering
  target; fixtures prove target is not simply each area center.
- **Risk:** old meshes with imperfect overlaps.  Return typed `InvalidPortal` and
  replan/fail safely; do not silently walk through walls.

### P3-03 — Grounded floor, door, stairs and narrow-passage steering

- **Goal:** traverse the common movement matrix with adapter queries.
- **Files/modules:** local controller, `nearestGrounded`, clearance/floor probes,
  motor intent translation.
- **Implementation outline:** fixed-rate steering, desired velocity/view,
  look-ahead clearance, step/floor continuity, door use/wait state, speed/strafe
  control and corridor containment.  All traces return value results.
- **Dependencies:** P3-02 and Phase 1 command pump.
- **Tests:** fake-query floor/step/drop/door/narrow scenarios, tick replay, trace
  budget and stale query result.
- **Acceptance:** live Bot reaches selected areas across open floor, a doorway,
  stairs and a narrow passage without center-chain behavior or forbidden drops.
- **Risk:** map geometry and tick-rate sensitivity.  Record position/portal/
  command per tick and test at low/normal/high server FPS.

### P3-04 — Player blocking and dynamic obstacle overlay

- **Goal:** avoid/yield/replan without writing dynamic facts into static Nav.
- **Files/modules:** short-lived traversal overlay, neighbour observation,
  avoidance/yield policy.
- **Implementation outline:** classify teammate/enemy/other blocker, predict
  short-horizon occupancy, choose bounded side/yield behavior, expire facts, and
  invalidate/replan when a door/breakable/moving obstruction closes a portal.
- **Dependencies:** P3-03.
- **Tests:** head-on/same-direction/intersecting teammates, immobile blocker,
  semiclip capability, door closes/reopens, overlay expiry and deterministic
  priority tie-break.
- **Acceptance:** two live Bots do not deadlock in the chosen narrow scenario;
  a dynamic obstacle causes a reasoned wait/avoid/replan and recovery.
- **Risk:** oscillation between symmetric agents.  Stable priority uses IDs plus
  time-bounded ownership/yield, with starvation telemetry.

### P3-05 — Crouch and jump traversal states

- **Goal:** execute annotated transitions without contaminating A* with motor state.
- **Files/modules:** local traversal state machine, movement action flags.
- **Implementation outline:** approach/alignment, precondition/clearance check,
  press/hold/release crouch or jump, success/failure/timeout, cooldown and replan.
- **Dependencies:** P3-03; can proceed in parallel with P3-04 after P3-03.
- **Tests:** crouch-only portal, jump/duck-jump, low ceiling, missed jump, timeout,
  repeated button edge semantics.
- **Acceptance:** pinned live scenarios reach the far area and trace transition,
  command flags, completion or typed recovery reason.
- **Risk:** `.nav` attributes describe areas rather than an exact takeoff point.
  Local probes and scenario-specific acceptance are required.

### P3-06 — Live ladder enrichment and ladder state machine

- **Goal:** discover map ladders outside `.nav` and traverse one in each direction.
- **Files/modules:** `adapter/cstrike/nav/ladder_scanner`, immutable enrichment,
  local ladder controller.
- **Implementation outline:** enumerate/classify `func_ladder` on map activation,
  derive bounds/facing through adapter traces, link validated nearby areas,
  publish map-generation enrichment; implement approach, mount, align, ascend/
  descend, dismount, timeout.  Never expose entity pointers.
- **Dependencies:** Phase 2 enrichment contract; P3-03.
- **Tests:** synthetic adapter entities/traces, multiple/invalid/unlinked ladders,
  map-generation cleanup, up/down route and state replay.
- **Acceptance:** live scan reports reproducible ladder/link facts; the Bot reaches
  both endpoints up and down; map change invalidates old enrichment.
- **Risk:** ladder facing/top exits require geometry traces and differ by map.
  Keep failed links observable and do not invent a serialized ladder record.

### P3-07 — Progress/oscillation stuck detection and recovery ladder

- **Goal:** detect lack of corridor progress and recover finitely.
- **Files/modules:** progress history, recovery policy, replan reasons/counters.
- **Implementation outline:** sample projected corridor progress, displacement,
  velocity-goal alignment and oscillation; escalate bounded wait → side step →
  backoff/realign → overlay invalidation/replan → abort.  Reset only on measured
  progress or route generation change.
- **Dependencies:** P3-04–P3-06.
- **Tests:** stationary collision, oscillating doorway, teammate clears, permanent
  block, ladder timeout, false-positive slow crouch, maximum recovery attempts.
- **Acceptance:** live forced-stuck case is detected within documented bound,
  recovers or aborts without infinite input, and emits exact replan reason.
- **Risk:** false positives and command thrashing.  State-specific thresholds are
  configuration values captured in the decision trace.

### P3-08 — Live scenario matrix, budgets and gate report

- **Goal:** prove end-to-end correctness and measure likely hot paths.
- **Files/modules:** scenario manifest, operator commands, trace assertions,
  benchmark/report documentation.
- **Implementation outline:** choose redistributable/local map locations with
  coordinates/area IDs and hashes; automate setup where safe; collect route,
  portal, steering, trace counts/time, A* expansions, replan/recovery results at
  multiple server FPS and 1/8/16 Bot scheduling load.
- **Dependencies:** P3-07.
- **Tests:** repeat every scenario from clean map start and after map change;
  deterministic simulation replay for each failure found live.
- **Acceptance:** all required matrix rows below have pass evidence; no unbounded
  trace/replan spike; failures retain logs and do not get relabeled as passes.
- **Risk:** map/server acceptance is hard to automate.  Keep manual observations
  separate from unit/simulation results and pin the entire environment.

## Acceptance matrix

| Scenario | Required evidence |
|---|---|
| Floor | stable corridor progress and goal-area arrival |
| Doorway | valid portal, no wall clipping, open/use/wait state as applicable |
| Stairs | grounded continuity and arrival at elevation |
| Narrow passage | clearance-respecting steering, no center oscillation |
| Crouch | correct hold/release and far-area arrival |
| Jump | aligned action, landing/progress or bounded typed failure |
| Ladder | live-discovered enrichment; mount/travel/dismount up and down |
| Player blocking | deterministic yield/avoid; no two-Bot deadlock |
| Dynamic obstacle | overlay invalidation and wait/avoid/replan reason |
| Stuck recovery | bounded detection/escalation and recover-or-abort result |

For every row, record map/nav hash, start/goal areas, corridor/portals, command
ticks, route/component cost, replans/reasons, final area and elapsed server time.
Phase 3 passes only when these are live results; offline simulation is necessary
regression coverage but not a substitute.
