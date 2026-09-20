# AstraBot P07.6 Audit Fixes Design

Status: design approved in chat; implementation not started

Scope: F01-F08 from `docs/AstraBot_Source_Audit_6ec3f063/AUDIT_JA.md`.
P08 Combat/Aim/Weapon remains out of scope.

## Goal

Close the confirmed P07.6 NAV/objective/runtime discrepancies while preserving:

- Compatibility versus Enhanced behavior boundaries;
- P02 timing, command `msec`, freeze gating, and route persistence;
- existing search-budget/corridor-capacity limits;
- SDK-free public observation boundaries;
- unrelated dirty and untracked workspace files.

Offline tests, deployed-DLL identity, and live HLDS/ReHLDS acceptance remain separate gates.

## Evidence classification

- F01: Compatibility roam selects the first reachable adjacent link and can repeatedly converge or oscillate.
- F02: follower progress uses Area proximity without proving the current portal/segment was crossed.
- F03: current height and nearest calculations use averaged Z and XY-only distance; pinned CSBot uses interpolated Area surface Z.
- F04: Jump can be rejected by ordinary step-height checks; traversal ownership is not switched for later corridor links; JumpDrop receives a corridor whose terminal contract may not match the active link.
- F05: production clearance is constant and progress detection can be defeated by small lateral jitter.
- F06: site identity and cache identity are retained, but each site currently maps to one nearest candidate Area and profile counters do not distinguish registered sites from evaluated sites during cache hits.
- F07: diagnostic fields mix retained route targets with current local steering values and use names whose units/semantics are not truthful.
- F08: Objective code has an effective-team fallback while Compatibility State consumes raw observation team only.

## Work order

### Phase A - observation contract first

Add failing tests for F07/F08, then:

- introduce one effective-team resolution at the public observation boundary;
- use it in Objective, Compatibility State, and action decisions;
- attach current local target, corridor segment, intent, final buttons, and command sequence to one diagnostic record;
- separate retained goal, current steering target, elapsed stuck duration, and recovery state.

No behavior is changed beyond resolving an unavailable raw team through a fresh managed TeamInfo observation.

### Phase B - geometry and follower safety

Add failing fixtures for F02/F03, then:

- implement ReGameDLL-compatible surface-Z interpolation from the existing Area corner representation;
- make closest-point and vertical distance use the interpolated surface;
- preserve a clearly named XY-only query for callers that explicitly require it;
- prevent corridor index advancement until the current segment/portal completion condition is met;
- preserve final-goal tolerance as a separate condition;
- ensure a stationary observation cannot advance multiple corridor segments.

The change must not replace all targets with Area centers or set tolerance to zero.

### Phase C - traversal link ownership

Add failing fixtures for F04, then:

- evaluate the active corridor link on every link transition;
- start the appropriate Walk/Step/Jump/Drop/Ladder controller for that link;
- evaluate Jump attributes before ordinary step rejection;
- pass a one-link JumpDrop envelope with matching launch and landing terminal Areas;
- keep airborne, landing, and corridor completion states explicit.

No stuck-triggered arbitrary jump or repeated `IN_JUMP` recovery is permitted.

### Phase D - progress and terrain observation

Add failing fixtures for F05, then:

- retain unavailable terrain observations as unavailable;
- add the smallest typed production terrain observation needed by the existing public adapter boundary;
- replace single-frame horizontal jitter detection with a bounded progress window that distinguishes freeze, airborne, ladder, recovery, and genuine lack of progress;
- retain existing path/search backoff and profiler counters.

### Phase E - Goal and objective selection

Add failing fixtures for F01/F06, then:

- separate Compatibility Goal production from adjacent-link route execution;
- compare Goal creation, retention, re-selection, and RNG consumption with the pinned ReGameDLL-CS source before choosing a selector;
- never use actor-ID modulo, forced team spreading, or arbitrary randomization as a compatibility substitute;
- retain site identity across registration, candidate evaluation, selection, and cache hit;
- expose `registered_site_count`, `evaluated_this_window`, `cache_hit`, `selected_site_id`, and per-site rejection reasons;
- evaluate multiple viable Area candidates per site only at required invalidation/selection times, never every Full Update.

## Testing strategy

Every production change follows RED/GREEN:

1. add one minimal failing test or fixture;
2. run it and record the expected failure;
3. implement the smallest fix;
4. run the focused test;
5. run the relevant existing regression tests;
6. run the full x86 build and CTest gate.

Required final gates:

- F01-F08 focused tests;
- existing P02 timing, P06 perception, P07 NAV, P07.6 profiler tests;
- external `ASTRABOT_DE_DUST2_NAV` cases;
- Phase 8 fixture checks;
- FocalSpan fresh and follow-up query;
- source/build/deployed DLL size and SHA-256 identity;
- live ReHLDS intervals only after the matching DLL is loaded and server state is confirmed.

## Explicit non-goals

- no Combat/Aim/Weapon implementation;
- no A* rewrite or async pathfinding;
- no de_dust2 hardcode;
- no compatibility Enhanced tactics;
- no broad source formatting or unrelated manifest cleanup;
- no claim of live acceptance from offline CTest or synthetic fixtures.
