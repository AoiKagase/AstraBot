# RealBot learning analysis

## Finding

RealBot does not contain a NavMesh learning layer.  It builds and persists a
mutable node/waypoint graph, then stores team-indexed danger/contact and a
visibility table beside it.  The reusable value is the feedback-loop concept;
the representation, code, and binary format are not suitable for AstraBot.

Evidence snapshot: Fundynamic/RealBot
[`a6649c826ce39912a2670d755671954242ccfaa3`](https://github.com/Fundynamic/RealBot/tree/a6649c826ce39912a2670d755671954242ccfaa3).

## What `NodeMachine` owns

`cNodeMachine` owns a fixed-size node array, neighbour graph, per-bot path
buffers, goals, trouble connections, visibility data, A* state, and persistence.
`tInfoNode` embeds `fDanger[2]` and `fContact[2]`
（[`NodeDataTypes.h`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeDataTypes.h#L149-L164)）。This is mutable global map state, not a static mesh plus query-local route state.

## What changes through play

| Observation / event | Actual mutation | Persistence |
|---|---|---|
| Any alive client moves more than `NODE_ZONE` | `addNodesForPlayers` calls `add2` at the client origin（[`NodeMachine.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeMachine.cpp#L1260-L1278)）。The loop has no human-vs-bot predicate. | Node origins, neighbours and flags are stored in `data/cstrike/maps/<map>.rbn`（[`NodeMachine.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeMachine.cpp#L1592-L1622)）。 |
| A Bot acquires a new enemy | `NodeMachine.contact(currentNode, team)` increments contact at the node and spreads it to trace-reachable nearby nodes（[`bot.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/bot.cpp#L556-L572), [`NodeMachine.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeMachine.cpp#L1791-L1820)）。 | Team contact values go to `.rbx`. |
| A Bot dies | `NodeMachine.danger(closeNode, team)` increments/spreads danger（[`bot.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/bot.cpp#L3158-L3172), [`NodeMachine.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeMachine.cpp#L1832-L1859)）。 | Team danger values go to `.rbx`. |
| New round | `scale_danger` and `scale_contact` rescale accumulated values（[`dll.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/dll.cpp#L908-L922)）。 | Later save persists the scaled state. |
| Visibility test | Bits in `cVisTable` and checked-state are updated. | Visibility bit table and checked-state are also written to `.rbx`. |
| Route search | Neighbour cost adds `fDanger[team] * baseCost`; contact is present but commented out（[`NodeMachine.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeMachine.cpp#L2424-L2460)）。 | The effect appears in later path choices, not a separate route log. |

Therefore repeated play changes graph coverage/connectivity, danger/contact, and
visibility cache.  It does not learn a policy, utility model, enemy belief model,
or separate human strategy distribution.

## Persistence weaknesses

`.rbn` and `.rbx` use direct native `fwrite`/`fread` of C/C++ values.  In
particular, the `.rbx` writer writes scalar `float` members through
`sizeof(Vector)`（[`NodeMachine.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeMachine.cpp#L1424-L1455)）and the reader repeats the same fragile layout.  Other fields use native `int`,
`unsigned long`, arrays, and fixed maxima.  This makes ABI, bounds, corruption,
endianness, and migration behavior hard to reason about.  The file has a version
integer, but not a transactional/versioned schema with independently validated
records.

The core files also have no repository-level license grant and declare generic
GPL protection plus partial HPB-Bot/other-bot heritage.  The code and formats are
therefore reference-only under the Phase 0 policy.

## Concept to reuse

- persistent map-local observations should influence future route costs;
- danger and contact are different signals and should retain team/direction/time
  context rather than one opaque score;
- visibility can be cached when keyed by a static map fingerprint and analysis
  version;
- bad traversal, stuck recovery, encounter, death, objective success and traffic
  should be explicit events, not direct mutation from arbitrary Bot code;
- learning must be bounded, decayed, inspectable, and removable from route cost
  for A/B comparison.

## Code/design not to reuse

- `NodeMachine` source, `.rbn`/`.rbx`, raw native binary persistence;
- node creation from every player without provenance;
- static graph and learned values in the same mutable singleton;
- fixed `MAX_NODES`/per-bot path arrays and global A* state;
- danger spreading through live engine traces during storage mutation;
- route-cost formulas without a component/reason trace;
- ambiguous GPL/mixed-heritage implementation.

## AstraBot event and persistence model

Each learning input is an immutable event with at least:

```text
EventId, MapFingerprint, RoundId, TickId, ActorId,
ActorKind(Human|AstraBot|OtherBot|Unknown), Team,
AreaId, optional FromAreaId/ToAreaId, EventKind,
Outcome, Magnitude, confidence, source
```

`ActorKind` is recorded at observation time; it is not inferred later from a
name or current slot.  Aggregates keep human, AstraBot, other-bot, and unknown
sample counts separately.  Weight is a query/config decision, so changing
`human=1.0` and `bot=0.25` does not rewrite raw evidence.  Unknown samples are not
silently treated as human.

The reducer produces a versioned `ExperienceSnapshot` keyed by
`(MapFingerprint, NavAreaId, Team, EventKind[, direction/context])`.  Route
queries receive that immutable snapshot and return the contribution of each
experience term.  Round-boundary transactions make a crash lose at most the
current uncommitted batch; map-fingerprint mismatch quarantines old aggregates.

## Decision

Adopt RealBot's persistent feedback-loop concept, team-aware danger/contact, and
the idea of observed traversal.  Replace its implementation with event sourcing
at the adapter boundary, independently authored reducers, explicit human/bot
provenance, static Nav/Experience separation, and a versioned persistence port.
SQLite is evaluated separately in [architecture.md](../architecture.md) and is
not implemented in Phase 0.
