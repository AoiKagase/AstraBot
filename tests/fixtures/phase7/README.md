# Phase 7 Scenario Fixtures

These fixtures are deterministic, source-independent contract traces. Their
provenance is synthetic and behavior-only; they do not contain copied
ReGameDLL-CS code, private symbols, hidden enemy coordinates, or live-server
acceptance claims.

Each trace carries map, round, tick, and observer actor-generation identity.
`Unknown`, `ObservedAbsent`, and `ObservedPresent` remain separate. Positions
are supplied only with an explicit observation or a separately labeled memory
sample. Expected values describe Core contracts such as recovery, proposal,
intent, and uncertainty; they do not describe dispatch receipts, delivered
messages, damage, ammunition mutation, or gameplay completion.

Run the deterministic verifier from the repository root:

```text
python tests/phase7_scenarios.py
```

Real HLDS/ReHLDS differential behavior and live CSBot parity remain Phase 8
acceptance items.
