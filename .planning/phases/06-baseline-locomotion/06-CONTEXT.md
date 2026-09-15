# Phase 6 Context: Baseline locomotion

## Goal

Use the immutable legacy Nav snapshot to produce bounded, directed routes and
movement targets that can later be translated into GoldSrc input. Phase 6 does
not claim that a route or target is movement evidence; only the input dispatch
and runtime feedback layers can establish that evidence.

## Locked decisions

- Core navigation remains SDK-free. No engine entity, SDK vector, engine
  function table, private GameDLL type, or ReAPI symbol crosses the Core
  boundary.
- NavDocument and NavSnapshot remain read-only. A query may derive indexes,
  links, corridors, and progress state, but it must not mutate the published
  document or write a legacy .nav file.
- The four NavArea::connections collections are directed. A reverse edge is
  never inferred when the source file does not contain one.
- Spatial containment uses the existing area XY extent and the two stored floor
  heights with explicit finite tolerances. It must be deterministic when areas
  overlap: prefer the smallest vertical distance, then the smallest AreaId.
- All query result collections and route searches have explicit upper bounds.
  Resource exhaustion and invalid input are returned as result values.
- A corridor records the snapshot revision and map generation that produced it.
  A follower rejects a corridor against another map generation or Nav revision.
- The path follower reports target selection, advancement, completion, stale
  route, and no-route states. It never reports movement success merely because
  a target or corridor exists.
- Walk/Crouch/Step/Jump/Drop/Ladder/Door/NarrowPassage support is added in
  separate plans so each movement primitive has its own envelope and feedback
  tests.
- Wall penetration, AstraNav writing, learned traversal, hidden enemy state,
  and adaptive tactical routing remain outside this phase.

## Existing boundaries to preserve

- include/astrabot/nav/nav_model.hpp is the format-neutral model.
- include/astrabot/nav/nav_snapshot.hpp is the immutable publication boundary.
- include/astrabot/runtime/bot_command.hpp and
  include/astrabot/metamod/input_dispatcher.hpp are the later input path.
- PluginRuntime::navigationSnapshot() exposes a snapshot for adapter-side
  integration; Phase 6 Core code must not pull PluginRuntime into navigation.

## Verification boundary

Portable x86 Core tests prove deterministic queries, directed route behavior,
bounded failure, and stale-route rejection. Metamod tests continue to prove
that the existing six-export and FakeClient/input contracts remain intact.
Real HLDS/ReHLDS locomotion, collision response, and multi-Bot stability remain
Phase 8 acceptance evidence and are not replaced by offline tests.
