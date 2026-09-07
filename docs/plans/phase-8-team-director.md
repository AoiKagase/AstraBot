# Phase 8 — Team Director

Phase 8 coordinates multiple Bots so that they divide responsibilities instead
of duplicating the same decision. It is not a shared omniscient hive mind.

## Goal

Support roles such as:

```text
Entry
Trade
Support
Lurk
FlankWatch
Anchor
Rotator
Defuser
```

## P8-01 — Team Role Contracts

Keep team roles separate from each Bot's individual Action and tactical Intent.

## P8-02 — Role Assignment

Assign roles deterministically from:

- weapon;
- health;
- position;
- objective;
- personality; and
- the current tactical plan.

## P8-03 — Role Reassignment

Reassign roles on:

- Bot death;
- bomb drop;
- defuser death;
- objective transition; and
- Bot disconnect.

## P8-04 — Team Strategy

Implement the minimum team strategies:

- attack split;
- defense split;
- retake group; and
- escort or defuse priority.

## P8-05 — Communication Boundary

The team layer may share:

- explicit observations;
- tactical proposals;
- role assignments; and
- objective status.

It must not share hidden engine truth or convert private uncertainty into
complete certainty for other Bots.

## P8-06 — Coordination Tests

Minimum scenario:

```text
5 Bots
→ unique roles
→ one Bot dies
→ roles are reassigned
```

Verify that duplicate actions such as multiple Bots attempting to defuse are
prevented.

## P8-07 — Phase 8 Gate

Record the result as:

```text
Phase 8 Offline: PASS / FAIL
```

