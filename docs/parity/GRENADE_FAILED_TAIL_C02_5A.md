# C02-5a 失敗経路のtail履歴監査

2026-11-24。基点 `5a7a64df760e4e5ad2baa7ed910b7acd882b65c1`。固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。**固定失敗分岐の監査のみ。製品修正・製品RED・実機受入れではない。**

## 確定した挙動

`ComputePathPositions` はslot0と手前のground portalを書いた後、UP/DOWN梯子が見つからないとfalseになる。呼出元 `ComputePath` はDestroyPathしてfalseを返すが、書込み済み位置も未書込み位置も消さない。次のtrivial経路はslot0/1だけを書き、slot2の履歴が全可視投擲へ現れる。

既存の固定実関数fixtureへparentHowを明示入力として追加した。手前にground2区間がある失敗ではslot2=`(905,0,0)`、同じtrivial経路へ再構築すると標本化先=`(900,0,46)`。新botのUP/DOWN失敗と既存経路を持つUP失敗の3対照で一致する。最初のedgeで失敗する対照では旧slot2=`(405,0,0)`を上書きせず、投擲先=`(400,0,46)`を保持する。欠落梯子slot3とendpointは書かれない。

証拠rootは `artifacts/local/debug/csbot-failed-tail-20261124/`。`reference-1/manifest.json` と `reference.jsonl` は、従来11対照に失敗直後/再構築8対照を追加した19/19、指定MSVC Hostx86/x86、C++14 `/Od /fp:precise`、参照/driver/tool/executableのSHA256を記録する。固定関数本体とVectorは変更せず実行した。

`success-only-mutant-2/` は失敗時に位置を巻き戻す**意図的な誤モデル**であり、slot0書込みの検査がexit1になる。監査fixtureが成功経路限定モデルを反証できることのmutation対照で、Astra製品REDとは数えない。元の固定本文による19/19をGREEN証拠とするが、製品のRED→GREEN修正とは呼ばない。

## 製品修正に進まなかった理由

この失敗入力は、fixtureの探索stubが「sourceの梯子リストに存在しない梯子edgeをparentに返す」という不整合を明示している。固定 `NavAreaBuildPath`（`regamedll/game_shared/bot/nav_area.h:710–855`、保存本文 `pinned-nav-area.h`）は、UPではsourceのLADDER_UPリストからforward/left/rightを、DOWNではLADDER_DOWNリストからbottomを選び、そのarea/howをparentに書く。位置計算も同じsourceリストを使う。したがって、安定した同一リストでの通常探索からこの不整合が生じるという証拠にはならない。リスト変化・破損等が不可能とも主張しない。

Astraは `validateNativeTopology` 後、`buildAStarCorridor` の同じtopologyからparentLinkとparentLadderを保存し、corridorのnativeLaddersへコピーする。位置公開はこのコピーを読む。欠落梯子失敗を通常入力で再現する経路は今回確認できなかった。無効topologyの事前拒否、探索NoRoute、locomotion開始拒否、はしご完了suffixは、固定ComputePathPositionsの欠落梯子失敗と同じ処理ではない。

このため、失敗した任意の要求を成功したground経路としてcacheする修正や、テスト専用の製品入口は追加しない。**失敗履歴の製品対応は未実装・未受入れとしてTASKSに残す。** 次に扱う条件は「固定探索から欠落梯子失敗に至る実入力、または同じ意味のAstra公開入力と製品RED」を確定すること。これを理由に、独立して進められる全可視/reset/rebuildの実機対照を止めない。

## 検証と保全

製品ソース/DLL/PDBは変更なし。指定MSVC環境でcombat_target_log/nav_query/nav_roam_controllerをbuildし、関連CTestを逐次実施、22/22合格（2.34秒）。最終件数と指紋は同rootの `acceptance.json`、`focused-build.log`、`focused-test.log`、`preservation-final.json` を正本とする。重点検証後の追加変更は台帳・説明文書のみで、製品ソースと検証入力は同一。全CTest・両OS・exports・固定履歴・広範実機は大分類完了時の未実施gate。

今回HLDS/CDB起動・再配備・通常設定変更なし。旧C02-5のDLL/PDBと保護ファイルは前後指紋で照合する。実機受入れ追加なし。全可視/reset/rebuild、NAV削除RemovePath、はしごsuffix/容量超過、HE/drop/着弾、FetchBomb未採録、V02正常終了等は継続し、大項目未完47件を維持する。
