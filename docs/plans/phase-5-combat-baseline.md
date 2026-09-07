# Phase 5 plan — Combat Baseline

Status: planning only, based on the P4-complete `main` at `0bf9e72`.
This document defines the next implementation phase; it does not implement
combat source, tests, adapter hooks, or a public API.  P4 offline completion is
not project-wide Finish, and real HLDS/ReHLDS combat acceptance remains a
post-Finish activity.

## Goal and authority

Phase 5 adds the first bounded combat capability: select a known opponent,
turn toward the target, fire when the information and weapon state permit it,
reload when necessary, and switch to an available weapon when the active one
cannot perform the requested action.

The [architecture decision](../architecture.md) and the completed
[Phase 4 perception and World Model plan](phase-4-perception-world-model.md)
are authoritative.  Combat consumes the read-only, observer-specific
`world::WorldSnapshot`; it never reads hidden engine state, raw entities, or a
privileged current position for an opponent.

Combat remains independent from the Action Planner, Tactical Planner, Team
Director, Experience persistence, and navigation goal selection.  It produces
a bounded combat decision and command input for the existing host command
path.  It does not choose where a Bot should move or which strategic intent it
should pursue.

## Information and command boundary

The future implementation uses SDK-free value contracts in `astrabot::core`
and an adapter-owned conversion layer.  The exact source files are selected
when the first slice is implemented, but the public shape is fixed here.

### Combat input

`combat::CombatInput` is an immutable frame value containing:

- the map, round, tick, simulation time, `PlayerId`, and `BotAgentId`;
- the Bot's alive state, team relation, eye position, current view angles,
  and any other self-owned pose values needed to calculate aim;
- a read-only `world::WorldSnapshot` for this observer;
- `combat::WeaponSnapshot`, including the active weapon, owned/available
  weapons, clip and reserve ammunition, reload state, primary-attack timing,
  and switch/reload capability; and
- bounded difficulty settings for reaction time, observation error, prediction
  error, aim noise, and decision quality.

The adapter owns entity pointers, engine weapon objects, message decoding, and
availability validation.  Core receives values only.  Missing, stale, invalid,
or non-finite input is an explicit failure and produces a safe non-combat
decision; defaults must not manufacture ammunition, ownership, visibility, or
cooldowns.

### Combat decision

`combat::CombatDecision` is a value result containing:

- one action: `NoOp`, `Track`, `Fire`, `Reload`, or `SwitchWeapon`;
- an optional generation-safe target `PlayerId`;
- an optional `combat::FireMode` when the action is `Fire`;
- bounded view angles and the command button mask;
- an optional selected weapon value;
- the source, age, and confidence of the knowledge used for the decision;
- a typed reason for acceptance, suppression, or rejection; and
- the input tick and a bounded validity/deadline value.

At most one combat action is emitted per input tick.  A decision is submitted
through the existing `(PlayerId, TickId, BotCommand)` host path.  The existing
`Button::Attack` and `Button::Reload` values are reused.  Weapon switching is
represented by a value-level selection request and is translated by the
adapter; no new DLL export or AMXX/ReAPI public surface is added.

### Fire-mode extension point

`combat::FireMode` is a small SDK-independent value enum carried by a future
fire decision so the action contract can distinguish the reason for firing:

- `DirectFire`: the only mode implemented by P5; it requires a current valid
  direct visual observation of the same target.
- `Wallbang`: reserved for a future belief-based decision.  It may use only
  observer-owned `VisualMemory` / `EnemyBelief`, last-known position,
  confidence, age, penetration possibility, weapon penetration, friendly-fire
  risk, and expected damage as value evidence.  It must not read hidden engine
  positions, and anonymous sound alone must never authorize a player-specific
  wallbang.
- `SuppressiveFire`: reserved for a future non-player-specific suppression
  policy; it is not implemented by P5.

P5 must emit `DirectFire` for every accepted `Fire` decision and reject the two
reserved modes as unsupported.  This extension point adds no penetration,
material, wall-geometry, or damage system ahead of the phase that needs it.

### Knowledge policy

- `ObservationSource::Vision` can identify an opponent when its identity,
  map, round, generation, age, and relation are valid.
- `ObservationSource::TeamReport` can supply a candidate for tracking or aim
  preparation, but never upgrades a report into direct current visibility.
- `ObservationSource::Sound` is anonymous.  It can influence a future
  investigation decision, but it cannot identify a player or authorize aim at
  or fire on a specific player, including a future `Wallbang` decision.
- `Relation::Self`, `Ally`, and `Unknown` are never valid fire targets.
  Unknown team state fails closed rather than being treated as hostile.
- Stale visual memory and team reports may support `Track`, subject to their
  recorded age and confidence, but `Fire` requires a current valid visual
  observation for the same target and frame lineage.
- A map change, round change, disconnect, generation reuse, target retirement,
  or invalid World Model snapshot invalidates the decision and cancels any
  pending combat action.

All target ordering, tie breaks, cooldown comparisons, and action suppression
are deterministic.  Aim noise uses a named, per-agent seeded stream and must
not perturb future planner or navigation randomness.

## Planned implementation slices

The following numbered items are the implementation order.  Unnumbered
checklists inside an item are independent reviewable commits; they do not
create additional P5 task numbers.

### P5-01 — Combat contracts and weapon observation

- Define `WeaponId`, `WeaponSnapshot`, `CombatInput`, `CombatDecision`, action
  kinds, `FireMode` (`DirectFire`, `Wallbang`, `SuppressiveFire`), reason
  values, and bounded difficulty settings in SDK-free Core.  Only
  `DirectFire` is accepted by P5.
- Define the adapter capability/value conversion for active weapon, inventory,
  ammunition, reload state, attack timing, and switch availability.
- Reject stale actor/map/round/tick values, invalid weapon state, non-finite
  pose values, and impossible ammunition values without emitting attack input.
- Preserve the existing command validation and host lifecycle contracts.

### P5-02 — Target detection and deterministic selection

- Read only the observer's `WorldSnapshot` and filter candidates by map,
  round, generation, relation, age, confidence, and source.
- Prefer valid direct vision, then eligible team reports for non-firing
  tracking.  Never resolve an anonymous sound to a player.
- Use a fixed deterministic ordering: valid direct vision, higher confidence,
  newer original observation time, shorter angular error, then `PlayerId`.
- Return `NoOp` with a typed reason when no eligible candidate remains.

### P5-03 — Aim and reaction control

- Calculate finite pitch/yaw from the Bot's own eye position to the known target
  point, normalize yaw through the shortest signed path, and clamp to the
  existing view-angle limits.
- Apply configured reaction delay, observation error, prediction error, and
  aim noise as bounded deterministic values.  Do not infer hidden target
  velocity or current position.
- Emit `Track` while the target is stale, reported, outside the fire gate, or
  still inside the configured reaction window.
- Keep aim calculation pure and independently replayable without engine calls.

### P5-04 — Fire gate and attack lifecycle

- Permit `Fire(DirectFire)` only for an alive Bot with a valid current visual
  target, matching map/round/generation, usable active weapon, available
  ammunition, and an elapsed primary-attack cooldown.  `Wallbang` and
  `SuppressiveFire` remain explicitly unsupported and cannot weaken this gate.
- Suppress fire for allies, unknown relations, stale-only knowledge, invalid
  visibility provenance, reloading, empty clip, invalid weapon state, and
  rejected or stale host ticks.
- Define deterministic attack button edge/hold behavior and prevent duplicate
  trigger events caused by repeated input frames.
- Record the reason for every fire suppression and every accepted command.

### P5-05 — Reload and weapon switching

- Request `Reload` when the active weapon has an empty or configured-low clip,
  reserve ammunition, and a valid reload capability.
- During reload, suppress fire and switching until the adapter reports a valid
  completion or failure state; never assume completion from elapsed time alone.
- Select a weapon only from the adapter-reported owned and available set, with
  deterministic priority and explicit rejection when no usable weapon exists.
- Prefer a usable weapon switch over an impossible fire request, while keeping
  action count bounded to one combat action per tick.

### P5-06 — Adapter, host, and observability integration

- Convert standard CS weapon and player observations into the value contracts
  without leaking `edict_t`, `entvars_t`, GameDLL private data, ReAPI types, or
  raw message buffers into Core.
- Submit only validated `BotCommand` values through the existing lifecycle and
  per-player generation guards.
- Add structured combat traces containing target/source/age, action, reason,
  weapon state, cooldown result, and command acceptance; never log raw
  pointers or hidden target state.
- Keep movement and navigation ownership unchanged.  Combat may set view and
  combat buttons but may not replace the movement controller's command.

### P5-07 — Offline combat gate

- Add deterministic Core/replay and fake-adapter evidence for acquisition,
  target loss, stale memory, team reports, anonymous sound, aim limits,
  reaction delay, aim noise, fire cooldown, empty clip, reload, weapon switch,
  invalid input, and rejected host submission.
- Exercise 1, 8, and 16 Bot loads with 8, 16, and 100ms frame intervals.
- Cover map/round changes, disconnects, slot and generation reuse, observer
  separation, queue/budget limits, repeated ticks, and movement coexistence.
- Register all gating tests with CTest and retain existing P4 perception,
  World Model, navigation, and host regression coverage.
- Report offline implementation and applicable verification separately from
  post-Finish live combat acceptance.

## Acceptance criteria

P5 implementation is ready for its offline gate only when:

1. Every decision is derived from immutable value inputs and has a typed reason.
2. P5 `DirectFire` requires current valid direct vision; no anonymous sound,
   stale-only memory, unknown team, or ally state can authorize a player-
   specific shot.  Reserved future modes are not accepted by P5.
3. Angles, timing, ammo, reload, switching, and button transitions are finite,
   bounded, deterministic, and generation-safe.
4. The adapter is the only owner of engine/weapon objects and no new binary
   export is required.
5. The replay and load matrix passes without unbounded queue growth, cross-Bot
   state leakage, or movement regression.
6. The report lists unsupported engine event paths and live checks instead of
   treating synthetic evidence as real-server acceptance.

## Explicitly out of scope

- Action Planner and Tactical Planner decisions.
- Navigation goals, movement replacement, path cost, or team role selection.
- Grenade selection/throwing, bomb objectives, damage learning, recoil model,
  hit prediction, or persistent Experience storage.
- Wallbang penetration calculation, material/geometry classification, and
  suppressive-fire policy; these are future `FireMode` consumers only.
- Hidden enemy positions, engine-complete target state, or automatic sound
  source identification.
- AMXX/ReAPI public API additions and third-party GameDLL private integration.
- Real HLDS/ReHLDS, map, weapon-event, and performance acceptance before the
  project-wide Finish decision.

## Common workflow and verification

Each future P5 implementation slice starts from the current `main`, uses a
dedicated `codex/p5NN-<purpose>` worktree, confirms FocalSpan and graph context,
adds focused tests before implementation, and stages only intended paths.
After all applicable slices, run the Windows VS 2026/NMake x86 portable and
Metamod Debug gates, the Linux GCC `-m32` Debug gate, and the Metamod Release
PE32/export check from `AGENTS.md`.  Merge into `main` only with
`git merge --ff-only`, then rerun the relevant gates on the merged main.

This planning commit itself changes documentation only.  Its verification is
link/reference review, FocalSpan refresh/query, `git diff --check`, and the
post-merge regression build/test requested for the project workflow.
