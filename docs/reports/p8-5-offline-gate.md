# Phase 8.5 Offline Gate

## Scope

This gate covers the engine-independent economy contracts, deterministic team
buy strategy, role-aware BuyPlan generation, equipment preservation, CT kit
allocation, and the CS purchase execution boundary introduced by Phase 8.5.
The planner remains separate from Combat AI and does not construct engine
command strings.

## Evidence

| Area | Evidence |
| --- | --- |
| Economy contracts | `src/core/economy.hpp` validates map/round/tick identity, bounded money/equipment state, player identity, and deterministic fixed-capacity request arrays. |
| Team strategy | `BuyPlanner::chooseStrategy` uses the team median and majority readiness, so one unusually rich member cannot force a FullBuy. It exposes a stable strategy reason. |
| Role-aware planning | `BuyPlanner::buildPlan` consumes Phase 8 roles and team strategy, preserves carried primary/secondary/armor/utility, and emits deterministic rifle/SMG/AWP fallback chains. |
| Team coordination | `planTeam` limits AWP ownership to one deterministic candidate and allocates the requested number of CT defuse kits only to eligible members. Role-specific smoke/flash/HE requests avoid identical utility plans. |
| Purchase boundary | `src/adapter/cstrike/buy_executor.*` accepts value-level purchase requests through `IBuyOperations`, checks buy-zone/freeze-time and identity, and verifies observed inventory after each dispatch. Bounded fallback attempts stop at `kMaxPurchaseAttempts`. |
| Scenario coverage | `tests/economy_tests.cpp` covers pistol/full/eco/half/force/save strategies, mixed money, carried equipment, AWP fallback, CT kit allocation, role utility, 1/8/16-member determinism, stale identity, buy-time failure, inventory verification, and bounded retries. |

## Verification

Focused implementation verification:

```powershell
cmake --build build-portable-x86-test --target astrabot_economy_tests
ctest --test-dir build-portable-x86-test -R "^astrabot.economy$" --output-on-failure
```

The phase gate command is:

```powershell
tools/verify-canonical.ps1 -Profile All
```

Phase 8.5 Offline: PASS

The PASS verdict is valid only when the canonical command above succeeds on
the same tree as this report and the implementation. Live HLDS/ReHLDS and
real-device validation remain outside this offline gate.
