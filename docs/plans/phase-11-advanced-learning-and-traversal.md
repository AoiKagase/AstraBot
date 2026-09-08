# Phase 11 — Advanced Learning and Traversal

Phase 11 adds AstraBot's differentiating learning and traversal capabilities.
It may be split into multiple subphases; the subphases below are intentionally
independent.

## Goal

Extend contextual danger, human traversal discovery, traversal experience,
advanced motion primitives, opponent profiling, Wallbang, and SuppressiveFire
without making the NavMesh mutable or introducing hidden information.

The map-session contract is explicit: `ContextualDangerModel` is cleared on
`MapGeneration` changes and rejects observations from another generation.
Opponent profiles are map-session scoped and do not retain redundant round
metadata. Persistent Experience remains map-identified and separate.

Adaptive Route keeps learned danger separate from geometric exposure. Encounter,
grenade, and death observations update danger; exposure is supplied only by a
bounded provider and defaults to zero. Human/Bot traffic weights are applied
once during Experience update.

## P11-A — Contextual Danger

Extend the initial model:

```text
Danger(area, team)
```

to include context:

```text
Danger(
    area,
    team,
    approachDirection,
    enemyWeaponClass,
    likelyEnemyArea
)
```

## P11-B — Human Traversal Discovery

Observe human movement and detect a potential traversal when a player
successfully moves between areas without a normal Nav connection:

```text
Area A
↓
no normal Nav connection
↓
successful movement to Area B
```

## P11-C — Traversal Experience

Track at least:

```text
TraversalLink identity
human attempts
human success
bot attempts
bot success
```

## P11-D — Advanced Motion Primitives

Candidate motion primitives include:

```text
GapJump
LongJump
EdgeTraverse
NarrowPassage
Advanced Ladder
Landing Recovery
AirControl
```

Do not implement all primitives at once.

## P11-E — Learned Traversal Activation

Enable only Traversal candidates with sufficient human-success evidence. Keep
the NavMesh immutable and compose the result as:

```text
NavMesh
+
Traversal Enrichment
+
Traversal Experience
```

## P11-F — Opponent Profiling

Future profiling candidates include:

```text
aggression
rush probability
camp probability
preferred region
weapon preference
rotation speed
```

Avoid excessive dependence on privacy-sensitive persistent identities.

## P11-G — Wallbang

Implement the `Wallbang` mode reserved by Phase 5 using:

```text
EnemyBelief
last-known position
confidence / age
penetration geometry
weapon penetration
friendly-fire risk
expected damage
ammo cost
```

Never use:

- hidden current position;
- exact wallhack targeting; or
- anonymous sound alone to select a player-specific wallbang target.

## P11-H — Suppressive Fire

Suppressive fire targets a high-probability region rather than a specific
player's hidden exact position. Keep this policy separate from Wallbang.

## P11-I — Advanced Learning Gate

Verify that learning does not cause:

- loss of deterministic replay;
- unbounded database growth; or
- unstable route oscillation.
