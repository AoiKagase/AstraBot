# NAV ladder travel implementation plan
Spec: docs/superpowers/specs/2026-11-15-nav-ladder-travel-design.md
Base: 0b15098d1ffa88ce3bfc5f803fe8758cd9423fc5

## Task 1: Reconstruct and query native ladders
Interfaces: SDK-free public bounds/probe -> qualified topology -> NavQuery travel. Native length and selected-route center distance are different outputs.
- [x] Write semantic failing reconstruction/travel tests; run nav_query target and confirm failures.
- [x] Implement reconstruction, ordered graph search and travel distance; rerun nav_query and navigation regression targets.
Expected: native geometry/ordered graph cases pass; default ground corridor unchanged.

## Task 2: Public runtime provider
Interfaces: public engine/immutable NAV receipt -> topology -> existing behavior service. Existing movement context does not opt in.
- [x] Write public provider tests and see failures; implement lazy atomic caching and callback qualification.
- [x] Run runtime/navigation contract tests; final fresh most-capable read-only review, one Important fix pass if necessary.
Expected: valid public ladder graph, cache reuse, stale/missing inputs rejected; default movement unchanged.

## Task 3: Verify and integrate
- [x] Complete both OS builds; serialize full CTest; independent PE/ELF7exports and source manifest.
- [x] Prepare exact source/user/artifact hashes and explicit owned-path diff/ownership review. Commit, task-done, local FF and postmerge receipt are recorded afterward in the session RESULT.json; this precommit plan does not predeclare those results.
Expected: Windows all pass; Linux sole existing22 failure explicitly retained. Original07 and full goal remain active.

## Review Focus
Trace flags/ignored entity, native float expressions/order, top length before adjustment, asymmetric links and duplicate ladder list order, selected-route distance versus weighted cost, reentrant public engine replacement/map/NAV/entity invalidation, cache poisoning and unintended ground follower activation.
