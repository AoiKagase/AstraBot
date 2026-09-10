# AstraBot — Phase 8 Objective Role Extension

## Goal

Extend Phase 8 Team Director so AstraBot can coordinate classic Counter-Strike objective roles across the major map families, not only bomb-defusal maps.

Supported objective families:

```text
de_  Bomb Defusal
as_  VIP Assassination / Escort
cs_  Hostage Rescue
es_  Escape
```

This is an extension to Phase 8, not a replacement for the existing Team Director plan.

## Core Rules

- Keep Phase 7 Tactical Planner and Phase 8 Team Director responsibilities separate.
- Tactical Planner decides what the team is trying to accomplish.
- Team Director decides which bot is responsible for which role.
- Do not make Team Director issue movement or combat commands directly.
- Do not hard-code Team Director around bomb-defusal state only.
- Keep objective roles deterministic and observable.
- Reassign roles when the assigned bot dies, disconnects, becomes invalid, or the objective changes.
- Avoid duplicate ownership of exclusive tasks.
- Use World Model / objective snapshots, not hidden engine truth.
- Keep baseline support small; advanced map-specific strategy is deferred.

## OR-01 — Objective Family Contract

Define an engine-independent objective family.

Candidate:

```text
BombDefusal
VipEscort
HostageRescue
Escape
```

Required properties:

- map/round identity
- objective family
- current objective state
- validity
- deterministic comparison

Do not derive behavior only from map filename inside Core. Map detection belongs at the adapter/world boundary.

## OR-02 — Objective Role Contract

Extend Team Director role/assignment contracts to represent objective-specific responsibilities.

Baseline concepts:

### Bomb Defusal

```text
Defuser
DefuseCover
RetakeEntry
BombGuard
BombCarrier
BombEscort
```

### VIP

```text
VIP
VipEscort
VipGuard
VipPathClear
VipIntercept
```

### Hostage Rescue

```text
HostageRescuer
HostageEscort
RescueCover
RescueRouteGuard
HostageIntercept
```

### Escape

```text
EscapeRunner
EscapeEscort
EscapeRouteGuard
EscapeBlocker
EscapeIntercept
```

Do not force every concept into one permanent global `Role` enum if a smaller `ObjectiveAssignment` contract is cleaner. Prefer separating persistent/team roles from temporary objective assignments where possible.

## OR-03 — Exclusive Assignment

Prevent multiple bots from independently selecting the same exclusive objective task.

Examples:

```text
Defuse
VIP ownership/control
Hostage pickup responsibility
Specific escape-route responsibility
```

Exclusive assignment should use stable identity and generation-safe ownership.

Rules:

- one active owner per exclusive assignment
- deterministic replacement
- stale assignment cannot survive map/round/player generation changes

## OR-04 — Bomb Defusal Baseline

Preserve and formalize existing `de_` coordination.

CT baseline:

```text
best candidate
→ Defuser

others
→ Cover / Clear / Retake / Hold
```

Defuser selection should consider:

```text
defuse kit
distance to bomb
health
route availability
remaining bomb time
current combat pressure
```

Important:

- multiple bots must not simultaneously commit to defuse
- if Defuser dies or cannot continue, reassign immediately
- Cover bots must not steal the exclusive Defuser assignment without reassignment

T-side baseline:

```text
BombGuard
BombsiteCover
RouteGuard
```

Advanced post-plant tactics are deferred.

## OR-05 — VIP Baseline

Support `as_` maps.

CT objectives:

```text
VIP
VipEscort
VipGuard
VipPathClear
```

T objectives:

```text
VipIntercept
RouteBlock
Ambush/Guard candidate
```

VIP-specific rules:

- VIP identity is first-class objective state
- VIP must not be treated as a normal unrestricted combat role
- escort roles should account for VIP location and escape route
- VIP death immediately invalidates related assignments
- escape completion ends VIP assignments
- if an escort dies, another eligible bot may be reassigned

Advanced VIP route tactics are deferred.

## OR-06 — Hostage Rescue Baseline

Support `cs_` maps.

CT baseline:

```text
HostageRescuer
HostageEscort
RescueCover
RescueRouteGuard
```

Required reasoning:

- which hostage is being handled
- whether it is already following a player
- whether another bot owns that hostage assignment
- route to rescue zone
- reassignment if rescuer dies or hostage state changes

T baseline:

```text
HostageIntercept
RescueRouteGuard
ObjectiveDefense
```

Important:

- do not make every CT bot run to the same hostage
- support multiple hostages through stable target identity
- one bot may own one hostage task unless design evidence supports more

Advanced hostage path correction is deferred.

## OR-07 — Escape Baseline

Support `es_` maps.

T baseline:

```text
EscapeRunner
EscapeEscort
EscapeRouteGuard
```

CT baseline:

```text
EscapeBlocker
EscapeIntercept
RouteDefense
```

Inputs should include:

```text
remaining players
remaining time
escape-zone state
route availability
current enemy belief
```

Important:

- escape-capable bots should prioritize completion when tactically required
- not every T bot needs identical route ownership
- CT assignment should support blocking/intercept roles rather than generic attack behavior

Advanced rush timing and choke prediction are deferred.

## OR-08 — Assignment Replanning

Define events that invalidate or reconsider objective assignments.

Minimum triggers:

```text
assigned bot death
disconnect
player generation change
map change
round change
objective transition
bomb planted/dropped
defuser invalidated
VIP death
VIP reaches escape zone
hostage ownership change
hostage death/state change
escape progress change
route invalidation
```

Replanning must be deterministic.

Do not recalculate every assignment every frame. Use event-driven invalidation plus bounded periodic validation if needed.

## OR-09 — Tactical Planner Boundary

Keep Phase 7 and Phase 8 responsibilities clean.

Example:

```text
Phase 7 Tactical Intent:
RETAKE

Phase 8 Team Director:
Bot02 = Defuser
Bot03 = DefuseCover
Bot04 = RetakeEntry
```

Example:

```text
Phase 7 Tactical Intent:
ESCORT

Phase 8 Team Director:
Bot01 = VIP
Bot02 = VipEscort
Bot03 = VipPathClear
```

Team Director must not independently invent a different tactical objective. If tactical intent becomes incompatible with objective state, report invalidation/replan reason rather than silently overriding it.

## OR-10 — Observability

Expose at minimum:

```text
objective family
objective state
current assignments
exclusive owner
assignment reason
reassignment reason
assignment age
target identity
```

## OR-11 — Scenario Tests

Minimum scenarios:

### Bomb

```text
Bomb planted
→ one Defuser
→ others Cover
→ Defuser dies
→ deterministic reassignment
```

### VIP

```text
VIP active
→ escorts assigned
→ escort dies
→ reassignment

VIP dies
→ all VIP escort assignments invalidated
```

### Hostage

```text
multiple hostages
→ different ownership where appropriate
→ rescuer dies
→ assignment released/reassigned

hostage ownership changes
→ stale assignment removed
```

### Escape

```text
T escape objective active
→ runner/escort assignments

CT defense
→ blocker/intercept assignments

runner dies
→ reassignment
```

Also test:

```text
map/round transition
slot reuse
disconnect
1/8/16 bots
deterministic input-order independence
```

## OR-12 — Offline Gate

Required:

- Windows x86 tests
- Linux x86 tests
- deterministic assignment replay
- no hidden engine information
- no duplicate exclusive objective ownership
- stale assignment rejection
- bounded role/assignment work
- Phase 7 Tactical Planner regression
- existing Phase 8 Team Director regression
- Phase 6 Action Planner regression

Verdict:

```text
Objective Role Extension Offline: PASS / FAIL
```

## Relationship to Existing Phase 8

```text
Phase 7
Tactical Planner
↓
Phase 8
Team Director
├─ baseline team roles
├─ objective assignment
│  ├─ de_
│  ├─ as_
│  ├─ cs_
│  └─ es_
↓
Phase 8.5
Economy & Buy Planner
```

Phase 8.5 may later use objective roles for purchases, for example:

```text
Defuser
→ higher Defuse Kit priority

VipEscort
→ role-appropriate weapon/utility

RescueCover
→ utility preference
```

but purchase behavior is not part of this plan.

## Deferred Work

Do not require the following for the baseline extension:

```text
advanced VIP formation tactics
learned VIP routes
hostage navigation AI internals
complex multi-hostage optimization
escape choke prediction
objective-specific grenade playbooks
map-specific scripted strategies
human-learned objective roles
```

## Completion Definition

The extension is complete when AstraBot can treat the classic objective families as first-class team-coordination problems:

```text
de_
→ assign one Defuser and supporting roles

as_
→ identify VIP and assign escort/intercept roles

cs_
→ assign hostage ownership, escort, and cover

es_
→ assign escape runners/escorts and blockers/interceptors
```

Assignments must be deterministic, observable, generation-safe, and reassignable when objective state changes.
