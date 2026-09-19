---
status: fixing
trigger: "Idle状態でKickされた事からもわかるように、現状チーム参加後に自立行動はしておりません。"
created: 2026-09-18T13:25:00+09:00
updated: 2026-09-18T13:55:00+09:00
---

# Autonomous post-join action diagnosis

## Symptoms

- Expected: after GameDLL team/class acceptance and spawn readiness, each managed FakeClient produces autonomous movement/action and is not removed by the server idle-kick policy.
- Actual: Windows x86 logs show team entry and human damage/death, but no sustained autonomous movement; all four managed Bots were later removed by `Game_idle_kick`.
- Runtime evidence: movement samples report `dispatched=1`, `grounded=0`, `ready=0`; the captured sample also reports `team=0`, `solid=3`, `movetype=3`, and health `100.0`.
- Reproduction: start the current Windows x86 server, add the configured four Bots, allow team/class entry, wait without issuing Bot movement commands, and inspect `qconsole.log`.

## Current Focus

- hypothesis: the post-join action path has at least one confirmed source-level failure: `updateManagedBotMovement()`/`spawnReadiness()` rejects raw `entity->v.team == 0` even after TeamInfo confirms CT/TERRORIST. A separate Nav/locomotion failure may remain after this gate is corrected; deployed DLL provenance is not yet matched to the current source tree.
- test: trace TeamInfo confirmation, actor state transition, movement readiness, and route-intent generation against the current source and live log fields.
- expecting: after the readiness gate is corrected, a current-source runtime test will either reach Nav intent generation or expose a separate no-intent cause.
- next_action: implement the smallest explicit confirmed-team propagation boundary, then run the focused test and record any remaining Nav decision failure separately.

## Evidence

- timestamp: 2026-09-18T13:05:27+09:00
  observation: human `+ARUKARI-` damaged managed Bot `Bert` with USP; health reached -17 and the server logged the kill.
- timestamp: 2026-09-18T13:13:09+09:00
  observation: `Albert`, `Allen`, `Bert`, and `Bob` were all removed by `Game_idle_kick`.
- timestamp: 2026-09-18T13:20:00+09:00
  observation: current source trace shows `updateManagedBotMovement()` sends neutral movement whenever `spawnReadiness(before)` is `NotReady`; `spawnReadiness()` requires team 1/2, while `JoinController::readyEntity()` does not.
- timestamp: 2026-09-18T13:30:00+09:00
  observation: deployed server DLL is `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll`, SHA-256 `29E0AC6A4CD42D20CC44DDCFF04E53D023532231C0331AB1C990AF76062636F0`; workspace-root `astrabot_mm.dll` is a different artifact, so source-to-runtime identity is not yet proven.
- timestamp: 2026-09-18T13:31:00+09:00
  observation: `qconsole.log` contains `roam_no_intent` diagnostics after team entry, with `team=0`, `spectator=0`, `solid=3`, `movetype=3`, and Nav result fields; this confirms the runtime reached a no-intent path in at least one deployed build.
- timestamp: 2026-09-18T13:40:00+09:00
  observation: current-source x86 `astrabot_compat_actor_command` rebuilt with the initialized VS x86 environment and failed exactly at `heartbeat records dispatched spawn-ready physics sample` after TeamInfo was emitted while the test server left raw `entity->v.team == 0`; this is the intended RED.
- timestamp: 2026-09-18T13:55:00+09:00
  observation: Added explicit `teamConfirmed` propagation from JoinController TeamInfo state into movement readiness without changing raw `entity->v.team`. Current-source full x86 rebuild and CTest completed 41/41 passed.
- timestamp: 2026-09-18T14:05:00+09:00
  observation: Fresh deployed mixed CT/T run recorded `teamConfirmed=1`, `ready=1`, and no idle-kick. After spawn, `NavRoam` emitted `roam_no_intent` with `stage=7` and `locomotionResult=Stuck`; strict log verification found zero post-spawn sustained horizontal movement, zero Bot-to-Bot attacks, and zero Bot C4 plant/defuse events.

## Eliminated

- hypothesis: FakeClient command dispatch is completely absent.
  reason: live diagnostics report `dispatched=1` and server logs show team entry and damage/death.
- hypothesis: server/plugin is not running.
  reason: `hlds.exe` is running and `astrabot_mm.dll` is loaded as `RUN`.

## Resolution

- root_cause: "The initial post-join gate rejected raw team 0 and then, after that was fixed, NavRoam still returned no intent with LocomotionResult::Stuck. Combat and C4 events were absent because the fresh run never reached a successful Nav/action path."
- fix: "Added MovementPhysicsState.teamConfirmed, propagated JoinController::teamConfirmed() into before/after samples, treated managed slot ownership as FakeClient state, and preserved raw entity fields."
- verification: "RED reproduced in current-source x86 compat test; GREEN current-source full build and CTest 41/41 passed. Fresh deployed run passed TeamInfo/readiness/no-idle-kick but failed strict post-spawn movement, Bot combat, and C4 log gates."
- files_changed:
  - include/astrabot/runtime/movement_physics.hpp
  - src/core/runtime/movement_physics.cpp
  - src/adapter/metamod/plugin_runtime.hpp
  - src/adapter/metamod/plugin_runtime.cpp
  - tests/compat_actor_command_tests.cpp
  - tests/movement_physics_tests.cpp
