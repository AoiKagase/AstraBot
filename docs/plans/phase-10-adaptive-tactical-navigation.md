# Phase 10 — Adaptive Tactical Navigation

Phase 10 feeds Phase 9 Experience into route cost without modifying the
immutable navigation graph.

## Goal

Allow the same start and goal to select different routes, such as:

```text
safe route
aggressive route
experienced route
```

## P10-01 — Experience Cost Policy

Connect the following terms to the existing A* cost extension:

```text
distance
traversal
danger
experience
```

The NavMesh remains unchanged.

## P10-02 — Personality Weighting

Support personality-dependent danger weighting, for example:

```text
Aggressive:
dangerWeight low

Balanced:
normal

Cautious:
dangerWeight high
```

## P10-03 — Tactical Route Styles

Support route-style choices such as:

```text
FAST
SAFE
LOW_EXPOSURE
LOW_TRAFFIC
FLANK
OBJECTIVE_FAST
```

## P10-04 — Experience-Aware Traversal

Allow future traversal links to incorporate:

```text
human success
bot success
failure risk
```

GapJump discovery itself is not required in this phase.

## P10-05 — Adaptive Route Tests

With the graph held constant, change only Experience and verify that the
selected route can change:

```text
route A
→
route B
```

## P10-06 — Phase 10 Gate

Record the result as:

```text
Phase 10 Offline: PASS / FAIL
```

