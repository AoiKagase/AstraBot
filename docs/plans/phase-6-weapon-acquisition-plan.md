# AstraBot — Weapon Acquisition and Pickup Planning

## Goal

This plan defines how AstraBot should decide whether to acquire a dropped weapon during a round.

Weapon acquisition is not simple weapon switching.

```text
Weapon Switching
=
selecting among weapons already owned

Weapon Acquisition
=
deciding whether a dropped weapon is worth pursuing,
reaching it safely,
replacing current equipment if necessary,
and verifying the pickup result
```

Weapon acquisition belongs primarily to the Action Planner layer.

The goal is not to pick up any weapon that is stronger than the current weapon. The goal is to decide whether a dropped weapon is more useful in the current tactical situation, considering ammunition, role, remaining players, round time, objective state, and pickup risk.

## Core Principles

- Do not rank weapons by raw damage alone.
- Evaluate weapon value relative to the current tactical situation.
- Consider current clip and reserve ammunition.
- Consider remaining round time and objective urgency.
- Consider remaining teammates and enemies.
- Consider likely combat range.
- Consider the current tactical role.
- Consider travel distance and exposure required to reach the weapon.
- Do not abandon a critical objective merely to obtain a better weapon.
- Do not use hidden enemy positions when estimating pickup risk.
- Use World Model / Enemy Belief only.
- Keep pickup planning separate from low-level item interaction.
- Verify successful acquisition from observed inventory state.

## Relationship to Existing Phases

Recommended placement:

```text
Phase 5
Combat Baseline
↓
Phase 6
Action Planner
    └─ AcquireWeapon
↓
Phase 7
Tactical Planner
```

Weapon acquisition should be introduced as a Phase 6 Action Planner capability. It must not be embedded directly into the Phase 5 WeaponSelector.

## WA-01 — Weapon Pickup Contracts

Define portable data structures for dropped weapons.

Candidate types:

```text
DroppedWeaponObservation
WeaponPickupCandidate
WeaponPickupEvaluation
WeaponAcquisitionIntent
WeaponAcquisitionResult
WeaponAcquisitionFailure
```

Candidate fields:

```text
entity/reference identity
weapon class
estimated ammo if observable
position
NavAreaId
observation timestamp
confidence
```

Do not expose engine entity pointers to Action Planner Core.

Acceptance:
- Portable types only.
- Reject stale map/round observations.
- Reject invalid/non-finite positions.
- No direct Engine object ownership.
- Deterministic equality/testing.

## WA-02 — Current Weapon Utility

Estimate how useful the currently held weapon is in the present situation.

Inputs:

```text
weapon class
clip ammo
reserve ammo
reload state
effective range
current health
current role
likely engagement distance
remaining round time
objective state
```

Example:

```text
Current weapon:
AWP

Round state:
1v2 retake
Bomb planted
18 seconds remaining
Expected close-range engagement

↓
Current weapon tactical utility decreases
```

This does not mean the AWP is objectively weak; it may simply be poorly suited to the immediate tactical requirement.

## WA-03 — Ammunition Pressure

Represent how urgently AstraBot needs an alternative weapon.

Consider:

```text
current clip
reserve ammunition
reload availability
secondary weapon viability
nearby combat pressure
```

Example:

```text
AK47
clip = 4
reserve = 0

↓
high acquisition pressure
```

A lower-tier weapon with usable ammunition may be preferable to an empty higher-tier weapon.

## WA-04 — Tactical Weapon Fit

Evaluate whether a candidate weapon fits the expected next situation.

Inputs:

```text
remaining enemies
remaining teammates
round time
bomb state
hostage/objective state
current tactical intent
expected engagement range
route geometry
role
```

Examples:

```text
AWP + long-range hold + 55 sec remaining
→ keep AWP
```

```text
AWP + 1v2 retake + bomb planted + 18 sec + close-range fight
→ nearby rifle may be preferable
```

```text
M4A1 with adequate ammo + MP5 candidate + mid-range defense
→ ignore MP5
```

## WA-05 — Role Fit

Evaluate candidates relative to the current role.

Examples:

```text
Entry   → Rifle / SMG preferred
Support → Rifle preferred
AWP     → Sniper preferred when tactically appropriate
Anchor  → Weapon suited to expected defensive range
```

Role preference is a weight, not an absolute restriction.

## WA-06 — Pickup Risk

Estimate the tactical cost of reaching the weapon.

Inputs:

```text
distance
route cost
route danger
exposure
enemy belief
time required
objective delay
current health
teammate support
```

Conceptual model:

```text
PickupRisk =
    TravelRisk
  + ExposureRisk
  + ObjectiveDelay
  + CombatInterruptionCost
```

The bot should not cross a dangerous open area for a minor upgrade.

## WA-07 — Weapon Upgrade Value

Estimate how much the candidate improves combat capability.

Inputs:

```text
weapon class
effective range
damage potential
rate of fire
mobility
current/candidate ammunition
role fit
tactical fit
```

Do not use one permanent global weapon ranking.

Conceptual formula:

```text
UpgradeValue =
    CombatSuitability(candidate)
  - CombatSuitability(current)
```

## WA-08 — Pickup Utility

Combine factors into an explainable utility score.

```text
PickupUtility =
    UpgradeValue
  + AmmoNeed
  + TacticalFit
  + RoleFit
  - TravelRisk
  - ExposureRisk
  - ObjectiveDelay
  - SwapCost
```

Weights must remain configurable/testable.

Expose score components for debugging.

## WA-09 — AcquireWeapon Action

Introduce a Phase 6 Action Planner action:

```text
AcquireWeapon
```

Lifecycle:

```text
Evaluate
↓
Select Candidate
↓
Commit
↓
Navigate
↓
Approach
↓
Acquire / Replace
↓
Verify
↓
Complete
```

Abort conditions:

```text
enemy appears
objective becomes urgent
candidate disappears
candidate is picked up by another player
route becomes unsafe
remaining time becomes insufficient
better action becomes critical
```

The action must be interruptible.

## WA-10 — Navigation Integration

Use existing Nav/Local Navigation to reach the weapon.

Action Planner specifies:

```text
target NavArea
target world position
candidate identity
maximum acceptable route cost
```

Navigation decides how to reach it. Weapon acquisition logic must not generate movement commands directly.

## WA-11 — Pickup / Replacement Execution

Handle low-level acquisition separately.

Responsibilities:

```text
approach item
confirm item still exists
trigger normal CS pickup behavior
drop/replace current weapon if required
observe resulting inventory
```

Important:

```text
attempted pickup
≠
successful pickup
```

Success is confirmed only through updated inventory state.

## WA-12 — Weapon Drop Decision

Support replacement when acquisition requires giving up the current primary.

Compare:

```text
current weapon retained value
candidate value
current ammo
candidate ammo
current tactical requirement
```

Dropping the current primary must be part of the acquisition decision, not an unconditional side effect.

## WA-13 — Secondary Weapon Consideration

Avoid unnecessary primary replacement if an owned secondary already solves the problem.

Example:

```text
Primary: AWP
Secondary: Deagle with ammo
Temporary close-range threat

→ WeaponSelector may choose Deagle
→ do not AcquireWeapon
```

Prefer usable owned weapons before risky external acquisition unless the pickup has strong long-term value.

## WA-14 — Round-Time and Objective Urgency

Make acquisition sensitive to time pressure.

Examples:

```text
60 sec remaining + no immediate objective pressure + high-value weapon nearby
→ acquisition may be reasonable
```

```text
Bomb planted + 12 sec remaining + weapon 10 sec away
→ acquisition forbidden
```

Objective actions must override `AcquireWeapon`.

## WA-15 — Candidate Lifetime

Dropped weapon observations must expire safely.

A candidate becomes invalid if:

```text
another player picks it up
round changes
map changes
entity disappears
observation becomes stale
```

Do not chase stale weapon positions indefinitely.

## WA-16 — Multi-Candidate Selection

Evaluate multiple candidates deterministically.

Tie-break using stable criteria such as:

```text
utility
route cost
distance
weapon preference
stable candidate identity
```

Do not depend on container iteration order.

## WA-17 — Team Considerations

Avoid all bots pursuing the same dropped weapon.

Future Team Director integration may reserve a candidate:

```text
Dropped AWP
↓
best-suited bot selected
↓
candidate reserved
```

Baseline Phase 6 may use a minimal reservation rule if required.

## WA-18 — Observability

Expose at minimum:

```text
current weapon
current ammunition
candidate weapon
candidate distance
candidate route cost
upgrade value
ammo need
tactical fit
role fit
pickup risk
objective delay
total utility
selected action
abort reason
```

Example:

```text
Bot03 Weapon Acquisition

Current:
AWP
clip=3 reserve=0

Candidate:
AK47

Situation:
1v2 retake
Bomb planted
17.4 sec remaining

Utility:
Upgrade         +8
AmmoNeed       +35
TacticalFit    +28
RoleFit         +2
TravelRisk      -5
ExposureRisk    -4
ObjectiveDelay  -6
SwapCost        -3
------------------
Total          +55

Decision:
AcquireWeapon
```

## WA-19 — Scenario Tests

Minimum scenarios:

- Weak current weapon + better nearby weapon + safe route → acquire.
- Usable rifle + minor upgrade + dangerous pickup → ignore.
- Empty primary + weak secondary + loaded SMG nearby → acquire.
- AWP + close retake + low time + rifle immediately nearby → rifle may be selected.
- AWP + long-range hold + adequate ammo → keep sniper.
- Critical bomb timer + high-value weapon nearby → objective wins.
- Candidate disappears during acquisition → abort/replan.
- AWP + usable pistol + temporary close threat → switch owned weapon instead of acquiring.

## WA-20 — Offline Gate

Required:

- deterministic utility decisions
- no hidden engine information
- stale candidate rejection
- bounded candidate count
- bounded evaluation work
- Action Planner integration
- Navigation regression
- Combat regression
- objective priority regression
- Windows x86 tests
- Linux x86 tests

Verdict:

```text
Weapon Acquisition Offline: PASS / FAIL
```

## Deferred Advanced Features

```text
learned weapon preference
map-specific pickup hotspots
weapon value learned from historical success
human pickup behavior imitation
enemy economy inference from dropped weapons
team weapon donation/drop requests
advanced bait detection
prediction that a teammate will take the weapon
```

## Future Experience Integration

Later phases may learn:

```text
weapon success by map
weapon success by role
weapon success by tactical state
pickup success/failure
death while attempting pickup
weapon replacement outcomes
```

Experience should adjust utility rather than hard-code behavior.

## Completion Definition

The feature is complete when AstraBot can reason:

```text
What do I currently have?
↓
Is my ammunition sufficient?
↓
What kind of fight is likely next?
↓
Does this dropped weapon fit the situation better?
↓
Can I reach it safely?
↓
Do I have enough time?
↓
Is another owned weapon already sufficient?
↓
Is acquiring it worth delaying the current objective?
↓
Acquire / Ignore
```

The resulting behavior should resemble a human tactical weapon pickup decision rather than a fixed weapon ranking table.
