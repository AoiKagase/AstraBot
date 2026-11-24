# C02-2d11 FetchBomb移動候補ゼロの限定実機

2026-11-24。基点 `f02a5d0d0d9e1429ba98cbeca6a8589013885f12`。製品ソース変更なし。C02の部分受入れであり、大項目未完47件を維持する。

## 候補ゼロと正例

既存一台 `D:/SteamCMD/cstrike_rehlds` へ同一最終DLL/PDBを検証再配備した。C02-2d10の公開fixture（敵露出、HE→rifle、C4 drop/拾得、origin/velocity）はバイナリも変更していない。専用6area NAVの4つの内部encounterを保持し、各spot listだけを1件（id42/t0）から0件へ変えた。area・座標・接続・hiding spot・静止approach数は同じである。検証器は両NAVを構造解析してこの差だけであることを確認する。通常NAVの自然発生とは区別する。

固定参照 `ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5` の `cs_bot_vision.cpp:498–585` は、移動中の受付後、候補列挙前に間隔を抽選して期限を更新する。候補0ならspot抽選・SetLookAt・ClearLookAtを実行しない。固定blob `5e4e30264bdfa8327cf2f8c672305c1bd2bffa23` を保存・照合した。現製品の対応は `bot_behavior_guard.cpp:282–321`。

| 専用NAV / actor1 | full / 時刻 | 遭遇interval / 更新期限 | 採録順序 |
|---|---|---|---|
| 候補0、初回 | 295 / 37.623939514 | 16.562669754 / 42.864471436 | 受付→interval→eligible0。spot抽選なし |
| 候補0、再到達 | 345 / 43.683074951 | 14.908781052 / 48.400306702 | 同上 |
| 元の候補1、別試行正例 | 300 / 37.677165985 | 20.861858368 / 44.277988434 | 受付→interval→eligible1→spot index0 |

各更新を同じslot/時刻/state6/fullで結合した。entryはmoving1/Native0、gateはencounter/middleあり・LOW/Noise/Nativeなし。両試行の初回対象は直前gateから実originの変位もある。表の全採録で同fullの静止候補ゼロFA_CLEARED採録はなく、更新した遭遇期限は次gateへ保持された。候補0の2回目まで連続した実移動が成立したという主張や、別試行間の同RNG列・同絶対時刻の主張はしない。

最終対照はHLDS40280/CDB4664（候補0）、HLDS31204/CDB41736（候補1）。両試行とも各実exit0、fixture phase5完走・C4回収・Server shutdownを確認。原文NAV/core復元・専用fixture/list除去・保護9件一致・実PIDゼロを確認した。読取りBPは、同じ製品の既存v9資格済み式からFA_BLINDだけを除いた8読点。新しい式や製品変更はない。両試行のBP採録は261/234行、読取りエラー0。

## 終了問題の切り分けと環境差

過去の成功5/失敗3試行を比較した。失敗3試行は全bot切断後、Server shutdownの出力前で止まり、既存のlive6非侵入stackはSteam/tier0 mutex待機を示す。成功試行にも約1万件の採録があり、採録件数だけでは成功と失敗を分離できない。旧Steam DLL内部の記号不足もあり、製品原因は断定しない。

今回の初回デバッガなし試行はAstraBot DLLロード失敗で除外した。実exit0だがbot条件は成立せず、製品の正常終了受入れではない。現在の標準探索先には `MSVCP140D.dll` / `VCRUNTIME140D.dll` が無い。既存Visual Studioの `VC/Redist/MSVC/14.50.35710/debug_nonredist/x86/Microsoft.VC145.DebugCRT` を子HLDS/CDBのPATHだけへ指定した。システムCRTのインストール・サーバーへのCRTコピー・通常設定変更は行っていない。DLL/PDBは同一でも、旧試行のCRT実体SHAが無いため、旧環境と同一とは主張しない。

| 今回の終了対照 | 実HLDS / CDB | 結果 |
|---|---|---|
| live1、CRT PATH指定なし | 40896 / なし | DLLロード失敗・入力不成立、exit0。受入れ除外 |
| live2、CRT PATH指定・デバッガなし | 27760 / なし | AstraBotロード・phase5完走・C4回収、実exit0（45.7秒） |
| live3、同CRT・製品BPゼロCDB | 29740 / 31308 | 実HLDS/CDB各exit0。実HLDSは独立handleでも確認 |
| live4 / live5、同CRT・8読点 | 40280 / 4664、31204 / 41736 | 上記候補0/1対照、各両exit0 |

今回の制御条件では正常終了を確認できたが、過去のSteam待機原因やFA_BLIND除去の因果関係は未確定。過去の終了不合格を合格へ変更せず、V02と通常起動時のDebug CRT依存を残す。強制終了0件、サーバーコピー追加0。

## 証拠・指紋・残件

検証器：[verify.py](../../artifacts/local/debug/csbot-fetch-exit-20261124/verify.py)。[最終receipt](../../artifacts/local/debug/csbot-fetch-exit-20261124/acceptance-final-v2.json)、[旧試行比較](../../artifacts/local/debug/csbot-fetch-exit-20261124/existing-comparison.json)、[入力とBP資格継承](../../artifacts/local/debug/csbot-fetch-exit-20261124/zero-input.json)。生ログ・launch/exit・復元前原文は同rootの各liveディレクトリに保存した。DLLロード不成立も保持している。空の補助modules.jsonはモジュール確認の証拠へ使用しない。

DLL SHA256 `2215e8a1fa7ad305ac2efaf6c73ec7241cbb3e551dc288594d80d2cd2097d86f`、PDB SHA256 `12363c7975cdf4da6c36ae3b43e574983d8c10294c967f8096f71f2e1ca1c956`。ソース200・製品2・保護10・検証入力3の現物一致を確認した。製品build・CTest・両OS/export・固定履歴・広範回帰は今回は実行していない。旧重点合格を今回の実行とは数えない。

保持Noise、通常NAVの自然候補ゼロ、全safe/blind/skill/期限境界、他map/Linux実機・継続性能、失敗tail/RemovePath/梯子suffix/容量、HE/drop/着弾は未受入れ。候補ゼロ2更新と正例1更新を、C02全体やV02全体の完了へ一般化しない。
