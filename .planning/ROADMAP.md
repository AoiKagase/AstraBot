# Roadmap: AstraBot

## Overview

The first milestone builds a standalone, source-independent Metamod-P plugin that replaces the ReGameDLL-CS CSBot runtime. Work proceeds from ABI and lifecycle safety through FakeClient control, compatibility commands, read-only legacy Nav loading, locomotion, full CSBot behavior, and cross-platform live acceptance. AstraNav authoring and advanced adaptive AI begin only after this parity milestone is accepted.

## Phases

- [x] **Phase 1: Foundation and ABI** - Establish the project toolchain, source boundary, public contracts, and x86 artifact checks. (completed 2026-09-15)
- [x] **Phase 2: Metamod lifecycle and native guard** - Load safely, receive hooks, manage map/round state, and prevent native Bot mixing. (completed 2026-09-15)
- [x] **Phase 3: FakeClient and input dispatch** - Create actor-safe FakeClients and validate command/input/receipt lifecycles. (completed 2026-09-15)
- [x] **Phase 4: CSBot compatibility surface** - Reproduce required `bot_*` commands/CVars, profiles, and server configuration behavior. (completed 2026-09-15)
- [x] **Phase 5: Read-only legacy Nav** - Load and validate existing `.nav` v1-v5 data into an immutable SDK-free model. (completed 2026-09-15)
- [x] **Phase 6: Baseline locomotion** - Execute existing Nav routes with CSBot-compatible movement and recovery. (offline contracts completed 2026-09-16; live real-server acceptance remains pending in Phase 8)
- [ ] **Phase 7: CSBot behavior parity** - Add perception, state machine, combat, objectives, radio/chatter, and round behavior.
- [ ] **Phase 8: Differential and live parity acceptance** - Verify Windows/Linux x86 artifacts and real-server replacement behavior.

## Future Milestones

- **AstraNav:** Nav generation, learning, editing, analysis, atomic persistence, `astranav`, and derived geometry chunks.
- **Adaptive AI:** Experience, learned traversal, tactical route styles, visibility/acoustic data, Wallbang, and advanced team behavior.

## Phase Details

### Phase 1: Foundation and ABI

**Goal**: A reproducible x86 project skeleton has explicit source-origin, SDK, ABI, and verification contracts.
**Depends on**: Nothing (first phase)
**Requirements**: HOST-01, HOST-02, HOST-03, TEST-01
**Success Criteria**:

  1. The project configures and builds a minimal target separately for Windows x86 and Linux x86.
  2. The approved Metamod-P SDK commit and ReGameDLL-CS behavioral reference commit are recorded.
  3. The initial plugin export/ABI contract and non-copy source manifest are testable.

**Plans**: 3/3 plans executed

Plans:

- [x] 01-01-PLAN.md
- [x] 01-02-PLAN.md
- [x] 01-03-PLAN.md

**Wave 1**

- [x] 01-01: Establish CMake/toolchain and x86 build identity.

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 01-02: Define plugin ABI/export and Core/adapter contract tests.

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 01-03: Add source-origin manifest and verification harness.

### Phase 2: Metamod lifecycle and native guard

**Goal**: The plugin loads into an unmodified ReGameDLL-CS server and owns map/round lifecycle without native Bot duplication.
**Depends on**: Phase 1
**Requirements**: LIFE-01, LIFE-03
**Success Criteria**:

  1. Plugin load, attach, map activation, frame dispatch, deactivation, and detach are observable and safe.
  2. Native CSBot is suppressed or a detected incompatibility prevents managed Bot creation.
  3. Map and round generations invalidate stale runtime state.

**Plans**: 3 plans

Plans:
**Wave 1**

- [x] 02-01: Implement Metamod entrypoints and hook table skeleton.

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 02-02: Implement map/round runtime session and generation ownership.

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 02-03: Implement native CSBot guard and runtime diagnostics.

### Phase 3: FakeClient and input dispatch

**Goal**: AstraBot can create and control actor-specific FakeClients with safe, generation-validated input.
**Depends on**: Phase 2
**Requirements**: LIFE-02, LIFE-04, TEST-02
**Success Criteria**:

  1. Bot creation, join, input dispatch, removal, and slot reuse are safe and traceable.
  2. Stale, duplicate, rejected, and actor-mismatched commands cannot advance Bot state.
  3. Multiple actors remain isolated in a fake host and in the real hook path.

**Plans**: 3 plans

Plans:
**Wave 1**

- [x] 03-01: Implement actor registry and FakeClient lifecycle.

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 03-02: Implement safe command and usercmd dispatch boundary.

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 03-03: Add generation/receipt diagnostics and multi-actor isolation tests.

### Phase 4: CSBot compatibility surface

**Goal**: Existing server operators can use the required CSBot commands, Cvars, profiles, and configuration surface.
**Depends on**: Phase 3
**Requirements**: COMP-01, COMP-02, COMP-03
**Success Criteria**:

  1. Required `bot_*` commands/CVars parse and report results compatibly.
  2. Existing profile files can select Bot identity/difficulty/team data.
  3. Enable/disable/add/remove/team/configuration flows do not reach native CSBot unexpectedly.

**Plans**: 3 plans

Plans:
**Wave 1**

- [x] 04-01: Define command/CVar compatibility contracts and parser tests.

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 04-02: Implement profile loading and selection boundary.

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 04-03: Integrate administrator command flows with the Bot runtime.

### Phase 5: Read-only legacy Nav

**Goal**: Existing compatible `.nav` files load transactionally into the immutable SDK-free Nav model.
**Depends on**: Phase 4
**Requirements**: NAV-01, NAV-02, NAV-03, NAV-04
**Success Criteria**:

  1. Supported `.nav` v1-v5 fixtures load with stable IDs, connectivity, and required geometry.
  2. Corrupt, oversized, mismatched, and non-finite input fails without partial publication.
  3. No v1 runtime command writes or edits the source Nav file.

**Plans**: 4 plans

Plans:

- [x] 05-01: Define format-neutral Nav model and validation limits.
- [x] 05-02: Implement bounded legacy `.nav` reader.
- [x] 05-03: Publish immutable Nav snapshot and map fingerprint.
- [x] 05-04: Add codec fixtures, corruption tests, and adapter load diagnostics.

### Phase 6: Baseline locomotion

**Goal**: Managed Bots execute valid Nav routes with the required CSBot movement primitives and bounded recovery.
**Depends on**: Phase 5
**Requirements**: PAR-01
**Success Criteria**:

  1. Bots follow Walk/Crouch/Step/Jump/Drop/Ladder/Door/narrow-passage routes under observed GoldSrc physics.
  2. Stuck, collision, stale route, and rejected input states recover or terminate explicitly.
  3. Route search success is not reported as movement success without dispatch and movement evidence.

**Plans**: 5 plans

Plans:

- [x] 06-01: Implement spatial query, directed links, corridor, and path follower.
- [x] 06-02: Implement Walk/Crouch/Step movement and support validation.
- [x] 06-03: Implement Jump/Drop movement envelopes and feedback.
- [x] 06-04: Implement Ladder/Door/narrow-passage traversal.
- [x] 06-05: Implement stuck recovery, route invalidation, and locomotion evidence.

### Phase 7: CSBot behavior parity

**Goal**: The runtime reproduces the observable CSBot behavior above the locomotion layer.
**Depends on**: Phase 6
**Requirements**: PAR-02, PAR-03, PAR-04, PAR-05
**Success Criteria**:

  1. Bots observe and remember visible/audible world state without hidden information.
  2. Bots select weapons, aim, fire, reload, survive/die, and recover as required by the reference behavior.
  3. Bomb, hostage, buy, attack, defend, state transitions, radio/chatter, and round behavior operate per actor/team.

**Plans**: 6 plans

Plans:

- [x] 07-01: Implement WorldSnapshot, perception, and uncertainty contracts. (offline contract complete 2026-09-16; live acceptance remains in Phase 8)
- [x] 07-02: Implement CSBot state machine and objective planner. (offline contract complete 2026-09-16; live acceptance remains in Phase 8)
- [ ] 07-03: Implement weapon inventory, aiming, firing, reload, and damage integration.
- [ ] 07-04: Implement bomb/hostage/buy/round objective behavior.
- [ ] 07-05: Implement radio/chatter and actor/team information boundaries.
- [ ] 07-06: Add behavior scenario tests and reference comparison traces.

### Phase 8: Differential and live parity acceptance

**Goal**: Windows/Linux x86 artifacts and real-server operation demonstrate CSBot replacement behavior.
**Depends on**: Phase 7
**Requirements**: PAR-06, TEST-03, TEST-04
**Success Criteria**:

  1. Portable, adapter, and Release/export checks pass for both x86 target environments.
  2. Differential evidence covers lifecycle, Nav movement, perception, combat, objectives, and recovery against the pinned reference behavior.
  3. An unmodified ReGameDLL-CS plus Metamod-P server accepts the plugin for 1v1, 2v2, and configured multi-Bot operation without native Bot mixing.
  4. Acceptance records distinguish offline test success from real-server movement, combat, stability, and multi-Bot evidence.

**Plans**: 4 plans

Plans:

- [ ] 08-01: Complete cross-platform artifact and export verification.
- [ ] 08-02: Run differential scenario and replay review.
- [ ] 08-03: Run real-server single/multi-Bot lifecycle and gameplay acceptance.
- [ ] 08-04: Audit v1 closure, documentation, and deferred AstraNav boundary.

## Progress

**Execution Order:**
Phases execute sequentially: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation and ABI | 3/3 | Complete    | 2026-09-15 |
| 2. Metamod lifecycle and native guard | 3/3 | Complete    | 2026-09-15 |
| 3. FakeClient and input dispatch | 3/3 | Complete    | 2026-09-15 |
| 4. CSBot compatibility surface | 3/3 | Complete | 2026-09-15 |
| 5. Read-only legacy Nav | 4/4 | Complete | 2026-09-15 |
| 6. Baseline locomotion | 5/5 | Complete (offline) | 2026-09-16 |
| 7. CSBot behavior parity | 2/6 | In progress | 2026-09-16 |
| 8. Differential and live parity acceptance | 0/4 | Not started | - |
