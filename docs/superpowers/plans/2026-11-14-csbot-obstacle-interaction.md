# CSBot Obstacle Interaction Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans for inline implementation.

**Goal:** Match pinned CSBot contact-driven breakable handling and touch-open door behavior.
**Architecture:** Public KeyValue material capture plus actor-local contact look/fire, consumed at the existing command boundary.
**Tech Stack:** C++14, GoldSrc public Engine/Metamod-P, Windows/Linux x86 CMake/CTest.
**Spec:** docs/superpowers/specs/2026-11-14-csbot-obstacle-interaction-design.md

## Global Constraints
Independent implementation; pinned reference read-only; no private offsets, NAV writes, deployment or unrelated edits. Preserve current dirty files. Work in the current authorized checkout; no worktree/branch changes.

## Review Focus
Touch callback order; reused entity/actor generations; current local path vs remote goal; grenade remains active after switch request; freeze and enemy-attack precedence.

## Task 1: Adapter reproduction
- [x] Reuse the actor-command fixture in tests/obstacle_interaction_runtime_tests.cpp; add a CMake target and source provenance.
- [x] Build complete Windows x86 baseline and run CTest before adding behavior.
- [x] Run the new target and observe missing contact fire assertion fail.

## Task 2: Contact behavior
- [x] Add bot_behavior_obstacles.cpp and actor/material state; public KeyValue and Touch delegation.
- [x] Capture current local navigation goal; consume look/fire through BotCommandServices before the execution gate.
- [x] Cover material, path, combat, grenade, teardown, freeze and door cases; run focused tests.

## Task 3: Verification
- [x] Run full Windows x86 build/CTest, PE verification separately; Linux x86 build/CTest and ELF verification.
- [x] Review diff against reference contracts, update status with exact evidence and native acceptance limits.

Verification note: Linux combat-policy has the documented pre-existing boundary failure; this task does not alter combat policy. Native acceptance remains open.
