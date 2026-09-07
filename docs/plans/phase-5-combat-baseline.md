# Phase 5 plan — Combat Baseline

Status: P5-01, P5-02, and P5-03 are implemented; P5-04 is in progress;
P5-05 through P5-07 are planned.  This document is the single authoritative
Phase 5 plan.  Phase 5 completion is not project-wide Finish, and real
HLDS/ReHLDS combat acceptance remains a post-Finish activity.

## Goal and authority

Phase 5 adds the first bounded combat capability: select a known opponent,
turn toward the target, react, fire when the information and weapon state
permit it, control fire cadence, reload when necessary, and switch to an
available weapon when the active one cannot perform the requested action.

The [architecture decision](../architecture.md) and the completed
[Phase 4 perception and World Model plan](phase-4-perception-world-model.md)
are authoritative. Combat consumes the read-only, observer-specific
`world::WorldSnapshot`; it never reads hidden engine state, raw entities, or a
privileged current position for an opponent.

The objective is a deterministic, bounded human-like baseline rather than a
strong aim bot. Target selection, aim, reaction, fire authorization, cadence,
reload, weapon handling, and command composition remain separable so each can
be tested and audited independently.

Combat remains independent from the Action Planner, Tactical Planner, Team
Director, Experience persistence, and navigation goal selection. It produces a
bounded combat decision and command input for the existing host command path;
it does not choose where a Bot should move or which strategic intent it should
pursue.

## Core rules

- Combat Core reads only the World Model, current self state, and
  `WeaponSnapshot`.
- `DirectFire` requires current direct vision of the same target. Visual memory
  alone cannot authorize a shot.
- Anonymous sound cannot be resolved to a player or authorize player-specific
  aim or fire.
- `Track`, `Aim`, and `Fire` remain separate decisions.
- Fire is represented by bounded tap, burst, or full-auto behavior, never by an
  unbounded persistent attack flag.
- All ordering, timing, noise, suppression, and replay behavior is
  deterministic.
- `Wallbang` and `SuppressiveFire` are extension points only; P5 does not
  implement them.

## Information and command boundary

The implementation uses SDK-free value contracts in `astrabot::core` and an
adapter-owned conversion layer. Engine and weapon objects remain outside
Core.

### Combat input

`combat::CombatInput` is an immutable frame value containing:

- map, round, tick, simulation time, `PlayerId`, and `BotAgentId`;
- the Bot's alive state, team relation, eye position, current view angles, and
  other self-owned pose values needed for aim;
- a read-only `world::WorldSnapshot` for this observer;
- `combat::WeaponSnapshot`, including active and owned/available weapons,
  clip and reserve ammunition, reload state, primary-attack timing, and
  switch/reload capability; and
- bounded difficulty settings for reaction time, observation error, prediction
  error, aim noise, and decision quality.

The adapter owns entity pointers, engine weapon objects, message decoding, and
availability validation. Core receives values only. Missing, stale, invalid,
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
- the source, age, and confidence of the knowledge used;
- a typed reason for acceptance, suppression, or rejection; and
- the input tick and a bounded validity/deadline value.

At most one combat action is emitted per input tick. A decision is submitted
through the existing `(PlayerId, TickId, BotCommand)` host path. Existing
`Button::Attack` and `Button::Reload` values are reused. Weapon switching is a
value-level selection request translated by the adapter; no new DLL export or
AMXX/ReAPI public surface is added.

### Fire-mode extension point

`combat::FireMode` distinguishes the reason for firing:

- `DirectFire`: the only mode implemented by P5; it requires a current valid
  direct visual observation of the same target.
- `Wallbang`: reserved for a future belief-based decision using only
  observer-owned `VisualMemory` / `EnemyBelief`, last-known position,
  confidence, age, penetration possibility, weapon penetration, friendly-fire
  risk, and expected damage. It must not read hidden engine positions, and
  anonymous sound alone must never authorize a player-specific wallbang.
- `SuppressiveFire`: reserved for a future non-player-specific suppression
  policy; it is not implemented by P5.

P5 emits `DirectFire` for every accepted `Fire` decision and rejects the two
reserved modes as unsupported. No penetration, material, wall geometry, or
damage system is added ahead of the phase that needs it.

### Knowledge policy

- `ObservationSource::Vision` can identify an opponent when identity, map,
  round, generation, age, and relation are valid.
- `ObservationSource::TeamReport` can supply a candidate for tracking or aim
  preparation, but never upgrades a report into current direct visibility.
- `ObservationSource::Sound` is anonymous. It may influence future
  investigation, but cannot identify or authorize aim at a specific player.
- `Relation::Self`, `Ally`, and `Unknown` are never valid fire targets.
  Unknown team state fails closed rather than being treated as hostile.
- Stale visual memory and team reports may support `Track`, subject to age and
  confidence, but `Fire` requires current valid vision for the same target and
  frame lineage.
- A map change, round change, disconnect, generation reuse, target retirement,
  or invalid World Model snapshot invalidates the decision and cancels pending
  combat action.

Aim noise uses a named, per-agent seeded stream and must not perturb future
planner or navigation randomness.

## Implementation slices

The following numbered items are the implementation order. Unnumbered
checklists are reviewable work within an item and do not create additional P5
task numbers.

### P5-01 — Combat contracts and weapon observation (Implemented)

- Define `WeaponId`, `WeaponSnapshot`, `CombatInput`, `CombatDecision`, action
  kinds, `FireMode`, typed reasons, and bounded difficulty settings in SDK-free
  Core. Only `DirectFire` is accepted by P5.
- Define adapter capability/value conversion for active weapon, inventory,
  ammunition, reload state, attack timing, and switch availability.
- Reject stale actor/map/round/tick values, invalid weapon state, non-finite
  pose values, and impossible ammunition values without emitting attack input.
- Preserve existing command validation and host lifecycle contracts.

### P5-02 — Target detection and deterministic selection (Implemented)

- Read only the observer's `WorldSnapshot` and filter candidates by map, round,
  generation, relation, age, confidence, and source.
- Prefer valid direct vision, then eligible team reports for non-firing
  tracking. Never resolve anonymous sound to a player.
- Use fixed ordering: valid direct vision, higher confidence, newer original
  observation time, shorter angular error, then `PlayerId`.
- Return `NoOp` with a typed reason when no eligible candidate remains.
- Do not fire from this slice; target selection only produces `Track` or
  `NoOp`.

### P5-03 — Aim and reaction control (Implemented)

- Calculate finite pitch/yaw from the Bot's own eye position to the known target
  point, normalize yaw through the shortest signed path, and clamp to existing
  view-angle limits.
- Apply bounded deterministic reaction delay, observation error, prediction
  error, and aim noise without inferring hidden target velocity or position.
- Keep reaction start time distinct from latest observation time; continuous
  visibility must not reset the reaction delay indefinitely.
- Emit `Track` while the target is stale, reported, outside the fire gate, or
  inside the configured reaction window.
- Keep aim calculation pure and independently replayable without engine calls.
- Preserve the fire cadence contract (`Tap`, `Burst`, `FullAuto`) for the
  later fire lifecycle; aim alone never emits an attack button.

### P5-04 — DirectFire authorization and attack lifecycle (In progress)

Permit `Fire(DirectFire)` only when all of the following hold:

```text
alive Bot
AND selected target
AND current direct visual confirmation of the same target
AND matching map/round/generation/frame lineage
AND reaction complete
AND aim acceptable
AND usable active weapon
AND ammunition available
AND weapon ready
AND primary-attack cooldown elapsed
AND not reloading
AND friendly-fire gate passed
```

- `ObservationSource::Vision` alone is insufficient; the target must still be
  directly visible in the current decision frame.
- Suppress fire for allies, unknown relations, stale-only knowledge, invalid
  visibility provenance, empty clips, invalid weapon state, and rejected or
  stale host ticks.
- Keep `Wallbang` and `SuppressiveFire` explicitly unsupported; they cannot
  weaken the DirectFire gate.
- Define deterministic attack-button edge/hold behavior and prevent duplicate
  trigger events from repeated input frames.
- Record a typed reason for every suppression and accepted command.

Required focused cases are current visibility, lost visibility, stale memory,
incomplete reaction, no ammunition, cooldown, invalid weapon state, ally
crossing the fire line, and repeated ticks.

### P5-05 — Fire cadence, reload, and weapon switching (Planned)

#### Fire cadence

The baseline policy is bounded and reevaluated after each burst or pause:

| Weapon class | Long range | Medium range | Close range |
| --- | --- | --- | --- |
| Rifle | Tap | Short burst | Burst or full-auto |
| SMG | — | Longer burst | Burst or full-auto |
| Pistol | Controlled single shots | Controlled single shots | Controlled single shots |
| Sniper | Single shot, then reacquire/cooldown | Single shot, then reacquire/cooldown | Single shot, then reacquire/cooldown |

- Cadence uses weapon class, target distance, aim error, ammunition, current
  visibility, and difficulty.
- A burst is followed by a short pause and reevaluation; attack is never held
  forever.
- Visibility loss and target replacement interrupt a stale fire plan.
- P5 does not implement advanced recoil modeling or spray-pattern
  optimization.

#### Reload and weapon handling

- Request `Reload` for an empty or configured-low clip when reserve ammunition
  and reload capability are available.
- During reload, suppress fire and switching until the adapter reports valid
  completion or failure; never assume completion from elapsed time alone.
- Avoid unnecessary reload while a visible threat remains and ammunition is
  still usable.
- Select a weapon only from the adapter-reported owned and available set, with
  deterministic priority and explicit rejection when none is usable.
- Prefer a usable weapon switch over an impossible fire request while keeping
  one combat action per input tick.
- Defer purchase AI, pickup planning, and complete weapon-preference AI.

Required focused cases are burst count, cooldown, deterministic cadence,
visibility interruption, empty clip, zero reserve, threat-time reload
suppression, stale weapon state, and unusable active-weapon switching.

### P5-06 — Command composition, adapter, host, and observability (Planned)

- Convert standard CS weapon and player observations into value contracts
  without leaking `edict_t`, `entvars_t`, GameDLL private data, ReAPI types, or
  raw message buffers into Core.
- Submit only validated `BotCommand` values through the existing lifecycle and
  per-player generation guards.
- Compose combat-owned view, attack, reload, and weapon-selection fields with
  navigation-owned movement fields. Combat must not replace the movement
  controller's command or write navigation state directly.
- Clear stale attack/reload state on target replacement, death, disconnect,
  map/round transition, and generation reuse.
- Add structured combat traces containing target/source/age, action, reason,
  weapon state, cooldown result, and command acceptance; never log raw pointers
  or hidden target state.

### P5-07 — Scenario tests and Phase 5 offline gate (Planned)

The minimum portable/fake-host scenario is:

```text
enemy appears
→ target acquire
→ reaction delay
→ aim
→ DirectFire
→ burst
→ pause and reevaluate
→ enemy hides
→ cease fire
```

Additional scenarios cover enemy reacquisition, target replacement, ammunition
depletion, reload, sniper single-shot behavior, close-range full-auto,
long-range tap, ally crossing the fire line, map/round transition,
death/disconnect, and 1/8/16 Bot scheduling.

The offline gate must provide deterministic Core/replay and fake-adapter
evidence for acquisition, target loss, stale memory, team reports, anonymous
sound, aim limits, reaction delay, aim noise, fire cooldown, cadence, empty
clip, reload, weapon switching, invalid input, and rejected host submission.
It must also exercise 1, 8, and 16 Bot loads with 8, 16, and 100ms frame
intervals; map/round changes, disconnects, slot and generation reuse, observer
separation, queue/budget limits, repeated ticks, and movement coexistence.

Register gating tests with CTest and retain existing P4 perception, World
Model, navigation, and host regression coverage. The report must record:

```text
Combat contracts
Target selection
Reaction and aim
DirectFire authorization
Fire cadence
Reload and weapon switching
Command composition
Scenario replay
Phase 5 Offline: PASS / FAIL
```

Offline evidence and applicable verification must remain separate from
post-Finish live combat acceptance. Synthetic fixtures must not be described
as real-server compatibility.

## Acceptance criteria

P5 is ready for its offline gate only when:

1. Every decision is derived from immutable value inputs and has a typed reason.
2. P5 `DirectFire` requires current valid direct vision; anonymous sound,
   stale-only memory, unknown team, or ally state cannot authorize a
   player-specific shot. Reserved future modes remain unsupported.
3. Angles, timing, ammunition, reload, switching, cadence, and button
   transitions are finite, bounded, deterministic, and generation-safe.
4. Tap, burst, and full-auto behavior reevaluates after bounded work and does
   not leave a stale attack command active.
5. The adapter is the only owner of engine/weapon objects and no new binary
   export is required.
6. Replay and load evidence passes without unbounded queue growth, cross-Bot
   state leakage, or movement regression.
7. The report lists unsupported engine event paths and live checks instead of
   treating synthetic evidence as real-server acceptance.

## Explicitly out of scope

- Action Planner and Tactical Planner decisions.
- Navigation goals, movement replacement, path cost, cover selection, retreat
  planning, or team role selection.
- Grenade selection/throwing, bomb objectives, damage learning, hit
  prediction, or persistent Experience storage.
- Wallbang penetration calculation, material/thickness/geometry evaluation,
  learned wallbang spots, and suppressive-fire policy.
- Advanced recoil control and spray-pattern optimization.
- Purchase AI, weapon economy, pickup planning, and complete weapon
  preference AI.
- Hidden enemy positions, engine-complete target state, or automatic sound
  source identification.
- AMXX/ReAPI public API additions and third-party GameDLL private integration.
- Real HLDS/ReHLDS, map, weapon-event, and performance acceptance before the
  project-wide Finish decision.

## Future Wallbang contract

Future Wallbang must remain separate from DirectFire and may use only:

```text
VisualMemory / EnemyBelief
last-known position
belief confidence and age
weapon penetration
surface / thickness
expected damage
friendly-fire risk
ammo cost
```

It must not use hidden engine current position, anonymous sound alone, or exact
wallhack targeting.

## Phase 5 completion definition

At Phase 5 completion, AstraBot can:

```text
see an enemy
→ select a target
→ react
→ aim
→ fire according to distance and weapon
→ avoid holding fire forever and reevaluate
→ reload or switch when necessary
```

This does not include advanced tactics. Later phases may add `TakeCover`,
`Retreat`, `Peek`, grenade use, objective-aware action, and team combat
coordination.

## Common workflow and verification

Each future P5 implementation slice starts from the current `main`, uses a
dedicated `codex/p5NN-<purpose>` worktree, confirms FocalSpan and graph
context, adds focused tests before implementation, and stages only intended
paths.

After all applicable slices, run the Windows VS 2026/NMake x86 portable and
Metamod Debug gates, the Linux GCC `-m32` Debug gate, and the Metamod Release
PE32/export check from `AGENTS.md`. Merge into `main` only with
`git merge --ff-only`, then rerun the relevant gates on the merged main.

This document-only change is verified with link/reference review, FocalSpan
refresh/query, and `git diff --check`; no combat implementation or Finish
decision is implied.
