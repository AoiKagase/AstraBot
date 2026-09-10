# AstraBot Full Source Review — Baseline `964d0da8e7d088fa4d9408207f66d1c58052fdd2`

## Scope

This review uses commit:

```text
964d0da8e7d088fa4d9408207f66d1c58052fdd2
```

as the baseline.

Reviewed areas include:

- `src/core`
- `src/host`
- `src/nav`
- `src/adapter/cstrike`
- `src/adapter/metamod`
- build/test structure
- lifecycle and identity handling
- perception / world model
- combat
- action / tactical / team planning
- economy
- persistent experience
- advanced learning
- Metamod integration
- code readability and test strategy

The goal of this report is to identify issues that should be addressed before final runtime integration and live-server acceptance.

---

# Executive Summary

The overall architecture is strong.

No fundamental redesign is required.

The strongest parts are:

```text
generation-safe identity
immutable NavMesh
deterministic A*
selected-edge evidence
Engine/Core boundary
imperfect-information World Model
bounded state
fail-closed validation
Persistent Experience separation
```

The largest remaining project risk is not the quality of the individual AI modules.

It is:

> connecting the already implemented P5–P11 systems into one real autonomous runtime loop inside the Metamod adapter.

Current overall assessment:

```text
Core architecture         A
Nav                       A
Identity/lifecycle        A
Perception                A
Combat                    A-
Action/Tactical/Team      A-
Economy                   A-
Experience                A-
Advanced Learning         B+
Metamod adapter           B+
Runtime integration       C / incomplete
Live play readiness       incomplete
```

---

# Severity Summary

| Severity | Count | Summary |
|---|---:|---|
| BLOCKER | 0 | No fundamental Core blocker identified |
| HIGH | 1 | High-level AI modules are not yet integrated into the autonomous Metamod runtime loop |
| MEDIUM | 4 | ContextualDanger lifecycle, OpponentProfile round metadata, Metamod interface strictness, Adaptive Route semantic overlap |
| LOW | 3 | Experience hash validation, persistence durability, traffic weighting / semantic cleanup |

---

# HIGH-01 — High-Level AI Is Not Yet Connected to the Autonomous Runtime Loop

## Finding

The major P5–P11 systems exist as Core implementations, but they are not yet fully driven by the Metamod runtime loop.

`LifecycleCoordinator` currently owns/runtime-drives systems such as:

```text
PlayerRegistry
BotAgentRegistry
MovementCoordinator
NavConsole
WorldModel
Vision
Sound
TeamRoster
Combat lifecycle helpers
```

However the following high-level systems are not yet clearly owned and scheduled as part of the autonomous runtime loop:

```text
ActionPlanner
TacticalPlanner
TeamDirector
Economy / BuyPlanner
ExperienceModel / ExperiencePipeline
ContextualDangerModel
OpponentProfileModel
advanced learning systems
```

The practical consequence is:

```text
AI components exist
↓
offline tests exist
↓
but no complete autonomous per-frame/per-tick orchestration yet
```

## Required Direction

Before adding more major features, create or finalize a runtime orchestration layer.

Target flow:

```text
StartFrame
↓
Perception
↓
WorldModel
↓
TeamDirector
↓
TacticalPlanner
↓
ActionPlanner
↓
Combat / Navigation
↓
Command Composition
↓
PLAYER_RUN_MOVE / engine dispatch
```

Economy and Experience should run at appropriate lifecycle/event frequencies rather than every frame.

## Recommendation

Treat this as the highest-priority integration task before final live acceptance.

Do not solve it by putting all logic into one giant `LifecycleCoordinator::startFrame()` function.

Prefer explicit scheduler/orchestrator boundaries.

---

# MEDIUM-01 — `ContextualDangerModel` Has No Explicit Map-Session Lifecycle

## Finding

`ContextualDangerKey` identifies danger by concepts such as:

```text
area
team
approach
enemy weapon class
likely enemy area
```

but does not encode map identity or map generation.

The model also lacks an explicit:

```text
beginMap(...)
reset(...)
```

style lifecycle contract.

If one instance survives a map transition, identical area IDs on a new map could inherit stale contextual danger.

Example:

```text
de_dust2 Area 42
↓
map change
↓
de_inferno Area 42
```

must not reuse the same contextual danger state.

## Recommendation

Add an explicit map-session lifecycle before runtime integration.

Preferred direction:

```text
ContextualDangerModel::beginMap(MapGeneration)
ContextualDangerModel::reset()
```

or an equivalent generation-safe design.

Keep this separate from Persistent Experience.

```text
Persistent Experience
= persistent, map-identified knowledge

ContextualDanger
= current map-session tactical state
```

---

# MEDIUM-02 — `OpponentProfile::round` Metadata Is Stale After Map-Session Lifetime Change

## Finding

Opponent profiles were changed from round-local lifetime to map-session lifetime.

That change is correct.

However the per-profile `round` field appears to be initialized on creation and not necessarily updated on later accepted observations.

This can produce:

```text
Round 1
profile created
profile.round = 1

Round 2
profile retained
new observation accumulated
profile.round still = 1

Round 3
profile retained
new observation accumulated
profile.round still = 1
```

The statistical profile itself remains valid, but the metadata becomes misleading.

## Recommendation

Choose one semantic explicitly.

### Option A — `round` means latest observed round

Update on every accepted observation:

```text
profile.round = observation.round
```

### Option B — remove per-profile `round`

If the profile lifetime is map-session scoped and the model already tracks current round globally, removing this redundant field may be cleaner.

Option B is preferred unless per-profile latest-round metadata is actually needed.

---

# MEDIUM-03 — Metamod Interface Version Matching Is Too Strict

## Finding

`Meta_Query()` uses exact string equality against `META_INTERFACE_VERSION`.

Conceptually:

```cpp
strcmp(interfaceVersion, META_INTERFACE_VERSION) == 0
```

is required.

This reduces compatibility with otherwise compatible Metamod / Metamod-P minor interface versions.

## Impact

If AstraBot intentionally targets one pinned Metamod-P version only, impact is limited.

If compatibility with newer compatible Metamod-P or other compatible implementations is desired, this is unnecessarily strict.

## Recommendation

Confirm project policy:

### ReGameDLL + pinned Metamod-P only

Current behavior can remain.

### Wider Metamod compatibility

Implement proper major/minor compatibility rules based on upstream Metamod API behavior.

Do not change this blindly without verifying upstream interface semantics.

---

# MEDIUM-04 — Adaptive Route `Danger` and `Exposure` Semantics Overlap

## Finding

Adaptive route cost currently uses learned danger and an exposure-like value derived from overlapping experience signals.

Current conceptual shape is similar to:

```text
danger
= learned team danger

exposure
= encounter rate
+ grenade threat
+ death rate
```

This can cause learned death/encounter information to influence route cost twice.

## Recommendation

Long-term semantic separation should be:

```text
Danger
= learned actual tactical harm / encounter risk

Exposure
= geometric visibility / sightline openness / lack of cover
```

Future geometric information may include:

```text
visibility graph
sightline openness
cover availability
portal exposure
```

This is not a blocker.

Do not perform a large refactor unless the next route-learning task requires it.

---

# LOW-01 — Human / Bot Traffic Weighting Becomes Flat in Adaptive Route Cost

## Finding

Persistent Experience intentionally separates:

```text
human traffic
bot traffic
```

but Adaptive Route combines them approximately as:

```text
humanTraffic + botTraffic
```

The same is true for some familiarity calculations.

This may be correct if weighting was fully applied during Experience update.

However, preserving separate fields suggests future route-level weighting may be useful.

## Recommendation

Verify semantics.

If update-time weighting is authoritative, document it.

Otherwise consider configurable:

```text
humanTrafficWeight
botTrafficWeight
```

in route policy.

No immediate change required if current tests and intended semantics agree.

---

# LOW-02 — `MapIdentity` Hash Structural Validation Is Minimal

## Finding

Map identity correctly includes strong map/NAV identity information.

However a state such as:

```text
hasBspHash = true
hash = all zero
```

may still be structurally accepted.

This is not a memory-safety issue because the hash has fixed size.

## Recommendation

Optional hardening:

```text
hasHash == true
→ reject all-zero hash
```

Apply only if it improves consistency without complicating migration/tests.

---

# LOW-03 — Experience Persistence Is Safe Logically but Not Fully Power-Loss Durable

## Finding

Persistence uses a good replacement sequence:

```text
write temp
↓
remove/rotate backup
↓
primary → backup
↓
temp → primary
```

This is good protection against normal process failure and corruption.

However OS-level durability primitives such as:

```text
fsync
FlushFileBuffers
directory fsync
```

are not part of the current guarantee.

## Recommendation

This is acceptable for a game bot Experience store.

Do not add platform-specific durability complexity unless real-world corruption demonstrates a need.

Document the durability guarantee precisely.

---

# Nav Core Review

## Verdict

Strong.

No major issue identified.

Positive properties:

```text
deterministic A*
selected edge preservation
cost evidence preservation
closed-node reopen support
bounded allocation
custom cost policy
Traversal identity
immutable NavMesh
```

The route result preserves the actual selected edge rather than only parent area IDs.

This is critical for future parallel traversal cases:

```text
A --Walk--> B
A --GapJump--> B
```

The selected traversal is not lost during reconstruction.

No major Nav Core rework is recommended.

---

# Local Navigation / Motion Review

## Verdict

Strong architectural separation.

Current structure separates concepts such as:

```text
walk
crouch
jump
ladder
ladder exit
blocker wait
door wait
ground probe
recovery
traversal constraints
```

This preserves the intended architecture:

```text
Nav Route
≠
Local Navigation
≠
Motion Primitive
```

This is a good foundation for future:

```text
GapJump
EdgeTraverse
NarrowPassage
LongJump
AirControl
```

No major change required.

---

# Perception Review

## Verdict

Strong.

Important safeguards include:

- generation checks
- map/round/tick validation
- entity serial validation
- alive/removal validation
- fail-closed trace behavior
- no raw hidden engine truth leaking into the World Model
- controlled visibility handling

The Perception boundary remains one of the strongest areas of AstraBot.

No major change required.

---

# Combat Review

## Verdict

Strong.

Positive properties:

```text
portable WeaponSnapshot
no SDK weapon objects in Core
current-map/tick revalidation
BotAgent binding checks
joined/alive checks
target-change cadence reset
DirectFire information boundary
Tap/Burst/FullAuto support
```

Target changes correctly invalidate stale cadence/attack state.

No major Combat Core issue identified.

---

# Action / Tactical / Team Review

## Verdict

Architecturally sound.

The boundaries remain:

```text
Tactical Planner
↓
Intent

Team Director
↓
Role / Objective Assignment

Action Planner
↓
Executable Action

Combat / Navigation
↓
low-level execution
```

This is the correct direction.

The main remaining issue is runtime orchestration, not Core design.

---

# Economy Review

## Verdict

Strong.

Important property:

```text
command dispatched
≠
purchase succeeded
```

Purchase success is verified by re-observing inventory.

Retry behavior is bounded.

The FullBuy + SMG → Rifle upgrade regression has already been corrected.

No major change required.

---

# Persistent Experience Review

## Verdict

Strong.

Positive properties:

```text
Experience separated from NavMesh
human/bot separation
decay
map identity
versioned persistence
backup/recovery
migration
bounded processing
deterministic behavior
```

Do not push all Experience fields directly into route cost.

Maintain semantic separation between:

```text
route-cost experience
tactical-planning experience
combat-learning experience
```

---

# Advanced Learning Review

## Verdict

Good, with lifecycle cleanup recommended.

Positive systems include:

```text
ContextualDangerModel
OpponentProfileModel
Wallbang
SuppressiveFire
Traversal learning
adaptive route integration
```

Wallbang correctly relies on belief/last-known information and does not require hidden current enemy position.

SuppressiveFire remains conceptually separate from exact player targeting.

Primary recommended cleanup:

```text
ContextualDanger map-session lifecycle
OpponentProfile round metadata
```

---

# Metamod Review

## Current Positive Change

User Message ID resolution is no longer necessarily required to succeed during initial attach.

Retry later in lifecycle is the correct direction.

## Remaining Policy Question

The adapter still strongly assumes ReGameDLL/newapi availability.

This is acceptable if AstraBot officially targets:

```text
ReHLDS / HLDS
+
Metamod-P
+
ReGameDLL_CS
```

If Vanilla GameDLL compatibility becomes a requirement, revisit:

```text
newapi_table required
hook table pointer equality
strict interface version requirements
```

Do not generalize prematurely.

---

# Code Readability

Architecture is clean, but source formatting is currently compact in many files.

Final project policy already intends:

```text
one statement per line
real tab indentation
tab width = 4
braces on separate lines
comments focused on "why"
readable multiline conditions
```

Do not perform repository-wide readability cleanup during active feature development.

Perform it in the dedicated final cleanup phase as a separate commit from functional changes.

---

# Test Strategy Review

The test architecture is excellent but expensive.

Current categories include concepts such as:

```text
unit
simulation
adapter
replay
fuzz
differential
live documentation
```

Recommended final cleanup:

```text
fast
integration
replay
fuzz
live
```

CTest labels should allow:

```text
development
→ focused / fast

phase gate / CI
→ full + replay + fuzz
```

Do not rerun the identical full test suite for the same Git tree merely because of:

```text
commit
merge
branch deletion
```

Only rerun when code/build inputs changed or the merge result differs.

---

# Recommended Fix Order

## Priority 1 — Runtime Integration

Highest priority.

Connect:

```text
TeamDirector
TacticalPlanner
ActionPlanner
Combat
Economy
Experience
Advanced Learning
```

into a real scheduled runtime loop.

Do not put all logic into one monolithic frame function.

Design explicit update frequencies and event-driven triggers.

---

## Priority 2 — ContextualDanger Map Lifecycle

Add explicit map-session lifecycle handling before runtime integration.

---

## Priority 3 — OpponentProfile Round Metadata

Either:

```text
update round on each accepted observation
```

or preferably:

```text
remove redundant per-profile round metadata
```

if it has no clear semantic use.

---

## Priority 4 — Confirm Metamod Compatibility Policy

Decide whether AstraBot is:

```text
ReGameDLL-first / pinned Metamod-P
```

or intends broader compatibility.

Only relax interface/newapi requirements if broader compatibility is an actual goal.

---

## Priority 5 — Adaptive Route Semantic Cleanup

Later, separate:

```text
learned Danger
```

from:

```text
geometric Exposure
```

Do not block integration on this.

---

# Codex Instructions

Use this review as an audit input, not as an instruction to refactor everything immediately.

Before changing code:

1. Verify every finding against the current checkout.
2. Current checkout is the source of truth.
3. If a finding is already fixed after `964d0da8...`, do not reimplement it.
4. Prefer minimal targeted fixes.
5. Do not change public behavior without tests.
6. Do not mix formatting cleanup with functional fixes.
7. Do not create unnecessary new phases/task numbers.
8. Preserve deterministic/bounded behavior.
9. Preserve Core/Adapter boundaries.
10. Keep Persistent Experience and map-session tactical state separate.

For each accepted fix, report briefly:

```text
Finding
Decision
Files changed
Tests added/updated
Validation result
```

---

# Final Review Verdict

```text
Fundamental redesign required: NO

Core quality:
GOOD / STRONG

Primary remaining risk:
runtime integration

Recommended next major activity:
integrate existing AI systems into the real Metamod runtime loop,
then perform live-server acceptance.
```
