# AstraBot Re-Review Report — `a811135abc60158d7619e26f27e9e2bff2db41c7`

## Scope

This re-review compares commit:

`a811135abc60158d7619e26f27e9e2bff2db41c7`

against the previous full-source review baseline:

`964d0da8e7d088fa4d9408207f66d1c58052fdd2`

The purpose is to identify which previous findings are closed, improved, deferred, or still open.

## Executive Summary

Most major findings from the previous full review have been addressed.

Key improvements:

- Runtime orchestration now exists.
- `ContextualDangerModel` now has map-session lifecycle handling.
- stale `OpponentProfile::round` metadata has been removed.
- learned Danger and geometric Exposure are separated.
- Human/Bot traffic weighting semantics are documented.
- CTest labeling has been introduced.
- later phase ordering has been cleaned up.

The largest remaining functional gap is now narrower:

> The runtime orchestrator exists, but the real adapter-owned `RuntimeActorInput` provider still needs to be wired so the implemented AI systems can autonomously drive live bots.

Current assessment:

```text
Core architecture            A
Nav                          A
Identity/lifecycle           A
Perception                   A
Combat                       A-
Action/Tactical/Team         A
Economy                      A-
Experience                   A-
Advanced Learning            A-
Runtime orchestration        B+ / mostly implemented
Metamod live integration     B / pending live validation
Live play readiness          not yet proven
```

## Previous Finding Status

| Previous Finding | Current Status |
|---|---|
| High-level AI not integrated into runtime | IMPROVED — RuntimeOrchestrator added; real input provider remains |
| ContextualDanger map lifecycle missing | CLOSED |
| OpponentProfile round metadata stale | CLOSED |
| strict Metamod interface/newapi requirements | DEFERRED to live integration |
| Danger / Exposure overlap | CLOSED |
| Human/Bot traffic weighting ambiguity | IMPROVED / semantics documented |
| MapIdentity hash hardening | STILL LOW / optional |
| persistence power-loss durability | STILL LOW / acceptable |
| Test-suite inefficiency | IMPROVED — CTest labels added |

# HIGH-01 — Runtime Integration Is Mostly Implemented

## Previous State

At `964d0da8...`, major AI systems existed but were not driven by one autonomous runtime loop.

Missing chain:

```text
Perception
↓
World Model
↓
Team Director
↓
Tactical Planner
↓
Action Planner
↓
Combat / Navigation
↓
Command Composition
↓
Engine dispatch
```

## Current State

A dedicated `RuntimeOrchestrator` now exists.

Its scheduling path conceptually covers:

```text
Perception
↓
Learning
↓
Team Director
↓
Tactical Planner
↓
Action Planner
↓
Combat
↓
Navigation
```

The scheduler uses bounded cadences plus event-driven invalidation.

`LifecycleCoordinator::startFrame()` now invokes the orchestrator and forwards navigation output toward the movement stack.

## Remaining Gap

The runtime still depends on an adapter-owned input provider:

```text
real live actor state
↓
RuntimeActorInput
↓
RuntimeOrchestrator
```

Without that provider, the scheduler exists but cannot autonomously drive real actors.

## Verdict

```text
Previous severity: HIGH
Current status: IMPROVED
Remaining risk: integration, not architecture
```

## Required Next Step

Before live acceptance:

1. finalize the real adapter-owned `RuntimeActorInput` provider
2. validate map/round/player generation
3. provide self state, World Model, weapon state, team state, tactical context, and objective state consistently
4. prevent stale input from reviving retired actors
5. ensure runtime output reaches command composition and engine dispatch

Do not collapse all logic into one giant `startFrame()` function.

# MEDIUM-01 — ContextualDanger Map Lifecycle

## Previous Finding

`ContextualDangerModel` lacked explicit map-session lifecycle handling.

## Current State

Explicit lifecycle handling has been added.

The model now supports map-generation-aware initialization/reset and rejects wrong-map observations.

Runtime orchestration also resets/reinitializes contextual danger on map transitions.

## Verdict

`CLOSED`

Keep semantics separate:

```text
Persistent Experience
= long-lived map-identified knowledge

Contextual Danger
= current map-session tactical state
```

# MEDIUM-02 — OpponentProfile Round Metadata

## Previous Finding

After switching Opponent Profile lifetime from round-local to map-session-local, per-profile `round` metadata could become stale.

## Current State

The redundant per-profile round metadata has been removed.

Round validity is handled by the model/current input rather than duplicated inside each profile.

## Verdict

`CLOSED`

# MEDIUM-03 — Metamod Interface Strictness

The adapter still appears intentionally strict about:

```text
Metamod interface expectations
ReGameDLL/newapi availability
hook table expectations
```

This is acceptable if AstraBot officially targets:

```text
ReHLDS / HLDS
+
Metamod-P
+
ReGameDLL_CS
```

The live-integration plan now explicitly includes:

- `Meta_Attach()` diagnostics
- User Message ID resolution timing
- newapi/hook table verification
- plugin load/reload
- map change validation

## Verdict

`DEFERRED`

Resolve through real Metamod-P/ReGameDLL live validation. Do not relax requirements speculatively.

# MEDIUM-04 — Danger / Exposure Semantic Overlap

## Previous Finding

Adaptive route cost reused learned experience for both Danger and Exposure.

## Current State

Exposure has been separated from learned danger.

Conceptually:

```text
danger
= learned tactical danger

exposure
= separate geometric/provider-based signal
```

When no exposure provider exists, exposure remains neutral instead of being synthesized from learned deaths/encounters.

## Verdict

`CLOSED`

Future Exposure may use:

```text
visibility graph
sightline openness
cover availability
portal exposure
```

# LOW-01 — Human / Bot Traffic Weighting

The intended semantic is now documented:

> Human/Bot weighting is applied once during Experience update, and route cost consumes already-weighted stored values.

Therefore route policy should not apply the same weighting again.

## Verdict

`IMPROVED / EFFECTIVELY CLOSED`

Recommended final cleanup: document this clearly at the Experience model boundary.

# LOW-02 — MapIdentity Hash Structural Hardening

Optional hardening remains:

```text
hasHash == true
→ reject all-zero hash
```

Only add this if it does not complicate migration or test fixtures.

## Verdict

`STILL LOW`

Do not block integration on this.

# LOW-03 — Persistence Power-Loss Durability

Persistence remains logically robust through:

```text
temp write
backup rotation
primary replacement
corruption recovery
version/schema checks
```

It is not necessarily a full fsync/FlushFileBuffers transactional system.

## Verdict

`ACCEPTABLE`

Do not add platform-specific durability complexity unless real failures justify it.

# RuntimeOrchestrator Review

This is the most important architectural improvement since the previous review.

Positive properties:

```text
Learning
Team coordination
Tactical planning
Action planning
Combat decisions
Navigation decisions
```

run through an explicit orchestration layer at different cadences.

This avoids:

```text
giant Bot::Think()
```

and:

```text
all AI systems every frame
```

When wiring the live provider, preserve:

```text
map generation
round generation
PlayerId generation
tick/time semantics
```

and continue using portable snapshots/contracts.

# Test Strategy Improvements

CTest labeling has been introduced.

Preferred usage:

```text
development:
focused tests
fast label

phase completion:
canonical full suite once

CI/gates:
integration + replay + fuzz as appropriate
```

Do not rerun the same full suite for an unchanged Git tree merely because of commit, merge, or branch deletion.

## Verdict

`IMPROVED`

# Phase Ordering Review

Current later-phase ordering is appropriate:

```text
P11 Advanced Learning / Traversal
P12 Final Integration / Live Acceptance
P13 Source Cleanup / Release
P14 AMXX API
```

Keeping AMXX API late is preferred because it is a public integration surface over otherwise stabilized internals.

# Remaining Priority Order

## Priority 1 — Complete RuntimeActorInput Wiring

Highest priority.

```text
real live actor state
↓
RuntimeActorInput
↓
RuntimeOrchestrator
↓
Planner/Combat/Nav outputs
↓
BotCommand
↓
engine
```

## Priority 2 — P12 Live Metamod Load Validation

Known historical issue:

```text
Meta_Attach() returns 0
```

Focus on:

```text
Meta_Query / Meta_Attach
newapi/hook tables
User Message registration timing
plugin reload
map change
```

## Priority 3 — Real `.nav` Validation

Use lawful real ReGameDLL/CS-generated NAV data.

Validate:

```text
load
area count
connections
nearest area
route
ladder
traversal
```

## Priority 4 — Live Movement / Combat / Team AI

Verify:

```text
walk
crouch
jump
ladder
stuck recovery
perception
reaction
aim
DirectFire
Tap/Burst/FullAuto
reload
Action Planner
Tactical Planner
Team Director
Economy
```

## Priority 5 — Performance / Stability

Measure 1/8/16 Bot scenarios.

Track:

```text
Perception
A*
Local Navigation
Combat
Action Planner
Tactical Planner
Team Director
Experience update
runtime orchestration
```

## Priority 6 — P13 Cleanup

After functional integration/live acceptance:

```text
source readability
one statement per line
real tabs / tab width 4
braces on separate lines
comments focused on why
phase-numbered production filenames removed
responsibility-based snake_case filenames
rename-only commits separated from formatting commits
```

## Priority 7 — P14 AMXX API

Keep this last or near-last.

# Re-Review Verdict

```text
Fundamental redesign required: NO

Previous HIGH runtime finding:
MOSTLY RESOLVED

ContextualDanger finding:
CLOSED

OpponentProfile metadata finding:
CLOSED

Danger/Exposure finding:
CLOSED

Metamod compatibility:
DEFERRED TO LIVE VALIDATION

Primary remaining technical risk:
LIVE RUNTIME INTEGRATION

Recommended next major activity:
finish RuntimeActorInput wiring and begin P12 live acceptance
```

# Codex Instructions

Use this report as re-review input only.

Before changing code:

1. verify every remaining finding against the current checkout
2. do not reimplement already-fixed items
3. keep fixes minimal
4. preserve deterministic/bounded behavior
5. preserve Core/Adapter boundaries
6. do not mix formatting cleanup with functional integration
7. do not invent new phase/task numbers unless current plans require them
8. prioritize live runtime wiring over adding more features

For each change, report briefly:

```text
Finding
Decision
Files changed
Tests
Result
```
