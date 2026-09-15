---
phase: 03-fakeclient-and-input-dispatch
status: approved
source: docs/superpowers/specs/2026-09-15-astrabot-csbot-clone-design.md
---

# Phase 3 Context: FakeClient and Input Dispatch

## Goal

AstraBot can create and remove actor-specific FakeClients through the public
GoldSrc engine/GameDLL boundary and dispatch generation-validated movement
input without cross-actor or stale-entity corruption.

## Locked decisions

- ReGameDLL-CS and Metamod-P remain read-only behavioral/ABI references.
- Core remains SDK-free; `edict_t`, `enginefuncs_t`, `DLL_FUNCTIONS`, and
  `Vector` stay in `src/adapter/metamod`.
- All FakeClient creation, GameDLL callbacks, and `pfnRunPlayerMove` calls
  are main-thread-only and bounded to one frame budget.
- Every actor has a stable slot plus lifecycle/entity/actor generations.
  Pointer identity alone is never sufficient for command acceptance.
- Creation, join, input dispatch, removal, and slot reuse use explicit
  state/result/receipt values. A rejected command cannot advance actor state.
- No worker thread calls an engine function and no C++ exception crosses a
  Metamod C ABI callback.
- Native CSBot mixing remains blocked by Phase 2's guard. Phase 3 may only
  mark a FakeClient as AstraBot-owned after the guard allows it.
- Bot commands/CVars/profile compatibility remains Phase 4; Nav and movement
  planning remain later phases.

## Public boundary evidence

The pinned Metamod-P engine table exposes `pfnCreateFakeClient`,
`pfnRunPlayerMove`, `pfnIndexOfEdict`, `pfnPEntityOfEntIndex`, and the
userinfo helpers. The GameDLL table exposes the public
`pfnClientPutInServer`/`pfnClientDisconnect` callbacks. Metamod's hook-table
utility can provide the currently hooked engine and DLL tables. The public
`pfnRunPlayerMove` ABI accepts view angles, movement values, buttons, impulse,
and msec; Core must represent those values without SDK types.

## Verification boundary

- Pure Core tests cover actor state transitions, slot reuse, bounded queue
  behavior, sequence/receipt rejection, and stale generation rejection.
- Adapter tests use fake public function tables and edict fixtures to prove
  create/join/remove/dispatch ordering and multi-actor isolation.
- Windows x86 and Linux x86 build/test checks are required when available.
- This phase does not claim live movement. Real `RunPlayerMove` locomotion
  and HLDS/ReHLDS acceptance remain later evidence.
