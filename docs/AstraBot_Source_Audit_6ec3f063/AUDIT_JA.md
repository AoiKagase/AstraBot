# AstraBot ナビゲーション統合・実ソース監査

対象申告 SHA: `6ec3f0635a6256f37e1e4ba4a85462bfea37fb1c`  
監査日: 2026-09-20  
対象: 添付 `AstraBot.zip` 内のソース。行番号はこの ZIP の展開内容に対する番号。  
状態: **修正箇所の特定・未変更ソースでの再現まで。製品コードの修正は行っていない。**

## 1. 対象の同一性と検証範囲

ZIP SHA-256:

`08dc2f65fb62d440b915517ccd07306a68e5f079c291b1ef8bbe93d09553da7c`

ZIP 内のファイル 433 件について展開後のバイト列を照合し、変更・欠落は 0 件だった。ルートの Git メタデータが含まれないため、上記コミットと ZIP の完全同一性は独立には検証していない。`third_party/cssdk/.git` はプロジェクト本体のコミット証明ではない。

主に調べたのは production の Goal 生成、NAV query/follower、locomotion、Jump/特殊移動、Metamod への command 変換、関連テストと診断である。全ソースの全行監査、セキュリティ監査、Combat の完成度監査ではない。

実行環境は Linux x86_64 / C++14。製品の x86 制約を変更せず、別の診断用 CMake プロジェクトから SDK-free の未変更ソースをリンクした。**Windows x86 DLL・Metamod ABI・HLDS/ReHLDS 実機検証ではない。** 実 `de_dust2.nav` と BSP は ZIP にないため、問題の現場の壁・段差をこの環境で再現したわけではない。

Pinned ReGameDLL-CS `b0889847fe6d03898be88acc9e366660efb40ab5` は今回独立に取得できていない。以下の再現は AstraBot 自体の入力・出力と内部契約に基づく。参照 CSBot の厳密な Goal 選択、RNG 呼び出し順、portal 到達、障害物検知、Jump 条件への適合は、その固定ソースを読める環境で別途照合する。

## 2. 結論

現在の問題は「A* が経路を返さない」だけではない。少なくとも次の実装が、同じルートへの収束・局所ループ・壁に向く移動・Jump への切替不成立を生じさせる。

| ID | 優先度 | 修正対象 | 根拠 |
|---|---|---|---|
| F01 | 高 | Compatibility Roam が最初の到達可能な隣接 Area に固定 | 静的確認 + R1/R2 |
| F02 | 最優先 | 未進入の中間 Area を到着扱いし、曲がり角を飛ばす | 静的確認 + R3 |
| F03 | 高 | Area 高さを定数化し、斜面を段差と誤判定。nearest は XY のみ | 静的確認 + R4/R7 |
| F04 | 最優先 | 通常段差判定で Jump を先に拒否。経路途中の特殊リンクへ切り替わらない | 静的確認 + R5/R6 |
| F05 | 高 | 実地形の clearance 観測が定数、微小な横ずれで停滞検知を解除 | 静的確認 + R8 |
| F06 | 高／要参照照合 | BombTarget は列挙されるが、候補 1 Area・最小コスト固定・選択保持 | 静的確認。片側だけになる正確な実機原因は未確定 |
| F07 | 診断修正 | ログの movement_target / link / stuck_time が実行状態を正しく表さない | 静的確認 |
| F08 | 高／別の統合不整合 | Objective と State が異なる Team 情報を消費 | 静的確認、既存ログと整合 |

**同じ Goal や同じルートを複数 Bot が選ぶこと自体は不具合ではない。** 修正目標は強制分散ではなく、Reference の意思決定と実行可能な経路追従を成立させることである。

## F01 — 最初の隣接 Area を Goal にする Compatibility Roam

### 箇所

- `src/core/runtime/nav_roam_controller.cpp:1208–1263` — `NavRoamController::selectRoamRoute()`
- 同ファイル `827–830`, `1308–1311` — failed link の保存と、その参照範囲
- `tests/nav_roam_controller_tests.cpp:307–355` — `testCompatibilityIgnoresActorSeededRouteRotation()`

### 確認した動作

Compatibility 側では `outgoingLinks(currentArea)` を先頭から走査し、最初に corridor の構築と startTraversal が成功したリンクで即 `return true` する。Goal はそのリンク先の隣接 Area である。

この分岐は Enhanced 用の `nextLinkIndex_` による候補巡回を使わず、`hasAvoidedLink_` も確認しない。Stuck 処理で `avoidedLink_` を設定しても、Compatibility の次回選択がその同じリンクを避ける保証はない。

### 再現

R1: 同一 Area から開始する別々の controller 4 個が、全て Area 2 を選択。

R2: A から B と C へ行ける graph で、B の最初の接続が A の場合:

`2 → 1 → 2 → 1 → 2 → 1`

という Goal 選択を反復する。C は存在するが、この条件では選択されない。位置更新を与えた controller テストであり、物理シミュレーションではない。

### 修正方針

「指定 Goal への A* neighbor 順」と「どこへ行くかの Goal 選択」を分離する。前者の安定性を保つことは、後者を最初の隣接 Area に固定する理由にならない。

Pinned CSBot の State/Hunt 等が生成する目的地・保持条件・再選択条件へ接続する。RNG が必要な参照箇所は既存 P03 boundary と消費順を使う。actor ID の剰余、全 Bot 強制分散、適当なランダム化を互換実装として投入しない。

既存テストは「actor seed に依存しない」を「全員 Area 2」として検証している。このテストだけではゲームとしての Goal 選択を証明できない。Enhanced RNG の隔離テストと Reference Goal 選択テストを分ける。

## F02 — Area の近傍にいるだけで corridor を先へ進める

### 箇所

- `src/core/nav/nav_query.cpp:104–112` — `isWithinTarget()`
- 同ファイル `938–986` — `NavPathFollower::update()`
- 同ファイル `115–224` — 既存の `portalSteeringPoint()` 群
- `src/core/runtime/nav_roam_controller.cpp:21–24` — production tolerance 設定
- `tests/locomotion_tests.cpp:309–334` — `testReferenceArrivalTolerance()`

### 確認した動作

実装には portal steering が既にある。問題は portal 不在ではなく、進行判定である。

`isWithinTarget()` は「現在位置から対象 Area の矩形までの距離が horizontalTolerance 以下か」を判定する。算出した portal 通過点への到達や、次の steering segment が corridor 内に収まるかはここでは検査しない。条件成立時は `currentIndex_` を進める。

### 再現 R3

平面 graph を以下で作成した。

- A: X=[0,100], Y=[0,100]
- B: X=[100,140], Y=[0,200]
- C: X=[140,240], Y=[100,200]
- 接続 A→B→C。開始位置 (90,20,0)。horizontalTolerance=20。

1 回目: B 向け目標 `(116,20)`、index=1。

**位置を動かさず 2 回目を実行**すると、まだ B に入っていないのに C 向け `(156,116)`、index=2 になる。この位置から返された目標への直線を検査すると、`(140.16,92.96)` で A/B/C の歩行領域の和集合から外れる。

この fixture では、グラフ接続が正しくても局所目標が曲がり角を切り、指定された歩行領域外へ向くことを示した。実 BSP の衝突を実行した結果ではない。

### 修正方針

Area 存在判定、corridor 進行判定、最終 Goal 到着判定を分ける。参照に基づく現在 segment／portal の通過条件と、必要な安全な lookahead 条件で更新する。

「20 を 0 にする」「全ての目標を Area 中心に戻す」だけでは解決しない。停止中に通過済み Area が増えないこと、曲がり角の目標線が壁側へ抜けないことをテストする。

既存の `testReferenceArrivalTolerance()` は対象 Area の 20-unit 近傍なら到着を認めるテストで、狭い接続や屈曲経路を含まない。名称に Reference とあってもこの一般化の正しさは証明しない。

## F03 — 高さの単純化と nearest の XY-only 選択

### 箇所

- `src/core/nav/nav_query.cpp:44–65,93–112` — `floorDistance`, `centerOf`, `closestPointOnArea`
- 同ファイル `401–453` — `NavQuery::findNearest()`
- `src/core/nav/locomotion.cpp:225–230,311–313` — `stepHeight`, `floorHeight`
- `src/core/runtime/nav_roam_controller.cpp:79–90` — 同様の床高さ・中心計算

### 確認した動作

床高さに `northEastZ` と `southWestZ` の平均を使うため、XY 上の位置に応じた床高さにならない。`floorDistance()` もこの 2 点の min/max だけを使い、Area の全 4 corner から求める面上の高さではない。

`findNearest()` は XY 矩形距離だけを比較する。上下階が同じ XY を持つ場合、Z 差ではなく同距離時の ID 順で選ばれ得る。XY 専用 API という設計自体はあり得るが、Objective の 3D 座標や current-area recovery からはその区別なく使われている。

### 再現

R4: X 方向に Z=0→60 と上る Area A と、高さ 60 の Area B を接続した。境界の高さ差は 0 だが、平均高さ差 30 を通常段差と扱い `StepTooHigh` / `ReplanRequired` になった。

R7: XY が同じ Z=200 の Area 1 と Z=0 の Area 2 に対し、query=(32,32,0)、maximumDistance=16 で Area 1 が選択され、返却点 Z=200、distanceSquared=0 となった。

### 修正方針

全 4 corner に基づく面上の高さを実装し、現在位置・portal・着地候補の XY で評価する。平均床高さの差を「踏み越える段差」の高さにしない。

nearest は呼び出し元の必要条件を明示する。純 XY の query を残す場合は名称・契約を区別し、実際に立てる Area の選択には Z、到達可能性、必要な可視性・足場条件を適用する。全呼び出し元の意味を確認せず一括置換しない。

## F04 — Jump と corridor 内のリンク種別の接続不良

### 箇所

- `src/core/nav/locomotion.cpp:225–233` — 段差上限判定が `requiresJump` より前
- `src/core/runtime/nav_roam_controller.cpp:942–997` — `startTraversal()`
- 同ファイル `1063–1104` — `selectRoute()` の初回リンク選択
- 同ファイル `744–777` — corridor 進行時の更新
- `src/core/nav/jump_drop.cpp:81–85` — envelope と corridor の終端契約

### A. Jump 属性でも通常段差判定で先に拒否

destination に `kJump` があっても、その判定より先に平均床高さ差で `StepTooHigh` を返す。

R5: 高さ差 32、destination に `kJump` を持つ corridor が `StepTooHigh` になり、Jump intent 生成へ進まない。ここで「32 ならどんな条件でも跳べ」とするのではなく、通常歩行段差と Jump 実行可否の検証順を正す必要がある。

### B. 途中の特殊リンクへ切り替わらない

`selectRoute()` は corridor の最初のリンクを選んで `startTraversal()` を呼ぶ。後続 update は follower の index を更新するが、現在リンクから `activeTraversal_` を再選択し、そのリンクの特殊 controller を始める処理がない。

R6: A→B は Walk、B→C は AstraBot が Jump と解釈する `how=6` のリンクを使う fixture。

- A から C へ開始し、B まで位置更新: C 向け intent は Walk。
- B から C へ直接開始: 同じ B→C で Jump。

これは ZIP の内部 how 解釈を使った境界テストであり、外部 NAV の全 how 値が CSBot と一致することを認定したものではない。

### C. 最初が特殊リンクの長い経路でも契約不一致

`startTraversal()` は最初の隣接リンクから launch/landing を作りつつ、**全 corridor** を `jumpDrop_.start()` に渡す。一方 `JumpDropController::start()` は `corridor.back()==landing.area` を要求する。3 Area 以上で最初の隣接リンクが Jump の場合、この条件は一致しない。これは静的確認であり、本ハーネスの独立項目としては数えていない。

### 修正方針

経路全体の所有者と、現在リンクの Approach→Launch→Airborne→Landing→Complete の所有者を分ける。リンクを進めるたびに現在リンクの traversal を選択し、1 リンク分の envelope と全体 corridor の契約を一致させる。

通常段差、Jump、Drop、Ladder を適切に分岐し、Reference の許可条件・タイミングを保つ。`stuck` を見て無条件に `IN_JUMP` を連打する修正は行わない。

## F05 — 地形観測の定数化と停滞検出

### 箇所

- `src/adapter/metamod/plugin_runtime.cpp:2011–2042` — `NavRoamObservation` の production 組み立て
- `src/core/nav/locomotion.cpp:12,189–205` — `kMinimumProgressDistance`, `recordProgress()`
- `src/core/runtime/nav_roam_controller.cpp:804–837` — recovery と route reset

### 確認した動作

`standingClearance=72.0f`、`crouchingClearance=36.0f` が固定値で渡される。production movement に前方障害物の hull trace 結果や段差の実測値が入っていない。この値は「観測した clearance」としての意味を持たない。

停滞判定は前回座標との水平差が 0.01 未満かどうかで、0.01 以上なら停滞回数を 0 に戻す。経路方向への進行、一定時間窓での実移動量、目標距離の減少ではない。

R8: Goal 方向 X には一切進まず、Y へ毎回 0.02 だけずれる位置を 500 update 与えた。Stuck は一度も返らず controller は active のまま。軽微な押し合い・横ずれがある場面に弱いことを示す。500 update を特定の秒数に読み替えない。

### 修正方針

Reference の移動用地形検査が必要な境界へ、SDK-free の typed observation として実測結果を渡す。観測 unavailable を「十分空いている」に置換しない。Vision 用 trace と用途を混同しない。

停滞は参照の時間窓・移動意図・実進行条件と照合する。freeze、待機、空中、ladder、他者干渉を区別し、route の頻繁な再作成が検出履歴を消していないか確認する。観測追加で以前の過剰 trace / search storm を再発させない。

## F06 — BombTarget は列挙されるが、選択・候補化が限定的

### 箇所

- `src/adapter/metamod/plugin_runtime.cpp:405–438` — classname と site extent
- 同ファイル `2458–2740` — `buildManagedObjectiveTarget()`
- `2514–2531` — cache hit 早期 return
- `2585–2604` — entity 全走査・site identity 生成
- `2619–2683` — center の nearest Area 1 個だけを評価
- `2697–2703` — 最小 pathCost の候補を選択

### 前回のログ推論の訂正

コードは `func_bomb_target` と `info_bomb_target` の双方を対象にし、複数 site を列挙する。dedup key も entityIndex/siteIdentity/area を含む。**「A/B が一つに dedup される」「列挙結果が使われていない」とは確認できなかった。**

objectiveStats は関数冒頭で初期化され、その後の cache hit では列挙前に return する。このため、選択済み Goal がある区間で `bombSites=0` が出ることは説明できる。0 を登録 site 総数と読んだ以前の説明は訂正する。

### 実際の確認点

1. 各 site につき center から nearest を 1 Area だけ選ぶ。その Area が到達不能／site外なら同じ site 内の別の到達可能 Area を試さない。
2. nearest が F03 の XY-only 選択であり、site 内判定もこの箇所では XY のみ。
3. validCandidate の最小 pathCost を採用し、同値は先着を保持する。ここに Reference RNG による site 選択は接続されていない。
4. 選択後は actor/map/round/team/C4 状態が同じで選択 entity の classname が有効なら cache を返す。

したがって「同じ開始条件で安い site を固定的に選び、その選択を保持する」経路は存在する。しかし、この実機で本当にもう片側が欠落しているか、単に同じ評価結果なのか、参照 CSBot がいつ再選択するかは別問題である。

### 修正方針

site identity を維持し、zone 内で実際に使える候補点・Area を適切に対応付ける。候補構築は map/geometry の有効期間で共有・保持し、選択や経路評価を必要なタイミングに限定する。毎 Full Update で全 site×全 Area を A* し直さない。

Reference が乱択する箇所と、その範囲・消費順・保持条件を確認する。A/B を強制交互・50/50 にして互換と主張しない。

診断は `registered_site_count`、`evaluated_this_window`、`cache_hit`、`selected_site_id`、全 site の選択／除外理由を分ける。profile を途中で ON にしても登録済み site の一覧を確認できるようにする。

## F07 — 移動診断が実行中の値と一致しない

### 箇所

- `src/core/runtime/nav_roam_controller.cpp:1336–1385` — `rememberRoute()` / `populateRouteDecision()`
- `src/adapter/metamod/plugin_runtime.cpp:3551–3597` — `logGoalAssignmentDiagnostic()`
- 同ファイル `3600–3662` — `logTraversalDiagnostic()`
- `src/core/nav/locomotion.cpp:135–169,254–265` — 実際の局所目標から intent を作る経路
- `src/adapter/metamod/plugin_runtime.cpp:2131–2141,2156–2194` — neutral gate / direction の command 投影

### 確認した動作と訂正

`activeTargetPosition_` は経路作成時に最初のリンク先中心から保存され、diagnostic の `decision.targetPosition` に使われる。実際の歩行方向は follower がその更新で返した局所目標から作られる `locomotionIntent.direction` であり、通常歩行で診断の targetPosition を直接使う構造ではない。

したがって、**「ログの target_point が古いので、その古い座標へ実際に直進している」とは言えない。** 同様に `next_area=0` はそれだけで不正な直進の証拠ではない。`roamResult != IntentReady` の production 分岐には neutral command がある。

ただしログ自体の不整合は確定する。

- `next_area` は intent.targetArea、`movement_target` は decision.targetPosition という別の更新時点・意味の値。
- `link_*` は最初に保存した activeLink_ の情報が残る。
- `distance_to_transition` は古い decision 座標と body origin の距離であり、実際の局所 portal 距離とは限らない。
- `stuck_time` は秒数ではなく `locomotionResult == Stuck` の 0/1。
- `recovery_state` は failureReason の数値で、recovery FSM state ではない。
- Walk の traversal 行は出力対象外、さらに 1 秒に 1 回へ制限される。

### 修正方針

同一 command sequence に、実際に使用した local target、feet position、current segment/link、intent、最終 buttons、観測時刻・実行時刻を結び付ける。保持 Goal と現在の steering target を別フィールドにする。停滞 elapsed と結果 enum を分ける。

ログを増量するのでなく、値の意味と時点を直す。既存ログ解析器も合わせて更新する。

## F08 — TeamInfo の採用経路が統一されていない

### 箇所

- `src/adapter/metamod/observation_adapter.cpp:203–204` — `player.team = entity->v.team`
- `src/adapter/metamod/plugin_runtime.cpp:2492–2503` — Objective 側の managed TeamInfo fallback
- 同ファイル `3394–3405` — State 側は observation.player.team のみで分岐

### 確認した動作

Objective 側は raw team <1 なら managedBotTeamNumbers_ 等へ fallback するが、Compatibility State の C4 条件は observation.player.team.value==1 を要求する。

raw team=0、TeamInfo=T、C4ありという入力では、Objective は T として目的地を選べても State 側の条件は成立しない。既存ログにある team=0 / teamInfo=1 と carrier の STATE-IDLE はこの構造と整合する。ただし、全 State 問題がこれだけで解消するわけではない。

### 修正方針

identity/generation/freshness を検査した effective team を一つの observation boundary から提供し、Objective と State で統一する。engine の v.team を勝手に書き換えたり、Unavailable を任意チームへ変換しない。

非 carrier の高位 State→Goal producer の接続も別途確認する。これを機に未計画の全 Objective/Combat を実装するのではなく、現在のデータ経路の不整合を閉じる。

## 3. 実行した検証

### 未変更 production ソースを使う独立 CMake

既存テスト 7 個:

`nav_model`, `nav_snapshot`, `nav_query`, `locomotion`, `jump_drop`, `special_traversal`, `nav_roam_controller`

全て build / 実行 exit=0。

追加ハーネス:

- `audit_diagnostic_observations`: 完走、8/8 の観察結果を再現、exit=0。
- `audit_core_safety`: R3–R6 の 4 条件で fail、exit=1。
- 独立 CTest: **8/9 PASS、1 FAIL（追加した安全性テストの意図した RED）**。

観察用 exit=0 は「不具合なし」を意味しない。R1 の全 Bot 同 Goalなどを一律に不正扱いしないため、観察と geometry/traversal の RED gate を分けている。このハーネスだけを最終互換受入れ条件にしない。

### Python の一部 contract

`p076_performance_contract.py`, `runtime_movement_integration_contract.py`, `p076_production_runtime_docs.py` を実行。

結果: **12 PASS / 1 FAIL**。FAIL は ZIP に含まれない `.planning/STATE.md` を開くテストの FileNotFoundError。NAV 実行失敗ではなく、添付 snapshot の不足により当該文書 contract を検証できない。

これはこの Linux 環境での結果であり、ユーザー Windows の Python process spawn 問題や PE verifier を解決したという意味ではない。

### 未実施

製品の Windows x86 build、公式全 CTest、実外部 de_dust2.nav regression、HLDS/ReHLDS、GameDLL physics、fixed reference の独立 differential 実行。

## 4. 修正順と受入れ条件

1. F07 の診断を、実行する値と同一 sequence にそろえる。修正途中の測定基盤にする。
2. F02/F03 を閉じる。単一 Bot・屈曲・坂・上下階で局所目標が正しいことを確認する。
3. F04 を閉じる。最初／途中／最後の Jump、複数の特殊リンク、Approach/着地後の継続を検証する。
4. F01/F05 を閉じる。高位 Goal を持たせたうえで、停滞→同じ失敗 edge の反復、微小 jitter による検知回避を確認する。
5. F06/F08 を閉じる。両 site の候補化と Reference 選択、共通 Team observation を確認する。
6. 同じ DLL・同じ NAV・同じ map/config で live 照合する。全 Bot を強制分散しなくても、必要な局所移動が成立していることを確認する。

修正は対象別の小さな差分にし、既存の search budget 分離・path persistence・objective cache 性能を回帰させない。P08 は移動の live blocker を閉じるまで保留を推奨する。

## 5. 同梱物

- `audit_repro.cpp`: 未変更ソースへリンクする診断ハーネス。
- `CMakeLists.txt`: 製品 CMake と独立したホスト診断ビルド。
- `README_JA.md`: 再実行手順・結果の読み方。
- `source_evidence.md`: 上記行範囲の実ソース抜粋。
- `source_manifest.json`: ZIP のファイル別 SHA-256、展開内容との照合結果。
- `repro_results.txt`, `core_safety_results.txt`, `standalone_ctest_results.txt`: 今回の実行結果。
- `python_contract_results.txt`: 一部 Python contract 実行結果。

**本成果物は修正済み DLL やパッチではない。** Codex が参照ソースと照合しながら修正・回帰テスト化するための監査資料である。
