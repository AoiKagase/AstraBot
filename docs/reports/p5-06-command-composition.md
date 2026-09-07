# P5-06 — Command composition, adapter, host, and observability

Status: 実装・focused offline verification complete. Real-device acceptance is not performed.

## Scope

P5-06 closes the value-only combat boundary and connects it to the existing
movement/lifecycle transport. Core owns combat view and combat buttons; the
navigation command remains the owner of movement, non-combat buttons, impulse,
and frame duration.

## Implemented contracts

- `BotCommand::weaponSelect` is a value-level request. Zero means keep the
  active weapon selected.
- `composeCommand()` validates both inputs, copies navigation-owned fields,
  replaces only attack/reload buttons, and emits a weapon request only for
  `SwitchWeapon`.
- `CombatObservation` and `toCombatInput()` keep adapter observations free of
  `edict_t`, `entvars_t`, private GameDLL data, ReAPI types, and raw message
  buffers.
- The Metamod movement adapter invokes an adapter-owned weapon-selection
  handler before `pfnRunPlayerMove`; unavailable or rejected selection fails
  closed and suppresses movement dispatch.
- `LifecycleCoordinator` owns per-client attack lifecycle state and clears it
  on map/round/death/disconnect/removal and target replacement boundaries.
- `CombatTrace` records target provenance, action/reason, weapon/ammo/cooldown,
  command construction, transport, host acceptance, and a monotonic sequence as
  value data only.

## Verification evidence

- `astrabot_combat_contract_tests.exe` passed, covering adapter conversion and
  structured combat trace delivery.
- `astrabot_movement_tests.exe` passed, covering accepted and rejected weapon
  selection handler paths.
- `astrabot_adapter_entry_tests.exe` passed, covering lifecycle rejection and
  combat trace emission for an invalid actor.
- Focused CMake builds used the confirmed Visual Studio 2026 x86 developer
  environment with the NMake generator.

Project-wide `Finish` remains undeclared. HLDS/ReHLDS live validation is outside
the pre-Finish offline gate.
