# P12 実機受入れに向けた全ソース監査

更新日: 2026-09-09（Asia/Tokyo）  
監査対象: AstraBot `main` / `fc34d9beef64720e5fa752f0bc0c8d3f442af33b`  
目的: BOTが `source=Idle` のまま反応しない問題と、死亡アニメーションが最後まで再生されず震える問題の根因候補を、実装・テスト・参照Botの証拠に基づいて固定する。

この文書は、コンテキスト圧縮後の再開時に最初に読み直す監査台帳である。ここにない事実は、確定事項として扱わない。

## 1. 調査境界と現在の状態

### 1.1 Git / worktree

| 対象 | 状態 |
|---|---|
| 監査用 worktree | `H:\sourcecode\003.Game\amxmodx\AstraBot\.worktrees\main-integration` |
| ブランチ | `main` |
| HEAD | `fc34d9b` (`Accept managed bots without FL_FAKECLIENT`) |
| worktree | clean、`main...origin/main` |
| 本体 worktree | `codex/p12-console-debug`、無関係な未コミット変更・未追跡ファイルを保持 |
| ソース修正 | 今回は未実施。監査文書のみを追加する |

本体 worktree はユーザーの既存作業を保持するため、強制的に `main` へ切り替えていない。`main-integration` がすでに clean な `main` であり、ここへ監査文書を出力する。

### 1.2 解析器

#### code-review-graph フルリビルド

`main-integration` を明示的なリポジトリルートとして、full postprocess 付きで一度フルリビルドした。

```text
build_type       = full
branch           = main
built_at_sha     = fc34d9b...
head_sha         = fc34d9b...
head_matches     = true
files_parsed     = 274
nodes            = 3,132
edges            = 23,114
FTS indexed      = 3,037
FTS rebuilt      = true
flows            = 371
communities      = 11
errors           = []
updated_at       = 2026-09-09T11:12:53
```

フルリビルド前の `fc34d9b` 変更レビューでは、直接変更ノード42、影響ノード500、影響ファイル130、risk score 0.40、影響flow 0 が返っていた。`runtimeActorStaleReasonName` と `staleReason` のテストギャップも返っている。これはコミット差分のレビュー結果であり、今回の全ソース監査の証拠件数とは分けて扱う。

なお、C++メンバー関数の抽出には限界があり、複合的な semantic search が0件になる場合がある。そのため、グラフを入口にし、重要な契約はFocalSpanと直接ソース確認の両方で検証した。

#### FocalSpan

監査開始時の `focalspan status --json` は正常だった。

```text
ready=true
stale=false
index_fresh=true
mcp_ready=true
file_count=408
symbol_count=5007
relation_count=19625
diagnostic_count=171
revision=3bfbfb24ab56a538
```

code-review-graph と FocalSpan は収録対象と数え方が異なるため、274ファイルと408ファイルの差は不一致とは判定しない。FocalSpanは `startFrame` → Vision / World / Runtime / NAV / Combat / Movement の関係を確認するために使用した。

### 1.3 受入れ・Finishの境界

`docs/plans/phase-12-final-integration-and-live-acceptance.md:3-22,26-53` は、offline Runtime gate と real HLDS/ReHLDS acceptance を分離している。`tests/live/nav/manifest.json:3-20` は次の状態である。

```text
gate=post-Finish
status=Not yet validated
finishConfirmed=false
results=[]
```

したがって、今回の監査では新HLDSを起動していない。プロジェクト全体のFinishが未確認のまま、実機起動・HLDS/ReHLDS・長時間安定性・性能受入れを完了扱いにしてはならない。

前セッションから引き継いだ実機情報は次の通りである。

- 新DLLを使った新規HLDSセッションでの確認は未完了。
- HLDS起動直後に `0xC0000409` で終了した。
- AstraBot DLLを外しても再現したため、HLDS起動環境側の問題も候補である。
- 旧セッションでは `source=Idle` のみで、`source=Command`、attack、damage、killが未確認。
- 新DLLでの複数BOT維持、spawn、round継続は未再確認。
- 過去の `docs/reports/p12-console-debug.md` 相当の報告には、別の旧セッションで `astrabot_addbot 3` が `requested=3 created=3`、status上は自動primaryを含む4 BOTになったという記録がある。ただし、新DLL・新HLDSセッションの現在証拠ではなく、今回の受入れ完了の根拠にはしない。

## 2. 結論の要約

現時点の最有力な問題は、CoreのA*や戦術アルゴリズムではなく、Metamod adapterのランタイム接続である。

| ID | 重要度 | 判定 | 要点 |
|---|---|---|---|
| HIGH-01 | High | 有力候補、実機未確定 | objective未取得時に `teamDecision_` を空にするため `decision.team.shared.map` が空になり、NAV適用がmap mismatchで黙って拒否される |
| HIGH-02 | High | 有力候補、実機未確定 | 死亡後は `DEAD_NO` 条件で `RunPlayerMove` が止まり、FakeClientのGameDLL側シミュレーション／アニメーション更新が止まる可能性がある |
| HIGH-03 | High | 条件付き、実機未確定 | `FL_FAKECLIENT` 必須をRuntime入力からは外したが、NAVのActor分類とVisionの接続判定には依然として残っている |
| MEDIUM-01 | Medium | 有力候補 | TeamInfo未取得またはUnknown teamにより TacticalContext が無効になる |
| MEDIUM-02 | Medium | 有力候補 | `pfnUpdateClientData` / `pfnGetWeaponData` がRuntime入力の必須条件だが、Meta_Attach必須検査に含まれない |
| MEDIUM-03 | Medium | 有力候補 | CS武器分類の漏れ、特にXM1014/GALIL/M249/M3/Knifeで攻撃が成立しない可能性がある |
| MEDIUM-04 | Medium | 仕様上の制限 | 複数BOTを保持できても、Runtime AI入力を作るのはprimary一体だけ |
| MEDIUM-05 | Medium | 有力候補 | NAV index、current area、route state の成立条件が多く、`MissingNav` 等でRuntimeが無効化される |
| LOW-01 | Low | 未確認 | BOT作成時の `model` が空で、BOTごとの明示的なモデル選択がない |
| LOW-02 | Low | 観測性不足 | MovementログはRuntime拒否理由を直接示さず、フレーム順序のため前フレームのstatusを表示し得る |

上記は「ソースから導ける根因候補」であり、実機の `source=Command`、Runtime reason、NAV apply結果、dead後のanimation stateを取得するまで確定バグとは呼ばない。

## 3. BOTが棒立ちになる実行経路

### 3.1 現在のフレーム順序

`src/adapter/metamod/lifecycle.cpp` の `LifecycleCoordinator::startFrame()` は、概略次の順で動く。

```text
registry_.startFrame / join progress
  ↓
runtimeOwnedActor の ready 検査
  ↓
NAV beforeDispatch
  ↓
movement_.dispatchAtFrameEnd                     [lifecycle.cpp:441-450]
  ↓
vision_.frame                                      [lifecycle.cpp:452-455]
sound_.frame / world_.publish                       [lifecycle.cpp:456-460]
  ↓
buildRuntimeInputs                                 [lifecycle.cpp:490-500]
  ↓
runtime_.run                                       [lifecycle.cpp:502-503]
  ↓
executable decision の NAV適用                    [lifecycle.cpp:504-526]
  ↓
navConsole_.moveFrame                              [lifecycle.cpp:530-533]
  ↓
combat decision を neutral movement としてsubmit   [lifecycle.cpp:534-543]
  ↓
次フレームの movement dispatch で Command が実行
```

従って、Runtimeが正常でも、同じフレームには `Command` が出ず、次のフレームで初めて出る設計である。数フレーム観測しても `Idle` のみなら、単なる「同フレーム遅延」ではなく、Runtime入力の生成、Runtime decision、NAV command queueのいずれかが成立していない可能性が高い。

### 3.2 Runtime入力の成立条件

`src/adapter/metamod/runtime_input.cpp` はprimary BOT一体だけを入力化する。

- `runtime_input.cpp:115-134`
  - `owner.joinState().player()` だけを取得する。
  - primaryが無ければ `MissingPrimary` で0件。
  - `input.primary=true` を設定する。
- `runtime_input.cpp:23-55`
  - map active、map generation、tick、round、PlayerId generation、BotAgent binding、entity、removal、Joined、deadflag、health、spectatorを検証する。
- `runtime_input.cpp:145-150`
  - WorldModel snapshot、NAV state、current area、positionを要求する。
  - それぞれ `MissingWorld`、`MissingNav`、`MissingCurrentArea`、`MissingPosition` になり得る。
- `runtime_input.cpp:58-98`
  - `pfnUpdateClientData` と `pfnGetWeaponData` の両方が必要。
  - weapon id、ammo、clip、reload、timer、CAN_SHOOT/freeze bitを検証する。
- `runtime_input.cpp:162-170`
  - Team affiliationが無ければ `MissingTeam`。
  - Combat DTO変換が失敗すれば `CombatConversionFailed`。
- `runtime_input.cpp:173-210`
  - objective/economy readerは未実装で、neutral objective/economyを使用する。
  - selfには現在位置/current area/teamを設定する。
  - 既存のexecutable routeがある場合だけ tactical navigation routeを作る。

Runtime build reasonの名前は `src/adapter/metamod/console_debug.cpp:164-179` で定義されている。

### 3.3 HIGH-01: neutral TeamDecisionのmap stamp欠落

#### ソース証拠

`runtime_input.cpp:173-180` では、入力側の `TeamSnapshot` に以下を設定している。

```cpp
input.team.map = frame.map;
input.team.round = frame.round;
input.team.tick = frame.tick;
input.team.nowMicros = frame.nowMicros;
input.teamObjectiveAvailable = objective.available; // 現在はfalse
```

しかし、`src/adapter/metamod/runtime_orchestrator.cpp:386-393` のobjective unavailable分岐は、次だけを行う。

```cpp
team_.reset();
teamDecision_ = {};
teamDecision_.accepted = true;
teamDecisionReady_ = true;
```

`RuntimeDecision`には `decision.team = teamDecision_` が `runtime_orchestrator.cpp:442` 付近でコピーされる。`TeamDecision.shared` は `TeamDecision{}` のままなので、`decision.team.shared.map` はinvalid/defaultのままとなる。

一方、`src/adapter/cstrike/nav/console.cpp:97-115` の `applyRuntimeNavigation()` は、次を必須にしている。

```cpp
owner.registry().mapGeneration() == decision.team.shared.map
s.kind == ActorKind::ManagedBot
s.actor == decision.player
s.agent == decision.agent
s.map == current map
s.connected && s.alive && s.joined
```

特にmap検査は `console.cpp:104-105` にあり、neutral decisionのmap stampが空なら即時returnする。

#### 影響の連鎖

```text
RuntimeActorInputは入力側のstampでvalidになる
  ↓
objective unavailable分岐はaccepted=trueにする
  ↓
Runtime decisionは作られる可能性がある
  ↓
NAV goalを持つdecisionでも decision.team.shared.map が空
  ↓
applyRuntimeNavigation が黙ってreturn
  ↓
route session / motion command が作られない
  ↓
観測上はIdleのみになり得る
```

これは「objective readerが未実装なのでneutralにする」という仕様自体とは別の、neutral stateにもフレームidentityを保持すべきという統合契約の欠落である。実機では `runtimeResult.executableCount` と `hasNavigationGoal`、NAV statusを同時に記録して確定する。

### 3.4 HIGH-03: `FL_FAKECLIENT` 変更の適用範囲が不一致

HEADのコミット `fc34d9b` は `runtime_input.cpp:46-47` の `MissingFakeClientFlag` 検査を削除し、Runtime入力側ではFakeClient flagを必須にしない変更である。これはコミットメッセージ「Accept managed bots without FL_FAKECLIENT」と一致する。

しかし、同じコミットでは次の依存は残っている。

- `src/adapter/cstrike/nav/console.cpp:255-266`
  - `snapshotFor()` は `FL_FAKECLIENT` がある場合だけ `ActorKind::ManagedBot` にする。
  - 無ければ `Human` となる。
- `console.cpp:100-115`
  - `applyRuntimeNavigation()` は `ActorKind::ManagedBot` を要求する。
- `src/adapter/cstrike/vision.cpp:10-15`
  - `connected()` は `FL_CLIENT | FL_FAKECLIENT` のいずれかを要求する。
  - 両方がGameDLL側で消えるとVisionの対象外になる。

したがって、次の二つを分ける必要がある。

```text
Runtime input identity check       FL_FAKECLIENT dependency removed
NAV managed-bot classification     FL_FAKECLIENT dependency remains
Vision connected-entity check      FL_CLIENT/FL_FAKECLIENT dependency remains
```

GameDLLが `FL_FAKECLIENT` だけを消し `FL_CLIENT` は保持する場合、Visionは通る可能性があるが、NAVはHuman扱いで拒否される。両方を消す場合はVision/Worldも成立しない可能性がある。実機でflagsの値を取得して確定する。

### 3.5 MEDIUM-01: TeamInfo / Unknown team

`src/adapter/metamod/lifecycle.cpp:935-949` はTeamInfoの文字列を次の4値だけに変換する。

```text
TERRORIST     → Team::Terrorist
CT            → Team::CounterTerrorist
SPECTATOR     → Team::Spectator
その他        → Team::Unknown
```

`src/core/tactical_planner.cpp:176-181` の `SelfState::valid()` は `playing(team)` を要求し、Terrorist/CounterTerrorist以外を受け付けない。`buildTacticalContext()` も `tactical_planner.cpp:230-241` で非playing teamを無効化する。

そのため、fake clientにTeamInfoが届かない、文字列が想定外、またはTeamInfoのslot/generation検証に失敗した場合、次の経路になり得る。

```text
teams_.find(player) が無い/Unknown
  → MissingTeam、または self.valid() false
  → InvalidTacticalInput
  → executable=false
  → Commandなし、Idleのみ
```

TeamInfo処理は `messageMap_` と `messagePlayers_` のgenerationも `lifecycle.cpp:918-920` で検証している。fail-closedとして正しいが、実機でTeamInfoが本当に届いたかが現在のログから直接分からない。

### 3.6 MEDIUM-02: weapon callback検査の不一致

Runtime入力は `runtime_input.cpp:61-67` で次を直接呼ぶ。

```cpp
dll->pfnUpdateClientData(entity, 1, &client);
dll->pfnGetWeaponData(entity, weapons.data());
```

だが、`src/adapter/metamod/plugin_entry.cpp:63-70` の `hasRequiredFakeClientHookTable()` が検査するGameDLL hookは次だけである。

```text
pfnClientConnect
pfnClientPutInServer
pfnClientDisconnect
pfnClientCommand
```

`Meta_Attach()` は `plugin_entry.cpp:196-200` でこの表を必須にするが、`pfnUpdateClientData` / `pfnGetWeaponData` は必須条件にしていない。よってMeta_Attachに成功しても、live hook tableでweapon callbacksがnullまたは互換性のない場合、毎フレーム `WeaponUnavailable` になり得る。

### 3.7 MEDIUM-03: weapon分類漏れ

`runtime_input.cpp:13-21` の `weaponClass()` は次だけを明示分類している。

```text
Pistol:  1, 10, 11, 16, 17, 26
Sniper:  3, 13, 18, 24
SMG:     7, 12, 19, 23, 30
Rifle:   8, 15, 22, 27, 28
```

主な未分類IDは次である。

```text
5  XM1014
14 GALIL
20 M249
21 M3
29 Knife
```

Unknownは `runtime_input.cpp:96-98` で `primaryAttackReadyMicros` が現在時刻から約60秒後に設定される。`canReload` もUnknownではfalseになる（`runtime_input.cpp:78-85`）。

その結果、該当weaponでは次が起こり得る。

- firearmの攻撃許可が出ない。
- Knifeを持ってspawnした場合、Knife用の近接攻撃pathが現状見当たらない。
- weapon observation自体は通っても、Combat decisionにattackが出ない。

これは「BOTが一切移動しない」一次原因とは限らないが、攻撃反応を確認するP12-05では独立に検証が必要である。

### 3.8 MEDIUM-05: NAV stateの成立条件

`src/adapter/cstrike/nav/console.cpp:268-290` の `runtimeState()` は、次のいずれかで空のoptionalを返す。

- route request中
- deferred invalidation中
- `navigation_.map != owner.registry().mapGeneration()`
- NAV spatial index未構築

indexがあっても、current areaは `console.cpp:272-276` の概略にある `index_->containing(position, 72.0)` が成功したときだけ設定される。Runtime入力は `runtime_input.cpp:148-150` でcurrent areaを必須にする。

実機でNAV loadは成功していても、spawn位置がindexから72 unitsより外、map generationが異なる、load中、またはindexが未構築なら `MissingNav` / `MissingCurrentArea` になり得る。

### 3.9 直接の切り分けに使えるRuntime reason

`RuntimeInputBuildStatus` は `src/adapter/metamod/runtime_input.hpp:22-65` にあり、現在は次のreasonを持つ。

```text
InvalidFrame
MissingPrimary
StaleActor
MissingWorld
MissingNav
MissingCurrentArea
MissingPosition
WeaponUnavailable
MissingTeam
CombatConversionFailed
```

`staleReason` は `runtime_input.cpp:23-50` にあり、MapInactive、map/tick/round mismatch、Player generation mismatch、binding mismatch、MissingEntity、EntityFree、RemovalPending、NotJoined、Dead、InvalidHealth、SpectatorState、SpectatorFlagを区別する。

現状はこれらをMovement traceへ添付するだけで、Runtime decisionのaccepted/executable/rejectionやNAV `applyRuntimeNavigation()` の拒否理由を直接出していない。そのため、`Idle` の観測だけでは、どの境界で止まったかを確定できない。

## 4. 死亡後にアニメーションが震える経路

### 4.1 現在の死亡処理

`src/adapter/metamod/lifecycle.cpp:393-401` は各clientを走査し、entityの `deadflag != DEAD_NO` を検出すると次を実行する。

```cpp
client.combat = {};
runtime_.onDeath(player);
```

`RuntimeOrchestrator::onDeath()` は `src/adapter/metamod/runtime_orchestrator.cpp:578-583` でslotのplanner/decision/combat stateとopponent profileをclearする。これは stale commandを止める安全動作である。

### 4.2 RunPlayerMoveが死体に対して止まる

`src/adapter/metamod/movement.cpp:169-192` の `dispatchAtFrameEnd()` はpending commandがない場合、joined・entity valid・`entity->v.deadflag == DEAD_NO` のときだけidle `RunPlayerMove`を呼ぶ。

```cpp
if (!dispatchedThisFrame &&
    joinPhase == Joined && entity != nullptr &&
    !entity->free && entity->v.deadflag == DEAD_NO) {
    dispatchJoinProgress(..., MovementTraceSource::Idle);
}
```

pending commandが残っていた場合も `movement.cpp:294-300` で死体を `DeadPlayer` として拒否する。さらに、

- `lifecycle.cpp:576-583`: submit ingressがdeadflagで `DeadPlayer`。
- `lifecycle.cpp:616-627`: combat submitがdead playerをrejectし、combat stateをclear。
- `lifecycle.cpp:904-910`: round changeでもmovement/NAV/runtime/combat stateをclear。

という構造である。

### 4.3 なぜ震えの根因候補になるか

FakeClientには通常のnetwork usercmd streamがない。AstraBot自身も `movement.cpp:178-181` のコメントで、ReGameDLLが重力・animation等のplayer simulationを `RunPlayerMove` から進めると説明している。死亡後にneutral `RunPlayerMove`まで停止すると、GameDLLが死体のsequence/frame/framerateを進める契機を失い、死亡アニメーション終盤が同じ状態を反復して震える可能性がある。

ただし、AstraBotは `v.sequence`、`v.frame`、`v.framerate`、`SetAnimation` などを直接制御していない。ソース検索でも死亡アニメーションを直接設定する処理やdamage/kill hookは見つからなかった。従って、次の判定である。

```text
死亡後RunPlayerMove停止       = ソース上確定
死亡アニメーション停止が原因 = 実機で確認すべき有力仮説
GameDLL/engine自身のanimation bug = 未排除
```

### 4.4 参照Botとの相違

参照チェックアウトは、死体・freezetime・移動不要の状態でも、古い攻撃を送らずneutral入力をengineへ流す設計を持つ。

| 参照 | ソース上の観測 | AstraBotとの関係 |
|---|---|---|
| YaPB | `src/botlib.cpp:3124` で毎Think `FL_CLIENT | FL_FAKECLIENT` を復元。`botlib.cpp:3141-3182` のdead分岐後も `runMovement()` に到達し、`botlib.cpp:4016-4041` で毎frame `pfnRunPlayerMove`。 | FakeClient flag復元とdead後のmovement継続の参考 |
| podbot_mm | `bot.cpp:49,582,592,8583` でFakeClient flagを設定。`bot.cpp:8683-8700` のneutral branchでも `pfnRunPlayerMove`、通常branchも `bot.cpp:9344` で呼ぶ。 | dead/idle時にもengine simulationを進める参考 |
| SyPB | `Project SyPB/SyPB_BOT/source/basecode.cpp:3261-3296` の `Bot::Think()` がFakeClient flagを復元し、非aliveを含むThink後に `RunPlayerMovement()`。 | flag復元とdead後movementの参考 |
| RealBot | `bot.cpp:3982-3996` 付近でThink → Act後、毎frame `g_engfuncs.pfnRunPlayerMove`。 | main threadから毎frame engineへ流す参考 |

これらは monolithic なBot実装であり、AstraBotのgeneration-safe・fail-closed契約へそのままコピーするものではない。特に死体へneutral `RunPlayerMove`を送る場合も、attack/movementの古いcommandを再利用しないことが必要である。

### 4.5 死亡受入れで必要な証拠

P12-05で次を同じplayer generationについて記録する。

```text
damage発生時刻 / health
deadflagがDEAD_NOから変化したtick
死亡後のRunPlayerMove呼出し回数とmsec
呼出しsource（Command/Idle/None）
buttons=0、forward/side/up=0であること
GameDLL側のsequence/frame/framerate（取得可能な外部trace）
アニメーション完了時刻、respawn時刻
```

現行Movement traceにはsequence/frame/framerateがないため、現状ログだけでは「RunPlayerMoveが止まった」までは証明できても、「animation stateが止まった」までは証明できない。

## 5. その他の全ソース監査結果

### 5.1 既に良好と判断した設計

今回の症状に対して、次のCore設計は一次原因とは見ていない。

- `PlayerId{slot,generation}` と `BotAgentId` のgeneration-safe identity。
- map / round / tick のstamp検証と、古いcommandのfail-closed破棄。
- immutable NAV snapshot、A*、spatial index、route sessionの分離。
- local navigationとmotion primitive / ladder / jump / recoveryの分離。
- Vision、WorldModel、sound、visual effectの不正入力拒否。
- Combat入力のweapon/ammo/reload/cooldown/target検証。
- ExperienceとContextualDangerの分離。
- map-session lifecycle、round/death/disconnect時のstate退避。
- bounded diagnostics、fixed-size slot state、main threadへのengine call閉じ込め。
- FakeClientの複数slot registry自体は `lifecycle.hpp:272-280` の32 slot配列で保持される。

これは「高位AIの計算が正しい」ことを意味するだけで、live GameDLL integrationが成立していることを意味しない。

### 5.2 Runtimeの仕様上の未実装範囲

`docs/plans/phase-12-final-integration-and-live-acceptance.md:36-44` と `runtime_input.cpp:173-210` に一致して、次は未実装またはneutralである。

- bomb state / objective event reader。
- funds / purchase / economy reader。
- objective-dependent TeamDirector strategy。
- full live objective/economy support。

したがって、objectiveが無いこと自体を理由にteam strategyを合成してはいけない。今回のHIGH-01は、neutralにしたことではなく、neutral decisionのstampまで空にしたことが問題である。

### 5.3 複数BOTとsingle-primary

`runtime_orchestrator.hpp:71-85` は「current adapter contract executes exactly one primary bot」と明記する。`runtime_input.cpp:123-134` もprimaryだけを取り、secondaryをpromotionしない。

一方で、`lifecycle.hpp:274-280` は32 client slotを保持し、`startFrame()` のmovement/NAV loopは全clientを走査する。このため、現状の意味は次の通りである。

```text
astrabot_addbot 3
  → 複数FakeClientを保持できる可能性
  → movement/join管理は複数slot
  → 高位 Runtime input は自動primary一体だけ
  → 他BOTはIdle/join/NAV stateの範囲で動く
```

「複数BOTが存在する」と「複数BOTが高位AIで動く」は別の受入れ項目である。P12の現行計画はsingle-primaryを仕様としているため、複数BOTの棒立ちを直ちにsingle-primaryの実装バグとは判定しない。ただしstatusの複数BOT維持とprimaryのCommand発生は確認する。

### 5.4 BOTモデル

`src/adapter/metamod/fake_client.cpp:146-147` は次のようにモデルキーを空文字で初期化する。

```cpp
char modelKey[] = "model";
char modelValue[] = "";
```

`fake_client.cpp:171-193` では `*bot=1`、`_vgui_menus=0`、rate等を設定するが、BOTごとの明示的なmodel選択はない。GameDLLがTeam/class確定後にmodelを上書きする可能性はあるため、全BOT同一モデル問題は現状Low/未確認とする。liveではBOTごとのinfo key、team、class、実render modelを同時に採取する。

### 5.5 damage / kill観測

AstraBot側で検索できる `Death` は主にExperienceの値型イベントと `RuntimeOrchestrator::onDeath()` である。`lifecycle.cpp:398-400` はdeadflagを検出して内部stateをclearするが、damage eventやplayer-killed hookを外部から収集していない。

よって、P12で「人間がダメージを与えた」「BOTがkillされた」を証明するには、AstraBotの既存movement/runtime traceだけでは不足する。GameDLL/AMXX/サーバー標準ログなど別の観測源を同時に使用し、player generation/slotを照合する必要がある。

### 5.6 ログの観測限界

`src/adapter/metamod/console_debug.cpp:154-200` はMovement sourceとRuntime input/stale reasonの名前を持つ。`console_debug.cpp:444-470` のmovement traceは次の制限がある。

- `astrabot_debug 1` などdebug enabledでなければ出ない。
- `trace.engineCall` でないものは出さない。
- 同じsourceはcall count 512回まで抑制する。
- 表示するRuntime statusは `lifecycle_->runtimeInputBuildStatus()` の現在値である。

しかし、`lifecycle.cpp:449` のmovement dispatchが `lifecycle.cpp:498-503` のRuntime input build/runより先にある。そのため、同じ行の `runtime=... stale=... weapon=...` は、そのIdle/Command呼出しを発生させたフレームではなく、前フレームのstatusになり得る。

現状の `source=Idle` だけというログは、「idle engine callは発生している」「その時点でCommand engine callは発生していない」ことは示すが、Runtimeの拒否理由を一意には示さない。

## 6. 参照ソース4種から得た採用可能な知見

参照チェックアウトは次に置かれている。

```text
H:\sourcecode\003.Game\amxmodx\podbot_mm
H:\sourcecode\003.Game\amxmodx\yapb
H:\sourcecode\003.Game\amxmodx\SyPB
H:\sourcecode\003.Game\amxmodx\RealBot
```

### YaPB

- FakeClient flagをThinkごとに復元する。
- dead / buying / freeze / movement disabledでもneutral stateをengineに渡す。
- `pfnRunPlayerMove`のmsecをframe intervalから計算し、毎frame・毎BOT呼ぶ。
- weapon、message、sound、visibilityのlive境界がまとまっている。

AstraBotへ取り込むべきなのは「毎frame engine simulationを進める」「flagがGameDLLに消される前提でadapter-owned identityを保つ」という契約であり、YaPBのmutable Bot stateやwaypoint graphではない。

### podbot_mm

- FakeClient作成直後、Think開始時、movement pathでFakeClient flagを再設定する。
- dead / no movement pathでも `pfnRunPlayerMove` をneutral inputで呼ぶ。
- msecは1〜100msにbounded。

死亡アニメーション問題の切り分けでは、AstraBotの `DEAD_NO` 条件と最も直接的に比較できる。

### SyPB

- Think開始時に `FL_FAKECLIENT` を復元する。
- think頻度を分けながら、movement/action pathは非alive状態を含めて`RunPlayerMovement()`へ到達する。

### RealBot

- Think → Act → `pfnRunPlayerMove` の順序をBotごとに毎frame実行する。
- FakeClient作成、menu、model/team、node learningがmonolithic runtimeに含まれる。

RealBotは古い単一runtimeであり、AstraBotのCore設計へコードを移植する対象ではない。今回の確認では、FakeClient lifecycleと毎frame movementの挙動比較に限定して使用した。

既存のupstream比較では、YaPBをHost/behavior reference、SyPBをAPI concept、RealBotをlearning conceptとして位置づけている。コードコピー、waypoint layer移植、private pointer公開は行わない。license / provenanceは既存のresearch資料の管理範囲とする。

## 7. テストカバレッジと未検証項目

### 7.1 既存テストで確認できること

- `tests/adapter/runtime_input_tests.hpp`
  - NAV/current area付きのproduction input。
  - weapon/ammo/reloadと`WeaponUnavailable`境界。
  - objective unavailable / economy unavailable。
  - GoldSrc yaw `270° → -90°` 正規化。
  - Runtime commandは次tickにdispatchされる契約。
- `tests/adapter/runtime_orchestrator_tests.cpp:215-230`
  - `primary=false` の入力は実行されない。
- `tests/adapter/movement_tests.cpp`
  - pending command、stale tick、dead/engine unavailable等のmovement契約。
- `tests/adapter/fake_client_tests.cpp`、`join_state_tests.cpp`、`plugin_entry_tests.cpp`
  - FakeClient lifecycle、menu/join、command registration、detach。
- `tests/nav/*`
  - NAV parser、mesh、spatial index、route、local movement、ladder/jump/recoveryのvalue/synthetic fixture。

### 7.2 未検証または不足していること

- neutral `TeamDecision.shared.map` をNAV適用まで通すproduction regression。
- `FL_FAKECLIENT`消失後もadapter-owned ManagedBotとしてNAVを通すproduction regression。
- GameDLLの `pfnUpdateClientData` / `pfnGetWeaponData` 欠落をMeta_Attachまたはruntime diagnosticsで明示するテスト。
- `runtimeActorStaleReasonName` / `staleReason` の全列挙テスト。
- 死亡後にもneutral `RunPlayerMove`を継続し、古いattack/movementを送らないテスト。
- death animationのsequence/frame/framerate完了を証明する実GameDLLテスト。
- damage/killを外部ログとAstraBot PlayerIdへ照合するテスト。
- 実weapon ID 5/14/20/21/29のfire/knife acceptance。
- 複数BOTを8/16体で保持し、primary一体だけが高位AIを実行するlive確認。
- すべての実CS/ReGameDLL `.nav` のprovenance/version差分。synthetic NAV fixtureだけではreal-map compatibilityを主張できない。

## 8. P12再開時の切り分け手順

Finish/post-Finish境界が解除された後、以下の順で一回のlive sessionから証拠を採る。順番は、最初の失敗地点を隠さないためのものとする。

1. DLLあり／DLLなしで同一HLDS起動を比較し、`0xC0000409` がAstraBot外でも出ることを別記録する。
2. `astrabot_debug 1` を有効にし、map、Metamod attach identity、FakeClient create、join phase、TeamInfo、model/info keyを記録する。
3. NAV load成功、map generation、BOT origin、current area、route indexの成立を確認する。
4. 少なくとも100 frame以上、各BOTの `source`、`runtime`、`stale`、`weapon`、`calls` を収集する。
5. 併せて `runtimeInputBuildStatus.reason`、Runtime `decisionCount/executableCount/rejection`、`hasNavigationGoal`、NAV apply/route traceを出せる観測を追加または外部取得する。
6. `source=Idle` のみなら、次のどれかを確定する。
   - MissingPrimary
   - MissingWorld
   - MissingNav / MissingCurrentArea
   - WeaponUnavailable
   - MissingTeam / Unknown team
   - InvalidTacticalInput
   - neutral TeamDecision map stampによるNAV拒否
   - ManagedBot分類のFL_FAKECLIENT依存
7. humanからbody shot / lethal damageを与え、death tickからrespawnまでの `deadflag`、RunPlayerMove呼出し、buttons、movement、animation stateを記録する。
8. death時にold commandが廃棄されていることと、死体へのneutral simulationが必要かを分けて判定する。
9. primary一体、追加BOT、8/16体について、BOT数維持・round継続・primary以外のIdle仕様を分けて判定する。

## 9. 修正優先順位（監査結果からの候補）

これは実装承認ではなく、次の実装計画で解くべき順序である。

1. **Runtime/NAVの失敗点を一意に観測する。** reason、decision rejection、NAV apply rejection、current areaを同じtickで出す。
2. **neutral TeamDecisionにframe stampを保持させる。** `accepted=true` だけでなく、map/round/tick/timeの契約を満たすneutral shared stateを作る。
3. **ManagedBot判定をvolatileなFL_FAKECLIENTだけに依存させない。** adapter-owned bindingとentity identityを主にし、GameDLLがflagを変更した場合のVision/NAV契約を設計する。
4. **死亡後のengine simulationを検証する。** neutral `RunPlayerMove`継続が必要なら、dead flag中はbuttons/movementをゼロにし、古いpending commandを再送しない専用pathとテストを作る。
5. **weapon callback availabilityをattach/runtime契約に揃える。** 取得不能時はsilent Idleではなく明示 reasonにする。
6. **CS weapon分類とKnife近接攻撃を実装範囲として決める。** 未分類IDを追加するだけでなく、各weaponのattack semanticsを確認する。
7. **model選択の仕様を決める。** GameDLL任せならliveでteam/class/modelを証明し、BOT別モデルが必要ならFakeClient metadataまたはjoin後の選択を設計する。

## 10. 最終判定

```text
全ソース構造監査             完了（graph/FocalSpan + 直接ソース確認）
code-review-graphフルビルド  完了（main / fc34d9b / errors=0）
FocalSpan確認                 完了（fresh/ready）
参照Bot比較                  完了（podbot_mm/YaPB/SyPB/RealBot）
BOT棒立ちの根因              Runtime integrationの複数候補を特定、実機未確定
死亡アニメーションの根因     死亡後RunPlayerMove停止を有力候補として特定、実機未確定
ソース修正                    未実施
P12実機受入れ                 未完了。Finish/post-Finish境界により今回は未実行
```

次のセッションで最初にこの文書を再読し、上記「8. P12再開時の切り分け手順」から再開する。
