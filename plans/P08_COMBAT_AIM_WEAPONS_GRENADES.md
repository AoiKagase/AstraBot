# P08 — Combat, Aim, Weapons and Grenades Parity

## Goal

Reproduce CSBot combat behavior, including the deliberately imperfect timing/accuracy that comes from profile skill and reference heuristics.

Do not make compatibility aim “better.”

## Reference focus

- `cs_bot_weapon.cpp`
- combat/aim portions of `cs_bot_update.cpp`
- Attack state
- base command/aim helpers
- weapon private state read by CSBot

## Tasks

1. Map target/aim point selection and visible body-region preferences.
2. Map reaction delay and aim-update cadence relative to P02.
3. Map skill/profile influence on aim error, tracking and prediction.
4. Map recoil/punch/weapon accuracy effects actually used by the reference.
5. Map attack start/stop conditions and fire cadence/burst behavior.
6. Map range-dependent weapon use/selection.
7. Map reload start/interrupt/movement interaction and empty-weapon behavior.
8. Map scope decisions/FOV usage and sniper timing.
9. Map silencer/secondary-attack decisions and required weapon-state observations.
10. Map weapon switching, pickup and dropped-weapon decisions.
11. Map grenade selection, aiming, toss timing, wait-to-throw behavior and movement suppression.
12. Ensure all required private weapon state is backed by P04 exact observations. Do not fake `accuracy`, reload or silencer state from delayed HUD messages if the reference needs same-tick state.
13. Add golden fixtures parameterized by weapon/profile/range/visibility/RNG tape.
14. Compare **ordered commands over time**, not only hit rate.
15. Add live cases for pistol, rifle, SMG, shotgun, sniper, knife and grenades as practical.

## Acceptance criteria

- compatibility aim/fire output is deterministic under a fixed observation stream and RNG tape;
- profile/skill changes produce reference-like behavior directions and trace parity for covered fixtures;
- scope/reload/silencer/grenade paths are represented;
- compatibility mode has no Astra aim assistance beyond reference behavior;
- build/smoke gate passes.

## Commit

Suggested message:

`feat(parity): align compatibility combat and weapon behavior with CSBot`

Update STATUS/matrix, mark P08 complete, set P09 next, commit, stop.
