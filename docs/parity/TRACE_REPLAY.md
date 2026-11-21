# P11 differential trace comparison

## Plant入場更新のNAV入力・Runとcallback失効（2026-11-22）

基点 `6546606`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。参照の経路進捗5秒期限切れは移動入力を作った後にRun/StandUp/DestroyPathを行いPATH_FAILUREを返す。続くPlant OnEnterはしゃがみ・自己防衛・床視線を設定し、既存の移動/ジャンプを消さない。C4選択・PrimaryAttack・通常の移動停止は次のPlant OnUpdateに属する。

互換MoveTo/Plant taskに限定して期限切れで既に作ったNAV intentを保持する。Actor/edict serial・frame/time・full-update・revision・normal/Attack epochが一致するNAV入場receiptだけ、plugin終端中立分岐を通さずmovement commandへ渡す。入場更新は既存入力にDuckと床視線を加え、C4選択・手榴弾取消しを先行しない。期限切れRunは近い最終areaの余分な0.4倍歩行を抑止し、次fullでreceiptとともに消える。EndOfPathで入力がゼロなら前のコマンドは捏造しない。draft作成前に入場epochを保存し、draft後・action準備後に保存epochと完全なreceiptを再検査して、callback後の旧NAV入力を破棄する。

実onStartFrame回帰は4area内の局所移動で速度stuckを避け、strict >5秒の経路期限切れ→PathFailure→Plant→実RunPlayerMoveの移動+Duck、入場C4選択なし、次fullの停止/C4選択1回を確認する。別の実コマンド対照でJumpと最終areaのRun速度を確認する。追加回帰はpost-entry collectActorのtrace callbackで同じActorをPlant→Huntへ正規遷移させ、古いNAVコマンドが送られないことを確認する。修正前 `red-entry-callback.log` はcallback発火の前提合格後に旧コマンド送信だけ失敗し、`green-entry-callback-isolated.log` は重点3/3合格。旧fixtureの時刻使切り・2area/区域外site・後続goalcache汚染を含む試行は合格証拠から除外した。

最終v2オフライン：Windows x86 **171/171・PE7**、Linux x86 **170/170・ELF7**。全targetをビルド後、CTestはWindows→Linuxの直列。DLL SHA-256 `1db9c898ad18a8adfce9f9e2594fc4b76347f8b0311c5aae87a0d6edb54322e0`、PDB `3960ba34acd4b34cfde2836f2c55269f983653894ff23cd14c5716d5c842b2e3`、ELF `ef0852bf37f0aa0a8a6adf077516adeea9f555d2b52531ebc6229869ce85eee5`。v1候補はcallback対策前として別保存し、最終合格に流用していない。初回v2 ELFの改行不備とリンク途中の読取りは除外し、完了製品を独立検証した。

固定DLL/PDBの隔離実機0/7/8/11と読取りCDB11は、次の起動前に実PID終了0・forced=falseを逐次確認した。

- final-v2-natural-0: PID36436 exit0/forced=false; plant-fixture planted time=14.653230; plant-fixture complete mode=0 planted=1
- final-v2-natural-7: PID39116 exit0/forced=false; terminal-fixture leave-zone now=11.544034 has=1; plant-fixture complete mode=7 planted=0
- final-v2-natural-8: PID2300 exit0/forced=false; terminal-fixture actual-drop now=11.745628 entity=99 has=0; plant-fixture complete mode=8 planted=0
- final-v2-natural-11: PID43620 exit0/forced=false; plant-fixture planted time=63.578010; plant-fixture complete mode=11 planted=1
- final-v2-route-debug-11: PID35256 exit0/forced=false; plant-fixture planted time=63.467300; plant-fixture complete mode=11 planted=1

CDBはDLL module-loadを待ってBPを設定。最初のPlantは `59.37293243` 秒/full `0x1d3`、MoveTo→EndOfPath2→Plant。生成NAV入力ゼロのためDuck4/AKで入り、その後C4へ移る。固定nativeのPATH_FAILURE時buttons14とは終端結果が異なり、同条件14対4比較とは主張しない。タイムアウト入力保持・callback失効は実フレームのオフライン回帰、実機は到着/区域外/実drop/設置の受入れとして区別する。fixtureの明示的C4在庫bit同期は残る。

固定baseline・既存補正を再生成せず、両OSとも親の152258行と全byte一致。RNG510行も各OSの親と順序・値まで一致し、新規補正なし。既存のOS間丸め差は残る。証拠と指紋は `artifacts/local/debug/csbot-plant-entry-nav-input-20261122/evidence-final-v2/acceptance.json`、製品は同root `final-v2/`、raw試行は `C:/Temp/astrabot-plant-entry-nav-input`。push・実環境配布なし。

最初のCDB入場後の実コマンド：

```text
terminal-command seq=584 time=59.372932 buttons=4 active=28 carrying=1 signal=2 state=2 flags=8712 pitch=0.000000 yaw=51.126491
terminal-command seq=585 time=59.407981 buttons=4 active=28 carrying=1 signal=2 state=2 flags=8712 pitch=3.333333 yaw=50.853363
terminal-command seq=586 time=59.450000 buttons=4 active=28 carrying=1 signal=2 state=2 flags=8712 pitch=10.000000 yaw=50.997581
terminal-command seq=587 time=59.487377 buttons=4 active=6 carrying=1 signal=2 state=2 flags=8712 pitch=20.000003 yaw=51.612396
terminal-command seq=588 time=59.523590 buttons=4 active=6 carrying=1 signal=2 state=2 flags=8712 pitch=33.333339 yaw=53.824722
terminal-command seq=589 time=59.565559 buttons=4 active=6 carrying=1 signal=2 state=2 flags=8712 pitch=50.000007 yaw=59.370376
```


## MoveTo terminal navigation before Plant, 2026-11-22

Base95aec579, fixed native b088984. Final DLL `f176ab5079cc2e6b4debbe27c64e95c76d73b3675c727ee30bbb5661d1dbc003`, ELF `609201ff9e46859d202be10691f3f6bf4d6cfdf513949f7461480e1093860aef`. Windows171/171+PE7, then Linux170/170+ELF7 passed serially. See STATUS's latest section and `artifacts/local/debug/csbot-move-plant-order-20261122/README.md` for source/fixture limits and evidence identity.

`C:/Temp/astrabot-move-plant-order/run.py final-natural <frozen-DLL> 0 7 8 11` verifies actual HLDS exit before every next launch. Mode11 prepares real de_dust2 NAV area41 at35s, past the shared plant deadline, and does not force pose/input/goal thereafter. Explicit fixture-only C4 inventory synchronization remains. Modes0/11 plant successfully;7 leaves the zone after arming;8 records real drop entity99 and lost possession. All five final trials, including read-only `final-route-debug`11, exit0/forced=false with identical DLL/PDB hashes.

CDB waits for ld:astrabot_mm.dll then installs symbol-based BP. Layouts are verified by dt/disassembly: StateUpdateContext.behavior+0x28, StateBehaviorInput.navigation+0xc, NavigationStep.outcome+0x20. In final debugger.log lines8395–8413, post-path result1 is adapted to EndOfPath2; core finishNavigation still sees MoveTo/PlantBomb, then invokeEnter Plant runs at60.29094315/full0x1d9. Pinned native records real UpdatePathMovement return2(PATH_FAILURE) then Plant at58.207436. Baseline95aec579 records Plant before its following NAV result at64.39331818. Earlier candidate raw-debug used Idle→Plant and is only a negative boundary control. Failed candidate-route-debug used an invalid CDB expression and is excluded; its script/log/forced-exit manifest are preserved.

`csbot-move-plant-order-{platform}.json.gz` pins19 blocks by original offset, exact before/after rows and chain/output SHA.32 added rows are8 fresh terminal zone observations;22 removed rows are11 legacy PlantAttempt duplicate geometry scans. Final traces152258 rows: Windows2854d213daf889fde4056badf94b5dc09541afe4d938c5c5add00a7cae0c88ac, Linuxc8fbb6d78179a6029db535e3890ab759bd0d62571e4cb1169adbd923fc7c88b1. RNG510 rows are unchanged in ordered per-platform comparison. Baselines remain immutable. Native entry buttons14 vs finalAstra4 remains a separate NAV-tail command issue, along with initial Idle path preparation.


## Plant pre-entry admission / retained Ahead, 2026-11-22

Base `3cde77b`; native pin `b0889847fe6d03898be88acc9e366660efb40ab5`. Final-v2 DLL `fee8c705b979e8e2dfc35c29a15d54a68313d628127d05a23e71517812daef04`, ELF `1789fb95da504f22021204723b887c3140affd8025603183b845a0e05dd1a1bb`. Windows171/171+PE7 then Linux170/170+ELF7 passed serially. Source, binaries and preserved user-file fingerprints are under `artifacts/local/debug/csbot-plant-ahead-supply-20261122/final-v2`; raw evidence and scripts are in `C:/Temp/astrabot-plant-ahead-supply`.

`run.py final-v2 <frozen-DLL> 0 7 8` clones isolated servers, refuses existing evidence destinations, locks serial execution and verifies each actual HLDS PID has exited before proceeding. `final-v2-debug` mode7 uses `final-v2-debug.cdb`: wait for `ld:astrabot_mm.dll`, then load symbols/probes. No game memory or input is rewritten by CDB. `equipment.amxx` SHA `262a8491675684abf00dc75f3587d08216fc543c2eac4b17c0907c907c68029a` retains the explicit fixture-only C4 inventory synchronization from earlier acceptance; this is not natural pickup acceptance. Modes0/7/8 require real BarTime3/arming;7 moves outside once,8 requires a real dropped entity and lost possession.

Final-v2 game PIDs41360/3040/16748 and debugger game PID37736 all exit0/forced=false. Reference resume0/8 PIDs38948/42744 also exit0; pinned reference7 and earlier baseline/candidate evidence remain separate. Final0 plants successfully. Final7 leaves at11.620512 and exits at Plant age5.027; final8 drops entity99 at11.472638 with hasC4=0. CDB captures first Plant at10.50985813, `BotCommandRuntime::actors_._Elems[1].defaultLookAheadYaw` =0. `advanceUpkeepLook` supplies the real command-runtime `this` in ECX; `CompatibilityStateMachine::invokeEnter` state12 and context atESP+0x14 are verified against the final module disassembly. Native first Plant8.6012106 has retainedAhead0. Do not infer identical decision times: initial Idle generalpath1 preparation remains, while the unnecessary pre-Plant Hunt/Ahead75 supply is removed.

Offline controls cover authoritative API false vs overlapping geometry, blinking-icon/unknown geometry fallback with cold registry, direct Idle and live-leader Follow, Hunt yielding without route selection then next-Idle routing, strict>3s contact and inclusive shared deadline, candidate-only vs recognized visible contact without Attack, Buy priority cancellation, Actor/edict/full-update reentry, forced lifecycle resets, OnExit and ResetPath nested transition ownership. These are separate from real-server acceptance.

Historical baselines are unchanged. New `csbot-plant-entry-{platform}.json.gz` corrections pin789 blocks/628 patterns by original offsets, exact before rows and chain/output SHA. Both traces have152248 records; Windows SHA `1b43e025216775da39565b5d166ce3ce1017e759f9a2b74b4ee19e00a2d763d0`, Linux `9f7ee3de5d69901cdc135b3deefecc5a21c76d8d3aabd826f50f83297bef0fb3`. RNG504→510 has3 ordered delta blocks: skip one pre-entry Teamwork.AutoFollow; its raw sample feeds earlier STUCK-JUMP-DELAY instead; the short-round fixture's added31s/.125s updates introduce7 later requests (RogueInterval, RogueChance, AutoFollow, PATH-RETRY, WIGGLE direction/duration/jump interval). C4-only read-only semantic CDB produces a full byte-identical Windows trace and identifies all193 calls in that segment. The earlier all-program semantic-debug attempt ended after an87466-row prefix and is excluded. OS numeric rounding differences remain separately pinned, including one RNG value difference already present in the parent.

## Plant terminal input / exit ordering, 2026-11-21

Base `d83a6d6`; fixed native `b0889847fe6d03898be88acc9e366660efb40ab5`. Evidence: `C:/Temp/astrabot-plant-terminal-order`, frozen modules and summary under `artifacts/local/debug/csbot-plant-terminal-20261121`. Final-v2 DLL SHA-256 `fcdd52c9ee91c64404d7cd4ff8013d5c9f263f29f223fb48ac9c24dd9e20e42c`. Prior candidate `4597335e…` is preserved separately and is not final acceptance.

`equipment.amxx` SHA-256 `8c9fdaf946a47cb97b693dddbeee8c3e92ed4ded759d9e4f4969b5463a545a07`. Modes0/7/8 use actual GameDLL planting: normal completion, one position change outside the planting zone after real arming, and actual `rg_drop_item("weapon_c4")` after arming. Require BarTime3/arming before intervention, genuine zone-signal loss for7 and a positive dropped entity plus hasC4=0 for8. CmdStart records every actual input's sequence, active weapon, ownership, signals, flags and angles without rewriting buttons. The old conditional C4 inventory-mask synchronization remains fixture-only and is logged; do not count it as natural acquisition acceptance.

Run reference0/7/8, before0/7/8 and release-v2-0/7/8 strictly serially using `run.py`. Require each real game PID exit0 and forced=false before the next launch. Baseline7 exits early toMoveTo after leaving the zone; native and final instead remainPlant until strict>5, then enterIdle while retainingPrimaryAttack after switching toAK and standing. Final age5.022 emitsAttack on commands378/379/380 and clears it at the next full decision381. Completion/drop selects absentC4 before exit's best weapon and emits no freshPrimaryAttack.

Read-only `reference-order-debug` BPs are function entries `PlantBombState::OnUpdate`, `PlantBombState::OnExit`, `CBasePlayer::SelectItem`; `release-order-v2-debug` reads `PluginRuntime::command_dispatchClientCommand`'s real string argument. Debug8 confirms SelectC4→exit→AK at one update, with final actual activeAK/buttons0 even when CurWeapon's previous C4 sample is stale. Offline regressions include inverse-cache prediction, unavailable fallback, missing CurWeapon, serial/round/full-sequence reentry, failed outer publication and real command dispatch with NAV180° versus a retained terminal67°.

The final full command preserves its own Upkeep angle instead of snapping toNAV180°. The following light updates still lack the normal Idle look-goal/spring resumption: native7 yaw98.221519→94.888183→88.221519, final97.783432 held. Track this separately from the matching three-command Attack retention and first command's view preservation. General view/Hide/NAV/RNG parity is not asserted.

## C4 equipment / native equip clock, 2026-11-21

Evidence root: `C:/Temp/astrabot-c4-equipment`; frozen modules and acceptance notes: `artifacts/local/debug/csbot-c4-equipment-20261121/README.md`. Base `c78b4c0`, fixed CSBot `b0889847fe6d03898be88acc9e366660efb40ab5`. Final Windows module SHA-256 `b46033d043ff2493712e8df14b60e73c5c20f662ae4443d0281a0b607218e5d6`.

Use `run-fixed.py` with roles `reference-fixed-v4`, `baseline-fixed-v4`, `final-fixed-v4`, modes1/2. The compiled fixture SHA is `93d8c48ac01e2ab44754579ab860ec01e03c3270e74594bf37e71a5efb5f6180`. Normal GameDLL planting must reach real arming and BarTime3 before setting empty-primary/usable-pistol or both-empty ammo. Model/view and CmdStart view are fixed only during the one-second exposure; inspect actual recognition/Attack and weapon transitions, not merely the equal fixture hash. The accepted reference/baseline/final sequences select pistol/empty-AK/pistol in mode1 and knife/empty-AK/knife in mode2. C4 arming clears on the accepted final switch. Do not infer equal reaction or future planting times from unequal state-entry times.

`run.py` modes0/6 preserve natural view and cover ordinary planting exit with the already-active AK, and C4-owned-but-AK-active Attack from Hunt. Mode6 is not a PlantBomb-first-update unequipped acceptance. Original V3 baseline1/2 did not recognize the enemy and are excluded from causal equipment comparison. Permission mode4 uses the separate `run-allow.py` fixture (SHA `a3eefa5d0f007dfcbbeafc3edf9e13f68d6f466ea5c3f17b3b42a8882abbf283`) which logs CVar existence and actual zero value, registering only if absent.

Read-only CDB `reference-clock-v4-debug` reads `EquipBestWeapon+0x18`, `DoEquip+0x5e` and `DoEquip+0x85`, derived from the same DLL's saved disassembly. These expose the same active/target pointer and clock change after SelectItem; Plant exit changes old4.7424269 to now12.820281 even though AK is already active. `final-clock-v5-debug` reads `WeaponEquipmentPlanner::equipped` at `weapon_equipment.hpp:98`, after the write: now19.71290588, weapon28 and equippedAt19.71290588, followed by the same-active28 sent1 log. Exact five-second boundary, time-zero sentinel, failed actual switch, mustEquip and knife/tactical clock separation have offline/runtime regressions. Native failed ambiguous source-line BP and final-clock-v4 invalid reference-local expression logs remain excluded. Game process identity, exit0 and forced=false are required per manifest; each trial checks all game processes have exited before starting another.

`tools/compare_parity_traces.py REFERENCE.jsonl CANDIDATE.jsonl` compares fixed
captures and prints a JSON report. Exit codes: **0** equal, **1** differences,
**2** malformed input or invalid options. Python standard library only.

## Pinned offline hearing replay

On Linux x86, use a current `astrabot_core` build and a local ReGameDLL-CS
checkout containing the pinned commit. The runner builds both drivers and
writes raw and normalized traces, a build and input manifest, and a comparison
report in a new output directory.

```sh
rtk proxy cmake --build build/linux-x86-debug --target astrabot_core
rtk proxy python3 tools/replay_csbot_noise.py \
  --reference ../ReGameDLL_CS \
  --core-library build/linux-x86-debug/libastrabot_core.a \
  --output /tmp/astrabot-csbot-hearing
```

The runner reads native function bodies and two event-handler fragments directly
from Git at the pin. It compiles them with `REGAMEDLL_FIXES` and a small host shim
that supplies a stationary observer, a single flat NAV area, preclassified
sounds, explicit visibility, and scripted RNG values. The Astra driver calls
its production perception assembler, NAV projection, investigation planner and
look planner. Each driver emits the delivered sound inputs, memory snapshots,
ordered float requests, investigation decisions, noise consumption, and startle
look decisions with inhibition deadlines. The clock is controlled by the
fixture. State-machine, command and path fields outside the measured callbacks
are unknown.

This fixture covers hearing and composed startle at time zero, the three-second overwrite boundary,
retention past twenty seconds when no full update runs, quarter-second investigation cadence, and
startle at 1,000 units with obscured and out-of-range controls. It does not run
the GameDLL, engine event classification, native NAV search, or an HLDS match.
The startle excerpt measures the resulting look and inhibition after the
branch returns, and captures RNG calls at the shared callback boundary.
The `Z` rows feed a shot through the normal intake before checking startle at
time zero, at 0.5 seconds, and just after the cutoff. The startle checks use
independent inhibition state to expose the freshness boundary. The standalone
`L` rows provide range, visibility, and inhibition controls. Starting a map in
ReHLDS sets server time to 1.0 seconds, so the time-zero case uses the controlled
clock in this fixture.

## Windows x86 HLDS capture

Run the pinned `b088984` Win32 GameDLL in two local HLDS sessions with the same
`de_dust2` BSP, NAV, and bot profiles. Run the native bots on one side and
AstraBot in Compatibility mode on the other. The reference capture adds console
records to the audible-event classifier and successful multi-area path builder.
Both sides sample `CmdStart` before the capture fixture's input overrides.
On AstraBot, `astrabot_profile 1` enables `profile nativeWeaponSound` and the
`goalAssignmentCorrelation` and `motionCommand` diagnostics.

A controlled AK-47 shot in both sessions produced the same weapon playback
event and classified as `HIGH` with range 2,000. Additional captures matched
AK-47 reload (`LOW`, 1,100), AWP fire (`HIGH`, 99,999), and TMP fire (`HIGH`,
1,000). These records measure classification before AstraBot's per-observer
delivery. At the start of free movement,
the CT bot used area 5 on both sides. The native bot selected area 21 via
1522, and AstraBot selected area 874 via 60. The two runs chose different
destinations and entered Hunt at different times. This capture records those
differences and their command effects.

The fixed plant probe starts the CT at `(-1361.3, 2447.0, 37.9)` after the
normal `Planted_The_Bomb` event. Both captures show a path toward NAV area
293 beginning with `157 -> 54`. Their timing and command axes differ, and
subsequent goals diverge. The probe compares a shared route segment and
observed dispatch. Complete route, movement-command and live RNG equality
remain open.

## Windows x86 Hunt goal selection

Two isolated `de_dust2` HLDS runs used the same map, profiles, and pinned
Win32 GameDLL. The reference probe records the Hunt goal draw and the path
retry draw. An AstraBot test build records the same call sites before and
after the change. Each engine uses its own RNG stream.

For the first CT Hunt update, the old AstraBot build drew the retry delay
before the goal and dispatched forward input in the same update. The fixed
build drew the goal before the delay, like CSBot, and dispatched zero movement
while the new corridor was prepared. During the cooldown it selected a goal
without drawing another delay or searching. The deterministic controller test
also covers the exact cooldown boundary and a path invalidated before retry.

A separate control sent the reference's measured NAV start 5 and goal 842 to
AstraBot's Safest search using the captured `de_dust2.nav` file. All 13 corridor
areas matched the reference run after removing its duplicated endpoint area.
A separate live probe captured `StatusIcon buyzone`: AstraBot briefly left Buy
before its first zone notification during the scheduled restart. The fix holds
Buy while the notification is missing and the restart is pending. It uses any
known zone result and still requires an observed buy zone for purchase commands.
In the follow-up live run the first Hunt decision moved from before
`Round_Start` to after it. Full-match RNG and movement-command equality
remain open.

## Linux x86 Hunt ground routes

The controlled ReHLDS replay used `de_dust2`, identical CT spawn coordinates,
and a small separate NAV fixture in each isolated instance. Diagnostic builds
used the pinned CSBot GameDLL and kept each Hunt random call while injecting
the selected index. The fixture fixes the requested area for each case.

| Case | Requested area | Ground route and endpoint on both builds |
| --- | --- | --- |
| Same area | 5 | Area 5, `(437.5, 2412.5, -128)` |
| Disconnected goal | 7 | Areas 5 → 6, `(437.5, 2200, -128)` |

After selecting area 5, Hunt selected area 7 on the next update while
`ComputePath` was throttled. Both bots retained and followed the existing
same-area route after the second selection. In the disconnected case, ground
A* retained the closest reachable area and aligned the endpoint with the
last portal. Both fixed scenarios produced movement on the retained path.

The waypoint and slowdown replay used diagnostic builds of AstraBot
`9026f53` and CSBot `b0889847fe6d03898be88acc9e366660efb40ab5` on
ReHLDS 3.15.0.896. Each
case ran twice with `mp_freezetime 0`. Updates are counted from the first
movement update on the route through areas 5 and 6. The same-area case
first ran a one-area route, which walked from its first movement update in
both builds and produced four walking updates in each run.

| Case | Portal-to-endpoint switch | First walking update | Switch time from first route movement, CSBot / AstraBot |
| --- | ---: | ---: | --- |
| Same-area route followed by fallback | 6 | 7 | 641.4 / 646.3 ms, 645.5 / 647.1 ms |
| Direct fallback to the closest reachable area | 8 | 9 | 904.4 / 893.8 ms, 902.0 / 899.3 ms |

Both builds ran at 250 units/s on the switching update and walked at
100 units/s on the next update in all four replays. The sessions sampled
server frames independently, and the elapsed switch times differed by up
to 10.6 ms.

The replay covers ground routes. Hunt ladder reachability and world-height
sampling for a projected endpoint remain outside this replay.

The subsequent Hunt ladder regression exercises a goal reachable only by a
native ladder. It verifies that Hunt retains the ladder route after planning
and starts traversing it on the following update. This is an offline check.

### `cs_assault` ladder replay

An isolated Linux x86 ReHLDS replay used the shipped `cs_assault` geometry and
NAV, with hostage objectives disabled in the scratch BSP. The diagnostic builds
fixed the Hunt target without skipping the random call. Both bots started at
`(0, 2810, 372.031)`, took the ladder from area 1201 to 1206, and dismounted
onto the upper floor at an observed hull minimum of `435.031`.

For descent on ladder `*26`, both bots selected the 1206 → 1201 ladder link
from `(0, 2744, 472.031)`. The reference reached MountDown, and both sent
forward input at the upper lip without changing position or making ladder
contact. A standing hull trace was already solid at the spawn point. The
crouching hull trace cleared the same horizontal approach. The result
isolates a fixture placement issue for this ladder. On ladder `*28`, both
rode the ladder down to a hull minimum of `435.031` and stepped off. The
planned route there was a floor-to-floor NAV connection.

The roof ladder `*69` provided a planned descent from area 140 to area 2.
At the common start `(-743, 1536, 412.031)` with yaw 0°, both hulls cleared the
upper approach, both bots mounted, and both descended with `MOVETYPE_FLY`.
With area 2 repeatedly selected as the Hunt target, the reference replanned
once area 2 became its last known NAV area. Its descent input stopped with
the hull minimum still at `63.259`, and it remained attached. AstraBot at
`68c601b` continued to the floor and stepped off. The fixed controller checks
the known Hunt area before advancing its current ladder and retains the
newly selected goal when replacing the old path. The retry cooldown still
keeps the old path available while suppressing traversal for that update.

With the fix, the repeated area 2 goal left both bots attached and stationary
above the floor through the last sample. The two paired trials stopped at
hull minima of `52.686` and `47.703` for the reference, and `59.838` and
`70.958` for AstraBot. Each height difference was smaller than one full
update's descent at the observed speed. With area 1 beyond the
ladder as the goal, both took the same downward link and dismounted onto the
lower floor. In a repeat of the area 1 trial, both reached origin Z
`36.031` while still attached. After the reference logged `Ladder traversed`
and continued on its ground path, its next command included `IN_JUMP`;
AstraBot continued forward on the ground. This explains the higher first
walking sample in the repeat. The runs used the same start and a fixed
goal draw for the measured bot in isolated Linux x86 ReHLDS.

The final-build replay kept the same fixture, start, and two goal draws.
The module for this replay reused the final build objects and replaced only
the random-source object to force the Hunt goal after drawing. For area 2,
the reference held at a hull minimum of `66.363` and AstraBot at `73.450`.
For area 1, both first reported walking movement at a hull minimum of
`-0.968`. Logs and module fingerprints are in
`C:/Temp/astrabot-descent-resume`.

The earlier Area 1 trace located a command difference at the end of roof
ladder `*69`. The reference resumed floor steering and sent `IN_FORWARD |
IN_JUMP` with forward input 250 while the engine still reported
`MOVETYPE_FLY`. AstraBot resumed floor steering with `IN_FORWARD`. The
compatibility controller now requests the forced jump when floor steering
resumes while the player is still attached. It uses the existing jump state,
cooldown and crouch checks. Hunt's early arrival check for Area 2 still runs
before ladder or floor steering.

The repeat in isolated Linux x86 ReHLDS started at `(-743, 1536, 412.031)`
with yaw 0. For Area 1, both the reference and the updated build sent
buttons 10 with forward input 250 on the first floor steering command
while still on the ladder. Both then reported `MOVETYPE_WALK` and cleared
the jump bit at the next full update. The hull minimum at the first command
was 10.664 in the reference and -0.968 in AstraBot. Reference pitch was 40°
with the ground flag clear, and the next command showed descent. AstraBot
had pitch 60° and the ground flag set, and the next command showed upward
velocity 236.328. An earlier reference run sent the jump at -0.968 as well.
The timing of ground contact, view angle and resulting trajectory remain
separate from the verified command order. For Area 2,
both stopped while still attached and sent no jump during the stop.
The live probe replaced only the random source object to fix the Hunt
destination. Its unmodified relink matched the verified ELF byte for byte.
The command traces and fingerprints are in
`artifacts/local/debug/cs-assault-ladder-exit-jump-20261121`.

### Roof ladder descent look

The fixed reference applies view upkeep before the full path update. During
`MOVE_TO_DESTINATION` it has already released the ladder pitch override and
uses its path lookahead. Movement still uses the ladder-local axes until the
destination area contains the player origin. AstraBot now preserves those
axes while updating the view before the decision. One fixed-step spring
continues through the first floor commands and the landing. A throttled Hunt
replan can send zero movement without advancing the same spring twice.

The height used by the native ladder controller comes from the raw player
origin and the ladder's intended posture, using the CSBot offsets 36 for
standing and 18 for crouching. Destination containment uses the raw origin.
The replay records both the raw origin and the engine hull minimum, so the
native feet height can be calculated independently. The engine owns contact,
movement type and velocity after the matched start.

The final isolated Linux x86 replay used the same start, Area 1 route and
Area 2 control. An unmodified relink matched the production ELF byte for
byte. The controlled module replaced only the random source object to
select the Hunt goal.

| Command | Fixed CSBot airborne repeat | AstraBot airborne repeat |
| --- | --- | --- |
| First floor decision, still `MOVETYPE_FLY` | Hull minimum +2.936, pitch/yaw 40.000/1.542, vertical speed -281.757 | Hull minimum +4.194, pitch/yaw 40.000/1.660, vertical speed -281.756 |
| First detached command, `MOVETYPE_WALK` | Hull minimum +2.296, pitch/yaw 26.666/0.603, vertical speed -32.000 | Hull minimum +3.522, pitch/yaw 26.666/1.681, vertical speed -32.799 |

Both commands in both runs were ungrounded and used forward/side 250/0
with buttons 10. Pitch angular velocity went from -300 to -400 degrees per
second. The AstraBot repeat touched down one command later. Its other run
reported ground contact on the first detached command. Both Area 2 runs
stayed attached with zero inputs and no jump. The fixed reference relaxed
pitch during that stop, and AstraBot held its ladder view. Logs and module
fingerprints are in `artifacts/local/debug/cs-assault-ladder-descent-look-20261121`.

### Area 2 stopped Hunt look

The Area 2 replay sets the Hunt goal to the ladder destination itself. The
reference reaches its known goal area and replaces the ladder route with a
same-area path. The replacement update emits zero movement with the view
already computed by upkeep. The next command uses the old ordinary
lookahead and begins relaxing the ladder pitch with the same spring.

AstraBot now keeps that lookahead after a successful replacement. A
throttled attempt continues to use the existing ladder controller. A
cancelled route clears its view, and movement on the replacement route
supplies its own lookahead. Neither the view handoff nor the neutral
command changes the player's engine movement type or contact state.

In the isolated Linux x86 replay, Area 2 produced a same-area path on full
update 64. Command 128 held pitch 60° with the old ladder view. Commands
129 and 130 sent 56.666° and 50° with angular velocity -100 and -200
degrees per second. The pinned CSBot run followed the same pitch sequence.
Both stayed in `MOVETYPE_FLY` with zero input and no jump. The measured
hold heights were +64.434 for AstraBot and +77.116 for the pinned run.

The Area 1 control retained forward/side 250/0, buttons 10, and pitch
40° then 26.666° across the first floor and detached commands. Both
controlled runs reused the production build objects, with only the
random source replaced to fix the goal draw. An unmodified relink matched
the production ELF byte for byte. The live logs and artifact fingerprints
are in `artifacts/local/debug/cs-assault-ladder-area2-look-20261121`.

The separate 13.005-second Hunt stop remains unverified.

## C4 approach view and movement

The isolated Windows x86 replay uses the existing normal plant fixture and
two matched physics samples at MoveTo entry and its next full update. It
records both command axes, pitch and yaw, the path point, the subsequent
look destination, path retry draws, the first USE command, and BarTime.
The second reference run initializes the view spring velocity to zero
at the matched sample. The final AstraBot probe records its existing
spring velocity at that point. Raw logs, inputs and the uninstrumented
control are in `artifacts/local/debug/c4-approach-look-20261121`. See
[C4 approach results](STATUS.md#c4へのmovetoでの視線移動軸と解除への切替).

## Windows x86 round opening Buy

The isolated `de_dust2` replay compared two $800 bots against Win32
ReGameDLL-CS `b0889847fe6d03898be88acc9e366660efb40ab5`. AstraBot had previously
entered Hunt before the BuyZone HUD update arrived. The reference stays in
Buy for its initial two-second delay, then attempts purchases at intervals
strictly greater than 0.2 seconds. Each stock primary attempt draws sniper
eligibility before the team stock index, even when all the weapons are too
expensive. With no primary, the seventh attempt leads to equipment purchases
in the same update. A finished bot stays in Buy until freeze ends.

The replay recorded the kit, grenade, and pistol preference draws for CT,
and the grenade and pistol preference draws for T. Each bot then consumed
seven ordered sniper and stock-index draw pairs. The controller regression
also fixes the RNG values and checks the resulting command order. The live
servers drew from independent engine streams.

| Freeze | Reference CT / T first Hunt after Round_Start | AstraBot CT / T |
| --- | --- | --- |
| 0 s | 3.80 / 3.81 s | 3.83 / 3.85 s |
| 5 s | 0.12 / 0.13 s | 0.15 / 0.15 s |

## Full runtime replay remains open

The semantic record classes come from `docs/TRACE_SCHEMA.md` and
`docs/parity/TRACE_SCHEMA.md`. Existing RNG, state, observation and timing sinks
use independent sequences and do not emit one common capture envelope. The
canonical envelope below is the comparator's input contract. A production
collector, full native binary capture, production RNG replay tape, and live
scenario acceptance remain open. Synthetic unit tests exercise comparison
only and are never golden measured traces. `live_parity_accepted` is always
false: even an equal file pair requires independent capture provenance and
scenario acceptance before closing parity.

## Canonical JSONL envelope, version 1

First line is one metadata object with these mandatory fields:

```json
{"kind":"metadata","schema":"astrabot-parity-v1","capture_kind":"fixture","reference_pin":"b0889847fe6d03898be88acc9e366660efb40ab5","map":"de_fixture","scenario":"synthetic format example only","profiles":{"1:1":{"name":"fixture","skill":50}},"cvars":{"bot_difficulty":2},"rng_seed":7,"identity_settings":{"actors":"slot:generation","nav":"map-native-area-id"},"coverage":["command"]}
```

`capture_kind` is `fixture` or `live`, declared by the caller; it does not prove
native provenance. Both files contain the same comparison configuration:
full pinned reference SHA, map/scenario, resolved profiles (including inherited
values), all decision-relevant CVars, RNG seed and actor/NAV identity mapping.
Record build hashes, environment, capture commands and provenance in a separate
capture manifest: reference/candidate executable hashes necessarily differ.
Every metadata field, including extensions, compares exactly; do not silently
remove differing settings in a normalizer. `coverage` lists the distinct record
kinds actually present. Partial captures may compare equal, but the report's
`complete_record_classes` remains false unless all six classes are present.

Every subsequent line has this envelope (also a synthetic example):

```json
{"id":"1:1/1/command/0","kind":"command","sequence":1,"timestamp":1.0,"actor":"1:1","frame":{"map_generation":1,"round_generation":1,"tick":1},"data":{"forwardmove":0.0,"sidemove":0.0,"upmove":0.0,"buttons":0,"view":{"pitch":0.0,"yaw":0.0,"roll":0.0},"msec":33,"action":null}}
```

`id` is a unique semantic alignment key, assigned independently of output line
order or producer sequence: actor/frame/kind/local callback identity or ordinal
within that semantic scope is suitable. A normalizer must preserve extra
callbacks and order; it must not derive IDs from whole-file line numbers or
reassign IDs to hide missing events. `sequence` is a strictly increasing global
capture sequence; local sink sequences remain additional data. `timestamp` is
nonnegative finite game time in seconds. `actor` is `slot:generation`, or null
for genuinely global records. Frame generations are positive; tick is
nonnegative. Pointers/process addresses must never become identities.

Required `data` fields by class (additional fields also compare):

| Kind | Required fields |
|---|---|
| think | command_tick, full_think_tick, task, state, attack_overlay, enemy, nav_area, route |
| observation | self, weapon, objective, visible_enemies, noise, remembered_contacts |
| rng | call_sequence, call_site, range, type, value |
| transition | old_state, new_state, task_before, task_after, reason, side_effects |
| command | forwardmove, sidemove, upmove, buttons, view, msec, action |
| path | source_area, target_area, target_position, route_type, areas, transitions, cost |

Use `self` for origin/velocity/view/FOV/posture/life/health/armor/team; `weapon`
for active weapon/ammo/reload state; `objective` for bomb/hostage/zone state.
Retain known/unknown, freshness, quality and observation-source classifications.
Unknown values are explicit null or an observation object, never fabricated
zero/false. Position/vector objects use x/y/z; view objects use pitch/yaw/roll.
Arrays preserve observable order. Normalize equivalent public semantics only;
document mappings for task/state/target/reason IDs instead of dropping fields.

## Exact comparison and justified precision

Default comparison is exact, including timestamps, sequence, identity,
configuration, state/target/path, buttons, movement, command actions and RNG.
Missing/extra IDs are reported separately; common-ID order detects reordering.
Each common event's complete envelope and nested payload are compared, so
changed array order, missing fields and changed timing cannot be hidden by ID
alignment. Duplicate JSON keys/IDs, non-finite numbers, invalid sequences and
empty captures are errors. Numeric representation types compare exactly unless
an explicit scalar precision override applies.

An absolute epsilon requires an explicit matching evidence explanation:

```text
python tools/compare_parity_traces.py reference.jsonl candidate.jsonl --precision /data/view/yaw=0.0001 --precision-reason "/data/view/yaw=Capture manifest section 4 measured serializer rounding bound"
```

This is an option example, **not an approved measured tolerance**. The report
records every epsilon and reason. Supported scalar paths: `/timestamp`,
`/data/cost`, `/data/forwardmove`, `/data/sidemove`, `/data/upmove`,
`/data/view/{pitch,yaw,roll}`, `/data/target_position/{x,y,z}`,
`/data/self/{origin,velocity}/{x,y,z}`, `/data/self/view/{pitch,yaw,roll}`,
and `/data/self/fov`. Supply each concrete path separately. No relative epsilon,
wildcards, rounding, resampling, angle wrapping or tolerances for configuration,
IDs, sequence, RNG, buttons, msec or command order. Reasons are recorded claims;
the reviewer must verify their linked measurement evidence.

## Offline comparator verification

```text
python -m unittest discover -s tests -p test_compare_parity_traces.py -v
```

The tests generate temporary synthetic fixtures covering equal comparison,
missing/extra/reorder, configuration and identity mismatches, explicit precision,
malformed inputs and CLI exit codes. Run the pinned fixture separately to
compare actual native function results with the current Astra build.
# ClearLook default-goal and spring resumption (2026-11-22)

This unit starts at local master `4771df2c9b4e59e1171cd176314ee097c26e6436`.
Fixed native `b0889847fe6d03898be88acc9e366660efb40ab5` clears the spot owner
without clearing Ahead or yaw/pitch angular velocity. The next Upkeep integrates
Ahead/pitch0 plus drift with the retained velocities. A normal round retains
Ahead but resets velocities; native live restart retained Ahead137 while both
velocities became 0. New actor creation starts Ahead0.

The command runtime retains these separate values and resumes only after the
priority owner releases control. The previous Plant terminal full contract is
kept, including its real view, post-exit attack bit and rejection of its late
NAV draft as an Ahead source. Ordinary Use/Defuse releases need no terminal
flag. New explicit look owners supersede the default; lifecycle/serial guards
discard stale continuation. Native path Ahead uses integer engine VecToYaw
semantics; spot angles remain separate. Scope drift is mode-scoped.

Evidence index: `artifacts/local/debug/csbot-clear-look-upkeep-20261121/README.md`;
raw trials and proofs: `C:/Temp/astrabot-clear-look-upkeep`.
Eleven strictly serial trials verify actual game exits, including four native,
two before, three final plain and two final read-only CDB trials. One CDB trial
records raw stack/disassembly; only the subsequent verified ABI trial provides
the interpreted goal/spring evidence. The final DLL is
`35ca6e8e4f696f12f73053d6797565770c1f408f0543dbf94cff33d1b5d69578`.
`live-integrator-proof.json` independently predicts the first released view
within .0001 degree using actual goal/velocity samples. CmdStart splits or
duplicates are not treated as extra Upkeep integrations.

The native trial's retained Ahead0 and Astra's prior-path Ahead75 differ.
Do not treat the fixed first-light acceleration as proof of equal absolute
trajectories. Pre-Plant path/Ahead supply timing is the next independent issue;
the exact write that first supplied75 still needs timestamped instrumentation.
All-state Idle/Buy default supply and all Combat/Guard spring handoffs are
outside this acceptance.

Final offline gates: Windows170/170 and PE x86 exactly7 exports, followed by
Linux169/169 and ELF x86 exactly7 exports. ELF SHA-256:
`bbff5856bb7962c66cc8c4d20ff6718556685f6e6b202f4e65495f075fecc094`.
The reviewed trace correction pins138 move records (136 view-only, two
projections) without changing any other callback/RNG or count153320. Actual
world direction(-1,0)/speed240 reproduces the two side threshold crossings.
Debug and ordinary Windows traces match byte for byte. Corrected SHA-256:
Windows `bfd4c0bc21dd9c408eae4bbec327aacc0976c7ba8d7eea467f1bc3e62b6220d5`,
Linux `0d659ca24829b665ca3a59eef1a223756bca474b275367f284a036185751fb65`.
