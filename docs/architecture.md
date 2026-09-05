# AstraBot validated architecture

Status: Phase 0 decision record, with Phase 3 planning addendum 2026-09-05. This document supersedes design
v0.2 where they conflict; the two original inputs remain unchanged as historical
hypotheses.  Evidence is pinned in [the source manifest](research/source-manifest.md).

## Executive decision

Adopt **Architecture B: new AstraBot + upstream reference**.

AstraBot is an external Metamod-P Bot with an independently authored Core and
navigation implementation.  The first runtime target is ReHLDS/HLDS +
Metamod-P + ReGameDLL_CS, but Phase 1 uses the standard Metamod/HLSDK host path
rather than making ReAPI a Core or lifecycle dependency.  GoldSrc entities and
GameDLL private data remain owned by the engine/GameDLL; AstraBot associates an
external `BotAgent` with a generation-safe value ID.

```text
Metamod-P / GoldSrc / GameDLL
          |
  GoldSrc Host Adapter  -- optional ReGame capabilities
          |
     IGameHost boundary (values, snapshots, events, commands)
          |
  +-------+--------+----------------+
  |       |        |                |
World  Perception  Independent Nav  Experience
Model              + Route queries  snapshots
  |                    |                |
  +------------ Planner / Team --------+
                       |
              DecisionResult + BotCommand
```

Phase 0 introduced no production source.  P1-01 introduces only the portable
Core/host value contracts; names below remain deliberately small design
contracts for later phases.

## A / B / C comparison

| Axis | A. YaPB fork | B. New AstraBot + YaPB reference | C. ReGameDLL bridge/built-in host |
|---|---|---|---|
| Development effort | Lowest to first FakeClient, then high because waypoint assumptions must be removed throughout | Highest initial host skeleton, bounded by small phase gates | Low for built-in zBot lifecycle, high to expose a stable external Core bridge |
| Architectural cleanliness | Low: graph/task/lifecycle share Bot mutable state | **High:** boundaries are designed around snapshots and commands | Low-medium: `CCSBot`/GameRules/global nav remain authoritative |
| Maintainability | Fork divergence plus large upstream merges | **Owned contracts; references can advance independently** | ReGameDLL ABI/internal changes become AstraBot changes |
| Testability | Host and graph-heavy tests | **Core/Nav offline by construction** | GameDLL process required for much behavior |
| NavMesh suitability | Fatal rewrite: current creation rejects a missing waypoint graph | **Native NavMesh representation** | Strong built-in mesh but tied to GameDLL globals and code/license provenance |
| World Model suitability | Existing Bot state leaks engine/current truth | **Immutable imperfect-information model** | Private player/GameRules types encourage complete-information coupling |
| Learning suitability | Practice is waypoint-index oriented | **Event-derived Experience separate from Nav** | Static nav/analysis and Bot state are coupled; external schema still needed |
| Metamod portability | Good | **Good; primary adapter** | Weak: architecture is ReGameDLL-specific |
| Vanilla GameDLL future support | Possible but fork retains other coupling | **Possible via second adapter; live proof deferred** | Poor; built-in classes do not exist in Vanilla |
| AMXX integration | Existing ecosystem patterns, not a clean stable Core API | **Optional versioned consumer can be designed narrowly** | ReAPI/AMXX hooks are rich but become hard dependencies easily |
| Binary/API coupling | YaPB ABI/source fork | **Value boundary; ReAPI optional** | Highest; GameDLL concrete classes, hook ABI and private lifecycle |
| Upstream maintenance | Must continuously merge YaPB | Reference snapshots are updated deliberately | Must track ReGameDLL internals |
| License risk | YaPB itself is MIT, but Metamod/SDK distribution remains | **Copied-code risk lowest; Metamod/SDK combined-work gate remains** | ReGame nav headers/root/Valve provenance are mixed |
| Long-term project risk | Waypoint replacement consumes the project | **More early work, lowest architectural lock-in** | Fast prototype can become an irreversible ReGameDLL-only product |

Fatal constraint A: current YaPB refuses Bot creation when its graph is absent;
waypoint state is not a replaceable leaf（[evidence](research/upstream-comparison.md#yapbから得るnavigation以外の境界)）。
Fatal constraint C: ReGameDLL creates a `CCSBot` and invokes private GameDLL menu
handlers（[evidence](research/goldsrc-host.md#regamedllとvanillaの分離)）。

B retains two residual risks: correctness of the independently written host
lifecycle and license terms of the eventual Metamod-P combined binary.  Both are
explicit gates; neither contaminates the Core data model.

## Component and dependency boundaries

| Component | Owns | Must not own/reference |
|---|---|---|
| Core time/identity | `EntityId`, generation-safe `PlayerId`, `BotAgentId`, `TickId`, duration, seeded RNG streams | engine indices as durable identity, wall-clock reads, pointers |
| Observation / Perception | timestamped visible/audible/message observations and confidence | undisclosed enemy engine state, raw messages after adaptation |
| World Model | immutable frame snapshot and imperfect-information beliefs | `edict_t`, GameDLL globals, unobserved truth |
| Static Nav | immutable `.nav` area/connection/hiding/approach/encounter/Place data | ladders inferred from file, dynamic blockers, learned danger |
| Traversal enrichment | immutable per-map ladder/host/BSP traversal links | frame-lived obstacle state, mutable Experience |
| Local navigation | corridor portals/look-ahead, steering, traversal state, stuck state | strategic goal selection or engine command calls |
| Experience | immutable aggregates produced from versioned observation events | mutation inside `NavArea`, anonymous human/bot samples |
| Planner/team/combat | decisions over snapshots with reason/cost output | direct traces, message parsing, every-frame monolithic `Think()` |
| GoldSrc adapter | all SDK types, entity validation, hook/message parsing, trace calls, command submission | planning policy or persistent learned weights |
| ReGame optional capability | ReAPI/ReGame-only events converted to common values | public ReAPI objects across the boundary |
| AMXX bridge | versioned native/query/event translation | Core ownership, internal pointers, mutable nav access |

Future source direction is `src/core`, `world`, `perception`, `nav`,
`experience`, `ai`, `combat`, with all `edict_t`, `entvars_t`, `CBaseEntity`,
`CBasePlayer`, `CCSPlayer`, ReAPI and Metamod concrete objects confined to
`src/adapter`.  This is a one-way compile dependency: adapters depend on Core
contracts, Core never depends on adapters.

## Host boundary contract

An `IGameHost`-equivalent port provides:

- map/player/round lifecycle events with ordered sequence and generation;
- immutable observations and snapshots built at a named simulation tick;
- trace, point-content, and entity-class queries returning AstraBot value types;
- monotonic time and explicit `TickId`, never hidden global time;
- `BotCommand` submission with `(PlayerId, TickId)` stale/duplicate rejection;
- structured trace output for every lifecycle transition and failed host call.

The exact FakeClient state machine is fixed in
[goldsrc-host.md](research/goldsrc-host.md#正確なlifecycle).  Phase 1 targets the
standard Metamod-P path; ReGameDLL is the first tested GameDLL, not a type system
dependency.  Vanilla support is feasible through the same public path but
remains “likely, needs live proof,” not a completed feature.

P1-01 fixes the portable surface to generation-safe IDs, explicit simulation
ticks, bounded `BotCommand` values, lifecycle results/errors, simulation time,
and command submission.  These headers use only fixed-width C++17 values.  SDK
headers and concrete engine/GameDLL objects begin in a later adapter target and
cannot be included by `src/core` or `src/host`.

## Navigation contract

Use an independently authored, explicit little-endian `.nav` v1–v5 reader.
[nav-extraction.md](research/nav-extraction.md) defines the bytes, validation and
license reason.  The data model has four inputs:

1. `SerializedNavSnapshot`: immutable file data;
2. `TraversalEnrichment`: immutable map-generation data such as ladders;
3. `DynamicTraversalOverlay`: frame-lived door/blocker/obstacle facts;
4. `ExperienceSnapshot`: learned costs keyed by map fingerprint and area ID.

Pure nearest-area means geometry containment/distance with stable ID tie-breaks.
Grounded/visible nearest-area is a separate query using a host trace port.  A*
uses query-local open/closed/parent/cost records and returns an immutable result:
corridor area/edge sequence, total cost, per-component cost, traversal kind,
failure/partial-route reason and expansion metrics.

Version-5 files do not store ladders.  Phase 2 therefore validates synthetic
ladder enrichment offline; Phase 3 is the first live ladder-discovery/traversal
gate.  Area-center chains are debug output only, not final movement paths.

## Corridor and local steering boundary

The [Phase 3 contract below](#phase-3-local-navigation-decision-2026-09-05)
refines this section's implementation order and scheduling. Existing Phase 3
task IDs remain authoritative.

The global planner chooses a sequence of traversable area edges.  A corridor
builder derives shared-edge/portal constraints and traversal annotations.  Local
navigation chooses short-horizon targets inside that corridor, performs hull/
floor/door/blocker queries, yields around players, and emits desired velocity,
view and action flags.  The motor clamps/translates that result to one
`BotCommand` per tick.

Initial local features, in implementation order:

1. floor/step traversal and portal look-ahead through doors/narrow passages;
2. swept clearance, wall avoidance and dynamic obstacle invalidation;
3. teammate/player yield plus congestion timeout;
4. progress/oscillation-based stuck detection and bounded recovery/replan;
5. crouch/jump annotated transitions;
6. ladder mount, travel and dismount state machine.

Every replan records `BlockedPortal`, `DynamicObstacle`, `Stuck`,
`TraversalFailed`, `GoalChanged`, or another versioned reason.

## Experience and human/bot observations

Static Nav answers “where traversal is possible”; Experience answers “what
happened there.”  Experience is built only from immutable events carrying
`ActorKind` (`Human`, `AstraBot`, `OtherBot`, `Unknown`) at observation time.
Raw counts remain separated; weights are configuration/query parameters, not
destructive rewrites.  Details and the RealBot correction are in
[realbot-learning.md](research/realbot-learning.md).

Map identity is `(map name, BSP size, content hash when available, nav format
version, nav content hash)`.  A mismatch quarantines prior experience.  Writes
use one persistence owner and round/map-boundary transactions; planners consume
an immutable snapshot so persistence cannot change a decision mid-tick.

## Persistence decision for Phase 9

| Axis | SQLite | Versioned binary | Structured text | Hybrid |
|---|---|---|---|---|
| GoldSrc deployment | One extra compiled C library; no service | Smallest runtime | No library, parser required | Both dependencies |
| Windows/Linux | Mature and portable | Must define widths/endian/locking | Portable if encoding/newlines fixed | More matrix cases |
| Static linking / size | Official amalgamation supports static build; binary grows | Best size | Medium parser/code size | Largest |
| Dependency management | Pin amalgamation version/hash | Fully owned | Fully owned or parser dependency | Most complex |
| Crash safety | Transactions, journal/WAL; one writer policy | Must implement atomic replace/log | Atomic replace required; partial writes risky | SQLite can protect index, blobs still need protocol |
| Schema migration | Explicit/user_version and SQL migrations | Custom migration per version | Readable but custom migrations | Two coordinated schemas |
| Debugging | `sqlite3` tooling and queries | Requires decoder | Best direct readability | Mixed |
| Performance | Sufficient for round-batched ≤16 Bot aggregates; benchmark later | Fast compact reads | Parse/serialization overhead | Potentially good for large blobs, premature here |
| Backup | Online/transactional options; single map DB copy with rules | Copy after atomic close | Easy file copy when closed | Multiple-file consistency problem |
| Portability/future analytics | Strong queryability | Format lock-in | Easy ad-hoc, weak constraints | Highest design burden |

**Recommendation: SQLite**, but only behind an abstract persistence port until
Phase 9.  Use a pinned official SQLite amalgamation under its public-domain
dedication, not AMX Mod X's GPL wrapper.  Start with one writer, prepared
statements, bounded batches at round/map boundaries, `user_version` migrations,
foreign keys/constraints, integrity/error telemetry, and documented backup.
Do not select WAL until server-filesystem measurements justify it.

## Determinism

The target equation is:

```text
same immutable World/Nav/Experience snapshots
+ same configuration version
+ same explicit TickId and event order
+ same per-agent seeded RNG stream
= same DecisionResult and BotCommand
```

No Core component reads `gpGlobals`, wall time, a shared global RNG, unordered
container iteration, or mutable adapter objects.  Stable ordering/tie-breaking
uses value IDs.  RNG seed derivation includes match seed + agent ID + named
stream; adding aim noise must not perturb planner randomness.  Engine traces are
recorded as snapshot/query results for simulation/replay tests.

## Observability

Observability is result data, not a later UI.  At minimum each decision exposes:

- current/goal area, route areas/edges, route and component costs;
- current action/intent/role and utility candidates/scores;
- enemy belief distribution/confidence and observation age;
- replan reason, rejected alternatives and constraint failures;
- Experience contribution with source kind/sample count;
- scheduler tick, elapsed/budget/deferral and host command result.

Release builds may change sinks/rate limits, but must not remove the reason model.
IDs and reason enums are versioned; logs never include raw pointers.

## Scheduler and performance assumptions

Target assumption is a 32-slot server with at most 16 AstraBots.  Frequencies are
starting budgets, not verified performance claims.

| System | Initial cadence | Invalidation / likely hot path |
|---|---|---|
| Motor | each server frame / bounded command msec | command translation and submission |
| Local navigation | 20–30 Hz per active Bot | traces, clearance, player avoidance |
| Perception | 10–20 Hz, staggered | visibility/hull traces and entity filtering |
| Action planner | 5–10 Hz or event | utility candidates |
| Tactical planner | 1–5 Hz or goal/event | route queries and belief expansion |
| Team director | 1–2 Hz or roster/objective event | assignment search |
| Experience reducer | event-driven | bounded aggregation |
| Persistence | round/map boundary | single-writer transaction |

Work is staggered by agent ID and budget; urgent events enqueue bounded
invalidation rather than running all planners immediately in one frame.  Engine
calls are main-thread only.  Future worker jobs receive copied immutable inputs
and return values; deadline misses retain the last valid result and are traced.
Measure trace count/time, A* expansions/time/cache hit, planner candidates/time,
per-Bot and aggregate frame budget before optimization.

## Optional AMXX boundary

`astrabot_amxx` is an optional consumer of the Metamod runtime, preferably a
separate bridge binary.  Core and normal Bot operation do not load AMXX.  The
future ABI is small and versioned:

- queries: is AstraBot, agent ID, current intent/action/area, read-only decision
  snapshot;
- commands: add/remove, set high-level goal, request replan, bounded behavior
  override with ownership/timeout;
- forwards/events: created, joined, decision changed, goal reached, disconnected,
  error;
- capability/version negotiation and explicit error codes.

No native returns internal pointers, references mutable Nav/Experience, or sets
low-level movement indefinitely.  SyPB proves native registration/control is
possible but also shows the coupling to avoid
（[comparison](research/upstream-comparison.md)）。AMXX/ReAPI license handling follows
[the matrix](research/license-matrix.md).

## Testing strategy

| Level | Server required | Scope |
|---|---|---|
| Unit | No | byte parser, geometry, nearest area, connectivity/A*, cost breakdown, IDs/RNG, event reducers, utility/belief logic |
| Simulation | No | scripted immutable snapshots/events; same seed replay; corridor/local navigation against deterministic fake trace/query host |
| Integration | Process harness where possible | adapter mapping, hook/message state machines, command translation, persistence migrations/recovery |
| Live server | Yes | actual Metamod-P load, FakeClient lifecycle, CS join, movement/collision, map change, ReGame/Vanilla capability behavior |

Build/test success never substitutes for a live gate.  Phase 1–3 record server,
engine/GameDLL/Metamod SHAs or versions, map/fixture hashes, command trace and
observed cleanup.

## v0.2 claim classification

| Classification | Claims |
|---|---|
| **Verified** | Architecture B direction; external BotAgent rather than `CBot:CBasePlayer`; Core/adapter value boundary; Nav/Experience separation; imperfect information; optional AMXX; observability and deterministic-test goals; small phase gates. |
| **Likely correct but needs proof** | Vanilla GameDLL can be added through standard host hooks; proposed scheduler rates meet 16-Bot budget; ReGameDLL is the first practical live target; public message/entity observation is sufficient for later gameplay features. |
| **Needs modification** | `.nav` “ladder data” becomes adapter/BSP enrichment; SQLite changes from assumed first choice to Phase 9 recommendation behind a port; ReAPI is optional, not Phase 1-required; MPL is file-level policy with a separate Metamod combined-binary license gate. |
| **Incorrect** | RealBot already separates human and Bot learning; RealBot is a NavMesh example; `.nav` loading alone yields ladders; ReGameDLL nav source can be treated as cleanly MIT solely from the root license. |
| **Deferred decision** | nav generation, stronger BSP parser, final schema/tables and WAL mode, exact public AMXX ABI, worker threading, advanced planner/learning. |

## Changes from v0.2

| Changed | Why | Evidence | Impact |
|---|---|---|---|
| Formal name is AstraBot; future namespace/module/config roots are `astrabot`, `astrabot_mm`, `astrabot_amxx`, `astrabot.cfg`, `addons/astrabot/`. | Remove historical placeholders without rewriting inputs. | Project identity requirement and preserved baseline. | Consistent future public naming. |
| ReGameDLL nav extraction → independent v1–v5 reader. | File-header/root/provenance ambiguity and global GameDLL coupling. | [Nav/license research](research/nav-extraction.md). | More Phase 2 parser work; much stronger test/license boundary. |
| Ladder moves out of serialized Nav. | v5 file contains no ladder record; loader calls live `BuildLadders`. | [Nav decision](research/nav-extraction.md#executive-decision). | Synthetic enrichment test in Phase 2; live discovery/traversal in Phase 3. |
| “Human movement learning” → all-client observation in RealBot. | Source loop has no actor-kind test. | [RealBot analysis](research/realbot-learning.md#what-changes-through-play). | AstraBot event schema must record actor provenance explicitly. |
| ReAPI-first host → standard Metamod-P lifecycle with optional ReGame capability. | FakeClient/join/movement need only standard interfaces; reduces Vanilla/Core coupling. | [Host validation](research/goldsrc-host.md). | Phase 1 is smaller; richer ReGame events can be added later. |
| MPL-only wording → MPL-authored files plus a fixed conservative combined-runtime policy. | Metamod-P is GPL-2.0/GPL-2.0-or-later and its FAQ treats plugins as GPL. | [License matrix](research/license-matrix.md) and [toolchain policy](toolchain-policy.md). | Treat `astrabot_mm` as GPL-2.0-or-later-compatible for distribution planning, preserve MPL/SDK notices, and block release until exact dependency review. |
| SQLite assumed → SQLite recommended, persistence port undecided until Phase 9. | Deployment is feasible, but measurements/schema/release packaging are future work. | Persistence comparison above; AMXX bundles SQLite statically as feasibility evidence. | No DB/build dependency in early phases. |

## Answers to the 13 Phase 0 questions

1. **独立Metamod Botとして成立するか:** Yes, source-level feasibility is
   verified.  Standard FakeClient/GameDLL lifecycle supports an external agent;
   live acceptance remains Phase 1.
2. **YaPB forkは必要か:** No.  Its mature host is valuable evidence, but graph
   dependency pervades Bot creation/navigation/AI and conflicts with the new Core.
3. **FakeClient lifecycleの最小正解:** Metamod attach → map activate →
   `pfnCreateFakeClient` → GameDLL `player` factory → info keys →
   `MDLL_ClientConnect` → `MDLL_ClientPutInServer` → message-driven team/class
   commands → generation-safe mapping → one `pfnRunPlayerMove` per tick → kick →
   `ClientDisconnect` cleanup → `ServerDeactivate` cleanup.
4. **ReGameDLL `.nav`をそのまま利用するのが最良か:** Use existing `.nav`
   **as immutable input data**, yes; using its loader/runtime objects directly,
   no.  Ladders and dynamics require separate enrichment.
5. **Nav code方式:** B, format-compatible independent implementation.
6. **ReGameDLL dependencyをadapterへ閉じ込められるか:** Yes for the selected
   scope.  Common values/events/traces cross the boundary; no ReAPI/GameDLL type
   does.  A requested feature that cannot obey this is optional adapter behavior.
7. **Vanillaを後から追加できるか:** Likely yes because the Phase 1 critical
   path uses standard Metamod/HLSDK interfaces.  Exact menu/message/weapon/event
   behavior requires a separate Vanilla live gate.
8. **最初のLocal Navigation機能:** portal/corridor look-ahead, grounded hull
   clearance, floor/step/door handling, player yield, progress/oscillation stuck
   recovery; then crouch/jump and ladder state machines.
9. **RealBotから採用するもの:** persistent map feedback, team-aware danger/
   contact, visibility/traffic concepts and observed traversal.  Not its code,
   node graph, raw files, global mutation, or claimed human/bot separation.
10. **保存形式:** SQLite is the Phase 9 recommendation for migration, crash
    safety and debugging, with one writer/round transactions.  Keep a persistence
    interface until schema and deployment benchmarks are ready.
11. **MPL-2.0維持のupstream扱い:** YaPB/CS-EBOT are reference-only by default
    despite permissive/file licenses; GPL/custom/mixed sources are reference-only
    + independent implementation; Metamod/HLSDK official headers are isolated
    and the combined binary is planned under the conservative
    GPL-2.0-or-later-compatible policy recorded in
    [toolchain-policy.md](toolchain-policy.md), pending exact release inventory.
12. **Phase 1–3最大risk:** Phase 1 is exact FakeClient join/cleanup across
    server variants; Phase 2 is safe/compatible parsing plus license provenance;
    Phase 3 is corridor-to-motor traversal under ladders/dynamic blockers/stuck
    without frame spikes.
13. **最小成功条件:** Phase 1 live load/join/mapping/move/disconnect/map-change;
    Phase 2 offline v1–v5 load, geometry/connectivity/synthetic ladder enrichment,
    nearest/A*, malformed rejection; Phase 3 live floor/door/stairs/narrow/
    crouch/jump/ladder/blocker/dynamic/stuck scenarios using corridor + local
    steering rather than center chains.

## Phase gates and next action

- Phase 1 gate: [live host scenario](research/goldsrc-host.md#phase-1-live-gate).
- Phase 2 gate: [revised offline nav scenario](research/nav-extraction.md#revised-phase-23-gate).
- Phase 3 gate: live movement matrix defined in
  [the Phase 3 plan](plans/phase-3-nav-movement.md).

P1-01 begins with the portable Core value/command contracts and host port only.
The next task is the Metamod-P load/unload adapter skeleton, plus an offline
fake-host test and load trace.  Do not begin Nav, planning, DB, AMXX, or combat
in P1-01.

## Phase 3 local navigation decision (2026-09-05)

The inspector, portable session and console adapter integration are implemented.
Movement below remains planned. Audited planning base: `b49f4da`.
See [session evidence](reports/p3-01-route-session.md) for the implemented boundary.
See [console evidence](reports/p3-01-nav-console.md) for operator commands,
grounded queries and the approved six-export engine bootstrap.
Task ownership is in [the existing plan](plans/phase-3-nav-movement.md);
real-byte readiness is in [the compatibility protocol](research/real-nav-compatibility.md).

### Alternatives and selected boundary

| Approach | Cost / consequence | Decision |
|---|---|---|
| Put movement in the GoldSrc adapter | Small initial seam, engine-bound state and difficult replay | Reject |
| Portable corridor + LocalNavigator + small explicit primitive state | Value snapshots, deterministic tests, separate motion lifecycle | Select |
| General plugin/behavior graph for arbitrary traversal | Extra registry, serialization and extension framework before use cases | Defer |

```text
NavMeshSnapshot + map-generation TraversalEnrichment
                    |
NavGraph / NavRouteSearch -> owned NavRouteResult (selected edges)
                    |
RouteSession -> Corridor / transition constraints
                    |
LocalNavigator -> MotionPrimitive state -> MovementIntent
                    |
portable Motor translation -> BotCommand
                    |
IGameHost / GoldSrc adapter -> validated RunPlayerMove
                    ^
MovementSnapshot + bounded value query results
```

The names introduced below are internal contracts, not a stable AMXX ABI.
Session/snapshot/query headers exist in `src/nav/runtime`; subsequent movement
contracts remain planned. Reuse `NavRouteResult`, `NavRouteStep`,
`NavTraversalKind`, `NavTraversalLink`, `PlayerId`, `MapGeneration`,
`TickId` and `BotCommand`; do not introduce a duplicate NavRoute model.
A* stays synchronous and bounded, with no per-frame motor state or hidden traces.

### Ownership and input/output values

- **RouteSession** owns an immutable mesh/graph reference, copied route result,
  actor/map/route generations, goal, cursor and terminal reason. Retain selected
  edges including external link identity; never recover an edge by area IDs alone.
- **MovementSnapshot** contains actor/map/tick, explicit elapsed simulation time,
  position, velocity, view, grounded/ducked/alive/joined state, hull dimensions,
  movement speed limit and ladder contact facts. Unknown facts are explicit,
  not zero-valued proof of ground or clearance.
- **World query port** returns swept-hull fraction/end/normal, floor height/
  normal, clearance, door/use capability and blocker value ID/class. Requests and
  replies carry actor/map/tick and a bounded request ordinal. Reject stale or
  budget-exhausted results; never retain an adapter pointer or call an SDK API.
- **Corridor transition** owns from/to, selected edge identity, portal bounds,
  source/target support height and traversal constraints. Walk portals are shared
  cardinal boundary overlap, shrunk by hull clearance. Test slope and vertical
  discontinuity with ground probes. No overlap or exhausted clearance yields
  `InvalidPortal`; an external link uses its entry/exit plus verified probes.
- **MovementIntent** expresses world-space desired direction/speed, bounded
  lateral correction, desired view and action requests (duck/jump/use). Optional
  wall normal and clearance come from observations. Thus narrow-edge control
  can add corrections without replacing a target-point-only API.
- **Motor** transforms desired velocity into view-relative forward/side/up,
  clamps each command component to the existing 400-unit contract and observed
  player speed, validates finite angles/buttons, and emits fresh commands.
  Button press/hold/release belongs to primitive state; never replay a jump edge
  merely because steering reused an intent.
- **DecisionTrace** reports actor/map/tick/route generation, current/goal area,
  ordered route/step, traversal/link, primitive/state, local target, desired
  speed/lateral correction, stuck state/reason, recovery attempt, replan reason,
  arrival/terminal state and command submission result. Bound/rate-limit sinks;
  preserve terminal events and reasons.

GoldSrc entity pointers, entvars, CBasePlayer/CBaseEntity, ReAPI concrete types,
Metamod globals and engine globals stay wholly inside `src/adapter`.
Doors/player facts are short-lived overlays, not mutations of the mesh.
No new World Model, Experience database or public plugin registry is required.

The P3-04 host consumes a dynamic-blocker timeout only with a fresh stamped
observation and a selected directed edge. It queues one automatic replan for a
later tick, excluding that edge for one second through a borrowed pure cost
policy. Static identity uses source/target/direction; external edges retain
source/generation/link/direction identity. Ordinary distance and external-link
costs remain intact. Each actor's explicit goal has at most one automatic retry,
carried across route replacement; failed/expired requests do not self-retry.
Cancellation and actor/map invalidation clear pending facts. The attempt and
fact-lifetime limits and DynamicObstacle reason are printed in diagnostics.
See [bounded replan evidence](reports/p3-04-bounded-replan.md).

### Traversal and primitive lifecycle

NavMesh describes connectivity. Traversal describes how a selected directed
edge is crossed. A primitive controls per-tick execution. The existing enum
already has Walk/Crouch/Jump/Ladder/Drop; raw NAV attributes and approach bytes
are different metadata. File cardinal edges currently default to Walk.
P3-05 interprets supported movement attributes and local probes; NoJump or
unknown/contradictory constraints fail closed with a typed reason.

Use a small value-owned tagged state, not an inheritance/plugin framework:
`enter -> update -> Running | Complete | Failed`, with `abort` from any running
state. Enter initializes once per transition; terminal outcome is emitted once.
Abort clears requested action edges and hands a safe neutral/stop intent to the
motor while the actor remains valid; lifecycle removal invalidates the session
rather than sending commands to a dead/disconnected actor.

| Primitive | Minimum states and completion |
|---|---|
| Walk | follow portal/look-ahead; complete on validated target-area/support crossing |
| Crouch | approach, clearance check, hold duck, cross, release when headroom permits |
| Simple Jump | approach, align, accelerate, takeoff edge, airborne, land, recover; require landing in the intended support/area |
| Ladder | approach, align, acquire contact, climb up/down, exit, verify support; bounded timeout, falling-off failure and at most one validated re-acquire |
| Drop | vocabulary reserved; unsupported transition in Phase 3, never Walk fallback |

Simple Jump is a locally verified small transition, not general gap solving or
automatic duck-jump search. Preserve a transition constraint slot for approach
point, takeoff/landing region, required speed and facing. Populate only facts
needed and verified for the supported primitive. A future GapJump state machine
can use those constraints without changing A* or treating a link as a jump button.

Crouch now gates Walk before translation using observed duck/hull state, then
uses the same measured floor, swept hull and supported portal advancement.
Source/target crouch hints hold the posture across the boundary; a clear next
area releases only after a fresh standing-hull check and observed standing.
The host rechecks headroom before dispatching release. Cached intents and safe
stops retain duck while the actor is ducked; a neutral stop means zero movement,
not an unsafe forced stand under a ceiling. Arrival in a crouch-required goal
retains duck. See [crouch integration evidence](reports/p3-05-crouch-walk.md).

Ladder links reuse `sourceId/generation/linkId`, direction, entry/exit and BSP
fingerprint. P3-06 adapter discovery owns geometry/facing/contact facts; local
ladder state owns climb/exit/abort. Re-acquire requires a fresh same-generation
contact/clearance check; failure exits to recovery, never an infinite Walk retry.

### Route status, arrival and invalidation

`NavRouteStatus` is exactly Complete, Unreachable, ExpansionLimit. There is no
separate Partial enum; only opt-in ExpansionLimit can own a partial corridor.
Default goto uses `allowPartial=false`: only Complete starts movement.
ExpansionLimit with an opt-in prefix is diagnostic-only in Phase 3; stop and
report it, optionally request one bounded larger-budget replan. Unreachable has
no executable corridor. Parser/query errors are separate failures.

Complete means graph reachability, not arrival. Arrival requires the requested
goal area plus grounded/appropriate support confirmation; a ladder exit or jump
must finish its primitive. Same-area requests perform that check before success.
A partial endpoint is never goal arrival. Stale actor/map generation, goal
replacement, death, disconnect or map change aborts the active primitive and
invalidates pending local facts. New sessions use a new route generation.

### Steering, stuck and recovery scope

Phase 3 includes portal following, speed/turn/strafe control, wall clearance,
grounded stairs, ordinary door use/wait, clearance-based narrow passages and
bounded reactive player/dynamic-blocker yield. It excludes advanced wall-edge
balancing, crowd planning and predictive obstacle trajectories.

Stuck detection requires a movement command actually dispatched, expected
progress in the active primitive, insufficient projected corridor progress and
displacement over an explicit duration. Include oscillation and alignment;
exclude deliberate waiting, airborne/ladder-specific expected pauses and rejected
commands. Keep DoorBlocked, PlayerBlocked, GeometryBlocked, TraversalFailed and
Unknown distinct, without guessing a cause absent observations.

Initial deterministic recovery bounds for tests: one 500 ms no-progress window
for Walk (1000 ms for slow crouch), then wait up to 250 ms, one clearance-checked
sidestep up to 250 ms, one reverse/realign up to 250 ms, one replan, then abort if
the new route again fails. Primitive-specific timeouts are separate. Choose the
side by clearance, then stable actor ID on ties. Reset only on measured progress
or explicit new goal, not on target jitter; carry the attempt budget through
replans so replanning cannot evade termination. These are initial configuration
values, logged and tested, not live-tuned performance claims.

### Scheduling and command transport

The existing `LifecycleCoordinator::startFrame` dispatches queued commands
through `MovementCoordinator`; dispatch requires a later TickId than submission.
`quantizeMsec` rounds measured adapter frame duration and clamps to 1..255 ms;
it does not use caller msec as the movement duration. Preserve this contract.

Select **25 Hz LocalNavigator/primitive decisions** (40 ms target period) and
**one fresh motor command per eligible server frame**, consuming the latest valid
intent. Submit after that frame's dispatch for the next frame; never submit and
force dispatch at the same tick. Motor updates edge/hold expiry every frame.
Do not simply send a 40 ms intent once every 40 ms: the existing pump would use
only one frame's measured msec and under-drive movement.

Use explicit elapsed time, at most one local update per server frame, no catch-up
burst; below 25 FPS update once per frame and record missed deadlines. Stale
intent over 120 ms stops locomotion, releases actions and requests a reasoned
refresh. Replay recorded adapter elapsed time/results; Core reads no wall clock.
Map generation resets all timing state.

Replan on goal/map/route invalidation, confirmed blocked portal or bounded
recovery, not every frame. Coalesce reasons; initial minimum interval 200 ms and
at most one query per actor per local tick with finite expansion/memory limits.
While invalidated, stop; cooldown never authorizes use of an invalid route.
Record budget/deadline misses; tune using post-Finish evidence.

### Multi-Bot seam and future events

Portable navigation is per `PlayerId` plus map generation. It must never know
`primary fake client`. `LifecycleCoordinator` now owns a bounded set of client
states, each retaining its own `FakeClientCoordinator`, join and message decoder.
PlayerId/map/edict serial identify the mapped entity; submit and dispatch use
that actor's state and consume only its queue. Primary convenience APIs remain
for bootstrap. NavConsole shares immutable map navigation and owns per-player
route, Walk, IntentPump, queue and bounded history state. Debug goto/status/cancel
accept `slot:generation`; omission requires exactly one managed actor. Individual
retirement preserves other sessions, while map publication invalidates all.
See [per-player NAV evidence](reports/p3-04-multi-nav.md) for synthetic two-lane
navigation, generation reuse and reentrant invalidation coverage.

P3-01/P3-03 initially use one actor. The P3-04 adapter change has synthetic
two-client creation, interleaved menu, join/dispatch, removal/reuse and map-change
coverage; see [the host seam report](reports/p3-04-player-host.md). Synthetic
commands establish isolation, not simultaneous live navigation. Two-AstraBot
live acceptance and P3-08 8/16-Bot loads still require post-Finish live evidence.

A future event consumer can subscribe to terminal primitive outcome carrying
actor kind/ID, map fingerprint/generation, route generation, selected edge and
external link identity (if present), from/to, times and typed success/failure.
Static edges use source/target/cardinal direction plus map identity, not a
fabricated link ID. Phase 3 emits debug outcomes only; no successRate, learning,
adaptive costs, demonstration capture or persistence is introduced.
