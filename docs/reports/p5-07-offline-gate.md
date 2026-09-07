# P5-07 — Scenario replay and Phase 5 offline gate

Status: 実装・offline gate complete after canonical verification. Real-device acceptance is not performed.

## Combat contracts

The replay uses validated `CombatInput`, `CombatDecision`, `WeaponSnapshot`,
and `BotCommand` value contracts. Invalid decisions or navigation commands are
rejected deterministically before command transport.

## Target selection

The replay begins with deterministic target acquisition from the existing P5
target-selection contract and retains target identity, map, round, tick, and
generation lineage.

## Reaction and aim

The replay covers the reaction delay, aim gate, and the transition into the
fire authorization line. Repeated visual confirmations do not restart an
already active reaction interval.

## DirectFire authorization

Fire is emitted only after alive state, selected-target identity, current
visual confirmation, context lineage, reaction completion, aim acceptance, and
usable active-weapon checks pass. Visibility loss and target replacement clear
the attack lifecycle.

## Fire cadence

The scenario covers tap, burst, and full-auto cadence, including cooldown
crossing, bounded burst pauses, duplicate ticks, and repeated frame intervals
of 8, 16, and 100 ms.

## Reload and weapon switching

The scenario covers reload authorization, reload suppression while the weapon is
already reloading or unusable, weapon-switch requests, and adapter-side
selection acceptance/rejection. An unavailable or rejected adapter handler
cannot result in a movement dispatch.

## Command composition

Navigation movement and non-combat buttons coexist with combat view and
attack/reload ownership. A switch request is represented by `weaponSelect` and
does not leak combat button ownership into navigation fields.

## Scenario replay

`tests/combat_replay_tests.cpp` replays acquire → reaction → aim → DirectFire →
cadence pause/full-auto → visibility loss. It also covers reload, switching,
target replacement, ally filtering, death, map/round boundaries, 1/8/16 bot
loads, and duplicate ticks. `tests/combat_command_tests.cpp` covers command
composition and fail-closed behavior.

## Phase 5 Offline

Focused evidence:

- portable command-composition test: passed;
- portable combat replay test: passed;
- portable combat contract/adapter/trace test: passed;
- Metamod x86 movement test: passed;
- Metamod x86 adapter-entry test: passed.

The canonical `tools/verify-canonical.ps1 -Profile All` run is the required
project gate for this tree. Project-wide `Finish` is not declared, and no
HLDS/ReHLDS live validation has been started.
