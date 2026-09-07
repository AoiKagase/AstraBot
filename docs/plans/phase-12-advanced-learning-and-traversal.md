# Phase 12 — Advanced Learning and Traversal

Phase 12 adds AstraBot's differentiating learning and traversal capabilities.
It may be split into multiple subphases; the subphases below are intentionally
independent.

## Goal

Extend contextual danger, human traversal discovery, traversal experience,
advanced motion primitives, opponent profiling, Wallbang, and SuppressiveFire
without making the NavMesh mutable or introducing hidden information.

## P12-A — Contextual Danger

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

## P12-B — Human Traversal Discovery

Observe human movement and detect a potential traversal when a player
successfully moves between areas without a normal Nav connection:

```text
Area A
↓
no normal Nav connection
↓
successful movement to Area B
```

## P12-C — Traversal Experience

Track at least:

```text
TraversalLink identity
human attempts
human success
bot attempts
bot success
```

## P12-D — Advanced Motion Primitives

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

## P12-E — Learned Traversal Activation

Enable only Traversal candidates with sufficient human-success evidence. Keep
the NavMesh immutable and compose the result as:

```text
NavMesh
+
Traversal Enrichment
+
Traversal Experience
```

## P12-F — Opponent Profiling

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

## P12-G — Wallbang

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

## P12-H — Suppressive Fire

Suppressive fire targets a high-probability region rather than a specific
player's hidden exact position. Keep this policy separate from Wallbang.

## P12-I — Advanced Learning Gate

Verify that learning does not cause:

- loss of deterministic replay;
- unbounded database growth; or
- unstable route oscillation.

