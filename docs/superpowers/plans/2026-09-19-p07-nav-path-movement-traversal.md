# P07 NAV, Pathfinding, Movement Traversal Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` and `superpowers:test-driven-development`.

**Goal:** Make Compatibility Mode use deterministic CSBot-compatible NAV search, path persistence, movement posture, and traversal boundaries while preserving Enhanced-only behavior.

**Architecture:** Extend the existing SDK-free NAV model/query/follower boundary instead of replacing it. Compatibility path search will preserve reference neighbor order and use explicit route-cost semantics; the existing adaptive candidate rotation and recovery state remain available behind an Enhanced route policy. Runtime navigation will retain the existing P02 scheduler and P06 belief/observation inputs.

**Tech Stack:** C++14, CMake/NMake x86 Debug, CTest, PowerShell fixture scripts, FocalSpan, pinned ReGameDLL-CS source at `b0889847fe6d03898be88acc9e366660efb40ab5`.

**Spec:** `plans/P07_NAV_PATH_MOVEMENT_TRAVERSAL.md` and the user-provided P07 acceptance brief.

## Global Constraints

- Execute P07 only; do not start P08.
- Preserve existing dirty/untracked files in the original checkout.
- Compatibility Mode must not use adaptive route weighting, actor-derived route rotation, or traversal learning side effects.
- Do not change P02 scheduler, P05 state ownership, P06 belief/ground-truth scrubbing, combat, economy, or objective tactical decisions.
- Live HLDS/ReHLDS, PE/Python, ladder physics, jump physics, and exact engine movement remain separate acceptance gates.

## Review Focus

- Equal-cost paths must resolve by reference discovery order; test with reversed area IDs and stored neighbor order.
- Route state must persist across Full Updates and recompute only for goal/path/area/stuck/state invalidation; test update counts and reasons.
- NAV_CROUCH/NAV_JUMP must affect posture/traversal without invoking adaptive geometry; test jump-before-crouch precedence.
- Compatibility and Enhanced controllers must not share route or learning state; test two actors and policy behavior.
- Belief-only last-known goals must route without reintroducing hidden ground truth; test an explicit last-known target fixture.

## Tasks

### Task 1: Reference-compatible NAV model and path query

**Files:** Modify `include/astrabot/nav/nav_query.hpp`, `src/core/nav/nav_query.cpp`, and `tests/nav_query_tests.cpp`; add focused route-cost and deterministic fixture coverage.

- [ ] Add explicit route mode and path-search trace values without changing the existing public model ownership.
- [ ] Preserve NAV direction/connection enumeration order and implement stable equal-cost selection matching `CNavArea::AddToOpenList`.
- [ ] Add reference-derived distance, crouch, jump, and route-danger hooks with unavailable dynamic danger remaining explicit rather than fabricated.
- [ ] Add tests for direct, equal-cost, different-cost, neighbor-order, multi-bot isolation, and adaptive/traversal-learning exclusion.
- [ ] Run the focused NAV query target and inspect output.

### Task 2: Compatibility route policy and path lifecycle

**Files:** Modify `include/astrabot/runtime/nav_roam_controller.hpp`, `src/core/runtime/nav_roam_controller.cpp`, `tests/nav_roam_controller_tests.cpp`, and `tests/runtime_mode_policy_tests.cpp` if needed.

- [ ] Add a small Compatibility/Enhanced route-selection boundary.
- [ ] Keep compatibility route selection deterministic and remove actor-derived selection from the compatibility path.
- [ ] Retain existing enhanced candidate rotation/avoidance behind Enhanced policy.
- [ ] Make route persistence and recompute reasons observable in the existing decision fixture.
- [ ] Use the observation/objective target only; no ground-truth enemy position is introduced.
- [ ] Run focused route-controller and mode-policy tests.

### Task 3: Movement and traversal semantics

**Files:** Modify existing locomotion/traversal files only where the pinned reference and current contracts require; extend `tests/locomotion_tests.cpp`, `tests/jump_drop_tests.cpp`, and `tests/special_traversal_tests.cpp`.

- [ ] Apply NAV_CROUCH and NAV_JUMP metadata with reference precedence and keep movement direction independent from combat aim.
- [ ] Align path-following arrival and current-area semantics with the existing SDK-free portal/floor model.
- [ ] Keep jump/drop and ladder state machines bounded and explicit; record live physics limitations as unverified.
- [ ] Add fixture coverage for jump/cooldown integration, ladder enter/maintain/exit, crouch/narrow passage, and stuck thresholds.
- [ ] Run focused movement/traversal targets.

### Task 4: P07 documentation and gates

**Files:** Add `docs/parity/NAVIGATION_MODEL.md`; modify `docs/parity/STATUS.md`, `PARITY_MATRIX.md`, `SOURCE_MAP.md`, `KNOWN_DEVIATIONS.md`, and `TRACE_SCHEMA.md`.

- [ ] Record the pinned reference inventory, source-of-truth NAV choice, route cost/tie-break, persistence/recompute, movement, jump, ladder, crouch, and stuck semantics.
- [ ] Record exact test fixtures and separate offline implementation evidence from live acceptance.
- [ ] Run focused tests, complete x86 Debug build/CTest, Phase 8 fixture checks, `focalspan update --root .`, and a final diff/check review.
- [ ] Keep P07 `PARTIAL` when PE/Python, live physics, or private/live differential evidence is unavailable.
