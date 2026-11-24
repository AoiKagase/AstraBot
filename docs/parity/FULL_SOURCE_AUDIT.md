# 全製品ソースと残件の監査（2026-11-22）

## C02-2d6 Guard候補ゼロ実機対照（2026-11-24）

製品変更なし。専用ゼロ候補NAVで、保持Native1更新・保持Noise1更新のintervalのみ抽選→Clearを同一実機で確認。Native投擲待機は保持。通常NAVを一時入力へ切替後、各実PID終了時に原文復元した。通常マップの自然なゼロ候補とは区別する。状態間共有時計の実機など残件は継続。[詳細](GUARD_ZERO_LIVE_C02_2D6.md)。

## C02-2d5 Guard保持Noise実機対照（2026-11-24）

製品変更なし。C02-2d4と同じ最終DLL/PDBを同一サーバーへ再配備し、保持Noise54更新（期限前49、期限到達5）で抽選時計・抽選後LOW拒否・Noise保持を限定受入れ。音源位置を保持する入力対照で旧試行の場所再選択混入を除いた。HLDS37820/CDB26824はexit0、core原文復元、コピー追加0。候補ゼロ・Hunt↔Guard状態間時計の実機は残る。[詳細](GUARD_NOISE_LIVE_C02_2D5.md)。

## C02-2d4 Guard/Hunt共有視線時計（2026-11-24）

記憶敵候補・geometry再構築によるGuard時計resetを修正し、HuntとBot所有の静止時計を共有。Windows重点16/16＋再入補強1/1、同一実機Native保持63更新のうち期限前57更新は抽選なし、期限到達6更新は1〜2秒の次回期限設定を確認。HLDS22024/CDB35852はexit0、core復元・コピー追加0。Hunt↔Guardをまたぐ実機対照と保持Noise/zero実機は未完。大項目47件を維持。[詳細](GUARD_LOOK_CLOCK_C02_2D4.md)。

## C02-2d3 Guard受付・独立Ahead（2026-11-24）

Guard静止の抽選後Noise/Native受付、ゼロ候補Clear、独立Ahead、geometry/RNG再入停止、銃所持による視線省略を修正。Windows重点16/16＋テスト補強後1/1、同一実機のNative保持→interval→point→LOW拒否をactor1の31更新で限定確認。DLL `e1df4a813555…`、全実PID終了0・core復元・コピー追加0。保持Noise/ゼロ候補の実機と、記憶敵候補等でresetされるGuard抽選時計は未完。大分類の47残件を維持し、次はGuard時計。全履歴・両OS全体検証は大分類完了時。[詳細と限界](GUARD_LOOK_ADMISSION_C02_2D3.md)。

## C02-2d2 保持NoiseのHunt受付（2026-11-24）

保持Noise中の静止処理を抽選後のLOW拒否へ、移動処理を抽選前の拒否へ合わせた。静止候補ゼロはNoiseもClear。Windows重点12/12、既存実機のactor1/full56ほか7更新で保持Noise中の間隔→候補→受付判定を採録。最終DLL `75257f8b0b96…`。実PID exit0・core原文復元・コピー追加0。実機moving/zero/Guard・Linux・全履歴は未検証。GuardのNoise/Native受付と他状態の再入停止等は残る。大分類完了時に全体検証する方針を維持。[詳細と範囲](RETAINED_NOISE_ADMISSION_C02_2D2.md)。

## C02-2d1 新規Noiseのhandled-returnとHide姿勢（2026-11-24）

新規NoiseをHunt/Guardの選択前に処理し、拒否された音要求でも後続の低優先度抽選を止める。同full再入で旧処理を中止し、Hideのしゃがみを維持。重点14/14、Windows全体初回182/184後に台帳・厳密履歴補正を追加して関連3/3再検証、Linux183/183、PE/ELF各7export。固定履歴151933行の差は無武装Hideのしゃがみ6入力のみ、RNG510行不変。既存実機の公開銃声でactor slot2/full90のStartle→処理済み→Hunt打切り、LookAround抽選0を確認。実HLDS42184/CDB25376は終了0、core原文復元。大分類ごとの検証方針により今回のC4広範回帰は未実施。保持NoiseのHunt受付はC02-2d2で限定対応。GuardのNoise/Native受付・ゼロClear・AheadはC02-2d3で限定対応。Guard抽選時計と保持Noise/zero実機、他状態の後段再入停止は未完。C02部分対応・47残件を維持。[詳細](LOOK_NOISE_ORDER_C02_2D1.md)。`438842e` でコミット・ローカルmaster FF済み。検証時原文と製品・保護ファイルの指紋を後確認済み。

## C02-2c4 accepted-BEGINの旧Hunt LOW所有解除（2026-11-24）

Windows182/182＋PE7、Linux181/181＋ELF7合格。両OS履歴151933行・RNG510行差0。既存実機・元NAVでactor2/full191の旧LOW1→0を最初のgrenade Upkeepより前に採録。C4 0/7/8も合格、全実PID終了0・core原文復元。旧LOW残留の限定修正で、実失効後の長期再開・全owner・Linux実機の合格ではない。C02は部分対応、47残件を維持。[詳細](GRENADE_OWNER_RETIREMENT_C02_2C4.md)。`889c35c` でコミット・ローカルmaster FF済み。後指紋も一致。

## C02-2c3 moving Huntの遭遇視線抽選を限定対応（2026-11-24）

Windows181/181＋PE7、Linux180/180＋ELF7合格。最終DLL `9f1815da0af2…` を既存実機へ再配備し、元NAV上のmoving HuntでNative手榴弾視線がある間、遭遇視線の抽選前に戻ることを74組で確認した。旧LOW所有なし・有効encounter・unsafe・抽選期限到達と、同frameの実入力を結び、うち2組はpin保持中。公開Frozen準備後に通常NAV移動する専用fixtureであり、自然起動や全経路一致の受入れではない。C4 0/7/8も合格、全実PID終了0・設定原文復元、サーバーコピー追加0。C02部分対応・49歴史項目／47残件を維持。Native開始時に旧Hunt LOW保持を退役させない所有残件をC02内へ明記した。詳細は [GRENADE_MOVING_ADMISSION_C02_2C3.md](GRENADE_MOVING_ADMISSION_C02_2C3.md)。以下は各時点の履歴を保持する。

## C02-2c2 静止・候補ありのHunt視線受付を限定対応（2026-11-24）

Windows180/180＋PE7、Linux179/179＋ELF7合格。最終DLL `a8f15c2b5742…` を既存実機へ再配備し、元NAV・公開fixtureで候補抽選後の手榴弾視線優先を読取りCDBで確認した。同Actor/fullで候補拒否2件、native点・期限・待機を保持し、Hunt出力とcommand保持視線を変更しない。通常候補採用の対照も確認。C4 0/7/8は逐次合格、全実PID終了0・core.ini原文復元、追加サーバーコピー0。これは候補受付の限定対応で、同tick実角度差・FetchBomb実機・moving/Guard/Noise順序の一致は主張しない。C02部分対応、49歴史項目／47残件を維持。詳細は [GRENADE_HUNT_ADMISSION_C02_2C2.md](GRENADE_HUNT_ADMISSION_C02_2C2.md)。以下は各時点の履歴を保持する。

## C02-2c1 静止候補ゼロのClearLookを限定対応（2026-11-24）

Windows178/178＋PE7、Linux177/177＋ELF7合格。最終DLL `c76563d679cf…` を既存実機へ再配備し、候補配列だけをゼロにした有効な専用NAV入力で、手榴弾視線の解除・独立したピン保持32入力・期限後解放1入力を読取りCDB/実入力で確認した。形状・接続・隠れ場所・遭遇・場所情報は元NAVと同じで、試行終了後に元NAVの原文とmetadataを復元した。元NAVでC4 0/7/8も逐次合格、各実HLDS/CDB終了0・core.ini原文復元、追加サーバーコピー0。自然de_dust2の全経路一致とは扱わない。C02全体は部分対応、49歴史項目／47残件を維持。次はC02-2c2の非ゼロ・moving・Guard/Noise順序。詳細は [GRENADE_ZERO_APPROACH_C02_2C1.md](GRENADE_ZERO_APPROACH_C02_2C1.md)。以下は各時点の履歴を保持する。

## C02-2b 手榴弾視線中の音要求・抑制時計を部分修正（2026-11-23）

Windows176/176＋PE7、Linux175/175＋ELF7合格。最終DLL `257a64a4530f…` を既存実機へ再配備し、公開の危険銃声が視線受付で拒否されてもRNGと2〜4秒の抑制時計が進むことを読取りCDB・実入力で確認した。C4実機0/7/8も逐次合格、追加サーバーコピー0。Hunt/Guardの後続順・moving encounterはC02-2c2として残し（stationary0 Clearは後続C02-2c1で限定対応）、49歴史項目／47残件を維持する。詳細は [GRENADE_NOISE_ADMISSION_C02_2B.md](GRENADE_NOISE_ADMISSION_C02_2B.md)。以下のC02-2a/C02-1は当時の範囲を保持する。

## C02-2a 手榴弾視線中の破壊物割込みを部分修正（2026-11-23）

Windows175/175＋PE7、Linux174/174＋ELF7合格。最終DLL `6d662b9aee0a…` を既存実機へ再配備し、実マップ破壊物のTouchで手榴弾視線が上書きされないこと、保持入力と装備要求が残ることを読取りCDBと実入力で確認した。C4実機0/7/8も逐次確認。追加サーバーコピー0。C02は部分対応、49歴史項目／47残件を維持する。noise/Hunt/Guard等の受付とRNG、経路投擲、C10の共通装備時計・接触処理時刻は未完。詳細は [GRENADE_OBSTACLE_ADMISSION_C02_2A.md](GRENADE_OBSTACLE_ADMISSION_C02_2A.md)。以下のC02-1記録は当時の範囲を保持する。

## C02-1 手榴弾保持・認識敵への投擲を部分完了（2026-11-23）

Windows174/174＋PE7、Linux173/173＋ELF7合格。最終DLL `77b92327d98d…` を既存実機へ再配備し、敵不在の保持38入力、敵認識後の待機と接触喪失後のstrict 3秒解放を読取りCDB/実入力で確認した。C4実機0/7/8も直列確認。サーバーコピー追加0。C02は部分対応のまま残す。低優先度視線の受付/RNG・障害物との競合（C02-2）、安全時間終了後の経路向け投擲（C02-3）、C08の照準所有は未受入れ。49歴史項目／47残件。詳細は [GRENADE_LIFECYCLE_C02_1.md](GRENADE_LIFECYCLE_C02_1.md)。

## C16 full移動の視線公開を限定完了（2026-11-23）

Windows173/173＋PE7、Linux172/172＋ELF7合格。最終DLL `456ccc43…95610` を既存実機へ再配備し、同一Actor・実ゲーム時刻のfull移動598/598で共有視線と実RunPlayerMoveの一致を確認した。C16を限定完了へ更新。49歴史項目／47残件。C08・全参照リプレイ・両OS実機/継続性能は未完。詳細・指紋・除外区間は [FULL_LOOK_PUBLICATION_C16.md](FULL_LOOK_PUBLICATION_C16.md)。次はC02の手榴弾開始/投擲経路。

従来の5群は優先事項の要約であり、残件の完全な一覧にはなっていなかった。手榴弾・通常装備・狙撃銃、非C4シナリオ、移動幾何、管理/profile等の具体名を追加し、完了済みの旧記録を分離した。以下のIDを現行残件の正本とする。

監査基点はAstraBot `dd1301e35f29a7d3f03b647980187547fec29f6e`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。この監査時点では実装変更はなく、製品は既存検証済み `6fdadbd` のまま。後続のC01修正は [限定完了記録](SHARED_LOOK_C01.md) に追記し、当時のファイル指紋を現在値へ置き換えない。

## 対象と網羅範囲

| 対象 | 件数 | 今回の扱い |
|---|---:|---|
| Astra src/includeの製品C/C++ | 198 | 全ファイルに担当・指紋・照合記録。機能実装は全体読取り、結線ヘッダ/大規模pluginは宣言/構造と実行call chain照合を区別 |
| テストC/C++ | 125 | 一覧と指紋、関連assertion/fixtureを照合。全fixture再帰読取りや再実行は主張しない |
| SDK/API・資料中のC/C++ | 185 | SDK/API179、過去資料6。依存/履歴の棚卸しでありBot機能の未実装とは数えない |
| 追加script/build/export | 38 | build/export4、検証27、資料7を分類。CMake/公開7関数の面と検証の責務を確認 |
| 固定参照C/C++ | 363 | Bot/NAV53＋公開GameDLL文脈4のfull/focused照合、一般GameDLL/SDK/テスト306は依存棚卸し。一般GameDLL全行の意味解析完了とはしない |

全ファイル名・行数・SHA・担当・関連残件は [FULL_SOURCE_INVENTORY.json](FULL_SOURCE_INVENTORY.json)。詳細報告は `artifacts/local/debug/csbot-full-source-audit-20261122/audit-{states,combat,navigation,parent}.json`。ファイルの担当漏れ0は全分岐一致・全パス実行の証明ではない。未確認をMATCHへ昇格しない。

## 現行残件（5群）

台帳は49歴史項目、C01/C16限定完了を除いて47残件（2026-11-23更新）。C15はM05へ統合。S05とNAVの護送入力、C14とS13等の依存関係は残し、独立したバグ件数には換算しない。S14は通常対戦milestone外だが、全ソース棚卸しから隠さない。

### 1. 視線・戦闘・武器

| ID | 状態 | 修正/確認対象 | Astra根拠 | 参照根拠 |
|---|---|---|---|---|
| C01 | 限定完了 | [共有角速度・1回相当積分](SHARED_LOOK_C01.md)。live範囲0/3/5、全軌跡は未受入れ | `include/astrabot/compat/combat_behavior.hpp:401`<br>`src/adapter/metamod/runtime/command/bot_command_runtime.hpp` | `regamedll/dlls/bot/cs_bot_vision.cpp:57`<br>`regamedll/dlls/bot/cs_bot_update.cpp:179` |
| C16 | 限定完了 | Compatibility full視線保持・MoveTo/Follow/Escapeの次Ahead・停止時有限Ahead保持。最終実機598/598一致。旧86件の全branch帰属は主張しない | `src/adapter/metamod/runtime/command/bot_command_runtime.cpp:1273`<br>`src/adapter/metamod/runtime/command/bot_command_runtime.cpp:1489` | `regamedll/dlls/bot/cs_bot_vision.cpp:57`<br>`regamedll/dlls/bot/cs_bot_update.cpp:179` |
| C02 | 部分対応 | [保持/認識敵の投擲と明示Clear](GRENADE_LIFECYCLE_C02_1.md)と[破壊物視線拒否・保持入力](GRENADE_OBSTACLE_ADMISSION_C02_2A.md)は部分完了。[音の受付拒否・抑制時計](GRENADE_NOISE_ADMISSION_C02_2B.md)も部分完了。stationary0 ClearはC02-2c1、静止候補ありの低優先度拒否は[C02-2c2](GRENADE_HUNT_ADMISSION_C02_2C2.md)で限定対応。moving Huntの抽選前抑止は[C02-2c3](GRENADE_MOVING_ADMISSION_C02_2C3.md)で限定対応。Native開始時の旧LOW所有解除は[C02-2c4](GRENADE_OWNER_RETIREMENT_C02_2C4.md)で限定受入れ。新規Noiseのhandled-return・限定的な旧処理停止・Hide姿勢は[C02-2d1](LOOK_NOISE_ORDER_C02_2D1.md)で限定受入れ。保持NoiseのHunt受付はC02-2d2で限定対応。GuardのNoise/Native受付・ゼロClear・AheadはC02-2d3で限定対応。Guard抽選時計と保持Noise/zero実機、他状態の後段再入停止、FetchBomb実機、直接の再帰Root full更新、安全時間終了後の経路投擲は未完 | `src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1059`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1419` | `regamedll/dlls/bot/cs_bot_update.cpp:347`<br>`regamedll/dlls/bot/cs_bot_update.cpp:490` |
| C03 | 差確定 | 通常Updateの装備再選択・安全時間中の装備 | `src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:8`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:74` | `regamedll/dlls/bot/cs_bot_update.cpp:519`<br>`regamedll/dlls/bot/cs_bot_update.cpp:526` |
| C04 | 差確定 | SilencerCheck要求の実行時接続 | `include/astrabot/compat/weapon_actions.hpp:225`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1434` | `regamedll/dlls/bot/cs_bot_weapon.cpp:760`<br>`regamedll/dlls/bot/cs_bot_update.cpp:556` |
| C05 | 差確定 | 遠距離ショットガンから拳銃への切替 | `include/astrabot/compat/weapon_actions.hpp:236`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1374` | `regamedll/dlls/bot/states/cs_bot_attack.cpp:319` |
| C06 | 差確定 | 狙撃銃のscope解除/事前zoom/resumeZoom | `include/astrabot/compat/weapon_actions.hpp:250`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1428` | `regamedll/dlls/bot/states/cs_bot_attack.cpp:331`<br>`regamedll/dlls/bot/cs_bot_vision.cpp:453` |
| C07 | 差確定 | 敵を見失った後の回避・距離調整 | `src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1245`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1281` | `regamedll/dlls/bot/states/cs_bot_attack.cpp:481` |
| C08 | 差確定 | Combat軽更新/全更新の視認・照準積分順 | `src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:270`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1238` | `regamedll/dlls/bot/cs_bot_update.cpp:46`<br>`regamedll/dlls/bot/cs_bot_update.cpp:441` |
| C09 | 差確定 | friendly-fire・敵の盾を含む発砲/脅威判定 | `src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1177`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:318` | `regamedll/dlls/bot/cs_bot_weapon.cpp:54`<br>`regamedll/dlls/bot/cs_bot_weapon.cpp:78` |
| C10 | 差確定 | 破壊物対応の装備時計を共通EquipBestへ接続 | `src/adapter/metamod/runtime/behavior/bot_behavior_obstacles.cpp:44`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_obstacles.cpp:258` | `regamedll/dlls/bot/cs_bot.cpp:318`<br>`regamedll/dlls/bot/cs_bot_weapon.cpp:437` |
| C11 | 差確定 | 敵非可視で再装填中の最終移動停止 | `src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:186`<br>`src/adapter/metamod/runtime/command/bot_command_runtime.cpp:1399` | `regamedll/dlls/bot/cs_bot_update.cpp:788` |
| C13 | 差確定 | BendLineOfSightの整数角・地面探索 | `src/adapter/metamod/runtime/behavior/bot_look_geometry.cpp:15`<br>`src/adapter/metamod/runtime/behavior/bot_look_geometry.cpp:48` | `regamedll/dlls/bot/cs_bot_vision.cpp:609` |

### 2. 状態・目的・C4/人質/VIP

| ID | 状態 | 修正/確認対象 | Astra根拠 | 参照根拠 |
|---|---|---|---|---|
| S01 | 差確定 | T設置先のzone選択・重複area抽選 | `src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:1033`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:1042` | `regamedll/dlls/bot/states/cs_bot_idle.cpp:183`<br>`regamedll/dlls/bot/states/cs_bot_idle.cpp:186` |
| S02 | 差確定 | CT探索siteの距離・経路コストの区別 | `src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1219`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1221` | `regamedll/dlls/bot/states/cs_bot_idle.cpp:312`<br>`regamedll/dlls/bot/states/cs_bot_idle.cpp:313` |
| S03 | 差確定 | Hunt30秒退出時のrogue解除 | `src/core/compat/state_machine.cpp:642`<br>`src/core/compat/state_machine.cpp:769` | `regamedll/dlls/bot/states/cs_bot_hunt.cpp:56`<br>`regamedll/dlls/bot/cs_bot.h:1029` |
| S04 | 差確定 | 攻撃側Idleの狙撃待機抽選 | `src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:342`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1600` | `regamedll/dlls/bot/states/cs_bot_idle.cpp:205`<br>`regamedll/dlls/bot/states/cs_bot_idle.cpp:529` |
| S05 | 部分対応 | CT人質収集/救助・護送数・移動比較 | `include/astrabot/compat/scenario_behavior.hpp:613`<br>`include/astrabot/compat/scenario_behavior.hpp:624` | `regamedll/dlls/bot/states/cs_bot_idle.cpp:672`<br>`regamedll/dlls/bot/states/cs_bot_idle.cpp:685` |
| S06 | 部分対応 | TのHunt/Hide中の人質警備変更 | `src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1517`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1605` | `regamedll/dlls/bot/states/cs_bot_hunt.cpp:126`<br>`regamedll/dlls/bot/states/cs_bot_hunt.cpp:148` |
| S07 | 差確定 | TのHide中の解除者迎撃 | `src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:684`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1825` | `regamedll/dlls/bot/states/cs_bot_hide.cpp:190`<br>`regamedll/dlls/bot/states/cs_bot_hide.cpp:195` |
| S08 | 部分対応 | VIP/Escapeの20秒境界・区域/戦術選択 | `include/astrabot/compat/scenario_behavior.hpp:456`<br>`include/astrabot/compat/scenario_behavior.hpp:462` | `regamedll/dlls/bot/states/cs_bot_idle.cpp:432`<br>`regamedll/dlls/bot/states/cs_bot_idle.cpp:496` |
| S09 | 差確定 | entityによるscenario判別とCT旧救出地点fallback | `src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:744`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:936` | `regamedll/dlls/bot/cs_bot_manager.cpp:1383`<br>`regamedll/dlls/bot/cs_bot_manager.cpp:1399` |
| S10 | 部分対応 | T爆弾警備の隠れ場所選定・寿命 | `include/astrabot/compat/bomb_guard.hpp:122`<br>`include/astrabot/compat/bomb_guard.hpp:181` | `regamedll/dlls/bot/states/cs_bot_idle.cpp:134`<br>`regamedll/dlls/bot/states/cs_bot_idle.cpp:138` |
| S11 | 部分対応 | BuyのCareer・状態進入/装備観測の厳密性 | `src/adapter/metamod/runtime/behavior/bot_behavior_buying.cpp:113`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_buying.cpp:149` | `regamedll/dlls/bot/states/cs_bot_buy.cpp:48`<br>`regamedll/dlls/bot/states/cs_bot_buy.cpp:68` |
| S12 | 部分対応 | 人間leaderのauto-follow許可の観測 | `include/astrabot/compat/teamwork_policy.hpp:68`<br>`include/astrabot/compat/teamwork_policy.hpp:206` | `regamedll/dlls/bot/states/cs_bot_idle.cpp:107`<br>`regamedll/dlls/bot/states/cs_bot_follow.cpp:103` |
| S13 | 実機未受入 | Plant修正後に残る自然拾得/標準GameDLL受入れ | `src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1721`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:2229` | `regamedll/dlls/bot/states/cs_bot_idle.cpp:169`<br>`regamedll/dlls/bot/states/cs_bot_move_to.cpp:263` |
| S15 | 差確定 | 通常Hideの占有・経路失敗時の場所再選択 | `src/core/compat/state_machine.cpp:397`<br>`src/core/compat/state_machine.cpp:680` | `regamedll/dlls/bot/states/cs_bot_hide.cpp:343`<br>`regamedll/dlls/bot/states/cs_bot_hide.cpp:348` |
| C12 | 公開観測調査 | 複数武器slotの先頭を確定する公開観測 | `src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:77`<br>`src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:102` | `regamedll/dlls/bot/cs_bot_weapon.cpp:443`<br>`regamedll/dlls/bot/cs_bot_weapon.cpp:482` |
| C14 | 実機未受入 | C4初回未装備・実水没の直接受入れ | `src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:74`<br>`src/adapter/metamod/runtime/command/bot_command_runtime.cpp:539` | `regamedll/dlls/bot/cs_bot_weapon.cpp:420` |

### 3. NAV・移動・知覚

| ID | 状態 | 修正/確認対象 | Astra根拠 | 参照根拠 |
|---|---|---|---|---|
| NAV-01 | 差確定 | 段差ジャンプ80/30/10と地面probeの順序/方向 | `src/core/nav/locomotion.cpp:336`<br>`src/core/nav/locomotion.cpp:346` | `regamedll/dlls/bot/cs_bot_nav.cpp:126`<br>`regamedll/dlls/bot/cs_bot_nav.cpp:199` |
| NAV-02 | 差確定 | 足元左右feelerによる壁回避操舵 | `src/adapter/metamod/runtime/navigation/bot_navigation_runtime.cpp:1211`<br>`src/core/runtime/nav_roam_controller.cpp:2593` | `regamedll/dlls/bot/cs_bot_pathfind.cpp:1169`<br>`regamedll/dlls/bot/cs_bot_pathfind.cpp:1461` |
| NAV-03 | 差確定 | 生存可能な有痛落下を拒むzero-damage gate | `src/adapter/metamod/runtime/navigation/bot_navigation_runtime.cpp:1073`<br>`src/core/nav/locomotion.cpp:360` | `regamedll/dlls/bot/cs_bot.h:1632`<br>`regamedll/dlls/bot/cs_bot.h:1640` |
| NAV-04 | 実行時接続が部分対応 | 人質護送数をpath costへ接続 | `include/astrabot/nav/nav_query.hpp:107`<br>`src/core/nav/nav_query.cpp:770` | `regamedll/dlls/bot/cs_bot.h:1667` |
| NAV-05 | 差確定 | NAV v1–4の.loc place付随データ読取り | `src/core/nav/legacy_nav_reader.cpp:360`<br>`src/adapter/metamod/navigation/nav_loader.cpp:236` | `regamedll/game_shared/bot/nav_file.cpp:639`<br>`regamedll/game_shared/bot/nav_file.cpp:697` |
| NAV-06 | 算法差確定・対照未実行 | A* open-node減額→同値時の並び順 | `src/core/nav/nav_query.cpp:825`<br>`src/core/nav/nav_query.cpp:840` | `regamedll/game_shared/bot/nav_area.cpp:2446`<br>`regamedll/game_shared/bot/nav_area.cpp:2494` |

### 4. 公開観測・イベント・発話・管理/profile

| ID | 状態 | 修正/確認対象 | Astra根拠 | 参照根拠 |
|---|---|---|---|---|
| S14 | 未実装・現milestone外 | NAV学習/解析/編集/保存（現対戦milestone外） | `docs/parity/IMPLEMENTATION_BACKLOG.md:21` | `regamedll/dlls/bot/cs_bot_learn.cpp:169`<br>`regamedll/dlls/bot/cs_bot_learn.cpp:385` |
| OBS-01 | 差確定 | 任意/非player soundからの不要な調査音生成 | `src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:636`<br>`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:668` | `regamedll/dlls/bot/cs_bot_event.cpp:275`<br>`regamedll/dlls/bot/cs_bot_event.cpp:280` |
| OBS-02 | 差確定 | 標準武器eventの受信時listener/局在化処理 | `src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:503`<br>`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:517` | `regamedll/dlls/bot/cs_bot_event.cpp:357`<br>`regamedll/dlls/bot/cs_bot_event.cpp:389` |
| M01 | 差確定 | quota縮小時の死亡Bot優先 | `src/adapter/metamod/plugin/plugin_runtime.cpp:1657` | `regamedll/game_shared/bot/bot_util.cpp:209` |
| M02 | 差確定 | 自動追加失敗時のquota補正 | `src/adapter/metamod/plugin/plugin_runtime.cpp:1651`<br>`src/adapter/metamod/plugin/plugin_runtime.cpp:2422` | `regamedll/dlls/bot/cs_bot_manager.cpp:819`<br>`regamedll/dlls/bot/cs_bot_manager.cpp:866` |
| M03 | 差確定 | Any-team追加前のprofile絞込み | `src/adapter/metamod/config/compat_surface.cpp:93`<br>`src/adapter/metamod/plugin/plugin_runtime.cpp:2422` | `regamedll/dlls/bot/cs_bot_manager.cpp:798`<br>`regamedll/dlls/bot/cs_bot_manager.cpp:818` |
| M04 | 差確定 | profile Skin/custom Skin・bot_prefix | `include/astrabot/compat/profile_catalog.hpp:43`<br>`src/adapter/metamod/config/profile_loader.cpp:378` | `regamedll/game_shared/bot/bot_profile.cpp:109`<br>`regamedll/game_shared/bot/bot_profile.cpp:347` |
| M05 | 差確定 | bot_mimic入力複製・診断コマンド面 | `src/core/compat/command_registry.cpp:69`<br>`src/adapter/metamod/config/compat_surface.cpp:128` | `regamedll/game_shared/bot/bot.cpp:278`<br>`regamedll/game_shared/bot/bot.cpp:301` |
| PUB-STD-DAMAGE | 公開観測調査 | 標準GameDLLの判定前inflictor/被弾時刻の確定観測 | `docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:3`<br>`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:16` | `regamedll/dlls/bot/cs_bot.cpp:98` |
| PUB-STD-BLIND | 公開観測調査 | 標準GameDLLの正確なgameplay失明期限 | `include/astrabot/compat/public_observation.hpp:155`<br>`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:1202` | `regamedll/dlls/bot/cs_bot_vision.cpp:1135` |
| PUB-STD-ANIMATION | 公開観測調査 | 標準GameDLLの武器animationイベント列 | `docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:20`<br>`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:22` | `regamedll/game_shared/bot/bot_util.cpp:639` |
| PUB-HOSTAGE-OWNERSHIP | 公開観測調査 | 人質の現在leader/追従終了/護送数の確定観測 | `docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:34`<br>`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:36` | `regamedll/dlls/bot/cs_gamestate.cpp:572` |

### 5. 固定参照/RNG・実機受入れ・継続性能

| ID | 状態 | 修正/確認対象 | Astra根拠 | 参照根拠 |
|---|---|---|---|---|
| V01 | 実機未受入 | 全体の固定参照リプレイ・共有RNG順 | `src/adapter/metamod/bridge/engine_random_source.cpp:49`<br>`src/adapter/metamod/plugin/plugin_runtime.cpp:1791` | `regamedll/game_shared/bot/bot.cpp:225`<br>`regamedll/dlls/bot/cs_bot_update.cpp:186` |
| V02 | 実機未受入 | 両OS実機・照準/旋回・多人数継続性能 | `src/adapter/metamod/diagnostics/runtime_profiler.cpp:314`<br>`src/adapter/metamod/plugin/plugin_runtime.cpp:854` | `regamedll/dlls/bot/cs_bot_vision.cpp:57`<br>`regamedll/dlls/bot/cs_bot_update.cpp:186` |

各IDの条件・実際の差・関連テスト・必要な対照・確信度はJSONの`findings`に保持した。行数が存在するだけの根拠や、型/関数の存在だけで実行時接続を完了扱いにしていない。

## 完了/更新できる旧記録

| 記録 | 現在の判定 | 根拠・残る境界 |
|---|---|---|
| 12通常状態＋Attack overlayがない | 実装済み | `compat/state_machine.hpp`と実runtime。各状態の戦術まで一致とはしない（S群） |
| 30/10Hz分離・cached commandがない | オフライン境界実装済み | `bot_timing_scheduler.cpp`、実pluginのUpkeep→Reset→Full→Execute。C08のCombat積分差は残る |
| 視線trace/FOV/煙/認識queueがない | 現行実装あり | public adapter/20slot recognition。C09/NAV-/OBS-群の判定差・実機条件は残る |
| 音源のNAV内局在化・実イベント入力がない | 現行実装あり | native weapon hook/footstep/event経路。標準fallback時刻と任意sound差はNAV-/OBS-群に残す |
| 動的danger/aggression/crowd/fall/ladderがない | コストモデル/公開梯子実装済み | NAV-query/runtime。護送数接続・物理挙動・tie差はNAV-/OBS-群 |
| 発話/radio/memeが全面未実装 | 表現済み経路は実装済み | 11 active meme、named Say、compound parts、linked queue、dead/round保持。標準イベント/人質観測と実音声受入れは未完 |
| backlog21 CRT音声shuffle近似 | オフライン解消 | `chatter_voice_random.hpp`、voice testの2bank/重複/32roundでnative random_shuffle対照。実音声/UIは別受入れ |
| backlog22 Linux胸部照準既知失敗 | 現在の失敗として解消 | 最新既存証拠 Windows171/171、Linux170/170、PE/ELF7。過去の失敗ログは保持 |
| ClearLook後の次light視線再開 | 限定完了 | 3cde77b、cached commandと独立CDB。共有ばねC01は後続で限定完了、full公開C16も後続で限定完了、更新順C08は未完 |
| Idle/Follow早期Plant・Hunt yield/Ahead75 | 限定完了 | 95aec579、実機保持Ahead0。一般の目標/RNG差はS01等に残す |
| MoveTo→Plantの経路処理順 | 限定完了 | 6546606、非PROGRESSING確認と更新receipt。全NAVの一致とはしない |
| Plant入場更新のNAV入力/Run/callback | 限定完了 | a924873、製品フレーム回帰・実機到着境界。異なるnative timeout条件の同値入力受入れは残す |
| Plant前Idleの不要なpath/input | 限定完了 | b75be3a、path0/input0/目標cache抑止の正規回帰と実機 |
| cold Idle/Buyの通常look供給 | 限定完了 | 6fdadbd、actual cached/light/再接続springと最終DLLのCDBowner確認 |
| Escape終端後の目標再選択がない | 現行実装あり | objective terminalでcache解除→次更新で再選択。S群の選択metric/実機同値は別 |
| 盾購入の実装がない | 現行実装あり | public BuyContext→controllerのshield経路。Career/正確な時刻/実機はS11 |

`REPORT_ROUND_END`は固定参照に生成箇所がなく、BotAllHostagesGoneMemeも構築箇所がない。KilledMyEnemyは唯一のcaller/callee条件が矛盾し通常生成へ到達しない。未使用定義を「実装漏れ」として増やさない。round-end/deadの保持規則は既存実装/回帰として扱う。

## 証拠と次の修正単位

以下は2026-11-22監査時の記録。最新C01の再実行/実機受入れは [SHARED_LOOK_C01.md](SHARED_LOOK_C01.md)。監査時のオフライン合格は既存 `csbot-idle-buy-look-supply-20261122/evidence-final-v2/acceptance.json` と原ログの照合結果であり、今回の再実行ではない。ソース・ユーザーファイル・製品SHAと参照固定SHAを再確認する。今回の変更は監査資料のみで、新しいC++実装・テスト・実機再配備・サーバーコピーを行わない。

C01の共有ばねとC16のfull/light視線公開は後続で限定完了。次はC02へ進む。C08のCombat軽/full更新順は別残件として保持する。以後の実機確認は [LIVE_VERIFICATION.md](LIVE_VERIFICATION.md) の既存実機再配備、必要時のみ固定1コピー再利用を守る。pushなし。
