# P07 Navigation, Pathfinding, Movement, and Traversal Model

## Reference identity

The pinned comparator is ReGameDLL-CS commit `b0889847fe6d03898be88acc9e366660efb40ab5`.
The relevant reference surfaces are:

- `game_shared/bot/nav.h`, `nav_area.h/.cpp`, `nav_file.cpp`, and `nav_path.h/.cpp`;
- `dlls/bot/cs_bot.h`, `cs_bot_pathfind.cpp`, `cs_bot_nav.cpp`, and `cs_bot_learn.cpp`;
- state callers in `cs_bot_move_to.cpp`, `cs_bot_follow.cpp`, `cs_bot_hunt.cpp`, `cs_bot_hide.cpp`, `cs_bot_investigate_noise.cpp`, `cs_bot_fetch_bomb.cpp`, and `cs_bot_escape_from_bomb.cpp`.

This document records the P07 baseline only. It does not claim live HLDS/ReHLDS physics parity.

## Reference NAV architecture

CSBot loads a read-only `.nav` mesh into `CNavArea` objects. Areas have stable IDs, extents/floor geometry, directional floor connections, attributes, hiding spots, approach metadata, encounter metadata, and place membership. Runtime ladder objects are linked into area ladder lists. `CNavPath::Compute` resolves a world start/goal to areas, calls `NavAreaBuildPath`, reconstructs parent links, then computes portal/ladder/drop path positions.

The reference file format uses magic `0xFEEDFACE` and versions 1 through 5. Version 4 carries BSP size metadata; version 5 carries the place directory. Area records include ID, attributes, extent/floor values, four directional connection lists, hiding spots, approach records, encounter records, and place membership. Attribute bits are `NAV_CROUCH=0x01`, `NAV_JUMP=0x02`, `NAV_PRECISE=0x04`, and `NAV_NO_JUMP=0x08`.

`MoveTo`, `Follow`, `Hide`, `Hunt`, `InvestigateNoise`, `FetchBomb`, and `EscapeFromBomb` choose the higher-level goal and route type. P07 only maps that goal to NAV search/path following; it does not change tactical state selection.

## AstraBot source of truth and mapping

Compatibility Mode uses the immutable, SDK-free `NavDocument` loaded by `LegacyNavReader`, published through `NavSnapshotPublisher`, and queried by `NavQuery`. The loaded NAV document is the source of topology truth. Runtime-generated geometry and adaptive learning are not accepted as Compatibility topology truth.

Current mapping:

| Reference concept | AstraBot mapping | P07 status |
| --- | --- | --- |
| Area ID, extent, floor | `NavArea::id`, `extent`, `northEastZ`, `southWestZ` | offline verified |
| Directional floor connections | `NavArea::connections[0..3]` | offline verified; stored order preserved |
| NAV attributes | `NavArea::attributes`, `kCrouch`, `kJump`, `kPrecise`, `kNoJump` | offline verified for crouch/jump intent and cost |
| Hiding spots | `NavArea::hidingSpots` | loaded/validated; selection parity remains open |
| Approach data | `NavArea::approaches` | loaded and used to recover directed `how` metadata |
| Encounter data | `NavArea::encounters` | loaded/validated; runtime encounter selection remains open |
| Place names/membership | `NavDocument::places`, `NavArea::placeId` | loader boundary present; behavior use remains open |
| Ladder objects | `SpecialTraversalController` capability/state boundary | ladder object loading/link identity is `NOT_YET_IMPLEMENTED` |
| Jump/drop path segments | `JumpDropController` and `TraversalAction` | offline envelope/state tests; engine physics open |

## NAV loading and generation

`LegacyNavReader` accepts existing compatible versions 1 through 5 read-only and rejects malformed or resource-exceeding documents transactionally. It does not write `.nav` files.

The reference has a learning/generation process (`StartLearnProcess`, `LearnStep`, `GenerateNavigationAreaMesh`, ladder building, analysis, and save). AstraBot does not substitute its own NAV generator for this process. Missing or invalid NAV remains a load/runtime gap, not a Compatibility success. Full CSBot-compatible generation is `NOT_YET_IMPLEMENTED` and deferred beyond P07.

## Path search and cost

Reference `NavAreaBuildPath` is A* with parent links. It enumerates floor directions in `NORTH, EAST, SOUTH, WEST` order, then ladder-up and ladder-down candidates. `CNavArea::AddToOpenList` inserts by ascending total cost and keeps equal-cost entries in discovery order; it does not use area ID as an equal-cost tie-break.

P07 `NavQuery` now preserves the serialized direction/connection order and uses stable discovery-order ties. `NavRouteType::Fastest` applies the reference static cost terms available in the SDK-free model:

- center-to-center traversal distance;
- `NAV_CROUCH`: `20 * distance` for FASTEST;
- `NAV_CROUCH`: `5 * distance` for SAFEST;
- `NAV_JUMP`: `1 * distance`;
- the existing directed edge/approach `how` metadata.

Dynamic danger, aggression, teammate density, fall-damage health margin, ladder length, and hostage-specific penalties require private/live state or ladder objects. They remain unavailable; SAFEST is therefore a bounded static approximation and is not promoted to a broad `MATCH`.

The resulting `NavCorridor` records `routeType` and static `cost` for diagnostics. Compatibility route selection does not add actor-derived weights, anti-camp weights, traversal learning, or random tie selection.

## Current area and path following

The runtime first uses `NavQuery::findContaining` with floor tolerance, then a bounded nearest-area recovery only when the actor is off mesh. The follower aims at portal points and retains the stamped corridor across updates. P07 exposes `pathSequence`, `fullUpdateSequence`, `recomputeReason`, `selectedPath`, `routeType`, and `pathCost` through `NavRoamDecision` for deterministic fixtures and traces.

Compatibility recompute reasons are explicit:

- `InitialGoal` on the first route;
- `GoalChanged` when the supplied goal changes;
- `MapOrRoundChanged` on lifecycle generation change;
- `PathInvalidated` for a follower/path failure;
- `Stuck` for a stuck-triggered recovery/replan.

An unchanged goal and valid active route do not cause a new path sequence on every Full Update.

Reference `CNavPathFollower` advances a path segment when the actor is within approximately 20 units of the segment point. AstraBot's default locomotion target tolerance is now 20 horizontal units; explicit test configurations may remain tighter for contract tests.

## Movement and view boundary

Reference `MoveTowardsPosition` projects the target against current forward/lateral view vectors; movement direction and look/aim are separate. AstraBot emits a locomotion direction/intent and the adapter translates it to movement commands. P07 does not route Combat Aim through locomotion and does not change the P02 command cadence.

The default locomotion speed remains the existing bounded 100-unit intent. Smooth steering, wall-centering, predictive obstacle smoothing, and other adaptive improvements remain Enhanced-only or unclaimed for Compatibility.

## Jump, crouch, and ladder

`NAV_CROUCH` selects crouch traversal unless a jump area takes precedence. `NAV_JUMP` selects a jump intent and prevents a normal step intent. Existing jump/drop envelopes preserve bounded launch/landing validation and cooldown/state handling.

Reference ladders are not ordinary floor links: path construction chooses ladder-up/ladder-down edges, path positions mount at ladder endpoints, and `SetupLadderMovement`/`UpdateLadderMovement` handle approach, facing, mount, climb, dismount, timeout, missed-ladder, and give-up recovery. AstraBot retains the separate `SpecialTraversalController` for enter/maintain/exit and bounded recovery. Exact ladder object identity, mount geometry, engine ladder flags, and climb physics are live-unverified.

## Stuck behavior and learning isolation

Reference stuck monitoring uses averaged displacement/velocity over Full Think samples, distinct ladder sensitivity, and a bounded wiggle/recovery path. AstraBot retains bounded stuck detection and recovery state, and exposes the decision as `Stuck`; exact reference RNG placement, wiggle directions, and physics response are not claimed.

Any future adaptive route weighting or traversal learning must stay behind `RuntimeModePolicy::allowsAdaptiveRouteWeighting()` / `allowsLearningSideEffects()`. P07 compatibility tests prove actor-derived candidate rotation does not affect the Compatibility route while explicit Enhanced controllers retain that behavior.

## Deterministic fixtures

The P07 golden fixtures are synthetic SDK-free graphs with explicit area IDs, stored neighbor order, fixed route type, and deterministic expected area sequences:

- direct route: `1 -> 2`;
- equal-cost order: stored `1 -> 20` before `1 -> 10` selects `1 -> 20 -> 4`;
- crouch cost: a geometrically shorter `NAV_CROUCH` branch loses to the longer normal branch under FASTEST;
- route persistence: the same goal retains `pathSequence=1` across Full Updates; a changed goal produces `GoalChanged` and `pathSequence=2`;
- Compatibility isolation: two actor identities select the same first stored link;
- NAV attributes: `NAV_CROUCH` and `NAV_JUMP` produce corresponding traversal intents;
- portal arrival: a point within 20 units of the next area is accepted.

An existing small `.nav` fixture is available in the surrounding AstraBot backup/build evidence, but no large map asset is copied or committed by P07. The production loader's version/field behavior is covered by existing NAV loader tests; real map geometry and live route physics remain separate gates.

## Live verification required

The following remain `Unit/reference semantics verified; Live parity unverified`:

- private CSBot trace/RNG placement and exact repath timer consumption;
- real `.nav` ladder links and ladder mount/climb/dismount physics;
- jump/drop collision, landing, fall damage, doors/use, and actual `RunPlayerMove` result;
- dynamic danger, teammate density, aggression, hostage escort, and private GameDLL state;
- real-map NAV geometry and sustained autonomous traversal on HLDS/ReHLDS.
# P07.6 production integration

Production Compatibility uses `Roam` as the pre-P08 Goal producer when no
observed public objective target is available. A temporary current-area miss
does not retire an active corridor; map/round, goal, invalidation, and bounded
stuck transitions remain the explicit retirement conditions. The adapter logs
Goal presence, current/goal area, path request/result, route identity, and NAV
apply rejection separately.
