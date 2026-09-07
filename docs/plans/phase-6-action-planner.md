# Phase 6 — Action Planner

Phase 6 introduces a planner that selects the most appropriate immediate
action from the current situation. It builds on the Phase 5 combat baseline
without replacing navigation or directly calling engine APIs.

## Goal

Select among bounded action candidates such as:

```text
Engage
Reload
TakeCover
Retreat
Peek
Hold
Follow
Plant
Defuse
Guard
RecoverBomb
```

Utility AI is the primary candidate for the scoring model.

## P6-01 — Action Contracts

Define portable action contracts, including:

```text
ActionType
ActionIntent
ActionScore
ActionPrecondition
ActionAbortReason
ActionResult
```

Actions must not call Engine APIs directly.

## P6-02 — Utility Scoring

Score each action from value-level inputs such as:

- self health;
- ammunition;
- current weapon;
- current enemy confidence;
- current direct vision;
- objective state;
- distance to cover or objective;
- teammate presence; and
- personality.

The score must be deterministic and explainable. Tie-breaking must also be
fixed and reproducible.

## P6-03 — Engage / Reload / Hold

Implement the minimum action set:

- `Engage`;
- `Reload`;
- `Hold`; and
- `FollowCurrentIntent`.

Keep the action planner's responsibility separate from the Phase 5 Combat
system.

## P6-04 — TakeCover / Retreat

Select cover candidates from the navigation data. Do not implement advanced
cover analysis in this phase.

The minimum behavior is:

- reduce exposure to the enemy belief;
- identify a retreatable route;
- identify a nearby safe area; and
- enforce action timeout and abort conditions.

## P6-05 — Peek

Implement a simple peek action with:

- short exposure;
- target reacquisition;
- return to the previous position or intent; and
- abort when the threat changes.

Pixel-perfect shoulder peeking is deferred.

## P6-06 — Objective Actions

Support the minimum objective actions:

- `Plant`;
- `Defuse`;
- `GuardBomb`;
- `RecoverBomb`;
- `RescueHostage`; and
- `EscortHostage`.

Game-rule dependencies must remain inside the adapter and World Model
boundaries.

## P6-07 — Action Arbitration

Define when the current action continues and when it is replaced. Account for:

- score hysteresis;
- interrupt priority;
- objective urgency;
- current direct threat; and
- action timeout.

Prevent per-tick action switching and resulting oscillation.

## P6-08 — Scenario Tests

Minimum scenarios:

```text
enemy seen → Engage
low ammo + safe → Reload
low health + pressure → Retreat
bomb planted + kit → Defuse
bomb dropped → RecoverBomb
```

Verify deterministic behavior at 1, 8, and 16 Bot loads.

## P6-09 — Phase 6 Gate

The offline gate must cover:

- Windows and Linux x86 builds;
- deterministic replay;
- bounded work;
- movement and combat regression;
- absence of hidden engine truth; and
- action-reason observability.

Record the result as:

```text
Phase 6 Offline: PASS / FAIL
```

