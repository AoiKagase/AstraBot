# P09 — Objectives, Scenario Logic, Teamwork and Radio Parity

## Goal

Reproduce scenario-specific behavior and reference team interactions rather than only deathmatch-like fighting.

## Reference focus

- bomb-related states: FetchBomb, PlantBomb, DefuseBomb, EscapeFromBomb
- UseEntity / Follow / Hide / Hunt / Idle as used by scenarios
- `cs_bot_radio.cpp`
- `cs_gamestate.*`
- scenario/event portions of `cs_bot_update.cpp`, `cs_bot_event.cpp`, `cs_bot_manager.cpp`

## Scenario coverage

Audit all reference-supported Counter-Strike scenario families, including at least:

- `de_` bomb/defuse;
- `cs_` hostage rescue;
- `as_` VIP/assassination;
- `es_` escape, if supported/available in the target stack.

Do not assume a scenario is unsupported simply because modern public servers rarely use it.

## Tasks

1. Map bomb ownership, dropped bomb search/fetch, carrier decisions, site approach, planting, planted-bomb reactions, defusing and escape-from-blast logic.
2. Map defuse-kit/timer decisions and interruption cases.
3. Map hostage discovery/use/escort/wait/rescue behavior and relevant team task changes.
4. Map VIP protection/escape logic where present.
5. Map escape-zone scenario logic where present.
6. Map follow/leader behavior and automatic follow transitions.
7. Map radio command handling, acknowledgement and behavior-changing team commands.
8. Map team offense/defense role decisions and any manager-level rush/strategy state.
9. Ensure Astra TeamDirector/TacticalPlanner cannot improve compatibility decisions.
10. Add deterministic scenario fixtures for state transitions and action outputs.
11. Add live scenario tests on representative stock maps where assets exist. Record unavailable legacy maps rather than silently skipping the scenario domain.

## Acceptance criteria

- de/cs and all available reference-supported scenario behaviors are mapped;
- team/radio inputs affect compatibility behavior only when the reference would react;
- advanced Astra team coordination is inert in compatibility mode;
- objective state resets correctly across round/map lifecycle;
- build/smoke gate passes.

## Commit

Suggested message:

`feat(parity): align compatibility objective and team behavior with CSBot`

Update STATUS/matrix, mark P09 complete, set P10 next, commit, stop.
