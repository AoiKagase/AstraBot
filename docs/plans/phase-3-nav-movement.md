# Phase 3 plan — Nav movement

Status: planning revision 2026-09-05, audited base `b49f4da`. The P3-01
inspector slice is implemented; see [evidence](../reports/p3-01-nav-inspector.md).
Existing **P3-01 through P3-08** are retained;
unnumbered checklist items are commit slices, not new task IDs.

## Goal and authority

An operator-only `astrabot_goto <area-id>` selects the managed actor, resolves
its grounded current area, computes an owned selected-edge route and follows
portals through Local Navigation / primitives / GoldSrc commands to the goal.
Initially the server-console command targets the single managed Bot. Never take
control of a human implicitly; ambiguous or invalid actor selection fails.

Follow [the architecture contract](../architecture.md#phase-3-local-navigation-decision-2026-09-05)
and [real NAV protocol](../research/real-nav-compatibility.md).
**Finish is not declared here.** Linux x86 offline builds/tests run before
Finish; real-device/live checks remain post-Finish.
The old plan's live-only Phase 3 completion conflicts with AGENTS.md's Finish
gate: distinguish implementation/applicable offline verification complete from
live accepted. All project plans must complete implementation, applicable
verification and documentation before a separate project-wide Finish decision.
Pending post-Finish rows never become passes because offline work is complete.

## Planned modules

| Location | Responsibility |
|---|---|
| `tools/nav-inspect.cpp`, `tests/nav/inspection_tests.cpp` | read-only loader/index/graph/search inspection |
| `src/nav/runtime/route_session.*`, `movement_snapshot.hpp` | actor/map/route generation, goal/status/cancel, movement/query values |
| `src/nav/corridor/*` | selected-edge portals, constraints and cursor |
| `src/nav/local/*` | LocalNavigator, primitive tagged state, steering/recovery |
| `src/core/motor.*` | MovementIntent to bounded BotCommand |
| `src/adapter/cstrike/nav/*` | ground/clearance/door/ladder value observations |
| `src/debug/nav_command.*` | operator parsing/status/trace |
| `tests/nav/simulation/*`, `tests/live/nav/*` | offline replay and post-Finish scenario evidence |

Unimplemented modules remain proposed. Portable targets remain SDK-free. The
console adapter and its tests now link `astrabot_core` and `astrabot_nav`, without
reversing dependencies. Core and Nav share their MSVC STL ABI setting.

## Common commit and verification protocol

Each unchecked slice normally means one independently reviewable commit:
confirm checkout/status, FocalSpan status/update/query; add focused failing
tests; implement only that slice; run appropriate Windows Debug checks;
review diff, update FocalSpan, stage explicit paths, check cached diff and commit.
A multi-slice task is not complete after its first commit.

Portable baseline:
`rtk proxy powershell -NoProfile -File tools/verify-nav-evidence.ps1 -Mode Debug`.
Register new test executables with CTest. Adapter slices use pinned x86 NMake
Debug build/tests from AGENTS.md; changed adapter linkage/exports also require
Release/export verification. Preserve the six undecorated exports in AGENTS.md, including the approved engine bootstrap.
Unit/simulation passes never substitute for live observations.

## P3-01 — Compatibility prerequisite, contracts and observable goto session

- **Goal:** real-file inspection plus portable observable route sessions, before movement.
- **Why now:** synthetic correctness does not prove real compatibility; every
  controller needs actor identity, route status and cancellation.
- **Files/modules:** inspector/tests/CMake, runtime contracts, debug command,
  lifecycle/Nav linkage and compatibility report.
- **Interfaces:** existing NavMeshLoader/NavSpatialIndex/NavGraph/NavRouteSearch;
  planned RouteSession/MovementSnapshot/query-result/DecisionTrace; reuse
  PlayerId/MapGeneration/TickId and owned selected edges.
- **Implementation outline / commit slices:**
  - [x] Add bounded SDK-free read-only inspector with synthetic CLI/metadata tests,
    using the profile and report contract in the compatibility protocol.
  - [ ] Compare lawful local real bytes to independent expected data; record
    load/nearest/route and unchanged hashes. Without a file, keep this sub-gate pending.
    Dust/dust2 now pass these offline comparisons; full compatibility remains
    partial pending provenance/detail rows in [the follow-up report](../reports/p3-01-real-nav-compatibility.md).
  - [x] Add portable session/snapshot/query contracts and fake-host tests; default
    allowPartial=false and keep ExpansionLimit prefixes diagnostic-only.
    See [portable session evidence](../reports/p3-01-route-session.md).
  - [x] Wire console goto/status/cancel to the managed actor and route result,
    with explicit Nav linkage and offline adapter tests.
    See [console evidence](../reports/p3-01-nav-console.md); no motion is issued.
- **Tests:** metadata, missing/oversized/corrupt input, limits, no input writes;
  invalid actor/area, same area, Complete/Unreachable/ExpansionLimit with/without
  prefix, goal replacement, stale actor/map/query, disconnect.
- **Live validation:** post-Finish command prints current/goal, edges/cost/status;
  this task alone claims no motion.
- **Acceptance criteria:** inspector tests pass; real compatibility separately
  records its actual status; sessions cannot execute partial/unreachable routes;
  console behavior has offline adapter evidence.
- **Dependencies:** Phase 2 offline PASS and Phase 1 host identity/transport.
- **Risks:** fixture rights, wire assumptions, absent grounded queries, public ABI.
- **Deferred work:** movement, stable AMXX API, live ladder discovery, learning.

## P3-02 — Corridor, primitive lifecycle and look-ahead

- **Goal:** usable transition constraints instead of area-center chains.
- **Why now:** all motion states share targeting and selected-edge identity.
- **Files/modules:** corridor builder/cursor, local motion primitive contracts,
  simulation tests and CMake.
- **Interfaces:** RouteSession, corridor transition, MovementIntent, value-owned
  enter/update/complete/failed/abort lifecycle.
- **Implementation outline / commit slices:**
  - [x] Derive directed overlap portals, hull shrink, support heights, external
    entry/exit and deterministic constrained look-ahead.
    See [corridor evidence](../reports/p3-02-corridor.md); world probes remain required.
  - [x] Add primitive lifecycle/dispatch skeleton retaining selected link identity;
    unknown kinds and unsupported Drop fail explicitly.
    See [lifecycle evidence](../reports/p3-02-corridor.md); motion controllers and
    session/host integration belong to subsequent slices.
- **Tests:** straight/L/zigzag/sloped/unequal/reversed/degenerate edges, no overlap,
  narrow clearance, jitter and parallel edges; enter once, terminal once, abort.
- **Live validation:** post-Finish inspect portals/targets against geometry.
- **Acceptance criteria:** targets stay inside constraints; InvalidPortal is typed;
  no silent center fallback; exact replay and no SDK headers.
- **Dependencies:** P3-01 session slice; real-file sub-gate can remain independent.
- **Risks:** imperfect real overlaps and unsafe external endpoints.
- **Deferred work:** spline optimization, generic traversal plugin framework.

## P3-03 — Walk, grounded steering and GoldSrc motor bridge

- **Goal:** first end-to-end single-Bot Walk with ground/door/stair clearance.
- **Why now:** supplies the observation/movement seams for special traversals.
- **Files/modules:** local controller, motor, adapter ground/clearance/door queries,
  lifecycle integration and movement/simulation tests.
- **Interfaces:** timestamped hull/floor/door results, MovementIntent, fresh
  BotCommand through existing host submission.
- **Implementation outline / commit slices:**
  - [x] Add bounded grounded-area/clearance queries, stacked-floor distinction,
    unsafe-drop rejection and scripted replay values.
    See [ground/clearance evidence](../reports/p3-03-ground-clearance.md).
  - [x] Add Walk/motor, 25 Hz decisions plus per-frame commands, later-tick
    dispatch and stale-intent stop; implement observability now.
    Motor and intent-pump components are implemented and tested in
    [their report](../reports/p3-03-motor-pump.md). The portable Walk controller
    and scripted arrival simulation are implemented in
    [Walk evidence](../reports/p3-03-walk-controller.md). Host wiring and
    fake-engine arrival/dispatch evidence are in
    [the host integration report](../reports/p3-03-walk-host.md).
  - [ ] Add ordinary door use/wait, stairs, wall avoidance and narrow-passage
    speed/lateral correction.
- **Tests:** corridor/same-area supported arrival, ceiling, stairs, door failure,
  unsafe drop, zero/long delta, low/normal/high FPS, 120 ms stale intent,
  rejected command and repeated button edges.
- **Live validation:** post-Finish spawn/goto/floor/stairs/door/narrow passage;
  capture portal/command/arrival at multiple FPS.
- **Acceptance criteria:** simulation reaches supported goal; unknown clearance
  stops; no center shortcut; preserve one queued command and later-tick dispatch.
- **Dependencies:** P3-02 and Phase 1 transport; live also requires real NAV and Finish.
- **Risks:** command msec uses measured frame time, not local decision duration;
  avoid under-driving at 25 Hz or catch-up bursts.
- **Deferred work:** advanced edge balance, general jumps, per-frame A*.

## P3-04 — Reactive blockers, overlay and necessary multi-Bot seam

- **Goal:** bounded deterministic yield/avoid/replan without static mesh mutation.
- **Why now:** local blockers must not become permanent Nav facts.
- **Files/modules:** expiring overlay, local avoidance, fake-client/lifecycle/
  movement identity seams and adapter tests.
- **Interfaces:** blocker value ID/class, expiring portal facts, stable actor
  priority; adapter-only generation-validated PlayerId-to-entity resolution.
- **Implementation outline / commit slices:**
  - [ ] Add reactive clearance-based side/yield, stable ID tie-break, expiry and
    door/obstacle invalidation; no trajectory prediction.
  - [ ] Before two-AstraBot acceptance, replace active-primary assumptions with
    per-player entity/join/dispatch validation in a separate narrow commit.
- **Tests:** head-on/same-direction/immobile players, door reopen, expiry, replay,
  capability absent/present; two clients with different join states, slot reuse,
  removal/map change and no cross-actor command.
- **Live validation:** post-Finish two-Bot doorway; 8/16-Bot rows remain blocked
  until the adapter mapping change is verified.
- **Acceptance criteria:** finite wait/avoid/replan, stable priority, immutable
  mesh, two-client isolation; synthetic actors are not live multi-Bot proof.
- **Dependencies:** P3-03 and per-actor portable sessions.
- **Risks:** symmetry/starvation and single-client lifecycle assumptions.
- **Deferred work:** broad host rewrite, crowd AI, obstacle prediction, combat.

## P3-05 — Crouch and Simple Jump primitives

- **Goal:** execute verified transitions without per-frame state in A*.
- **Why now:** lifecycle and clearance/ground facts exist.
- **Files/modules:** local traversal interpretation/states, simulation/action trace.
- **Interfaces:** selected edge plus supported area hints/local constraints;
  takeoff/landing/speed/facing only as needed and typed terminal outcome.
- **Implementation outline / commit slices:**
  - [ ] Interpret supported crouch/jump/no-jump constraints; add crouch hold/cross/
    headroom-safe release and explicit unknown/contradictory failure.
  - [ ] Add Simple Jump approach/align/accelerate/takeoff/airborne/land/recover,
    single press edge, timeout and cooldown.
- **Tests:** crouch, low ceiling, NoJump conflict, blocked takeoff, missed/wrong
  landing support, airborne timeout, repeated tick, abort/release.
- **Live validation:** post-Finish pinned crouch/small jump with command edges and landing.
- **Acceptance criteria:** completion needs movement snapshot evidence; unsupported
  transitions fail deterministically; no link-to-jump-button shortcut.
- **Dependencies:** P3-03; P3-04 not required for isolated primitive tests.
- **Risks:** area hints do not supply reliable takeoff points.
- **Deferred work:** GapJump/LongJump/duck-jump search, air-control planning, learning.

## P3-06 — Ladder enrichment and first-class motion

- **Goal:** generation-bound host ladder links plus distinct up/down traversal.
- **Why now:** P2-07 proves synthetic connectivity only.
- **Files/modules:** adapter ladder_scanner, local ladder state, enrichment/tests.
- **Interfaces:** existing NavTraversalLink identity/fingerprint/direction/points;
  value bounds/facing/contact and freshness.
- **Implementation outline / commit slices:**
  - [ ] Independently enumerate func_ladder and trace endpoints/facing; publish
    immutable same-map enrichment or explicit discovery failure.
  - [ ] Add approach/align/contact/climb-up or down/exit/support, abort/fall
    detection and at most one fresh clearance-checked re-acquire.
- **Tests:** absent/invalid/unlinked/multiple ladders, fingerprint/generation,
  up/down, wrong contact, fall, timeout and bounded retry.
- **Live validation:** post-Finish reproducible scan/mount/climb/dismount both
  directions, then map-change invalidation.
- **Acceptance criteria:** selected link identity survives through outcomes;
  unsupported geometry is explicit; no Walk fallback or serialized ladder invention.
- **Dependencies:** P3-03, P3-02 lifecycle and P2-07 enrichment.
- **Risks:** top exits/facing need real trace evidence.
- **Deferred work:** BSP ladder extraction, learned ladders, boosts.

## P3-07 — Progress-aware stuck detection and finite recovery

- **Goal:** distinguish intentional waiting from failed commanded progress.
- **Why now:** primitives/blockers now supply expected progress and reasons.
- **Files/modules:** local history/recovery, trace/replan counters.
- **Interfaces:** dispatched-command feedback, expected primitive progress,
  projected progress, typed cause and carried attempt budget.
- **Implementation outline / commit slices:**
  - [ ] Add windowed displacement/progress/oscillation checks, state-specific
    pauses/timeouts and cause classification.
  - [ ] Add finite wait/sidestep/reverse-realign/replan/abort using architecture
    bounds; carry recovery attempts across replans.
- **Tests:** collision/oscillation, slow crouch, deliberate yield, rejected command,
  transient/permanent block, ladder fall, Unknown cause, target jitter,
  replan budget evasion and terminal-event uniqueness.
- **Live validation:** post-Finish forced stuck/transient block/ladder timeout;
  record detection time and finite recover-or-abort.
- **Acceptance criteria:** no random button spam; repeatable decisions/reasons;
  bounded termination across replans; reset on progress/new explicit goal only.
- **Dependencies:** P3-04, P3-05, P3-06.
- **Risks:** false positives; do not invent causes without observations.
- **Deferred work:** adaptive thresholds, success rates, Experience storage.

## P3-08 — Offline gate, portability and post-Finish acceptance

- **Goal:** reproducible scenario/budget evidence with separate gate statuses.
- **Why now:** implemented/offline-tested must not imply live accepted.
- **Files/modules:** replay, tests/live/nav manifests, Phase 3 report and ongoing
  Linux x86 build/portable-test CI below.
- **Interfaces:** environment/map/NAV hashes, actor count/start/goal/expected
  result, route/portal/primitive/command/reason trace.
- **Implementation outline / commit slices:**
  - [ ] Complete offline matrix and finite trace/replan/time budgets; document
    every pending live row and missing real-file prerequisite.
  - [ ] Consolidate ongoing Linux x86 CI evidence below.
  - [ ] After project-wide Finish, run live gates including Linux HLDS/ReHLDS, record exact results
    and reopen failed work explicitly.
- **Tests:** clean-map/map-change replay for every row, finite budgets, synthetic
  scheduling at 1/8/16 actors (not proof of live capability).
- **Live validation:** all rows below at low/normal/high FPS, 1/8/16 Bots after
  P3-04 mapping, with pinned environment.
- **Acceptance criteria:** offline status needs applicable tests plus documented
  pending rows; live accepted needs observed passes for every required live row.
  Unavailable scenarios remain unverified, never waived.
- **Dependencies:** P3-01–P3-07 applicable evidence; real NAV/runtime for live;
  every project plan and explicit Finish before live execution. Linux x86
  build/portable-test CI runs independently of Finish.
- **Risks:** Finish/live circular wording, tool availability, manual evidence gaps.
- **Deferred work:** combat/tactics/learning and automatic deployment.

## Linux decision and ordering

**Approved policy (2026-09-06):** Windows and Linux targets are 32-bit/x86.
SDK-free Linux x86 builds and portable/Nav CTest run on every GitHub Actions
push and pull request, before and after Finish. The Ubuntu GCC job installs
gcc-multilib/g++-multilib, sets `-m32`, Debug, tests ON, Metamod OFF and
warnings-as-errors ON, and uses CMake directly. Existing Windows jobs remain.
P3-08 consolidates actual CI evidence; adding a job is not a hosted pass.

Linux real-device/live HLDS/ReHLDS validation remains strictly post-Finish.
Offline CI success does not establish live acceptance. Linux Metamod SDK/ELF
integration is outside this portable job; historical reports retain their
original verification scope.

## Post-Finish acceptance matrix

| Scenario | Required evidence |
|---|---|
| Spawn/goto | managed identity/joined/current area/goal |
| Floor/doorway | valid portal, supported progress, use/wait, arrival |
| Stairs | ground continuity and elevated supported arrival |
| Narrow passage | clearance, speed/lateral adjustment, no oscillation |
| Crouch | hold/release/headroom and far-area arrival |
| Simple Jump | single takeoff, airborne, supported landing or bounded failure |
| Ladder | discovered link, up/down mount/climb/dismount, abort/re-acquire bound |
| Player/dynamic blocker | deterministic yield, expiry and finite replan |
| Stuck | dispatched movement, failed expected progress, finite recovery/abort |
| Partial/unreachable | diagnostic result, no false arrival or unsafe execution |
| Map change | old actor/map/link/route invalidated; no stale commands |

Record map/NAV/environment hashes, start/goal, ordered selected edges, portals/
target, traversal/primitive, commands, replan reasons, arrival/time/budgets.
Failures retain logs/replays. GapJump/LongJump/EdgeTraverse/Boost, advanced
wall-edge balance, demonstration learning, RL/LLM, trajectory imitation,
Tactical Planner/Combat AI, Experience DB and crowd/prediction are deferred.

## Recommended next session

The first P3-01 slice (bounded read-only NAV inspector and synthetic tests) is
complete. Dust/dust2 load/query/route comparisons pass after the zero-ID and
nullable-Approach correction. Compatibility is partially validated, with its
remaining rows explicitly recorded. The portable session slice is also implemented.
Console goto/status/cancel integration also has offline adapter evidence.
P3-02 corridor/portals and primitive lifecycle now have Windows/Linux x86 offline
evidence and are integrated into main. P3-03 bounded grounded-area/clearance
queries and Walk/motor host integration now have offline evidence; next is
the existing ordinary door/stairs/wall/narrow-passage slice.
Real compatibility remains
partial; no new numbering or Finish declaration.
