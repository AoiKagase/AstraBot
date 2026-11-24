# C02-2d2 保持Noise中のHunt視線受付

2026-11-24。C02の部分修正。Windows重点12/12と下記の実機静止条件を検証済み。全体49項目・未完47項目は変えない。

## 参照と修正

固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5` の `cs_bot_vision.cpp:333-363,403-515` は、新規Noiseを処理した更新と、既に保持しているNoise視線を区別する。静止時は期限到達後に間隔と候補を抽選し、その後LOW要求を優先度で拒否する。移動時は保持視線を抽選前に確認する。静止候補ゼロでは間隔抽選後にClearLookAtする。

Hunt/FetchBomb共通helperの `noiseLook.active()` による一律早期returnを外した。保持Noiseがある間は旧Hunt LOWの出力所有を退役させつつ、静止時の抽選を行う。候補ありでは抽選後にLOWを拒否してNoiseを維持し、ゼロ候補ではNoiseもclearする。移動時は遭遇抽選の前に保持Noiseを確認する。新規Noiseのhandled-returnと独立抑制時計は維持する。Guardはこの修正に含めない。

## オフライン

公開EmitSoundで実際の知覚へ足音を入力し、実profileを割り当てた独立Behaviorの正規Hunt遷移で確認した。これはhelper統合検証であり、Rootの再帰full更新や実機移動条件の受入れではない。

- 旧製品は、保持Noise中の静止間隔抽選を省略するRED。最初のprofile未設定による入力不成立は製品REDから除外。
- ゼロ候補では間隔抽選1回後にNoise upkeep出力が消える。候補ありでは間隔・候補各1回を消費してLOWを拒否し、Noise upkeepは残る。
- 有効なmoving encounterでは保持Noise中の抽選0。同じ経路・時刻のNoiseなし対照は静止・移動とも実際に抽選してLOWを採用する。NAV外に置いた音源による入力不成立試行は修正してから判定。
- 最終候補DLLおよび関連targetをビルドし、Windows重点12/12合格。全CTest、Linux、exports、固定履歴の再実行は大分類完了時へ繰り延べた。既存baselineは変更していない。今回の意図したRNG差は保持Noise中の静止期限で追加される間隔と候補の抽選であり、全履歴の差数は未計測。

## 同じWindows実機での限定受入れ

`D:/SteamCMD/cstrike_rehlds` を再利用、サーバーコピー追加0。元NAV、最終DLL/PDBを使用。正規テスト上で式を検証した読取りCDBをDLL module-load後に設定した。

`live-1` はAMXX公開emit_soundのみでは対象Noiseが成立せず、受入れから除外。現行観測コードはnative bot event接続時のclient footstep fallbackを抑止している。`live-2` fixtureは公開RunPlayerMoveによる短い制御移動へ変更した。ただし対象の保持Noiseは最初の制御移動より前に成立しており、移動pulseがその音の発生源だったとは主張しない。生のlaunch/exit receiptのfixture_scope文字列は試行1由来であり、この訂正をacceptance.jsonに明記した。両試行の原ログとfixtureは保持する。

`live-2` のactor slot1、full56/66/76/90/103/117/127で、Noise処理結果NotHandled・保持active1 → StateInterval → 静止候補3 → ApproachPoint → LOW受付判定active1を順に確認した。例えばfull66、時刻9.649509ではraw log588–592。新規NoiseのDuration抽選はfull54/55にあり、保持中の後続受付を確認したものである。

HLDS25560/CDB35084（試行1）、HLDS41196/CDB2800（試行2）はすべてexit0。残実PID0。core.ini原文SHA `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b` を復元し、今回追加した一時plugin/listを除去、既存設定・NAVの指紋を確認した。

| 製品 | SHA256 |
|---|---|
| DLL | `75257f8b0b967c60d0a91978ece5d0c4e673ae90f60a133f8bc2744a6ea3c6a4` |
| PDB | `d5202be88cedc01883b954a4891b95296556068b4d0fd7ffebaac4987e35b6d0` |

証拠：[freeze](../../artifacts/local/debug/csbot-retained-noise-20261124/final/freeze.json)、[重点12件](../../artifacts/local/debug/csbot-retained-noise-20261124/focused-12-green.log)、[実機限定受入れ](../../artifacts/local/debug/csbot-retained-noise-20261124/live-2/acceptance.json)。製品ソース200件・保護対象12件を固定した。

## 残る範囲

GuardのNative/Noise受付・ゼロ候補Clear・全owner退役、他状態の後段Noise再入停止、FetchBomb実機、直接の再帰Root full更新、期限切れ後の長期所有権は未完。今回のmoving/zero条件はオフライン合格であり実機合格ではない。Linux実機・広範なC4回帰も今回未実施。

後続更新（2026-11-24）：Guardの受付・ゼロClear・独立Aheadは[C02-2d3](GUARD_LOOK_ADMISSION_C02_2D3.md)で実装・重点検証、実機Native受付を限定確認。Guard抽選時計、保持Noise/zero実機は引き続き未完。
