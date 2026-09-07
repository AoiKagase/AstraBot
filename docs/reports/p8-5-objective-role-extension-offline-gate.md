# Phase 8.5 Objective Role Extension Offline Gate

## Scope

This gate covers the objective-specific role extension built on the Phase 8
Team Director. The implementation remains engine-independent and keeps
objective assignment generation separate from tactical planning and engine
command emission.

## Evidence

| Area | Evidence |
| --- | --- |
| Objective contracts | `src/core/team_director.hpp` defines `ObjectiveFamily`, `ObjectiveState`, generation-safe `ObjectiveTargetId`, bounded objective targets, and bounded objective assignments for bomb defusal, VIP escort, hostage rescue, and escape objectives. |
| Assignment safety | `ObjectiveAssignment` carries map/round identity, assignment age, generation, reason, target identity, route area, and exclusivity. `SharedTeamState::valid` rejects stale identity, invalid age, duplicate player/agent ownership, and duplicate exclusive tasks. |
| Bomb roles | Planted/defuse states produce one exclusive defuser plus cover/retake roles; carried and dropped states produce carrier, escort, guard, or retake roles. Defuser death triggers a deterministic replacement. |
| VIP roles | Counter-Terrorist snapshots produce VIP, escort, guard, and path-clear roles; Terrorist snapshots produce intercept roles. VIP death/completion clears invalid assignments and the surviving roster is replanned. |
| Hostage roles | Each active hostage receives distinct rescuer ownership with escort, cover, route-guard, or intercept support. Hostage ownership/state changes preserve the target identity and reassign dead or disconnected owners. |
| Escape roles | Terrorist snapshots produce one runner with escort and route-guard support; Counter-Terrorist snapshots produce blocker, intercept, and route-defense roles. Escape progress changes trigger replanning. |
| Invalidation and determinism | Map/round changes, player-generation changes, objective transitions, route invalidation, death, disconnect, and target changes are represented as explicit events. Stable player/agent/target tie-breaks make assignment independent of input member order. |
| Scenario coverage | `tests/objective_role_tests.cpp` covers invalid/stale contracts, bomb defuser reassignment, VIP escort/intercept and completion, distinct hostage ownership/following, escape replanning for both sides, map/round/generation invalidation, exclusive-task uniqueness, and input-order stability. |

## Focused verification

The implementation was verified with the affected portable x86 Debug targets
and registered tests, plus the corresponding Metamod-P x86 Debug adapter/test
targets. The objective-role, Team Director, and economy focused cases passed.

## Canonical phase gate

The required full verification command is:

```powershell
tools/verify-canonical.ps1 -Profile All
```

The canonical command was run once at the P8 completion point on the same tree
as this report and the implementation, and all profiles passed. A commit,
merge, branch switch, or branch deletion does not require repeating a
successful result. Live HLDS/ReHLDS and real-device validation remain outside
this offline gate.

Phase 8.5 Objective Role Extension Offline: PASS
