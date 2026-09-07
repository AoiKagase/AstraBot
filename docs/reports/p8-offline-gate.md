# Phase 8 Offline Gate

## Scope

This gate covers the engine-independent Team Director introduced by Phase 8.
The economy and buy planner remains deferred to Phase 8.5.

## Evidence

| Area | Evidence |
| --- | --- |
| Team role contracts | `src/core/team_director.hpp` defines bounded contracts for roles, objectives, members, assignments, observations, and tactical proposals. |
| Deterministic assignment | `TeamDirector` scores health, weapon value, position, personality, objective state, and current tactical plan with stable PlayerId/agent tie-breaks. |
| Reassignment | `TeamEvents` covers bot death, bomb drop, defuser death, objective transition, and disconnect. Dead or disconnected members are excluded from new assignments. |
| Team strategies | Attack split, defense split, retake group, escort priority, and defuse priority are selected from the observed objective state. |
| Communication boundary | `SharedTeamState` exposes only explicit observations, evidence-backed tactical proposals, role assignments, and observed objective status. Private engine state is not part of the shared contract. |
| Coordination scenario | `tests/team_director_tests.cpp` covers five-bot unique roles, defuser handoff after death, bomb-drop retake, strategy branches, communication validation, and deterministic 16-bot bounded work. |

## Verification

The canonical command for this phase is:

```powershell
tools/verify-canonical.ps1 -Profile All
```

Phase 8 Offline: PASS

The PASS verdict is valid only when the canonical command above succeeds on
the same tree as this report and the implementation. Live HLDS/ReHLDS and
real-device validation remain outside this offline gate.
