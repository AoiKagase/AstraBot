# C02-2d9 FetchBomb実機と直接同時刻Root再帰

2026-11-24。C02の部分検証単位。製品ソース変更なし。既存C02-2d8 DLL/PDBを使用する。残件正本TASKS.mdの大項目47件は維持する。

## 直接Root再帰の正規対照

`astrabot_combat_target_log`の既存Native保持・ゼロ候補fixtureへ2つのscenarioを追加した。実Rootのfull更新から`LookAround.StateInterval`のRNG trace callback中に、同時刻のRoot hookへ直接再帰する。独立Behavior/helperだけの再入対照とは別である。

- `astrabot_native_grenade_root_reentry`は`onStartFramePost()`へ再帰。同frame・同時刻・同通常/Attack世代を保つ。nested fullは24→24、nested送信は24→24、外側だけ24→25。次fullまでinterval抽選は1回のままである。
- `astrabot_native_grenade_root_frame_reentry`は`onStartFrame()`→`onStartFramePost()`へ再帰。時刻4.209000111秒・full24・通常/Attack世代は維持し、adapter frameだけが進む。nested送信24→24、旧外側送信24→24、次fullでintervalを再抽選して計2回。旧clockを公開しない。

Root schedulerはcallbackより前にcommand/full期限を進めるため、同時刻の再帰呼出しから同一botの2つ目のfullは受付されない。今回の合格は「同時刻full処理中の直接Root再帰とその受付抑止」であり、private schedulerを改変して2つ目のfullを強制した実行ではない。

指定MSVC Hostx86/x86環境でtarget build成功、新規CTestは2/2合格。読取り専用レビューは重要所見なし。製品不具合のREDや製品修正はない。最終関連重点20/20合格（8.10秒）。初回は19/20で、hunt_approach_controlの`fopen_s`がdefault BotProfile fixtureを開けず失敗した。errnoは未採録で原因は未確定。製品処理前の入力準備失敗として保持する。同一ソース・製品・保護指紋と現時点の非破壊open成功を確認し、無編集で20件を再実行した。初回失敗を合格扱いに置き換えない。証拠は[offline](../../artifacts/local/debug/csbot-fetch-root-20261124/offline/)。

## 同一Windows実機のFetchBomb機能採録

固定参照`ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5`の`FetchBombState::OnUpdate`は`UpdateLookAround`→`UpdatePathMovement`。同一既存サーバー`D:/SteamCMD/cstrike_rehlds`・元de_dust2 NAVを使用し、公開敵露出・装備・C4落下/回収・位置/view補正の専用fixtureでT側actor1をFetchBombへ移行させた。内部Astra状態への書込みなし。サーバーコピー追加0。

最終DLL/PDBを再配備し、module-load後の読取りBPを採録した。4式は正規Guard/FetchBomb EXEと候補ありNative EXEで資格確認済み。初回資格確認でLOW受付点を通らなかった試行も保持し、式の資格確認成功とは扱わない。

Native spot保持のFetchBomb静止45更新で、期限到達4更新（full229/243/257/269）にinterval→候補抽選→LOW受付点を採録した。各受付点でNative `Towards`が残り、既存製品のNative優先分岐がLOWを拒否する。後続のFetchBomb移動8更新でもNativeを保持した。移動時encounterの全受付条件は採録していないため、移動抽選抑止の因果対照や全経路一致とは主張しない。公開C4回収と`FB_COMPLETE phase=5`まで到達した。

## 終了停止・未完のゲート

fixture完走とquit後もHLDS11228/CDB30732が残存。`CSteamEngine::BMainLoop appears to have stalled > 15 seconds`警告が出た。非侵入stackでmainはSteam/tier0 mutex待機を採録した。記号不足のstackから製品原因は断定しない。正常終了ゲートは不合格であり、機能採録と終了失敗を[acceptance-final.json](../../artifacts/local/debug/csbot-fetch-root-20261124/acceptance-final.json)へ分けて保存した。先行acceptance-functional.jsonは終了許可待ち時点の記録として保持する。

ユーザーの該当2 PID終了許可（「OK」）後に個別終了し、両exitは`0xFFFFFFFF`。実PIDゼロを起動時と同じ権限で確認した後、core.ini原文SHA `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b`へ復元、一時fixture/list除去、既存設定/NAV/サーバー製品の保護指紋一致を確認した。終了0を要求するlive.pyの最終assert失敗は意図した失敗記録であり、復元処理はその前に完了している。独立再照合は[verification-receipt.json](../../artifacts/local/debug/csbot-fetch-root-20261124/final/verification-receipt.json)。V02の正常終了問題は未完のまま。

| 製品 | SHA256 |
|---|---|
| DLL | `d9a73f2a0024dd15c3c2bb87874c14985f98fa6f21a4e48cb0feb2f7137a2ced` |
| PDB | `c86ae09cac726bf1906fd9ac8d50b407ca77c5874432192f31f7c41086871b82` |

corner投擲、Linux、全CTest、PE/ELF export、固定履歴、広範回帰は今回未実施。baseline再生成・RNG仕様変更なし。大分類完了時の全体gateを維持する。
