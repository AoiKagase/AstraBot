# P12 Idle遷移停止の原因切り分け計画

2026-09-10追記: 本文の入力拒否ログは過去の調査時点の証拠。現在の承認済み修正・検証境界は [全BOT自律戦闘計画](p12-autonomous-combat.md) を参照。探索Readyと移動実行失敗の分離、Jump/Drop、changelevel後NAV自動ロードを実装したが、実機受入れは未完了。

## 目的

実機でBOTが `source=Idle` から移行しない問題について、FakeClientの生成・Join・フレーム更新・Runtime入力・NAV・移動dispatchのどの境界で停止しているかを確定する。

本計画は原因調査と必要な最小修正のためのものであり、P12全体のlive acceptanceやFinish判定を代替しない。

## 現時点の証拠

ユーザー提供のMovementログでは、actor=1〜4について次が反復している。

- `phase=Joined`、`spawned=1`、`managed=1`、`connected=1`
- `source=Idle`、command値は全て0、座標は変化しない
- `runtime_reject=3` は `RuntimeRejectReason::InvalidActorInput`
- `nav=Rejected`、`nav_reason=NoExecutableGoal`
- `runtime_map=0` などのRuntime欄は、同一フレームの確定値として扱えない

したがって、StartFrame・Join・Idle時のneutral `RunPlayerMove`までは到達している。一方、Runtime入力が有効actorとして受理されず、実行可能なdecisionとNAV commandが生成されていない可能性が高い。

## 問題候補

### 1. Runtime入力の成立条件

`src/adapter/metamod/runtime_input.cpp::buildRuntimeInputs()` は、次のいずれかが成立しないとplayer/agentだけの無効入力を1件として返す。

- actor identity、generation、entity、Join/alive状態
- World snapshot
- NAV state、current area、position
- `pfnUpdateClientData` / `pfnGetWeaponData` とweapon observation
- team affiliation
- combat DTO変換

この入力は `RuntimeOrchestrator::run()` で `InvalidActorInput` として拒否されるため、NAVの `NoExecutableGoal` と次フレームのIdleが繰り返される。

### 2. 診断ログの時系列不整合

`LifecycleCoordinator::startFrame()` は、概ね次の順序で処理する。

```text
runtimeCorrelation_ reset
→ movement dispatch / Idle trace
→ buildRuntimeInputs
→ RuntimeOrchestrator::run
→ NAV適用
→ runtimeCorrelation_ 更新
```

そのため、現在のMovementログではRuntime入力理由が同一行に反映されず、`runtime_map=0` と前フレームのNAV状態が混在する。原因確定には同一actor・同一tickの相関が必要である。

### 3. 参照BOTとの設計差

YaPB、SyPB、podbot_mm、RealBot、ReGameDLL ZBotはいずれも、StartFrame系の中央フレーム入口からBOT更新を実行し、AIが停止している場合もneutral `RunPlayerMove`を継続する。

- SyPB/podbot_mmでは、Join menu未受信、`not_started`、dead、購入中、NAV未解決が初期停止条件になる。
- ReGameDLL ZBotでは通常のentity Thinkを使わず、`StartFrame → BotManager → BotThink → ExecuteCommand`で更新する。
- YaPB/RealBotでは、NAV目標未設定やscheduler gateと、engineへのcommand適用失敗を分離している。

参照実装のコード移植は行わず、ライフサイクル・毎フレーム更新・NAV未解決時の挙動を比較基準とする。

## 調査・実装手順

1. primary actor=1について、同一tickに以下を記録する。
   - `RuntimeInputBuildReason`、`staleReason`
   - map/round/tick、player/agent generation
   - current area、World、team、weapon callback状態
   - Runtime accepted/decision/rejection
   - NAV result/reason、queue outcome、dispatch outcome
2. 失敗理由を次の順に切り分ける。
   - `MissingCurrentArea` / `MissingNav`: NAV load、map generation、spatial index、spawn位置を確認
   - weapon理由: GameDLL callback tableと実weapon observationを確認
   - team理由: TeamInfo処理とgenerationを確認
   - world理由: World publish順序とactor bindingを確認
   - stale理由: entity serial、PlayerId generation、Join/alive状態を確認
3. 診断順序を必要最小限修正し、Movement traceが同一frameのRuntime結果を表示できるようにする。
4. 入力失敗が確定した場合のみ、該当境界を修正する。weaponやobjectiveの欠落を推測補完して安全性を損なわない。
5. Runtime入力が有効になった後、NAV goal生成・queue・frame末尾dispatchを確認する。

## 完了条件

- actor=1で `RuntimeInputBuildReason=None`、identity/generation一致、current area有効を確認できる。
- `RuntimeRejectReason=None` の実行可能decisionが生成される。
- NAVが `Applied` または妥当な `Unchanged` となる。
- `queueOutcome=Queued` → 次フレーム `dispatchOutcome=Dispatched` → `source=Command` を確認する。
- 非ゼロ入力またはbuttonsと、同一BOT・同一roundでの座標変化を確認する。
- 追加BOTについては、primaryのruntime動作と区別して評価する。

## 制約

- 調査中は既存の未追跡ファイル・無関係な変更を保持する。
- synthetic NAVやCTestの成功を実機受入れの代替にしない。
- 実機確認が完了するまで、プロジェクトFinishやP12 live acceptanceを完了扱いにしない。
- 実装後の検証は、まず実機の同一frame証拠を取得し、その後に変更範囲に応じたfocused verificationを行う。
