# Phase 9 — Persistent Experience

Phase 9 adds an independent Experience Layer on top of the navigation data so
that repeated play makes the Bot more familiar with a map. The NavMesh itself
remains immutable.

## Goal

Persist experience about areas, encounters, outcomes, and traversal while
keeping the navigation graph and its source data unchanged.

## P9-01 — Experience Event Contracts

Define at least these value-level events:

```text
AreaEntered
DamageDealt
DamageReceived
Kill
Death
Encounter
GrenadeExplosion
Plant
Defuse
AttackSuccess
AttackFailure
RetakeSuccess
RoundResult
HumanTraversal
BotTraversal
```

## P9-02 — Area Experience Model

The initial area model includes:

```text
visits
dangerT
dangerCT
encounterRate
deathRate
grenadeThreat
sniperThreat
pushSuccess
retakeSuccess
humanTraffic
botTraffic
```

## P9-03 — Human / Bot Separation

Store human and Bot observations in separate counters. Initial weighting may
use:

```text
human = 1.0
bot   = 0.25
```

The weighting must be configurable.

## P9-04 — Persistence Format

Re-evaluate the Phase 0 and earlier research before selecting the final format
from:

```text
SQLite
versioned binary
other
```

The selected format must support:

- schema and versioning;
- corruption handling;
- atomic save;
- migration;
- backup and recovery; and
- Windows and Linux x86.

## P9-05 — Decay

Apply decay to old experience. Candidate policies include:

```text
EWMA
time-based decay
round-based decay
```

The selected policy must be deterministic and explainable.

## P9-06 — Experience Update Pipeline

Never write to the database directly from an engine event. Use this boundary:

```text
adapter event
↓
ExperienceEvent
↓
ExperienceModel
↓
persistence boundary
```

## P9-07 — Persistence Tests

Cover:

- persistence across restart;
- corrupted database or file;
- schema mismatch;
- map identity mismatch;
- human/Bot weighting;
- decay; and
- deterministic updates.

## P9-08 — Phase 9 Gate

Record the result as:

```text
Phase 9 Offline: PASS / FAIL
```

