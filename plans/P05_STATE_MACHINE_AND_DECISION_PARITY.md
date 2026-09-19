# P05 — State Machine, Tasks and Decision-Flow Parity

## Goal

Make the compatibility decision flow reproduce CSBot state/task semantics even if AstraBot uses a different internal class architecture.

Internal structure may differ. Observable transition semantics may not.

## Reference focus

- `cs_bot_statemachine.cpp`
- `cs_bot_update.cpp`
- `cs_bot.h`
- all files under `dlls/bot/states/`

Pinned reference states currently include the functional groups represented by Attack, Buy, DefuseBomb, EscapeFromBomb, FetchBomb, Follow, Hide, Hunt, Idle, InvestigateNoise, MoveTo, PlantBomb and UseEntity. Verify exact contents from the pinned SHA.

## Tasks

1. Build/complete a state-and-task table in `PARITY_MATRIX.md`.
2. For every reference state record:
   - entry conditions;
   - OnEnter side effects;
   - OnUpdate decision order;
   - OnExit side effects;
   - timers initialized/cleared;
   - task/disposition changes;
   - path/movement changes;
   - next-state transitions and their ordering.
3. Reproduce `SetState` semantics, including transition ordering and timestamp behavior.
4. Reproduce the special attack overlay semantics: attacking is not merely another ordinary state if the reference treats it as an overlay that temporarily owns updates.
5. Verify how state changes interact with following, hiding, objectives, reload/grenade waits and stuck handling.
6. Keep Astra enhanced planner/team director out of compatibility decisions.
7. If current Astra architecture uses planner actions rather than state classes, create an explicit compatibility mapping instead of forcing a wholesale architectural rewrite.
8. Add transition-trace tests. Each fixture should compare ordered state/task transitions and relevant side effects, not only the final state.
9. Cover interruption cases: enemy appears, enemy lost, objective changes, round end, death, leader invalidation and path failure.

## Acceptance criteria

- every pinned reference state has a mapped Astra behavior;
- attack overlay behavior is represented and tested;
- transition ordering/side effects are deterministic in fixtures;
- no enhanced-only decision overrides a compatibility transition;
- no major reference state remains `UNMAPPED` or `MISSING` without a documented blocker;
- build/smoke gate passes.

## Commit

Suggested message:

`feat(parity): align compatibility state and task transitions with CSBot`

Update STATUS/matrix, mark P05 complete, set P06 next, commit, stop.
