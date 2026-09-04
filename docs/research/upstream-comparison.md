# Upstream comparison

調査snapshotは[source-manifest.md](source-manifest.md)に固定した。ここでの「利用」は
source copyを意味しない。Phase 0のdefaultは、観測可能な挙動と境界をreferenceにし、
AstraBotのcontractとtestから独立実装することである。

## 比較表

| Source | Purpose / navigation | AI / learning | Host・AMXX integration | 保守状況（snapshot時点） | AstraBotでの利用 |
|---|---|---|---|---|---|
| YaPB | 通常CS向け外部Bot。navigationはwaypoint graphで、Bot生成自体がgraphなしでは失敗する（[`BotManager::create`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/manager.cpp#L153-L173)）。 | task/planner、vision、sound、combat、grenade、practiceが同じBot/graph中心runtimeに統合される。practiceもwaypoint indexに結び付くためExperience基盤にはしない。 | `pfnCreateFakeClient`、GameDLL `player` entity、`MDLL_ClientConnect`/`ClientPutInServer`、message parsing、`pfnRunPlayerMove`まで最も完整な外部host例。AMXXは必須でない。 | C++17（[`CMakeLists.txt`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/CMakeLists.txt#L15-L16)）。snapshot commitは2026-03-01。 | **Host/behavior reference**。weapon/message/sound/visibilityの境界とlive acceptanceを学ぶ。forkもwaypoint layerの移植も行わない。 |
| SyPB | YaPB 2.7.2系を拡張したwaypoint BotとSwNPC。navigation/NPC controlはengine entityとwaypointへ密結合。 | 従来型Bot state/goalを外部から上書きできるが、独立World Model/Planner APIではない。 | AMXX moduleは`MF_AddNatives`でversioned control native群を登録し、enemy/move/look/goalを公開する（[`base.cpp`](https://github.com/CCNHsK-Dev/SyPB/blob/4c364fbe40d8356154f66527827bb75100aa7265/Project%20SyPB/SyPB_API/source/base.cpp#L572-L645)）。SwNPCはより広いentity APIを公開する。 | snapshot commitは2025-12-18だが、現代YaPBとの差分追従を保証する構造ではない。 | **API concept only**。小さいversioned native/query/event surfaceを参考にする。internal pointer、mutable nav、NPC APIは公開しない。 |
| CS-EBOT | Zombie/特殊modeを重視するwaypoint Bot。`FindGoalZombie`はterror/random waypointを選び（[`navigate.cpp`](https://github.com/EfeDursun125/CS-EBOT/blob/23335620127198c82e060a4215b8d812de28bb24/source/navigate.cpp#L40-L58)）、route costは近傍敵数の二乗項を含む（[同file](https://github.com/EfeDursun125/CS-EBOT/blob/23335620127198c82e060a4215b8d812de28bb24/source/navigate.cpp#L1207-L1252)）。 | dynamic pursuit/escape、group conflict、obstacle/stuck recoveryを持つ。`CheckStuck`は移動履歴・進捗・ladder/crouch状態を使う（[同file](https://github.com/EfeDursun125/CS-EBOT/blob/23335620127198c82e060a4215b8d812de28bb24/source/navigate.cpp#L2652-L2740)）。すべてwaypoint pathとBot mutable stateの上にある。 | Metamod外部Botだが、public control boundaryより専用runtimeが中心。 | snapshot commitは2026-06-02。root MPL-2.0と対象file MIT noticeの混在を追跡する必要がある。 | **Behavior reference**。crowd cost、yield、progress-based stuck detectionをNav corridor/local steering向けrequirementsへ翻訳する。codeはコピーしない。 |
| RealBot | play中にnodeを追加するwaypoint/node graph。`addNodesForPlayers`は全active clientを走査し、human/botを区別しない（[`NodeMachine.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeMachine.cpp#L1260-L1320)）。NavMeshではない。 | team別`fDanger[2]`/`fContact[2]`（[`NodeDataTypes.h`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeDataTypes.h#L149-L164)）、visibility、route dangerをplayで更新・保存する。 | 古いMetamod/HLSDKを同梱したmonolithic Bot。外部の安定APIではない。 | snapshot commitは2019-10-02。root license不在、fileは曖昧なGPL/mixed heritage。 | **Learning concept only**。persistent danger/contact/traffic/visibilityと観測由来を採用し、graph、raw format、codeは採用しない。 |
| ReGameDLL_CS / zBot | GameDLL内蔵CS Bot。static area/connection/hiding/approach/encounter/placeとA*-like pathを持つ。`.nav` v5 load後にladderをlive mapから再構築する。 | `CCSBot`と`CCSBotManager`がGameRules/CBasePlayer/global navへ直接接続する。汎用CoreではなくGameDLL内部実装。 | [`CCSBotManager::AddBot`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/dlls/bot/cs_bot_manager.cpp#L1583-L1642)は`CCSBot` playerを生成しGameDLL内menu handlerを直接呼ぶ。AMXX/ReAPIから観測可能でも独立host APIではない。 | snapshot commitは2026-08-28。活発だがAPI/ABIとlicense headerの混在がある。 | **Format/behavior/reference target**。`.nav` readerとA*は独立実装し、GameDLL/ReAPI型をadapter外へ出さない。 |

## YaPBから得るnavigation以外の境界

| Concern | Source-backed observation | AstraBot contractへの変換 |
|---|---|---|
| Plugin lifecycle | `ClientDisconnect`, `ServerActivate`, `ServerDeactivate`, `StartFrame`をDLL hook tableへ登録（[`linkage.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/linkage.cpp#L216-L413)）。 | Adapterがmap/player lifecycle eventをCoreへimmutable eventとして渡す。 |
| FakeClient | [`Game::createFakeClient`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/engine.cpp#L1366-L1384)がengine clientを確保し、[`Bot::Bot`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/manager.cpp#L1146-L1187)がGameDLL lifecycleへ通す。 | EntityはGameDLL所有、AstraBotはgeneration付き`PlayerId`から`BotAgent`を外部関連付けする。private dataを置換しない。 |
| CS menus/messages | `VGUIMenu`/`ShowMenu`からteam/class stateへ変換（[`message.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/message.cpp#L85-L126)）、`menuselect`を送る。 | Join state machineをadapterに置き、Coreは`JoinRequest`/`Joined`だけを見る。 |
| Motor | [`Bot::runMovement`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/botlib.cpp#L4001-L4045)がframe elapsed timeからmsecを求め`pfnRunPlayerMove`へ送る。 | Coreの`BotCommand`をadapterがmain-thread movement callへ変換する。 |
| Observation | `message.cpp`, `vision.cpp`, `sounds.cpp`, `combat.cpp`はGoldSrcのmessage/entity/trace stateを直接読む。 | 読み方は参考にするが、Coreにはvisibility/sound/weaponの値snapshotとsource timestampだけを渡す。 |
| Scheduling | AIは`StartFrame`後にBotごとに進む（[`linkage.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/linkage.cpp#L365-L416)）。 | Engine callbacksはmain thread限定。Core schedulingは明示tick、budget、seedを持ち、engine accessをworkerへ漏らさない。 |

## 結論

YaPBは現行外部hostとして最も具体的なreferenceだが、AstraBotのbaseではない。
CS-EBOTとSyPBもwaypoint/entity couplingを保持する。RealBotの価値はpersistent
feedback loop、ReGameDLL/Valveの価値はformatとruntime behaviorの証拠である。
したがってArchitecture B（new AstraBot + source-backed references）が、NavMesh、
World Model、deterministic offline test、将来のVanilla adapterを同時に成立させる。
