# Phase 8 differential fixtures

These fixtures define the narrow, source-independent comparison boundary for
observable AstraBot and CSBot behavior. The checked-in fixture is a synthetic
contract sample: it proves schema, bounds, actor-generation alignment, and
deterministic mismatch handling, but it is not a capture from HLDS/ReHLDS.

The pinned ReGameDLL-CS commit is provenance only. No reference source,
private symbol, hidden enemy coordinate, engine receipt, delivered message,
damage result, or objective completion is allowed in a trace.

Run the contract/replay gate from the repository root:

```text
python tests/phase8_differential.py
```

The harness keeps `TEST-03` pending until a real, pinned reference trace and a
corresponding AstraBot trace are captured and reviewed. Offline replay and
CRG/FocalSpan output do not replace real-server acceptance.
