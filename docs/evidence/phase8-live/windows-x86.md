# Phase 8 Windows x86 live evidence

Status: `partial`. The server/plugin lifecycle, team-entry, and human-attack/death checks below are captured; autonomous post-join action is failed, while Linux x86 and the remaining gameplay/lifecycle matrix remain open.

## Identity

| Input | Observed value |
|-------|----------------|
| Server root | `D:\SteamCMD\cstrike_rehlds` |
| HLDS | `hlds.exe`, SHA-256 `5A2B5FD39474FA39D8BBE09C2D6D18A45D0D25C50BDA3A863412B68A16B0808D` |
| HLDS build | `48/1.1.2.7/Stdio/9909` |
| ReGameDLL-CS | `5.30.0.830-dev+m`, source HEAD `b0889847fe6d03898be88acc9e366660efb40ab5` |
| Metamod-P | `1.21p109`, source HEAD `7ec9b014f8c0a947a724644aebe34eb33706e44b` |
| AstraBot source | HEAD `5a62a98243ce98081ebd64d24c5495295f53ec58`, working tree changes present |
| AstraBot DLL | Windows x86 Debug, SHA-256 `93C7E8634C65C90371766DD8D265334B8BD8E7DA9EA1CF5105408DE00D276774` |
| BSP | `de_dust2.bsp`, SHA-256 `15945389528D113562EDE0A2C80647EBFA799079ED1C05A25379BCF84E4E9286` |
| Legacy Nav | `de_dust2.nav`, SHA-256 `53B9889C0A5B45DA7C1284B13DB7D7B1218C185EA768DC261CE4C4D6E13372C2` |
| Test configuration | `game_init.cfg`, SHA-256 `B23492923004BF70976A13D671009CE5D26BEB81E9002AB676AA39875FA7D638` |

## Captured run

- Start command was equivalent to `hlds_start.bat`, on port `27016`; the test RCON password was supplied only as a process argument and is not recorded here.
- Metamod-P loaded AstraBot beside the unmodified ReGameDLL-CS. `BotProfile.db` and `de_dust2.nav` loaded successfully.
- `bot_enable 1`, `bot_quota 2`, `bot_add_t`, and `bot_add_ct` were issued.
- The current direct-command path issued standard `jointeam` and `joinclass` from the first post-join frame; it no longer selects `menuselect` based on VGUI/legacy menu notifications.
- T evidence: `menu show class value=#Terrorist_Select`, the server recorded `joined team "TERRORIST"`, and `Spawned_With_The_Bomb` occurred after `sv_restart 1`.
- CT evidence: `menu show class value=#CT_Select` and the server recorded `joined team "CT"`.
- Final RCON `status` reported both managed Bots as active after restart; post-restart diagnostics reported `spectator=0`, `deadflag=0`, `solid=3`, `movetype=3`, `effects=0`, and health `100.0`. No 27016 crash/hang was observed during the captured checks.

## 2026-09-18 live run: human attack/death and autonomous-action result

- The running server was `D:\SteamCMD\cstrike_rehlds\hlds.exe` (HLDS
  `48/1.1.2.7/Stdio/9909`), with AstraBot loaded as `astrabot_mm.dll` on
  `de_dust2` at `192.168.0.202:27016`. Evidence was read from
  `D:\SteamCMD\cstrike_rehlds\qconsole.log` and
  `cstrike\logs\L0918013.log`; no server command was issued by this check.
- The human player `+ARUKARI-` joined CT at `13:04:08`.
- At `13:05:27`-`13:05:28`, `+ARUKARI-` attacked `Bert<3><BOT><TERRORIST>`
  with `usp`. The logged health sequence was `100 -> 71 -> 41 -> 12 -> -17`,
  followed by `killed Bert with usp`.
- The same run did not demonstrate autonomous post-join behavior. Movement
  samples continued to report `dispatched=1`, `grounded=0`, `ready=0`, and
  no sustained route progress. At `13:13:09`, `Albert`, `Allen`, `Bert`, and
  `Bob` were all removed by `Game_idle_kick`.

## Acceptance matrix

| Criterion | Verdict | Evidence / limitation |
|-----------|---------|-----------------------|
| Plugin load beside unmodified ReGameDLL-CS | `passed` | Metamod-P loaded AstraBot and ReGameDLL-CS remained unmodified |
| BotProfile and legacy Nav load | `passed` | Current map diagnostics reported successful load |
| Spectator to T team/class entry | `passed` | Direct `jointeam`/`joinclass`, `joined team "TERRORIST"`, and bomb spawn event |
| Spectator to CT team/class entry | `passed` | Direct `jointeam`/`joinclass` and `joined team "CT"`; separate CT spawn event not captured |
| `sv_restart` lifecycle | `partial` | Both Bots changed from pre-restart spectator physics to active physics; a later human attack/death was captured, but a complete death/respawn state trace is not complete |
| Native Bot mixing | `passed` | Native controls were suppressed; no native Bot was mixed |
| Locomotion | `failed` | `RunPlayerMove` dispatches, but origin remains around `(-2080,1824,208)` and `navAreaResult=6/area=0`; Bot does not progress on the loaded Nav |
| Perception/combat/objective/radio | `partial` | Human USP damage and Bot death are confirmed; Bot weapon selection, aiming, reload/ammunition, objectives, radio, and autonomous combat are not established |
| Autonomous post-join action | `failed` | No sustained movement/action was observed after team entry; all four managed Bots were later `Game_idle_kick` targets |
| Map change, disconnect/reconnect, slot reuse | `partial` | Slot reuse was exercised once; complete matrix remains open |
| Bounded CPU/log behavior | `not-run` | No bounded measurement captured |

This report supports Windows x86 `PAR-03` only at partial live-evidence level. It does not close `PAR-06`, `TEST-03`, or `TEST-04`; autonomous action remains failed, Debian Linux x86 is absent, and the remaining live criteria are still required.

## 2026-09-18 current-source gap-closure build

- The TeamInfo/readiness regression was reproduced as RED in
  `astrabot_compat_actor_command` with raw `entity->v.team == 0`, then fixed by
  carrying an explicit `teamConfirmed` signal from JoinController into
  movement readiness without mutating the raw entity field.
- Current-source Windows x86 build output:
  `build-metamod-x86-test\astrabot_mm.dll`, SHA-256
  `956436FD7497D3A656CFAC9E357B0F31543C98B9F01D3F734407BB59FCA353E2`.
- Current-source full CTest: `41/41` passed. This is offline evidence only.
- The running server still has the earlier DLL SHA-256
  `29E0AC6A4CD42D20CC44DDCFF04E53D023532231C0331AB1C990AF76062636F0`;
  it was not overwritten or restarted by this step. Autonomous action is
  therefore still unverified after the fix.

## 2026-09-18 automated log-only retest

- Fresh qconsole interval starts at line `77406` after deploying the rebuilt
  DLL SHA-256 `5FC11FDB0F8CA791904AAAE83130491321F3F3DD10AD56504A04BD4BD13DB321`
  and starting four mixed CT/T Bot slots.
- TeamInfo/readiness: `teamConfirmed=1` and `ready=1` were recorded after spawn;
  four `Game_idle_kick` lines appeared later in the same interval.
- Movement: failed under the strict verifier. Spawn teleports and vertical
  settling were excluded; no subsequent consecutive horizontal coordinate
  movement met the threshold. Diagnostics reported `roam_no_intent`,
  `stage=7`, and `locomotionResult=Stuck`.
- Bot combat: failed/not observed; zero Bot-to-Bot attack lines in the fresh
  interval.
- C4 objective: failed/not observed; zero Bot `Planted_The_Bomb` and zero Bot
  `Defused_The_Bomb` lines. `Spawned_With_The_Bomb` is not counted as planting.
- The log verifier self-test passed, but this live interval does not close
  `PAR-01`, `PAR-03`, `PAR-04`, `PAR-06`, `TEST-03`, or `TEST-04`.

When a Bot is added after `Round_Start`, CS keeps the new player at the
intro/spectator camera with `DEAD_DEAD` until the next round transition. The
current run records the team assignment immediately and shows the pre-restart
state as `spectator=1`, `solid=0`, `movetype=8`, and `effects=160`. After
`sv_restart 1`, both managed Bots become active spawned entities with
`spectator=0`, `deadflag=0`, `solid=3`, `movetype=3`, and `effects=0`. The
pre-restart camera state is therefore not evidence that the direct team/class
commands failed.

AstraBot uses public Metamod-P, HLSDK Engine, and GameDLL boundaries. ReGameDLL-CS is the unmodified acceptance host, not a linked plugin dependency.

## 2026-09-18 plan 08-07 action-adapter checkpoint

- Offline current-source artifact: `build-action-adapter-x86-1451/astrabot_mm.dll`,
  SHA-256 `88B46CDE1BE92D81C42152DA6D85D75C4B3397900A5D357114F9DF114FE22543`.
- The stopped Windows HLDS deployment target was updated with that DLL and a
  timestamped `.pre-final-offline-*.bak` backup.
- The adapter bridge now maps Core Combat/Objective proposals to public
  `RunPlayerMove`/GameDLL inputs; focused x86 tests are 5/5 and the boundary
  verifier reports `LiveActionBoundaryAvailable=true`.
- A five-minute live observation was attempted with an earlier deployed
  action-sensor build. The flushed qconsole startup offset was `87247`
  (`18:12:51`); no `action sensor`/`bot action` diagnostics and no fresh Bot
  attack, `Planted_The_Bomb`, or `Defused_The_Bomb` event were available in
  that interval. This remains live `not observed`, not a pass.
- The final DLL was left deployed while HLDS remained stopped. Linux x86 and
  the remaining Phase 8 gates are still pending.

## 2026-09-18 movement-boundary continuation

- Historical live diagnostics isolated the movement failure at the public input boundary: NavRoam emitted target/intent and `RunPlayerMove` was dispatched, but velocity stayed zero until `LocomotionResult::Stuck`.
- Current-source fix synchronizes `edict_t::v.button` and `edict_t::v.impulse` before `pfnRunPlayerMove`. Full Windows x86 Debug CTest passed `42/42`; current DLL SHA-256 is `8512034d930ee5245859511c3903a45cfb7ff2d8e960afd40ab7a581a6180880`.
- Live movement and C4 plant/defuse are still pending: the current HLDS launch path terminated before a post-spawn observation window. No live pass is claimed from this offline result.
