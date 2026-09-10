# P12 Runtime入力拒否・Idle停止・自発Roam修正計画

## 概要

実機ログでは、4体のBotが全て `phase=Joined`、`managed=1`、`connected=1`、`spawned=1` のまま、`source=Idle` を継続している。一方で `runtime_reject=3`（`InvalidActorInput`）、`decision_tick=0`、`intent=None`、`nav=Rejected / NoExecutableGoal` となっている。

従って、neutral `RunPlayerMove` heartbeatは実行されており、停止点はIdle送信ではなく、`RuntimeActorInput::valid()` による入力拒否からplannerへ到達できない経路である。`runtime_map=0`、`input_actor=0:0` も併発しているため、拒否理由とactor相関の診断欠落を先に修正する。

Roam候補が0件であることは、入力拒否の後段で観測された値であり、現時点ではRoam/NAV graphの根因と断定しない。入力がvalidになった後に、Roam生成・NAV適用・command dispatchを再評価する。

## 実装単位

### 1. Runtime入力検証理由をactor単位で返す

対象は `RuntimeActorInput::valid()` と `RuntimeOrchestrator::run()` である。

- `RuntimeInputValidationReason` を追加し、少なくとも次を区別する。
  - InvalidFrame / InvalidActor / InvalidAgent
  - TeamStampMismatch / InvalidTeam
  - WorldStampMismatch / MissingWorldMemory
  - ActionStampMismatch / ActionIdentityMismatch / InvalidAction
  - CombatStampMismatch / CombatIdentityMismatch / InvalidCombat
  - TacticalStampMismatch / TacticalIdentityMismatch / InvalidTacticalContext
  - InvalidOptionalObservation
  - InvalidExperienceEvent
- 既存の `valid()` の真偽契約は維持し、詳細理由を返す内部検証関数へ委譲する。
- validation reasonには、入力actor、agent、map、round、tickを必ず併記する。
- objective unavailable時のneutral TeamDecisionは、従来どおりobjectiveを捏造せず、入力frameのmap/round/tick/timestampとactor identityを保持する。

### 2. production入力生成の失敗点とRuntime拒否を接続する

対象は `buildRuntimeInputs()`、`LifecycleCoordinator::startFrame()`、`RuntimeInputBuildStatus`、`RuntimeCorrelation` である。

- `buildRuntimeInputs()`が生成した各actorのstatusをslot/generation単位で保持する。
- `RuntimeInputBuildReason`（MissingWorld、MissingNav、MissingCurrentArea、weapon callback、team mismatch等）と、Runtime validation reasonを混同しない。
- 入力生成に失敗したactorがいても、他actorの入力生成・planner実行を停止しない。
- compatibility用のprimary statusを残す場合も、診断表示には使用せず、actor別statusを唯一の情報源とする。
- `runtime_map=0`、`runtime_round=0`、`input_actor=0:0` の空表示を、実際のactorの失敗理由へ置き換える。

### 3. 診断の相関を修正する

対象は `console_debug` とLifecycleの相関生成である。

各行に以下を同じactorについて記録する。

```text
map / round
PlayerId + generation / BotAgentId
edict identity / serial / managed / connected / removal / JoinPhase
input build tick / input build reason
runtime validation reason / RuntimeRejectReason
decision tick / intent / route / reason
NAV result / NAV reason / NAV tick / decision tick
queue result / queue tick
dispatch result / dispatch tick / source / msec / buttons / forward / side / up
```

- `source=Idle` はneutral Engine更新、`source=Command` はAI command dispatchとして維持する。
- 前frameのprimary decisionやstatusを別actorへ表示しない。
- 同一拒否の通常ログは状態変化・初回拒否中心に抑制し、bounded counterで繰り返し件数を補完する。
- `engineCall=false` の拒否も、runtime拒否と対応付けて欠落させない。

### 4. Runtime入力をvalidにした後でRoamへ接続する

入力検証修正後、既存のP12自発Roam契約を次の順序で確認する。

1. `RuntimeActorInput::valid()` 成功
2. `RuntimeDecision`生成
3. objectiveなし・明示routeなし・候補ありの場合に `IntentType::Roam`
4. actor固有のgoalとcandidate countが記録される
5. NAV `Applied`
6. command queue成功
7. `source=Command`
8. 座標が変化する

候補がvalid入力後も0件の場合だけ、`NavConsole::runtimeState()`、currentArea、loaded graph、map/session/generation一致条件を調査する。candidate生成前にRuntime検証を緩和しない。

### 5. Idle heartbeat契約を維持する

- Joined・有効edict・現generationのactorは、commandが無い場合もneutral dispatchを1回行う。
- 正常command dispatch済みのframeにはneutralを追加しない。
- pending command拒否後は、拒否commandを再送せず、新規neutralを最大1回だけ送る。
- stale actor、disconnect、map変更、edict再利用、generation不一致では送信しない。
- dead時は旧攻撃・移動・jump/use commandを再利用しない。

## 変更対象の候補

- `src/adapter/metamod/runtime_orchestrator.hpp/.cpp`
- `src/adapter/metamod/runtime_input.hpp/.cpp`
- `src/adapter/metamod/lifecycle.hpp/.cpp`
- `src/adapter/metamod/console_debug.hpp/.cpp`
- `src/adapter/metamod/movement.hpp/.cpp`（heartbeatと拒否後fallbackに不足がある場合のみ）
- `src/adapter/cstrike/nav/console.hpp/.cpp`（valid入力後もRoam候補が0件の場合のみ）
- 対応する `tests/adapter`、`tests/tactical_planner` のfocused contract

既存の未コミット・未追跡変更は保持し、対象外のMEDIUM/LOW実装や無関係なACL・設定差分を巻き戻さない。

## 回帰テスト

実機PASS前はテストソースの追加・修正のみ行い、configure/build/CTestは実施しない。

- 各Runtime validation reasonの再現とactor/tick相関
- 正常なproduction `buildRuntimeInputs()` が `valid()` を通過すること
- 4体のうち1体だけ不正でも他3体がplannerへ到達すること
- primaryのstatus/decisionがsecondaryへ混入しないこと
- objective unavailable時のneutral stamp保持
- team generation mismatch、world/action/combat/tactical stamp mismatch
- weapon callback不足とweapon observation不正の区別
- objectiveなし＋Roam候補ありでRoam生成
- Roam候補なしでHold/neutral heartbeat
- 明示objective・routeがRoamより優先されること
- Roam到達後の再計画、NAV拒否後の候補除外
- command正常送信とIdle neutralの同一frame二重送信防止
- command拒否後のneutral fallback
- stale actor、map変更、edict再利用、generation不一致の送信禁止

## 実機検証順序

### ユーザー明示PASS前

- ソース、診断、回帰テスト、状態文書のみ編集する。
- CTest用configure/build、CTest、canonical Allを実施しない。
- 必要なDLL確認は既存配置物の読み取りに限定する。
- `git diff --check`、FocalSpan更新、静的symbol確認のみ行う。

### ユーザー明示PASS後

1. PASS日時、対象SHA、DLL SHA-256、HLDS/ReHLDS環境を`.agent-state/STATE.md`へ記録する。
2. Windows x86 Debug Portable/Metamodをconfigure・buildする。
3. validation reason、multi-actor、Roam、heartbeatのfocused testを実行する。
4. 最後に一度だけ `tools/verify-canonical.ps1 -Profile All` を実行する。
5. Release DLLを再ビルドし、指定されたHLDSへ配置する。
6. `astrabot_debug 1`、Bot 4体追加、同一round内のログを取得する。
7. 次の実機証拠を確認する。

```text
runtime_validation=None
decision_tick > 0
intent=Roam または戦闘/objective intent
nav=Applied
source=Command
座標または速度が変化
```

8. 実機確認後に差分・FocalSpan・stage対象を確認し、狭いコミットを作成する。

## 成功条件

- `InvalidActorInput`発生時に、拒否した具体的フィールドをactor/tick単位で特定できる。
- 一体の入力不成立が他Botのplanner実行を止めない。
- 有効なJoined actorは、AI commandが無いframeでもneutral Engine更新を継続する。
- 有効なRoam候補があるactorは、IdleからRoam commandへ遷移する。
- `source=Idle`と`source=Command`の意味が混同されない。
- stale generation・旧edictへcommand、Profile、診断情報が流れない。
- offline test PASSを実機PASSまたはFinishとして扱わない。

## 受入れ上の前提

- 今回のログだけではRoam候補生成を根因と断定しない。
- objectiveが存在する場合は既存objective/tactical plannerを優先する。
- objective不明を推測・捏造しない。
- 実機PASS未完了のため、コミット・merge・CTest実行は別の明示指示とゲートに従う。
