# P2-07 Traversal Enrichment Implementation Plan

Execution: inline using executing-plans; user explicitly forbids subagents.
Spec: ../specs/2026-09-05-p207-traversal-enrichment-design.md
Base: main e37b9e4. Worktree: .worktrees/p207-traversal-enrichment.

## Global constraints

C++17 SDK-free, immutable transactional publication, no main merge/remotes,
no Linux/live until project Finish. Explicit staging; no local index commits.
Preserve Nav PUBLIC _ITERATOR_DEBUG_LEVEL=0. See spec for complete contracts.

### Task 1: Link validation and bounded composition

- [x] Record baseline Debug12/12 and FocalSpan context.
- [x] Add tests/nav/enrichment_tests.cpp and CMake test target.
- [x] RED: call NavGraph::compose with valid links and invalid field/budget cases.
- [x] Add enrichment/traversal_link.hpp value types, internal validation/budget
  helpers in enrichment/detail, and link diagnostic fields.
- [x] Add compose to query/graph.hpp/.cpp sharing static preflight/construction,
  retaining optional link values on NavDirectedEdge.
- [x] GREEN: run full Debug/CTest, verifying exact fields and graph ordering.

### Task 2: Route evidence and costs

- [x] RED: synthetic two-area vertical ladder, centers distance 10, upward
  additionalCost 2 -> total12; downward additionalCost 5 -> total15.
- [x] Update standard route policy to include metadata cost, preserve custom
  callback's complete authority and existing heuristic.
- [x] Check parallel identity selection, static-first ties, custom non-double
  addition, partial route evidence and input-order invariance.
- [x] GREEN: full Debug/CTest and static-only regression.

### Task 3: Failure safety, lifetime and completion

- [x] Budget exact/one-under checks before allocation; arithmetic helper overflow.
- [x] Sweep fail-after allocator through composition and route construction;
  require typed failure without publication, retaining previous results.
- [x] Input/snapshot/graph destruction and two concurrent repeatable searches.
- [x] Add independent fixture provenance and update roadmap/STATE.
- [x] Run canonical Debug and /analyze; review diff and requirements inline.
- [x] FocalSpan update/query, explicit git add, cached diff check, commit,
  verify log/status. Retain worktree and branch; report acceptance boundaries.
