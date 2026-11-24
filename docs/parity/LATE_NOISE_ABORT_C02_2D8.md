# C02-2d8 後段Noise処理の再入停止

2026-11-24。C02の部分修正。Windows重点18/18と最終DLLの通常Noise継続を限定確認。大項目49件／未完47件は維持する。

## 原因と修正

固定参照はReGameDLL_CS `b0889847fe6d03898be88acc9e366660efb40ab5`。`cs_bot_vision.cpp:403-446`のNoise処理済みreturnを基準とする。C02-2d1で追加した`processNoiseLook`はcallback後のactor・frame・時刻・実行revision・normal/Attack epoch・Clear・full失効を検出するが、後段の`applyNoiseLookCommand`が`Aborted`を破棄していた。Hunt/Guardの事前準備を通らないコマンドでは、失効前のローカルdraftから後続helper・enqueue・RunPlayerMoveへ進めた。

helperの継続可否を内部service/facadeへ返し、`Aborted`だけをfalseとしてコマンド実行側で`NoCommand`を返す。`NotHandled`と`Handled`は継続する。callbackが更新した共有templateはこのreturnで書き戻さない。公開exportは変更していない。

## 正規RED→GREENと重点検証

公開EmitSound→実際の知覚収集→MoveTo状態→実コマンドモジュールを使用する。観測/full段階を個別に駆動し、Hunt/Guard事前準備が先に音を消費しない条件を作る。直接のRoot再帰full更新テストではない。

- 無変更対照はRunPlayerMoveを1回送信。Noise Startle RNG callback中のAttack開始終了・通常状態変更・round変更では、旧RunPlayerMoveを送らず`NoCommand`で終了する。
- 旧製品の正規REDは`red-test-3.log`。Attack開始終了後も`Dispatched`、送信回数44→45。修正後は4条件を含む既存Noise runtimeテスト1/1合格。
- 初回sandboxのMSVC C1902、テスト側の型参照・shadow警告、独立モジュールのsequence不成立試行は別ログで保持し、製品REDから除外した。
- 読取りレビューでテスト用serviceの旧void overrideを発見し、bool/trueへ合わせた。該当scenarioを追加build・検証し、再レビューの追加所見0。
- 最終DLLと重点targetをx86開発環境でbuild後、Windows重点18/18合格（5.90秒）。Noise/core/state、共有look、Native grenade、Guard、runtime command scenarioを含む。全CTest合格とは表記しない。

## 同一Windows実機の通常継続確認

既存`D:/SteamCMD/cstrike_rehlds`へ最終DLL/PDBと既存Guard Noise専用fixtureを検証用再配備。コピー追加0。元NAVを使用し、module-load後に設定する読取りCDB式を同一ソースの正規Noiseテストで先に検証した。

最終DLLのactor1で`Handled`3更新を採録。公開RunPlayerMove音源pulse3回、通常Plant→Guard到達とfixture完走を確認した。これは通常Noise継続のsmokeであり、実機のcallback再入停止受入れではない。HLDS13084/CDB40860はexit0、実残プロセス0。core.ini原文SHA `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b`へ復元、一時fixture/listを除去し既存設定指紋を確認した。

| 製品 | SHA256 |
|---|---|
| DLL | `d9a73f2a0024dd15c3c2bb87874c14985f98fa6f21a4e48cb0feb2f7137a2ced` |
| PDB | `c86ae09cac726bf1906fd9ac8d50b407ca77c5874432192f31f7c41086871b82` |

証拠：[source/input/protected freeze](../../artifacts/local/debug/csbot-late-noise-20261124/final/freeze.json)、[重点18件](../../artifacts/local/debug/csbot-late-noise-20261124/offline/focused-green.log)、[実機限定判定](../../artifacts/local/debug/csbot-late-noise-20261124/acceptance-current.json)、[実PID/復元receipt](../../artifacts/local/debug/csbot-late-noise-20261124/live-1/exit.json)。初期acceptance.json/acceptance-fresh.jsonは旧qconsole混入を除き切れていない解析試行として保持し、今回追加分だけを検査したacceptance-current.jsonを採用する。raw debugger結果は同一試行である。

## 残る範囲

FetchBomb実機、直接同時刻Root再帰full、経路corner投擲はTASKS.mdの残件。実機callback失効そのもの、Linux、全CTest、PE/ELF exports、固定履歴比較、広範な実機回帰は今回未実施。通常NoiseでRNG変更を加えておらず、意図した差は失効した旧実行の後段処理・コマンド送信の停止だが、固定履歴差の全数は未計測。全体gateは大分類完了時に実施する。
