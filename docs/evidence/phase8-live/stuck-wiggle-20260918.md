# Phase 8 Windows x86: ZBot-aligned Stuck recovery and action boundary

Status: `partial`; Movement and no-IdleKick evidence improved, Combat and Objective remain failed.

## Artifact and runtime identity

- Source checkout: `H:\sourcecode\003.Game\amxmodx\AstraBot`
- Built artifact: `build-nav-recovery-x86/astrabot_mm.dll`
- Built/deployed SHA-256: `8CCBF6E10082A7B0806860305D06E8A812931E8767DCB7B3DE60EA3C5A128EC8`
- Deployment backup: `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll.pre-stuck-wiggle-cont-20260918-1702.bak`
- Server: `D:\SteamCMD\cstrike_rehlds`, `hlds.exe`, PID `50436` during observation

## ZBot/YaPB comparison used before implementation

- ReGameDLL-CS `cs_bot_nav.cpp` keeps `m_isStuck`, checks displacement, and calls `Wiggle()` while the path remains active.
- ReGameDLL-CS `cs_bot_pathfind.cpp` calls `Wiggle()` during `MoveTowardsPosition()` and only destroys/rebuilds a path for bounded failure conditions.
- ReGameDLL-CS computes portal-aware path points rather than steering every transition directly to an area center.
- YaPB writes action buttons to `pev->button` and passes them every frame through `engfuncs.pfnRunPlayerMove`; its movement code also retains `IN_USE` for environment interaction.

## Implemented offline

- `NavRoamController` retains the selected corridor, link, target position, intent direction, observed velocity, and route index in `NavRoamDecision` diagnostics.
- A `Stuck` result emits a bounded perpendicular recovery intent for eight frames, restarts the same corridor, and allows at most three recovery attempts before replan.
- Adapter diagnostics now include `target`, `intent`, `observedVelocity`, `corridorAreas`, `corridorIndex`, and `link` fields.
- Focused x86 tests passed: `astrabot_nav_roam_controller`, `astrabot_locomotion`, and `astrabot_compat_actor_command` (3/3).

## Fresh live interval

- qconsole offset: `84273` through `84288`
- Round interval: `17:02:52` `Round_Start` through `17:07:52` `Target_Saved` / `Round_End`
- Offset verifier result: `Movement.Passed=true`, `ReadySamples=256`, `MovingSamples=3`, `IdleKickLines=0`
- Bot-to-Bot attack lines: `0`
- Bot `Planted_The_Bomb` lines: `0`
- Bot `Defused_The_Bomb` lines: `0`

The new interval did not reproduce an IdleKick within `mp_roundtime`, but Movement remains a partial gate because only three moving samples satisfied the cumulative verifier. It does not prove every route succeeds.

## Confirmed missing live adapter boundary

`src/adapter/metamod/plugin_runtime.cpp` currently dispatches only `NavRoamController` movement. It produces `IN_DUCK` and `IN_JUMP` movement buttons; it does not call `CombatController` or `RoundObjectivePlanner`, and it does not generate `IN_ATTACK`, reload, weapon-selection, or `IN_USE` actions. The Core combat/objective contracts therefore remain offline-only and cannot produce live Bot damage or C4 events yet.

This keeps Combat/Objective gates explicitly failed rather than promoting Core-only proposals to live acceptance.
