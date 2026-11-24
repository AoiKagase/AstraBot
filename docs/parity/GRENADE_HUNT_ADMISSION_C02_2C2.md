# C02-2c2 静止Huntの候補あり視線受付

状態：オフライン・Windows実機の限定受入れ合格。C02全体は部分対応。基点 `c1c05a36b738c88ac783b0c3e2e8c1ce89be7ccc`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。

## 修正と参照順序

参照 `cs_bot_vision.cpp:470–495` は静止時に間隔RNG、候補RNGをこの順に消費し、LOW SetLookAtを要求する。`333–363` の受付は既存の高優先度spotがあれば、点・期限を書き換えず拒否する。native grenadeの優先度はUNINTERRUPTABLEである。

adapterの非ゼロ静止分岐で、既存callback/current検査と両抽選の後にnative spotの有無を確認する。存在すれば次のlook間隔だけ進め、Trackを生成しない。native点・look・待機・投擲期限・共有ばね・既存の低優先度保持点は変更しない。ゼロ候補の無条件Clearは前単位どおり残す。共用helperなのでFetchBombも同じ受付になるが、この単位の実機範囲はHuntだけ。

共有視線時計は既に同tickの低優先度stepを拒否する。今回のREDは候補生成・採用の差であり、同tickの実RPP角度差の証明ではない。未確認のview所有権や全LookAround順序を完了扱いにしない。

## 正規RED/GREENとオフライン検証

新規 `astrabot_native_grenade_hunt_admission` は実GameDLL武器観測で手榴弾を持ち、認識敵喪失後のnative spot/pinを保持する。接続2area＋候補付きの実NAVを読み、movement warmupを完了後、同じHuntで `StateInterval→ApproachPoint` を各1回消費して候補を拒否する。次fullでは再抽選しない。対照 `astrabot_hunt_approach_control` は敵を公開せずnative spotなしで同候補を採用する。

ガードを除いた最終対照の正規CMake/CTestは、関連10件中この拒否assertだけRED、通常採用対照を含む他9件は合格。ガード復帰で10件合格。初回REDログ名を初回GREENで上書きしたため、固有名 `red-canonical-final.log` にガード除去で再採録し保持した。CMD初期化の引用エラーは製品REDから除外。RNG順序の明示assert追加後にソースを固定し、全target build終了0→Windows180/180→Linux179/179を逐次実行した。PE/ELFは各7exact exports。

既存履歴は両OSとも151933行・順序付きRNG510行で前単位との差0、固定baseline再生成なし。新規受付fixtureはこの旧履歴に含まれない。今回の拒否はRNGを追加・削除せず、既存の間隔→候補順を保持する。

履歴ヘルパーが旧Linux証拠のarchive名をv3と仮定して停止した。旧ヘルパーでcorrected v3 rawを不変のv2 archive名へ保存していたことを確認し、実在するv2正本と既に生成済みの今回rawを比較して再開した。旧証拠変更・全build/CTest再実行なし。経緯と旧archive SHAは `history-helper-path-incident.json` に保持する。

| 製品 | SHA256 |
|---|---|
| DLL | `a8f15c2b57429365846cecb7f8201cb4b2ac06acdedfeb2b3b83ba6829d70517` |
| PDB | `c2c064b1fa2e844fa62e7bd2090f026bb16cf3384c2661d5ee4be0b0bbbeeb7b` |
| ELF | `884d2382db0ceec87a04eb1143a8313e66390d95d43b3e535df4d32e18533a23` |

## 既存実機の限定受入れ

`D:/SteamCMD/cstrike_rehlds` を再利用し、サーバーコピー追加0。前単位の公開fixtureと元NAVを再使用し、新しいfixture配置もNAV変更も行わない。最終DLL/PDBを再配備し、core.iniのみ専用listへ一時切替、実HLDS/CDBの終了0を確認してから原文へ復元した。module-load待機後にBPを設定し、最終PDBのnative objectとhelper/commandを読取りだけで観測する。

元NAV SHA `53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2`、core原文 SHA `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b`。

同Actor/fullの拒否2件で、native look非ゼロ・候補非ゼロ、点/期限/待機不変、次look時計進行、出力Unchangedを確認。順序を結んだ後段command 2件も保持active/spotを変えない。native spotなしの通常Track 2件を同試行で確認。少なくとも1件はpin待機中である。

最初のオフライン読取り試行はhelper値を読めたが、候補拒否後にneutralへ進むため移動command専用BPは未発火だった。全式の資格確認としては除外し、ROOT_BEFOREのfull/実frameと共通DISPATCHの実frameを結ぶv2へ変更した。native/ordinary両対照で構造化数値行を確認してから実機へ適用した。候補2件のnative object・fullと、各実frameのcached owner保持を記録し、neutralと移動経路を区別せず観測する。デバッガは製品の点・時計・状態を書き換えない。

| 試行 | 実PID | 結果 |
|---|---|---|
| hunt-1 | CDB 15260 / HLDS 35480 | 終了0・core原文復元 |
| c4-0-retry-2 | CDB 24216 / HLDS 27652 | 終了0・core原文復元 |
| c4-7 | CDB 33428 / HLDS 31976 | 終了0・core原文復元 |
| c4-8-balanced | CDB 37728 / HLDS 28032 | 終了0・core原文復元 |

C4は元NAVで自然設置0・実zone退出7・実C4喪失8を確認する。最初の `c4-0` は設置とfixture完了後のSteam終了処理で停止し、通常g継続と読取り中断でも終了しなかった。検証用HLDSだけを終了要求し、HLDS/CDBとも終了非0・実PID0・core原文復元を採録して除外した。旧ログは `c4-0/excluded.json` とともに保持する。C4専用の読取り設定をmodule-load後のDLL/PDB確認に絞り、Steamのprogram breakpointだけを非停止にして `c4-0-retry-2` を再試行し終了0で受け入れた。Huntの詳細BP採録設定は変更していない。

試行ごとのlaunch/exit/product/fixture/config/console suffixを結び、旧consoleを受入れに混ぜない。過去のDLL/PDB・ユーザー編集・旧証拠を保持し、pushなし。

## 残件と証拠

moving Huntの抽選前抑止は後続[C02-2c3](GRENADE_MOVING_ADMISSION_C02_2C3.md)で限定対応。Native開始時の旧Hunt LOW保持の退役、Guard/Noiseのhandled-returnと後続順、FetchBomb実機、直接同時刻full再入対照、安全時間終了後の経路投擲は未完。自然NAVの全経路一致・全priority一致・Linux実機・継続性能は未受入れ。

証拠：`artifacts/local/debug/csbot-grenade-hunt-admission-c02-2c2-20261124/`。200製品ソース・12保護ファイル・検証入力の最終指紋は `final/freeze.json`、候補とcommandの対応は `hunt-1/acceptance.json`、各raw logとC4 receiptを保持する。49歴史項目／47残件を維持。コミット・local master FF後の最終指紋は `final/post-merge.json` に記録する。
