---
phase: 7
name: CSBot behavior parity
status: planned
---

# Phase 7 Context: CSBot behavior parity

## Goal

Build the SDK-free behavior layer above the verified Phase 6 navigation and
locomotion contracts. The layer consumes only adapter-provided visible,
audible, gameplay, and lifecycle observations and emits bounded intents. It
does not claim that an intent was dispatched, that a weapon action succeeded,
or that a real server accepted the behavior.

## Locked decisions

- Core remains independent of HLSDK, Metamod-P, ReGameDLL-CS private symbols,
  ReAPI, `edict_t`, SDK `Vector`, and Engine/GameDLL function tables.
- `WorldSnapshot` is immutable value data for one observed frame. It carries
  map, round, tick, actor, entity, and Nav generation identity where relevant.
- Perception may contain only observations an adapter could legally provide:
  visible contacts, audible events, remembered observations, confidence,
  age, and explicit unknown state. Hidden enemy coordinates and omniscient
  certainty are forbidden.
- Behavior, objective, combat, and communication state is actor-scoped. No
  mutable state is shared as if it were a fact about another Bot.
- Every action is an intent or proposal. Dispatch receipts and engine physics
  feedback remain adapter-owned and are not inferred by Core.
- All vectors, counts, durations, confidence values, memory entries, and
  candidate lists have explicit finite upper bounds and failure results.
- Existing NavDocument/NavSnapshot remain read-only. AstraNav generation,
  learning, editing, persistence, Wallbang, and adaptive experience remain
  deferred.
- Clean-room implementation is required. ReGameDLL-CS and AstraBot_bk are
  behavioral/design references only; their classes and source are not copied.
- Phase 7 offline tests may close contract deliverables only. Real
  HLDS/ReHLDS perception, combat, objectives, stability, multi-Bot, and
  differential acceptance remain Phase 8 evidence.

## Phase slicing

1. 07-01: immutable WorldSnapshot, visible/audible observations, confidence,
   memory, and uncertainty.
2. 07-02: actor-specific behavior state machine and objective proposal
   boundary.
3. 07-03: weapon inventory, aiming, fire, reload, damage, and action intents.
4. 07-04: bomb, hostage, buy, round, attack, defend, and rescue objectives.
5. 07-05: radio/chatter and team-report information boundaries.
6. 07-06: bounded behavior scenarios, deterministic replay, and reference
   comparison trace format without copying reference source.

## Code context

- `include/astrabot/runtime/lifecycle.hpp` and
  `include/astrabot/runtime/actor_registry.hpp` provide existing generation
  and actor identity conventions.
- `include/astrabot/runtime/bot_command.hpp` separates Core command values
  from Metamod dispatch and receipts.
- `include/astrabot/nav/nav_snapshot.hpp` and the Phase 6 nav controllers
  provide immutable snapshot and stale-generation patterns.
- `include/astrabot/metamod/input_dispatcher.hpp` remains the adapter-owned
  input boundary and must not be imported by new Core behavior contracts.
- `CMakeLists.txt`, `docs/source-manifest.json`, and the portable/Metamod x86
  CTest targets are the existing integration and provenance patterns.

## Acceptance boundary

Phase 7 evidence must distinguish:

- observation accepted vs. enemy certainty;
- intent produced vs. dispatch receipt;
- planner decision vs. objective completion;
- weapon/fire/reload intent vs. damage or ammunition engine feedback;
- offline deterministic scenario result vs. live server parity.

## Deferred ideas

- Real-server differential traces and single/multi-Bot operation belong to
  Phase 8 (`TEST-03`, `TEST-04`, `PAR-06`).
- AstraNav derived visibility/acoustic/ballistic chunks, Wallbang, learned
  traversal, and adaptive tactical routing remain post-parity work.
- Personality, hidden information, and unverified current Nav facts must not
  be introduced as shortcuts for missing adapter observations.

## Canonical references

- `H:/sourcecode/003.Game/amxmodx/AstraBot/AGENTS.md`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/C_CPP_REFACTOR_RULES.md`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/.planning/PROJECT.md`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/.planning/REQUIREMENTS.md`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/.planning/ROADMAP.md`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/.planning/STATE.md`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/.planning/phases/06-baseline-locomotion/06-CONTEXT.md`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/docs/superpowers/specs/2026-09-15-astrabot-csbot-clone-design.md`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/include/astrabot/runtime/lifecycle.hpp`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/include/astrabot/runtime/actor_registry.hpp`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/include/astrabot/runtime/bot_command.hpp`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/include/astrabot/nav/nav_snapshot.hpp`
- `H:/sourcecode/003.Game/amxmodx/AstraBot/include/astrabot/metamod/input_dispatcher.hpp`
