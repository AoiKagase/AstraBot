# AstraBot — Phase 8.5 Economy & Buy Planner

## Goal

Phase 8.5 enables AstraBot to understand Counter-Strike round economy and make purchase decisions based on team strategy, assigned role, available money, and current equipment.

The buy AI must remain separate from Combat AI.

Core flow:

```text
Round / Economy State
↓
Team Buy Strategy
↓
Role
↓
Current Equipment
↓
Buy Plan
↓
Purchase Execution
↓
Post-Buy Verification
```

The goal is not to buy the most expensive weapon whenever possible, but to make purchase decisions that reflect team economy and the bot's assigned role.

## Core Rules

- Keep the Economy Planner separate from Combat Core.
- Separate purchase planning from low-level buy command execution.
- Determine team-wide economy strategy before individual buy plans.
- Preserve usable carried equipment instead of rebuying unnecessarily.
- Support role-aware purchases and affordable fallbacks.
- Preserve deterministic testability.
- Handle unsupported items and purchase failures safely.
- Do not depend on hidden engine state.
- Verify purchases through observed inventory state.
- Do not mix high-level `BuyPlan` semantics with menu/command execution details.

## P8.5-01 — Economy Contracts

Define engine-independent economy and purchase contracts.

Candidate types:

```text
EconomySnapshot
PlayerEconomySnapshot
EquipmentSnapshot
TeamEconomySnapshot
BuyStrategy
BuyPlan
BuyRequest
BuyResult
BuyFailureReason
```

Candidate information:

```text
money
team
round number
round phase
loss bonus information if available
current primary weapon
current secondary weapon
armor
helmet
defuse kit
grenades
ammo
```

Acceptance:

- Portable contracts only.
- Reject invalid money/state.
- Reject stale map/round identity.
- Deterministic equality/testing.
- No purchase command execution.

## P8.5-02 — Team Buy Strategy

Determine the baseline team economy strategy.

Minimum strategies:

```text
Eco
HalfBuy
ForceBuy
FullBuy
Save
```

Optional future strategies:

```text
PistolRound
AntiEco
AWPInvestment
```

Inputs:

```text
team money distribution
current equipment
round context
loss state
objective
team strategy
```

Avoid one rich bot independently Full Buying while the rest of the team Ecos.

Acceptance:

- Same snapshot → same strategy.
- One unusually rich player must not dominate the decision.
- Strategy reason must be observable.
- No circular dependency with Team Director.

## P8.5-03 — Weapon Roles and Preferences

Connect Phase 8 roles to equipment preferences.

Example:

```text
Entry   → Rifle + Armor + Flash
Trade   → Rifle + Armor + Flash
Support → Rifle + Smoke + Flash + HE
AWP     → AWP + Secondary + Armor
Anchor  → Rifle + Defensive Utility
```

Do not bind roles to one exact weapon. Provide affordable fallbacks.

Acceptance:

- Role-aware preference.
- Affordable fallback.
- Fallback for invalid/unavailable weapons.
- Deterministic selection.

## P8.5-04 — Buy Plan Generation

Generate `BuyPlan` from team strategy, role, money, and current equipment.

Possible priority:

```text
1. mandatory equipment
2. primary weapon
3. armor
4. secondary requirement
5. role utility
6. additional ammo/equipment
```

Verify exact priority against actual Counter-Strike rules.

Example:

```text
FullBuy / Entry

Primary:
AK47

Protection:
Kevlar + Helmet

Utility:
Flash
Flash
HE

Fallback:
Galil
→ SMG
→ Save remaining money
```

Keep the plan separate from post-purchase inventory state.

## P8.5-05 — Equipment Preservation

Avoid unnecessary purchases when valid equipment is carried over.

Consider:

- primary weapon
- secondary weapon
- armor
- helmet
- defuse kit
- grenades
- ammo

Acceptance:

- Preserve existing rifle.
- No unnecessary pistol replacement.
- Armor top-up decisions.
- Utility restock.
- Avoid wasting money.

## P8.5-06 — Utility Purchase

Purchase utility according to role and team strategy.

Initial items:

```text
HE
Flash
Smoke
```

Verify actual supported items from the target Counter-Strike environment.

Examples:

```text
Entry   → Flash priority
Support → Smoke + Flash
Retake CT → Flash + HE
Low economy → reduce utility
```

Leave room for future team-level utility distribution.

## P8.5-07 — CT-Specific Equipment

Handle CT-specific equipment, initially Defuse Kit.

Inputs:

```text
money
role
site responsibility
team kit count
team buy strategy
```

Avoid making every CT buy a kit. Coordinate desired kit count through Team Director.

## P8.5-08 — Purchase Execution Adapter

Translate portable `BuyPlan` into actual GoldSrc/CS purchase operations.

```text
BuyPlan
↓
Buy Executor
↓
Game command / menu
↓
Inventory change
```

The planner must not build engine command strings directly.

Verify:

- buy zone
- buy time
- round state
- menu/command semantics
- ReGameDLL / CS compatibility
- failed purchase handling

Important:

```text
command sent
≠
purchase succeeded
```

Verify success from inventory snapshots.

## P8.5-09 — Purchase Failure and Fallback

Handle failures safely.

Possible reasons:

```text
NotEnoughMoney
NotInBuyZone
BuyTimeExpired
ItemUnavailable
AlreadyOwned
InventoryConflict
CommandRejected
Unknown
```

Example:

```text
AWP failed
↓
Rifle fallback
↓
SMG fallback
↓
otherwise Save
```

Infinite retries are forbidden. Use a bounded retry budget.

## P8.5-10 — Team Economy Coordination

Connect Team Director and Buy Planner.

```text
Team decides FullBuy
↓
roles assigned
↓
AWP candidate selected
↓
kit allocation
↓
utility distribution
↓
individual BuyPlans
```

Prevent:

```text
all five bots buying AWP
all five bots prioritizing kit
all bots buying identical utility
one bot Full Buying while the rest Eco
```

Do not build a professional-team economy optimizer yet.

## P8.5-11 — Economy Scenario Tests

Minimum scenarios:

```text
Pistol round
Full buy
Eco
Half buy
Force buy
Mixed team money
Existing rifle carry-over
AWP role with insufficient money
CT kit allocation
Purchase failure + fallback
Buy time expired
```

Also verify 1/8/16 Bot conditions for determinism and bounded work.

## P8.5-12 — Phase 8.5 Offline Gate

Required:

- Windows x86 tests
- Linux x86 tests
- Metamod regression
- deterministic team economy replay
- bounded buy attempts
- inventory verification
- Phase 5 Combat regression
- Phase 8 Team Director regression

Gate report must cover:

```text
Economy contracts
Team strategy
Role-aware weapon selection
Buy plan
Equipment preservation
Utility
CT equipment
Execution
Fallback
Team coordination
Scenario replay
```

Verdict:

```text
Phase 8.5 Offline: PASS / FAIL
```

# Observability

Example:

```text
Bot03

money         = $5200
team strategy = FullBuy
role          = Support

current:
  Glock
  Armor 34

plan:
  AK47
  Kevlar+Helmet
  Smoke
  Flash
  HE

reason:
  team_full_buy
  support_role
  rifle_missing

fallback:
  Galil
```

Team-level example:

```text
Team Economy:
strategy = FullBuy

AWP allocation:
Bot05

Defuse Kit:
Bot02
Bot04
```

# Relationship to Existing Phases

```text
Phase 7  Tactical Planner
↓
Phase 8  Team Director
↓
Phase 8.5 Economy & Buy Planner
↓
Phase 9  Persistent Experience
```

Phase 8.5 must not depend on Phase 9.

Future Experience integration may learn from weapon success, role/weapon compatibility, map-specific preferences, and economy strategy success, but this is outside Phase 8.5.

# Deferred Work

```text
advanced economy prediction
enemy economy prediction
drop weapon requests
human teammate buy negotiation
learned weapon preference
map-specific weapon meta
professional-team economy simulation
dynamic weapon pricing assumptions
purchase AI based on opponent profiling
```

# Future Extensions

Future adaptation may support:

```text
Experience
↓
Buy Strategy

Example:
this map / this role / this economy
Rifle A success rate > Rifle B
```

Avoid overfitting to a fixed meta.

# Completion Definition

At the end of Phase 8.5, AstraBot can autonomously:

```text
round begins
↓
evaluate team economy
↓
choose Eco / Force / Full Buy etc.
↓
inspect role
↓
inspect current equipment
↓
generate buy plan
↓
execute purchases
↓
verify success from inventory
↓
use bounded fallback on failure
```

The buy AI must remain an independent Economy / Buy Planner rather than being embedded into Combat or Tactical Planner logic.
