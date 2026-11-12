# AstraBot 実NAV・移動統合監査

作成日: 2026-09-21（JST）  
入力: `AstraBot(1).zip`, `de_dust2.nav`, 既存 `qconsole(3).log`  
対象: 坂・階段・Jump・Area間の移動と復帰。**製品コードの修正・DLL作成ではない。**

## 1. 先に確認が必要な入力の同一性

今回のZIPを前回の `AstraBot.zip` と展開ファイル単位で比較した。

| 項目 | 結果 |
|---|---:|
| 旧ZIPのファイル数 | 433 |
| 今回ZIPのファイル数 | 549 |
| 共通ファイル | 430 |
| 共通ファイルの内容差分 | **0** |
| 追加 | 119 |
| 削除 | 3 |

追加は `.planning` 97、`docs` 16、`.superpowers` 5、`BotProfile.db` 1。削除は2個のOBJと `MANIFEST.sha256`。
`src` 54ファイル、`include` 45ファイル、`tests` 84ファイルは前回と同一。今回のZIPに前回の地形監査後の製品コード変更が入っているとは確認できない。

さらに、`qconsole(3).log` の最新起動区間（2026-09-21 23:11:13、原ファイル11195行から）には `raw_team=` / `effective_team=` の出力が885回、`registered_site_count=` が230回ある。一方、今回の製品CPPにはこれらの出力文字列がない。`plugin_runtime.cpp:3580–3584` の書式も古い `team=%d` のまま。

これは**少なくとも診断出力の実装がログと一致しない**という証拠である。今回のZIPの実装上の不整合を確認できても、最新実機DLLに同じ不整合が残ると自動的に認定してはいけない。ルートのGit情報もないためcommit SHAの独立認証はできない。

SHA-256:

- 旧ZIP: `08dc2f65fb62d440b915517ccd07306a68e5f079c291b1ef8bbe93d09553da7c`
- 今回ZIP: `f03b8f8ec4656a5af49cc1441a6c7e7c21b966facdbaecd78099f3b19c570fa5`
- 実NAV: `53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2`

根拠: `manifest.json`, `identity_evidence.md`。

## 2. 実行範囲

未変更の `LegacyNavReader` → `NavSnapshotPublisher` → `NavQuery` / `NavPathFollower` / `LocomotionController` / `NavRoamController` を、独立したLinux x86_64 / C++14 Debugビルドで実行した。投影テストには製品 `ActionAdapter::projectMovement` もリンクした（CSSDKのbutton定数ヘッダーを使用、engine呼び出しはしない）。

- Windows x86 DLL・Metamod ABI・HLDS/ReHLDS・BSP physicsは未実行。
- `de_dust2.bsp` は未提供。NAV面上の幾何とcontrollerの入出力を検証し、実際の壁・天井・着地を模擬したとはしない。
- Pinned ReGameDLL-CS `b0889847fe6d03898be88acc9e366660efb40ab5` の独立取得・差分実行は今回行っていない。安全性テストのGREENだけでCSBot parity MATCHにはしない。
- FocalSpan / code-review-graphは利用していない。根拠は実ソース、ハッシュ比較、コンパイラ、実行結果。
- プロダクトのCMakeやソース、入力NAVを変更していない。追加は別ディレクトリの監査ハーネスのみ。

## 3. 実NAVロードと探索

production readerでの解析結果:

```text
bytes=413585
sourceVersion=5
areas=737
places=0
NAV header bspSize=2057288
contentHash=3791816597
directed links=2876
attributes: 0=647, CROUCH(1)=11, JUMP(2)=79
```

BSP sizeはNAVヘッダーの値であり、実BSPの内容・衝突形状との一致を証明していない。

| 経路 | 結果 | 完成Area数 | expanded | enqueues |
|---|---|---:|---:|---:|
| 41→90・default | Found | 30 | 343 | 403 |
| 41→90・明示queue budget=256 | ResourceLimit | — | 226 | 256 |
| 41→1438 | Found | 2 | 2 | 6 |
| 5→1520 | Found | 2 | 2 | 10 |
| 46→119 | Found | 2 | 2 | 6 |
| 41→54 | Found | 28 | 122 | 160 |
| 46→54 | Found | 21 | 54 | 81 |

従来の256探索制限問題は、この入力のdefaultでは再現しない。ただし**raw A*成功は経路を実行できることを証明しない**。

公式 `nav_external_performance_tests.cpp` の実行は、兄弟ファイル `de_dust2.bsp` の読み出し要求で停止した。`official_external_result.txt` に記録した。BSPの偽物やダミーサイズファイルでPASSにはしていない。今回のNAV-only検証は独立ハーネスによるもので、公式external CTest PASSという報告ではない。

## 4. T01 — 実NAVの上り面でも偽のStepTooHigh

対象:

- `src/core/nav/locomotion.cpp:225–230,311–313`
- `src/core/nav/nav_query.cpp:44–65,93–112`
- `src/core/runtime/nav_roam_controller.cpp:79–90`

実NAVの `1778 / 1779 → 619 → 93` で次を確認した。

| Area | NAVが表す高さ |
|---|---|
| 1778 / 1779 | 約48.031の平面 |
| 619 | Y=1725で48.031、Y=1800で96.031へ上がる面 |
| 93 | 約96.031の平面 |

1778→619の境界は約48.031で連続し、619→93の境界は約96.031で連続する。ところが実装は619の平均値72.031を使い、どちらの方向にも約24unitの「段差」を計算する。

```text
1778→619: Found, LocomotionResult=StepTooHigh
1779→619: Found, LocomotionResult=StepTooHigh
619 →93 : Found, LocomotionResult=StepTooHigh
```

`real_nav_slope_entry`, `real_nav_slope_exit` がRED。

**修正方向:** 位置依存の床高さと、両Areaが接続する境界の不連続量を評価する。坂全体の上昇量を通常段差上限と比較しない。単に16を24/32へ増やして閉じない。NAV上の連続面とBSP上の細かい階段形状は別なので、physics受入れは別に必要。

## 5. T02 — 46→119の下降接続を外れ判定が逆向きにする

実NAV:

```text
Area46: Y=[-350,-150], Z=128.031
Area119: Y=[-125,0], corner Z=0.031〜29.782
46→119: directed connectionあり、方向2、抽出how2
Y方向のNAV矩形間の隙間: 25unit
```

**これは、このNAVでは上りJumpではなく下る接続である。** それ以外の上り障害物がないという意味ではない。

未変更controllerに46→54の有効なcorridorを持たせ、同じX=-1575で境界付近の位置観測を順に与えた。境界を出た観測はairborne=trueとしている。

| 与えた足元Y / Z | 判定 | 出力 |
|---|---|---|
| -151 / 128.031 | Area46内 | target119、directionY=+0.989588 |
| -149 / 128.031 | containingなし | recovery46、directionY=-1.0 |
| -140 / 128.031 | containingなし | recovery46、directionY=-1.0 |
| -136 / 128.031 | containingなし | recovery119、XYZ方向は主に下向き |
| -124 / 128.031 | 119のXY内、床から離れている | direction=(0,0,-1) |

`real_nav_gap_policy` がRED。

対象:

- `src/core/runtime/nav_roam_controller.cpp:444–480` — `NoAreaContaining`で先にgeneric recoveryへ入る。
- 同ファイル `53–75` — nearest closest pointへの3D回復入力。
- 同ファイル `92–115,958–979` — 特殊移動選択。

既存routeを保持していても、ここではlocomotion followerを進める前にreturnする。下降中の一時的なNAV外状態と、経路から逸脱した状態を分けられていない。

**修正方向:** 計画中の現在リンク・踏切・空中・着地を扱い、その区間を進んでいる間の一時的なarea lossが無条件に後退入力へ変わらないようにする。ただし全てのNoAreaContainingを無視したり、常に前進させたりしない。実BSP上の安全な下降か、踏切前の障害物かは実測が必要。

この観測座標は診断で与えたもので、実機ログから採取した正確な足元軌跡ではない。最新ログの46→119反復と整合する候補だが、最新DLLの根因確定ではない。

## 6. T03 — 上下階のnearest選択がruntime Goalを別階へ変える

実NAVに、XYが重なる以下のAreaがある。

```text
Area1520: X=[500,525], Y=[2050,2100], Z≈-128〜-125
Area351 : X=[425,525], Y=[2050,2100], Z≈96
```

Area1520面上の点 `(512.5,2075,-126.343262)` を与えると:

```text
findContaining: 1520
findNearest:    351
```

さらにraw queryだけでなく、production `NavRoamController` へArea5からこの3D目的地を渡すと、**実際の `selectedPath.back()` が351になる**。

```text
raw A*: 5→1520 = Found（2 Area）
runtime objective point on 1520: selectedPathEnd=351（17 Area）
```

約222.37unit上にある別階への置換である。`real_nav_goal_floor` がRED。

対象:

- `src/core/nav/nav_query.cpp:401–453` — XY距離のみ、同距離はID順。
- `src/core/runtime/nav_roam_controller.cpp:422–442` — 3D目的地を無条件にこのnearestでArea化。

**修正方向:** 目的地の3D位置と選択済みAreaの対応を維持する。XY queryが必要な他の用途まで一括で意味変更しない。目的地が面上に存在する場合の高さ・containment、面外のfallbackの有効距離と足場条件を契約として分離する。

## 7. T04 — vertical-only recoveryが実行時にゼロ入力になる

実NAVのArea1505は X=[-1200,-1175], Y=[1225,1275], Z≈48.031。

診断入力 `feet=(-1187.5,1250,-32)`（実測位置ではない）では:

```text
findContaining: なし
findNearest:    1505
runtime:        IntentReady / OffMeshRecovery
recovery direction=(0,0,+1), speed=32, action=Walk
ActionAdapter::projectMovement: forward=0, side=0
```

歩行用の復帰処理が、頭上のAreaへ「真上に歩く」入力を正常扱いで返す。一方、実行adapterは歩行方向のXYだけを使用する。実行可能な復帰になっていない。`real_nav_recovery_projection` がRED。

対象:

- `nav_roam_controller.cpp:48–75,444–480`
- `nav_query.cpp:401–453`
- `plugin_runtime.cpp:2183–2196`
- `action_adapter.cpp:47–67`

**修正方向:** 復帰先を実際の高さ・到達条件・現在リンクに基づいて選ぶ。XY成分ゼロの通常歩行を成功した回復入力として繰り返さない。観測不足・復帰不能・移動待機を明示する。origin/velocityを直接書き換えて上へ戻さない。

最新ログのBot1は `current_area=0 / next_area=1505` が反復している。しかしログにはこの足元Zがなく、今回の入力と同じ実状態とは断定しない。

## 8. T05 — 実NAVにもJump対象を先に拒否する経路

Area642にはJUMP属性がある。275→642は実graphの直接接続で、A*もFoundになる。それでも通常段差判定がJump判定の前にあり、`StepTooHigh`になる。

```text
275→642
destination attributes=2 (JUMP)
LocomotionResult=StepTooHigh
Jump実行可否の分岐前に拒否
```

`real_nav_jump_gate` がRED。このテストは「必ず物理Jump成功」を要求していない。**Jump候補を通常歩行上限だけで先に拒否する順序**を検出する。

また、実NAVからproduction queryで得た2876リンクのhowは `0:2873, 1:1, 2:2` で、今回の `traversalActionFor()` が特殊controllerへ送る4/5/6はない。したがってこのファイルのリンクhowだけではJumpDrop/Ladder controller開始条件に入らない。

ただし通常locomotionにも `destination.attributes & kJump` → Jump intentの経路はあり、**このNAVで全Jumpが不可能という結論にはならない**。実際、12→1784の診断ではJump intentが生成された。問題は普通の段差判定・NAV属性・リンク分類・実地形検査が整合していないこと。

対象:

- `locomotion.cpp:225–265`
- `nav_roam_controller.cpp:92–115,942–997,744–777`
- `nav_query.cpp:226–261` — approach how抽出

how=2を一律Dropへ変換するなどの修正は禁止。固定Referenceのenum・NAV属性・地形判定の役割を確認する。経路途中の特殊リンクに切り替わらない旧R6も、同じソースで再現した。

## 9. 全接続スキャンの読み方

2876 directed linksについて、各from Areaの中央面上の位置を与え、2Area corridorの最初のlocomotion結果を採取した。

- `StepTooHigh`になったサンプル: 191
- うちdestinationにJUMP属性: 38
- うち共有XY境界の中点で、4cornerによる補間高さ差の絶対値<0.1: 127

**191個の実機バグが確定したという意味ではない。** 入力はArea中心、通行条件はNAV＋controllerだけで、BSPの勾配・天井・壁を計測していない。高さ平均の問題が単一地点の特例ではないことを示す診断分布として扱う。

根拠: `edge_scan.csv`, `edge_summary.json`。中点補間は診断側の幾何計算で、Referenceの厳密なGetZ互換実装を認定していない。

## 10. テスト結果

独立CMakeの全18テスト:

- PASS 11
- FAIL 7
- Not Run 0（最終ビルド後）

内訳:

| 種類 | 結果 |
|---|---|
| 既存NAV系7テスト | 7/7 PASS |
| 旧診断ハーネス | 完走、8観察を再現 |
| 旧安全性テスト | FAIL（R3〜R6の4条件） |
| 新real NAV parse/dump | PASS |
| 新real NAV全リンク診断 | 完走（不具合なしという意味ではない） |
| 新real NAV A* baseline | PASS |
| 新real NAV安全性提案6テスト | 6件ともFAIL |

新たな6件: slope_entry, slope_exit, jump_gate, gap_policy, recovery_projection, goal_floor。

製品の公式全CTestではない。以前の54/54などの報告と件数比較しない。
独立harnessの初回拡張ビルドではCSSDK定数ヘッダーのinclude path不足があったが、**harness CMakeのみ**を修正して最終ビルド・全実行をやり直した。最終結果のNot Runは0である。

## 11. Codexで直す順番

1. **入力同一性:** 現worktree、ビルド対象branch、出力DLL、配置DLL、今回ZIPを比較。dirtyを破棄しない。古い展開物の修正を重ねない。
2. **T01/T03:** 局所高さ・目的地Area対応を正す。坂面と段差、XY検索と3D目的地を分離。
3. **T02/T04:** active route内のairborne/gapとgeneric off-mesh recoveryを分離。復帰入力がActionAdapterまで実行可能か検証。
4. **F02/T05:** segment通過・Jump gate・現在リンクのTraversal切替を修正。階段の先読みと曲がり角を飛ばす旧REDも維持。
5. **診断:** actual local target、feet、現在リンク、airborne、recovery結果、最終buttonsとprojectionを同一sequenceで出す。Goal metadataと古いtargetを混ぜない。
6. **Live:** 46→119、1778/1779→619→93、上下重なり、Jump対象を単一Botで検証。その後4Bot/C4で性能回帰を確認。

同じ目標を持つ複数Botの強制分散、ジャンプ力・stepsizeの水増し、無条件Jump、NAV接続の削除、大量A*再評価では閉じない。
P02 timing、RNG境界、256探索制限分離、path persistence、Objective評価キャッシュを維持する。

**P08 NOT READY:** 今回は実ソース＋実NAVの反例を追加した段階。最新DLLに修正が入っているかは、まず同一性を確認する。MAP上の実通過・Jump着地まで未確認である。
