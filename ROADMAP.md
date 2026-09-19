# AstraBot: CSBot Compatibility Baseline -> Enhanced Intelligence Roadmap

## Mission

AstraBot ultimately aims to exceed CSBot in human-like reasoning, tactical adaptation, memory, opponent modeling, route diversity, and teamwork. That work needs a stable control group. Therefore the first milestone is an independently implemented **CSBot behavioral compatibility baseline**.

The baseline is successful when AstraBot can run in a compatibility mode in which the same relevant world inputs, timing schedule, profile, random stream, and game rules produce the same meaningful state transitions and materially equivalent commands/behavior as the pinned ReGameDLL_CS CSBot reference.

“Equivalent” does not mean source-level similarity. It means observable semantics.

## Architecture target

```text
GoldSrc / CS GameDLL / Metamod
            |
            v
   Astra engine adapter
            |
            v
  normalized bot observation
            |
     +------+------+
     |             |
     v             v
Compatibility   Enhanced reasoning
CSBot core      (later phases)
     |             |
     +------+------+
            v
    action / command layer
            |
            v
      fake-client input
```

Compatibility mode must bypass every Astra-only behavior modifier.
Enhanced mode may override high-level choices, but low-level execution should reuse proven movement/combat/action machinery where possible.

## Definition of “CSBot clone baseline”

The baseline is not complete merely because a bot can join, move, buy, fight, plant, or defuse. The following must all be addressed:

1. **Pinned reference**: exact ReGameDLL_CS commit and build options are recorded.
2. **Timing parity**: command/update cadence and ordering match the reference semantics, including lightweight upkeep versus full AI update.
3. **Command parity**: button flags, movement values, view angles, command timing and fake-client execution semantics are equivalent.
4. **RNG parity tooling**: compatibility logic can record/replay a deterministic random tape so call ordering and branch consumption can be compared.
5. **Observation parity**: every GameDLL/player/weapon property CSBot relies on has a documented Astra source, timing, precision and fallback policy.
6. **State/task parity**: task assignment, state transitions, enter/exit side effects, attack overlay, timers and dispositions are covered.
7. **Perception parity**: vision, visibility, hearing/noise, memory, event reactions and enemy selection use the same information boundaries.
8. **Navigation parity**: NAV interpretation, path costs, route selection, tie-breaking, hiding spots, ladders, jumping, posture and stuck recovery are covered.
9. **Combat parity**: aim, reaction, recoil effects, fire cadence, reload, scopes, silencer, weapon switching/pickup and grenades are covered.
10. **Scenario parity**: de/cs/as/es-style objectives supported by the reference are mapped and tested where assets are available.
11. **Economy/profile parity**: buy behavior, BotProfile values, manager/quota behavior, relevant CVARs and chatter/radio influences are mapped.
12. **Differential oracle**: reference traces and Astra traces can be compared mechanically.
13. **Live validation**: representative real-server scenarios pass without relying solely on unit tests.
14. **Enhanced isolation**: adaptive route learning, opponent profiling, team director, tactical planner or other Astra intelligence cannot affect compatibility mode.
15. **Frozen baseline**: a documented release/tag establishes the reference point before enhanced intelligence resumes.

## Phase order

### Foundation and control

- **P00** Freeze the reference and audit current AstraBot.
- **P01** Establish the compatibility/enhanced mode boundary.
- **P02** Reproduce runtime timing and command pipeline semantics.
- **P03** Make randomness traceable and deterministic.
- **P04** Build the GameDLL observation/state parity matrix.

### Behavioral parity

- **P05** State machine, tasks, dispositions and decision flow.
- **P06** Perception: vision, sound, events and short-term knowledge.
- **P07** NAV, pathfinding, movement, ladders, jumping and stuck handling.
- **P08** Combat, aim, weapons, reload, scopes, silencer and grenades.
- **P09** Objectives, map scenarios, following, radio and teamwork.
- **P10** Buying, profiles, bot manager/quota, CVARs and chatter.

### Proof and release

- **P11** Differential oracle, golden traces and replay comparison.
- **P12** Live parity matrix, regression gate and compatibility baseline release.

### After the baseline

- **P13** Re-enable/build Astra enhanced intelligence above the compatibility baseline.

## Dependency graph

```text
P00 -> P01 -> P02 -> P03 -> P04
                         |
                         +--> P05 --> P06 --> P07 --> P08 --> P09 --> P10
                                                        \              /
                                                         +----> P11 <--+
                                                                |
                                                                v
                                                               P12
                                                                |
                                                                v
                                                               P13
```

P05-P10 may expose missing observation fields in P04. If so, extend P04 artifacts narrowly; do not restart prior phases or redesign unrelated layers.

## Compatibility mode policy

Compatibility mode is a **behavioral contract**, not a debug flag.

In compatibility mode:

- Astra-only learning/adaptation must not affect decisions.
- No opponent-profile persistence may influence a decision unless the reference has an equivalent input.
- No adaptive route weighting may alter reference route selection.
- No team-director optimization may override reference team/task decisions.
- No additional perception source may reveal information the reference bot could not know.
- No aim or reaction enhancement may exceed reference profile/skill behavior.
- No quality-of-life “fix” may silently change reference quirks.

Enhanced mode may use these features, but compatibility tests must remain permanently runnable.

## Working method for Codex

At the beginning of every plan:

1. Read the current plan and `docs/STATUS.md` only.
2. Inspect current AstraBot files related to the plan before editing.
3. Inspect corresponding ReGameDLL reference files.
4. Write/update a small mapping table: reference symbol/behavior -> Astra location -> status.
5. Identify existing Astra behavior that is already correct and leave it alone.
6. Implement the smallest coherent delta.
7. Run targeted tests.
8. Run the repository build/smoke gate.
9. Update status/matrix and note unresolved mismatches explicitly.
10. Commit and stop.

Do not solve future plans early unless a minimal interface is required by the active plan.

## Required persistent artifacts inside AstraBot

P00 should create these under a stable directory such as `docs/parity/`:

- `REFERENCE.md` — pinned ReGameDLL commit/options/environment.
- `SOURCE_MAP.md` — reference files/symbols mapped to Astra implementation.
- `PARITY_MATRIX.md` — behavior-by-behavior status.
- `OBSERVATION_MATRIX.md` — created/expanded by P04.
- `STATUS.md` — current phase, last commit, blockers, next entry point.
- `KNOWN_DEVIATIONS.md` — only intentional or technically unavoidable deviations, never a dumping ground for unfinished work.
- `TRACE_SCHEMA.md` — trace fields used by P11.

These files are specifically intended to survive context compression and new Codex sessions.

## Stop conditions

Stop a plan and record a blocker rather than inventing behavior when:

- the ReGameDLL behavior cannot be determined from source and reproducible execution;
- a required GameDLL private field cannot be observed with sufficient timing/precision from the current plugin boundary;
- a reference build option materially changes the target and has not been pinned;
- a NAV/map/test asset required for parity is unavailable;
- a change would require deleting or rewriting an unrelated subsystem merely to make one fixture pass.

A blocker is useful data. A guessed implementation is not.

## Baseline release gate

Before declaring `CSBot Baseline 1.0`:

- all P00-P12 acceptance criteria are complete or a deviation is explicitly approved by the project owner;
- compatibility mode passes its deterministic differential suite;
- representative live server tests are complete;
- enhanced-only features are proven inert in compatibility mode;
- no parity test depends on unspecified wall-clock timing;
- no known crash, fake-client lifecycle failure, spawn failure or Metamod load failure remains;
- the pinned ReGameDLL reference and Astra baseline commit are recorded together.

Only after this gate should P13 become normal development work.
