# P04 — GameDLL Observation and Private-State Parity

## Goal

Document and implement the exact information boundary available to compatibility logic.

This is the central risk of separating CSBot from GameDLL: the original bot can directly read player/weapon/GameRules private state, while a Metamod plugin may otherwise receive delayed or incomplete approximations.

## Required artifact

Create `docs/parity/OBSERVATION_MATRIX.md`.

Recommended columns:

| Reference datum | Reference access | Astra source | Timing | Precision | Status | Consumers | Notes |
|---|---|---|---|---|---|---|---|

Status should distinguish `EXACT`, `EXACT-DELAYED`, `DERIVED`, `APPROXIMATE`, `UNAVAILABLE`.

## Tasks

1. From the pinned CSBot reference, enumerate direct/transitive reads that can change decisions. Include player, weapon, world, GameRules, NAV and objective state.
2. At minimum investigate:
   - origin/velocity/angles/FOV/posture/flags;
   - team, health, armor, alive/dead/spawn/frozen state;
   - active weapon identity;
   - clip and reserve ammo;
   - reload state and weapon timing;
   - weapon-specific state such as scope/silencer/accuracy where referenced;
   - bomb carrier/dropped/planted/defuse state and timers;
   - hostage/VIP/escape/scenario state;
   - teammate/enemy entity identity and team relationships;
   - round state and important GameRules timers;
   - NAV area/ladder/hiding-area data.
3. For each datum identify how Astra obtains it:
   - engine/public entity state;
   - Metamod hook;
   - ReGameDLL API if intentionally supported;
   - pdata/private-data offset access;
   - user message/event observation;
   - derived inference.
4. Prefer same-tick exact state over message-based inference when compatibility requires it.
5. Centralize observations behind existing world-model/adapter interfaces so higher-level compatibility code does not scatter raw pdata/hook logic throughout the project.
6. Record synchronization point: when each observation becomes valid relative to the compatibility think tick.
7. For any `APPROXIMATE` or `UNAVAILABLE` datum, create a blocker entry. Do not silently substitute a heuristic.
8. Add focused tests for snapshot freshness and reset across spawn/round/death/map changes.
9. Check that entity indices/pointers are not retained past lifecycle boundaries.

## Decision rule

If exact parity requires a field unavailable through generic Metamod APIs, it is acceptable to add a narrowly isolated ReGameDLL-specific adapter or documented pdata access layer if that matches the project's deployment target. Do not contaminate the behavior core with GameDLL-specific memory layout assumptions.

## Acceptance criteria

- all behaviorally relevant reference data has a matrix row;
- no compatibility decision reads undocumented engine state directly;
- stale-state lifetime rules are tested;
- all approximate/unavailable observations are explicit blockers/deviations, not hidden heuristics;
- build/smoke gate passes.

## Commit

Suggested message:

`feat(parity): formalize exact CSBot observation boundary`

Update STATUS/matrix, mark P04 complete, set P05 next, commit, stop.
