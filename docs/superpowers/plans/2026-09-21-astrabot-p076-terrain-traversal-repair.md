# AstraBot P07.6 Terrain Traversal Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Repair P07.6 slope, stair-segment, and jump-transition decisions at the SDK-free NAV/Core boundary, with regression evidence and a live-verification preparation record.

**Architecture:** Keep NAV geometry, corridor segment progress, locomotion classification, special traversal, and engine command projection as separate contracts. Derive a transition height from the actual portal/local target surface rather than Area-center averages; preserve existing active-link Jump/Drop/Ladder ownership. Do not add private GameDLL access or alter engine physics.

**Tech Stack:** C++14, portable CTest executables, Windows x86 NMake/MSVC `Hostx86\\x86`, external `de_dust2.nav` through `ASTRABOT_DE_DUST2_NAV`, PowerShell Phase 8 fixtures.

**Spec:** User-provided P07.6 slope/stairs/jump traversal repair request in `C:\Users\SandS\.codex\attachments\8a1d74b3-08e0-4a69-b86a-65dd2121abf5\貼り付けたテキスト.txt`.

## Global Constraints

- Continue P07.6 only; do not start P08 Combat/Aim/Weapon.
- Preserve the current detached worktree `p076-audit-fixes` and unrelated `BotProfile.db`.
- Do not reset, restore, clean, stash, overwrite, or broadly stage existing files.
- Do not raise `maximumStepHeight` to hide the defect, set arrival tolerance to zero, spam `IN_JUMP`, teleport, alter velocity/gravity/movetype, weaken collision, or hardcode de_dust2 Area IDs.
- Keep P02 timing, P03 RNG boundaries, NAV search/corridor capacity separation, route persistence, and objective cache unchanged.
- Do not copy ReGameDLL-CS code; use its behavior as a comparator only.
- Treat synthetic NAV, offline CTest, built DLL identity, and live GoldSrc acceptance as separate gates.

## Review Focus

- A continuous ramp whose shared boundary has no discontinuity must not become `StepTooHigh`; test portal-local surface Z and transition delta.
- A stationary actor on a vertical stair fixture must not consume the next corridor segment; test both first advancement and unchanged-position update.
- A `NAV_JUMP` transition must reach Jump intent before ordinary step rejection; retain the existing regression and add the non-center height assertion.
- A Walk→Jump→Walk route must switch only when the current link changes and must not reinitialize the same controller every update; retain existing intermediate-link coverage and verify JumpDrop’s one-link corridor contract.
- Missing production hull/terrain measurement must remain explicit and must not be reported as live slope/stair/jump acceptance.

## Current Evidence and Root Cause

- The audit fixture reports old R3/R4/R5/R6 failures, but those results are not reused as current proof.
- The current worktree already contains the F02/F03/F04 repairs and existing tests for stationary portal progress, interpolated surface height, Jump attribute precedence, intermediate Jump-link switching, and one-link JumpDrop envelopes.
- Current source still has `portalSteeringPoint()` overloads using `to.northEastZ * 0.5f + to.southWestZ * 0.5f` for overlap portals, while `surfaceZAt()` is already available.
- `LocomotionController::buildIntent()` compares destination/current Area-center surface heights, so a sloped Area can create a false center-to-center rise even when the shared portal has equal floor height.
- The standalone audit harness cannot currently link the worktree because its independent CMake omits `src/core/compat/random_source.cpp`; this is recorded as harness limitation, not product evidence.
- Pinned reference anchors inspected: `ReGameDLL_CS/regamedll/dlls/bot/cs_bot_nav.cpp` (`GetSimpleGroundHeightWithFloor`, `DiscontinuityJump`, `MoveTowardsPosition`) and `game_shared/bot/nav_area.h` (`GetZ(pos)`). Exact behavioral parity remains unclaimed until live verification.

### Task 1: Add RED regression tests for transition surface and slope

**Files:**
- Modify: `tests/nav_query_tests.cpp` near `testInterpolatedSurfaceHeight()`.
- Modify: `tests/locomotion_tests.cpp` near `testStepAndStuckRecovery()`.
- Test target: existing `astrabot_nav_query` and `astrabot_locomotion` CTest targets.

**Interfaces:**
- Consume `astrabot::nav::surfaceZAt`, `NavPathFollower::update`, and `LocomotionController::update`.
- Produce explicit failures for portal Z and continuous-ramp transition classification.

- [x] **Step 1: Add portal-height regression.** Build a sloped destination Area with four corner heights and a shared X/Y portal; assert that the first follower target Z equals `surfaceZAt(destination, target.x, target.y)`, not the destination corner average.
- [x] **Step 2: Add continuous-ramp locomotion regression.** Build Area A rising from Z=0 to Z=60 over X=0..100 and Area B flat at Z=60 over X=100..200; start at `(80,32,48)` with maximum step height 16; assert `IntentReady`, `stepUp == false`, and no `StepTooHigh`.
- [x] **Step 3: Run only `astrabot_nav_query` and `astrabot_locomotion`.** RED observed before production changes: portal target Z and ramp update reported the old average-height behavior.

### Task 2: Add the vertical-stair segment regression

**Files:**
- Modify: `tests/nav_query_tests.cpp` near `testStationaryFollowerDoesNotSkipPortalSegment()`.

**Interfaces:**
- Consume `NavPathFollower::currentIndex()` and `NavPathFollower::update`.
- Produce a stair-specific guard that preserves the existing curved-portal guard.

- [x] **Step 1: Build a 0→16→32 fixture.** Use Area 1 X=0..100/Z=0, Area 2 X=100..140/Z=16, Area 3 X=140..220/Z=32 with directed connections.
- [x] **Step 2: Assert first update.** From `(90,32,0)`, assert `Advanced`, targetArea 2, and currentIndex 1.
- [x] **Step 3: Assert unchanged second update.** Reuse `(90,32,0)`, assert `TargetReady`, targetArea 2, currentIndex 1, and no advance to Area 3.
- [x] **Step 4: Assert real first-step progress.** Move to `(120,32,16)`, assert the follower can then advance toward Area 3 without a false intermediate-segment rejection.

### Task 3: Implement the minimal geometry/transition fix

**Files:**
- Modify: `src/core/nav/nav_query.cpp` portal steering overloads.
- Modify: `src/core/nav/locomotion.cpp` transition-height calculation.
- Modify: `tests/nav_query_tests.cpp` and `tests/locomotion_tests.cpp` only as required by Tasks 1–2.

**Interfaces:**
- `portalSteeringPoint()` returns a local target whose Z is the interpolated destination surface at that exact XY.
- `LocomotionController::buildIntent()` consumes that local target and compares destination surface Z against the current Area surface at the same transition XY.

- [x] **Step 1: Replace overlap-portal average Z.** Compute portal XY first, then set `portal.z = surfaceZAt(to, portal.x, portal.y)` in both generic and direction-aware portal steering paths. Preserve portal inset and direction geometry.
- [x] **Step 2: Replace center-to-center step delta.** Compute `currentFloor = surfaceZAt(*currentArea, target.x, target.y)` and `transitionHeight = target.z - currentFloor`; use this only for walk/step classification and `StepTooHigh`.
- [x] **Step 3: Preserve Jump precedence.** Keep `NAV_JUMP`/active traversal behavior ahead of ordinary `StepTooHigh`; do not turn every high wall into an unconditional Jump. Unavailable real terrain clearance remains explicit.
- [x] **Step 4: Run the two focused targets.** GREEN observed for portal, ramp, and stair regressions.

### Task 4: Verify existing Jump/traversal contracts without broad rewrite

**Files:**
- Inspect only unless a focused regression fails: `src/core/runtime/nav_roam_controller.cpp`, `src/core/nav/jump_drop.cpp`, `src/core/nav/special_traversal.cpp`.
- Test: `tests/nav_roam_controller.cpp`, `tests/jump_drop_tests.cpp`, `tests/special_traversal_tests.cpp`.

- [x] **Step 1: Run existing `NAV_JUMP` precedence, intermediate-link switch, JumpDrop landing, and special traversal tests.**
- [x] **Step 2: No existing traversal test failed; no traversal implementation rewrite was needed. The one-link launch/landing corridor ownership remains in place.
- [x] **Step 3: Do not modify combat, objective selection, team assignment, timing, or RNG.

### Task 5: Full regression and implementation record

**Files:**
- Update: this plan checkboxes and, only if code behavior changed, the relevant existing parity/status document.
- Preserve: audit ZIP/results and all unrelated untracked files.

- [x] **Step 1: Run the full x86 Debug build in the cached NMake/MSVC environment.** `100%` built with cached `Hostx86\\x86\\cl.exe`.
- [x] **Step 2: Run the full registered CTest list and compare registration count before/after.** `54` registered and full CTest exit 0.
- [x] **Step 3: Run external `de_dust2.nav` with required cases: 41→90 Found, explicit 256 ResourceLimit, 41→1438 Found, 5→1520 Found.** Passed with `bspSize=2057288` and `bspIdentityVerified=1`.
- [x] **Step 4: Run P02 timing, P07 navigation, P07.6 contracts/profiler, adapter tests, and Phase 8 fixture tests.** Python contracts, PE x86/7 exports, and Phase 8 PowerShell gates all exited 0.
- [x] **Step 5: Run FocalSpan update/query if available; report its current DB/path limitation if unavailable.** Update was attempted; `db_open=false`, `path_permissions=false`, `index_fresh=false` remains.
- [x] **Step 6: Inspect final diff, stage explicit paths only, commit one narrow terrain repair commit, and do not deploy until build/artifact identity matches.** Committed as `bdf7bfd`; build/deploy DLL identity matches.
- [x] **Step 7: Prepare live A–E procedures with source commit, DLL SHA-256, NAV/BSP identity, and explicit unverified status.** Final DLL is deployed, but no fresh matching-DLL server interval has been run.

## Stop Gate

Stop after slope/stair/Jump traversal repair, regression verification, and live-check preparation. Overall status is `PARTIAL` until fresh HLDS/ReHLDS intervals verify slope, stairs, Jump, negative cases, lifecycle, and multi-Bot performance.
