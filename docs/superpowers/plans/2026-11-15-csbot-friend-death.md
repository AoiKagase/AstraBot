# Friend Death Reactions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline.

**Goal:** Deliver all three friend-death behavioral branches with native order, memory provenance and real navigation.
**Architecture:** SDK-free friend-death decision and qualified enemy reaction memory; existing DeathMsg adapter validates event identities and creates the native initial-position exception. Behavior state/NAV consume the retained goal, and current-enemy public visibility remains separate from event provenance.
**Tech Stack:** C++14, public HLSDK/Metamod-P, MSVC/GCC x86, CMake.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-friend-death-design.md

## Global Constraints
Base18a2cc4. Existing feature branch/checkouts/caches, explicit paths only; preserve user files/frozen evidence, no push/deploy. Core SDK-free; exact7exports. Same VS Hostx86/x86 environment/full build before CTest; OS suites serialized; known Linux combat_policy remains open.

## Review Focus
- Retaliation has no victim FOV/radius condition, excludes inventory snipers, and consumes Hurry then only conditional knife-ambush RNG; rejected Attack still consumes the early return.
- Hunt precedes attacking reposition, including world/friendly killer cases; strict0.4 and exact1second boundaries, native SetDisposition exit cancellation.
- Event-seeded hidden point is immutable and never becomes visible/fire permission; current enemy serial/map/round/actor/time qualifiers and callbacks cannot transfer a receipt to reused entities.
- Lost-sight memory keeps attack reachable for friend events, fixed MoveTo goal/fastest route survives ordinary planning; death/reuse/end/failure/CT planted bomb exit cleanly.
- No duplicate perception/event RNG or frozen callback churn, immediate path invalidation and cached command gates; preserve freeze/crouch/objective controls and native moving fire during IgnoreEnemies.

### Task1: Core event/memory and task
Files: new include/astrabot/compat/friend_death.hpp, tests/friend_death_tests.cpp; state_machine.hpp/.cpp task override support/tests; CMake/source manifest.
Interfaces: FriendDeathInput/Decision + decideFriendDeath(input,random); EnemyReactionMemory lifecycle-qualified enemy/source/position/time, hurry/ignore, attack receipt and pursuit.
- [x] Core tests RED (branch matrix, RNG order/50 boundary, Hunt priority, strict0.4, timers/cancellation/provenance). Implement and GREEN.

### Task2: Runtime reaction integration
Files: behavior death/perception/runtime/scenario/priorities/objectives/combat/header; public observation adapter + observation runtime/shared services/PluginRuntime ports; ownership fixture; new friend_death_runtime_tests.cpp + CMake/manifest.
- [x] Registered DeathMsg test RED. Extend handler to all team/killer cases with callback identity revalidation and reuse existing victim visibility helper with optional FOV.
- [x] Bind memory from existing recognized-source receipt without extra generic lookups. Event enemy visibility uses existing public probes without FOV. Direct event state transition/path clear, native task override, fixed MoveTo goal/fastest route and scenario cancellation.
- [x] Full/cached action tests: hidden killer initial point, no fake visibility/fire, visible retaliation; sniper/reload/knife/hide/flash/smoke; Hunt and stale attack reposition, moving fire/ignore window, native exit cancellation, lifecycle/reuse and CT bomb priority. Relevant tests GREEN.

### Task3: Review and delivery
- [x] One fresh read-only final reviewer; Important/Critical one RED→GREEN fix pass, ledger all decisions/minors.
- [x] Full x86 builds, sequential suites, exact7exports, manifest/ports/ownership/frozen and source/artifact/user identity; update docs, explicit-path commit/local FF and byte identity after merge. Full goal remains active.
