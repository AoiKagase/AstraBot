# AstraBot — Phase 6 to Completion Plan

This document is the English roadmap index for the phases after the Phase 5
Combat Baseline. Each phase has its own detailed plan file.

## Overview

After Phase 5, AstraBot is expected to provide:

```text
Perception
↓
World Model
↓
Target Selection
↓
Aim / Reaction
↓
DirectFire
↓
Fire Cadence
↓
Reload / Basic Weapon Handling
```

The remaining roadmap adds:

```text
Action Planning
↓
Tactical Planning
↓
Team Coordination
↓
Persistent Experience
↓
Adaptive Navigation
↓
Advanced Learning
↓
Final Integration / Offline Gate
↓
Source Cleanup / Release Preparation
↓
AMXX Integration
```

## Global principles

- Do not build one giant implementation spanning multiple phases.
- Never obtain an opponent's hidden current position outside the World Model
  information boundary.
- Keep navigation, experience, and planners separate.
- Preserve deterministic testability.
- Continue Windows and Linux x86 CI.
- Keep live-server validation separate from offline gates.
- Target the pinned Metamod-P build and exact `META_INTERFACE_VERSION "5:13"`.
- Apply Human/Bot Experience weights at update time only; do not apply them
  again in Adaptive Route.
- Guarantee Experience atomic replace plus backup recovery, without claiming
  OS-level power-loss durability.
- Keep Runtime single-primary while using bounded slot-oriented internal state.
- Do not run HLDS/ReHLDS live validation before project-wide Finish is confirmed.
- Do not pull advanced features forward into a baseline phase.

## Phase documents

- [Phase 6 — Action Planner](phase-6-action-planner.md)
- [Phase 7 — Tactical Planner](phase-7-tactical-planner.md)
- [Phase 8 — Team Director](phase-8-team-director.md)
- [Phase 9 — Persistent Experience](phase-9-persistent-experience.md)
- [Phase 10 — Adaptive Tactical Navigation](phase-10-adaptive-tactical-navigation.md)
- [Phase 11 — Advanced Learning and Traversal](phase-11-advanced-learning-and-traversal.md)
- [Phase 12 — Final Integration and Live Acceptance](phase-12-final-integration-and-live-acceptance.md)
- [Phase 13 — Final Source Cleanup and Release Preparation](phase-13-source-cleanup-and-release.md)
- [Phase 14 — AMX Mod X API](phase-14-amxx-api.md)

## Final AstraBot capability target

At completion, AstraBot should consistently:

```text
SEE
↓
REMEMBER
↓
ESTIMATE
↓
PLAN
↓
COORDINATE
↓
MOVE / FIGHT
↓
OBSERVE RESULT
↓
LEARN
```

The target capability includes:

- NavMesh movement;
- Walk, crouch, jump, and ladder traversal;
- future GapJump and NarrowPassage capabilities;
- visual, audio, and teammate reports;
- an imperfect-information World Model;
- enemy beliefs;
- tap, burst, and full-auto combat;
- objective actions;
- tactical planning;
- team roles;
- persistent map learning;
- experience-aware routing;
- human traversal learning;
- optional Wallbang; and
- optional AMXX control.

## Project-wide deferred and optional features

The following are not required for the AstraBot baseline completion:

```text
LLM tactical director
Deep RL
Neural aim
full opponent fingerprinting
advanced economy optimization
all Zombie/Custom MOD compatibility
generic GoldSrc MOD compatibility
NPC support
```

These remain candidates for future work after the AstraBot baseline is
complete.
