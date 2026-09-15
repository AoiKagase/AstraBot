# Requirements: AstraBot

**Defined:** 2026-09-15
**Core Value:** An administrator can replace the ReGameDLL-CS CSBot runtime with a standalone AstraBot Metamod plugin without changing normal Bot commands, profiles, Nav inputs, or gameplay expectations.

## v1 Requirements

### Plugin and platform

- [ ] **HOST-01**: AstraBot loads as a Metamod-P plugin beside an unmodified ReGameDLL-CS without private ReGameDLL symbols, ReAPI, or DLL patching.
- [ ] **HOST-02**: The plugin exports and validates the required Metamod/HLSDK entrypoints and calling conventions for the supported GoldSrc ABI.
- [ ] **HOST-03**: Windows 32-bit and Linux 32-bit builds are reproducible with separately identified toolchains and SDK inputs.

### Lifecycle and actors

- [ ] **LIFE-01**: AstraBot initializes and retires map, round, Entity-slot, and actor generations without retaining stale references.
- [ ] **LIFE-02**: An administrator can create, join, control, remove, and recreate AstraBot FakeClients without a crash or identity collision.
- [ ] **LIFE-03**: Native ReGameDLL-CS CSBot is suppressed or detected before AstraBot creates managed actors, preventing silent double ownership.
- [ ] **LIFE-04**: Map changes, round restarts, disconnects, death/respawn, and edict reuse invalidate stale observations, commands, routes, and state.

### Compatibility surface

- [ ] **COMP-01**: Existing `bot_*` commands and CVar behavior required for normal CSBot operation are accepted, validated, and reported through the compatibility layer.
- [ ] **COMP-02**: Existing Bot profile input and selection behavior required by CSBot operation remains usable without copying ReGameDLL profile implementation.
- [ ] **COMP-03**: Existing server configuration can enable, disable, add, remove, team-select, and configure Bots through the compatible surface.

### Existing Nav input

- [ ] **NAV-01**: AstraBot can load each supported existing `.nav` version required by ReGameDLL-CS, including versions 1 through 5, through a read-only legacy reader.
- [ ] **NAV-02**: Nav loading validates bounds, IDs, indices, coordinates, connectivity, and allocations transactionally before publishing an immutable snapshot.
- [ ] **NAV-03**: Missing, invalid, or fingerprint-mismatched Nav produces an explicit diagnostic and never silently fabricates geometry or claims parity.
- [ ] **NAV-04**: The Core Nav model and query contracts do not expose legacy file-layout types, leaving a clean extension point for AstraNav.

### CSBot/ZBot runtime parity

- [ ] **PAR-01**: A managed Bot follows existing Nav routes and reproduces required Walk, Crouch, Step, Jump, Drop, Ladder, Door, narrow-passage, stopping, and stuck-recovery behavior.
- [ ] **PAR-02**: A managed Bot reproduces the observable visual, sound, threat, memory, and uncertainty behavior required for normal CSBot play without hidden engine truth.
- [ ] **PAR-03**: A managed Bot reproduces required weapon selection, aiming, firing, reload, ammunition, and damage/death behavior.
- [ ] **PAR-04**: A managed Bot reproduces Counter-Strike objective behavior required by the reference runtime, including attack/defend, bomb, hostage, buy, and round transitions.
- [ ] **PAR-05**: State transitions, radio/chatter, team interactions, and recovery remain actor-specific and do not share hidden state as certainty.
- [ ] **PAR-06**: The runtime remains stable across map changes, round restarts, actor removal, reconnect/reuse, and multiple simultaneous Bots.

### Verification and acceptance

- [ ] **TEST-01**: Portable Core contracts, Nav loading, lifecycle generation, command validation, and deterministic replay have automated tests on Windows/Linux x86.
- [ ] **TEST-02**: Adapter tests verify hook order, FakeClient dispatch, native Bot suppression, compatibility commands, and release export identity.
- [ ] **TEST-03**: A differential evidence set compares AstraBot against the pinned CSBot reference behavior without copying reference source.
- [ ] **TEST-04**: Real-server acceptance verifies the unmodified ReGameDLL-CS plus Metamod-P plugin on Windows x86 and Linux x86 for single- and multi-Bot operation.

## v2 Requirements

Deferred until v1 CSBot parity is accepted.

### AstraNav authoring

- **ASTRA-01**: AstraBot can generate Nav geometry for maps without an existing compatible `.nav`.
- **ASTRA-02**: AstraBot can learn, analyze, edit, validate, and atomically persist Nav data without mutating the immutable runtime graph in place.
- **ASTRA-03**: AstraNav can write a versioned `astranav` file with map/physics fingerprints, optional chunks, migration, corruption handling, and future extension space.
- **ASTRA-04**: AstraNav can enrich legacy `.nav` input with verified traversal, tactical, visibility, acoustic, and ballistic geometry.

### Advanced adaptive AI

- **AI-01**: Persistent experience can alter route policy using deterministic decay while separating human, Bot, session, and round evidence.
- **AI-02**: Learned traversal can activate only after bounded human/Bot success evidence and runtime revalidation.
- **AI-03**: Wallbang can use static penetration geometry plus current weapon, angle, distance, belief, and friendly-fire evaluation without wallhack targeting.
- **AI-04**: Tactical navigation can use cover, peek, choke, exposure, visibility, acoustic, traffic, flank, and adaptive route information.

## Out of Scope

| Feature | Reason |
|---------|--------|
| ReGameDLL-CS source reuse or private linking | Violates the independent plugin and non-copy boundary |
| ReAPI or DLL patch dependency | Changes the approved standard Metamod-P runtime boundary |
| Nav generation during v1 | Reserved for AstraNav after CSBot replacement behavior is stable |
| `.nav` write-back during v1 | Keeps the first runtime read-only and prevents legacy data corruption |
| `astranav` file production during v1 | Requires a separately designed and verified authoring format |
| Wallbang/adaptive experience during v1 | Advanced AI must not obscure whether baseline CSBot parity is complete |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| HOST-01 | Phase 1 | Pending |
| HOST-02 | Phase 1 | Pending |
| HOST-03 | Phase 1 | Pending |
| LIFE-01 | Phase 2 | Pending |
| LIFE-02 | Phase 3 | Pending |
| LIFE-03 | Phase 2 | Pending |
| LIFE-04 | Phase 3 | Pending |
| COMP-01 | Phase 4 | Pending |
| COMP-02 | Phase 4 | Pending |
| COMP-03 | Phase 4 | Pending |
| NAV-01 | Phase 5 | Pending |
| NAV-02 | Phase 5 | Pending |
| NAV-03 | Phase 5 | Pending |
| NAV-04 | Phase 5 | Pending |
| PAR-01 | Phase 6 | Pending |
| PAR-02 | Phase 7 | Pending |
| PAR-03 | Phase 7 | Pending |
| PAR-04 | Phase 7 | Pending |
| PAR-05 | Phase 7 | Pending |
| PAR-06 | Phase 8 | Pending |
| TEST-01 | Phase 1 | Pending |
| TEST-02 | Phase 3 | Pending |
| TEST-03 | Phase 8 | Pending |
| TEST-04 | Phase 8 | Pending |

**Coverage:**

- v1 requirements: 24 total
- Mapped phases: 24
- Unmapped: 0

---
*Requirements defined: 2026-09-15*
*Last updated: 2026-09-15 after initial scope definition*
