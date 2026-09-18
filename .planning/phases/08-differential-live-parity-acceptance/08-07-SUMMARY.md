---
phase: 8
plan: 07
status: implemented-offline-live-pending
requirements-progress: [PAR-03, PAR-04, PAR-06, TEST-04]
---

# Plan 08-07 summary: live action adapter bridge

## Comparator-grounded decisions

- ReGameDLL-CS/ZBot uses `PrimaryAttack()` for combat and C4 plant and
  `UseEnvironment()` for defuse.
- YaPB writes `IN_ATTACK`/`IN_USE` into per-frame button state and passes it
  through `pfnRunPlayerMove`; RealBot uses bounded keypress/FakeClient
  command paths for attack, reload, selection, and use.
- AstraBot keeps the Core SDK-free and translates only at the public
  Metamod/HLSDK/GameDLL boundary.

## Implemented

- Added `ActionAdapter` translation for fire/plant, defuse, reload, and
  locomotion-button preservation.
- Connected `CombatController` and `RoundObjectivePlanner` to bounded live
  edict observations and the existing `RunPlayerMove`/GameDLL command path.
- Added target/team observation fallback from TeamInfo when raw `edict_t.team`
  remains zero; slot reuse resets the cached role.
- Added action sensor/dispatch diagnostics and a required adapter-boundary
  verifier.
- Added C4 planted-bomb/bombsite observation for objective proposal to
  `IN_USE`/`IN_ATTACK` translation.

## Offline evidence

- Fresh MSVC x86 build configuration: `build-action-adapter-x86-1451`.
- Focused CTest: 5/5 passed:
  `astrabot_action_adapter`, `astrabot_compat_actor_command`,
  `astrabot_nav_roam_controller`, `astrabot_locomotion`,
  `astrabot_movement_physics`.
- Required boundary verifier: `LiveActionBoundaryAvailable=true`.
- Final DLL SHA-256: `88B46CDE1BE92D81C42152DA6D85D75C4B3397900A5D357114F9DF114FE22543`.
- FocalSpan: fresh, `stale=false`, 261 files / 2655 symbols.
- CRG incremental graph updated with no reported build errors; its graph was
  built at HEAD and remains advisory for dirty-worktree runtime proof.

## Live evidence

- Final DLL is placed on the stopped Windows HLDS target with a timestamped
  backup. The last live interval used an earlier action-sensor build and began
  at qconsole line `87247` (`18:12:51` startup).
- That interval completed the observation window but produced no flushed
  `action sensor`/`bot action` lines and no fresh Bot attack, C4 plant, or C4
  defuse event. It is not a live pass; qconsole/game-log output was
  buffered/empty for that interval and the server was stopped after the run
  for flush.
- Linux x86 live, pinned reference trace, and complete PAR-06/TEST-04 remain
  pending.

## 2026-09-18 movement-boundary continuation

- Historical live diagnostics showed NavRoam producing a non-zero target/intent while `RunPlayerMove` dispatched with zero post-dispatch velocity; `LocomotionResult::Stuck` was therefore downstream of the public input boundary.
- `InputDispatcher` now synchronizes public `edict_t::v.button` and `edict_t::v.impulse` before calling `pfnRunPlayerMove`; `tests/input_dispatcher_tests.cpp` proves the state is retained.
- Added bounded movement-command diagnostics for forward/side/up/yaw/buttons/msec/targetArea and fixed the `astrabot_fake_client_isolation` CMake target to link the existing `action_adapter.cpp`.
- Fresh Windows x86 Debug build `build-action-adapter-x86-1451`: full CTest `42/42` passed; action-boundary contract passed. DLL SHA-256: `8512034d930ee5245859511c3903a45cfb7ff2d8e960afd40ab7a581a6180880`.
- Live movement and C4 gates remain unverified because the current HLDS launch path terminates before a post-spawn observation window; do not promote this offline fix to live movement or C4 acceptance.

## 2026-09-19 movement and objective boundary continuation

- Separated Nav locomotion direction from action aim. Fire suppression restores
  the locomotion view, while movement is projected into forward/side input
  against the final view angle. Plant and defuse explicitly stop analog
  movement while retaining the required public input button.
- Added directed links to `NavCorridor`, direction-aware portal steering with
  a geometry-consistency fallback for synthetic legacy fixtures, and exposed
  the live corridor index through `LocomotionController` so Nav diagnostics
  report actual portal progress.
- Bombsite targets now use `absmin`/`absmax` centers and reachable overlapping
  Nav areas. Planted bomb discovery filters known non-C4 grenade models while
  retaining the existing model-empty synthetic boundary contract. Plant emits
  the public `use weapon_c4` selection command plus `IN_ATTACK`; defuse emits
  `IN_USE`.
- Fresh Windows x86 Debug and Release complete builds used the HostX86/x86
  MSVC environment and both complete CTest runs passed 42/42. The Python
  contract verifier remains environment-unverified because `py -3` cannot
  create its configured Python process.
- A fresh current-DLL loopback HLDS interval confirmed nonzero post-
  `RunPlayerMove` velocity, sustained origin progress, advancing corridor
  indices, and no server crash during the observation window. C4 possession and
  bombsite discovery are also visible, but no fresh `bot action=plant`,
  `Planted_The_Bomb`, or `Defused_The_Bomb` line was produced; C4 gameplay
  acceptance remains pending.
