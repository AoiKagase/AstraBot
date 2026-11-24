# C02-4 全可視・自明経路の未使用tail監査

2026-11-24。基点 `5c205ec97ac1a6f6fe806e3e1122fcd0cee0ea35`。固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。**監査完了、製品parityは未完。製品ソース変更なし。**

## 結論

`FindGrenadeTossPathTarget` は全点が可視なら `i == m_pathLength` のまま `m_path[i].pos` を使う。長さ256未満では固定配列の未使用位置であり、常に未初期化とも常にゼロとも扱えない。固定 `Vector()` は位置をゼロ初期化し、`CCSBot` constructor本体は空。`ResetValues` は経路length/indexを0にするがslotを消さない。`DestroyPath` もlengthとladderだけを消す。`BuildTrivialPath` はslot0/1だけを書き、長経路から短縮すると以前のslot2が残る。`RemovePath` は短縮後のtailを `Q_memset` する。

したがって現在の有効経路とindexだけでは全可視時の参照投擲先を再現できない。同じtrivial経路の先を終点へ固定する方針は、以下の履歴対照で反証された。現行Astra `bot_behavior_combat.cpp` は全可視時に安全拒否する。これを完全parityと数えず、tailの履歴保持とreset/remove/rebuildの対応を次の実装単位としてTASKSへ残す。

最大長256点ではslot256が配列外になる。監査hostはchecked accessorで読取り前にtrapし、参照GameDLLの実メモリや結果を推定しない。この境界に有効な参照投擲先は確定していない。

## 再現証拠

`tools/audit_csbot_grenade_tail.py` が固定commitからHasPath/DestroyPath/ComputePathPositions/BuildTrivialPath/ComputePath/RemovePath/FindGrenadeTossPathTargetの実関数とVectorを抽出する。`tests/fixtures/csbot_grenade_tail/reference_driver.cpp` はそのままの関数を実行する。初期化の静的証拠はconstructor/ResetValues/mathlibとして別保存する。

正本は `artifacts/local/debug/csbot-tail-20261124/reference-final/manifest.json`、同 `reference.jsonl`、`build.log`、`pinned-functions.inc`、`pinned-vector.h`、`constructor.txt`、`reset-values.txt`、`pinned-mathlib.h`。manifestは参照7ファイル・抽出・driver・tool・実行ファイルのSHA256、compiler pathとx86、実コマンドを記録する。Hostx86/x86 MSVC14.51.36231、C++14、`/Od /fp:precise`、非PLAY_GAMEDLLの `real_t=float`。最終11/11対照はexit0。

| 対照 | 結果 |
|---|---|
| 経路なし・最初が不可視・最大長で最初が不可視 | 拒否、target未変更、wall traceなし |
| 新規trivial・新規複数点・RemovePath短縮 | zero tailに向かい `(25,0,46)`、wall trace4回 |
| 長経路→同じtrivial・DestroyPath→再構築 | 保持slot2=`(405,0,0)` に向かい `(400,0,46)` |
| 終点=tail・tail区間が不可視 | それぞれ `(405,0,10)`・`(275,0,46)`。長さ0なら36の加算なし |
| 最大長全可視 | length256、slot256読取り要求をtrap、target未変更、wall traceなし |

NAV探索は明示parent chain、平坦・相互接続、可視性はx閾値、wall traceは全通過、timerは経過済み、RNGは下限を返す。native pathの生成・短縮コードは実行するが、NAV探索・FVisible/trace・scheduler・GameDLL全体を実行した証拠ではない。hostのポインタ初期化は参照の初期値を主張しない。drop/梯子・斜めfloat・実水没・実機投擲/着弾・Linux・別最適化は対象外。

## 重点検証と保全

製品ソースとDLL/PDBは変更していない。指定MSVC環境で `astrabot_combat_target_log` をbuildし、`^astrabot_native_grenade_corner` の既存重点7件を逐次実行、7/7合格（0.42秒）。結果は同rootの `focused-build.log` と `focused-test.log`、前後のソース200・製品2・保護10指紋は `preservation-before.json` と `preservation-verified.json`、master統合後は `preservation-merged.json` を参照する。参照11対照と製品テストの結果を合算して全体合格とはしない。

前chatの `reference-1` は空の未完出力として保持。`reference-2` はPython子gitの固定参照読取りがsandbox Permission deniedとなり失敗、受入れ証拠に使わない。`reference-3` は旧tool版11対照の成功として保持し、最終tool指紋の証拠には `reference-final` だけを使用する。

今回HLDS/CDB起動・再配備・通常設定変更なし。実機受入れを追加したとは主張しない。全build/両OS全CTest/exports/固定履歴/広範回帰は大分類完了時。C02全体、HE/drop/梯子・着弾、FetchBomb未採録範囲、V02過去の正常終了不合格を閉じない。大項目未完47件を維持する。
