# AstraBot — Codex監査台帳の独立レビュー

- 作成日: 2026-09-09
- 対象資料: `p12-live-source-audit.md`（全634行）
- ソース照合基準: `fc34d9beef64720e5fa752f0bc0c8d3f442af33b`
- 判定: **主要指摘は採用可能。ただし、確度・網羅性の表現修正と追加指摘が必要。**

## 1. 今回実施したこと／していないこと

添付台帳全体を読み、指摘間の整合性・証拠の十分性・修正方針を監査した。重要な指摘は、上記SHA固定のGitHub公開ソースに照合した。

今回は全ソース全行の再レビューではない。ビルド、CTest、HLDS起動、死亡アニメーション再現、配置済みDLLの同一性確認、Codexのローカル解析器の再実行は行っていない。したがって、テストPASS・実機根因確定・実プレイ可能とは判定しない。

本書の「静的確認」は、該当条件下のコード経路を確認できたことを表す。「実機症状の原因確定」とは別である。台帳に記載されたローカル参照Botのチェックアウトについても、同じSHA・同じ内容をこちらで検証したとは扱わない。

## 2. 既存指摘への判定

| 台帳ID | 独立レビューの判定 | 補足・修正方針 |
|---|---|---|
| HIGH-01 | 条件付き拒否経路を静的確認 | neutral TeamDecisionのshared.map欠落とNAV側map検査が矛盾。実機のIdle全件をこれだけで説明してはいけない。 |
| HIGH-02 | 死亡後の通常dispatch経路の停止を静的確認。アニメーション根因は仮説 | 行動入力停止と、GameDLLを進める中立入力の停止を分離する。 |
| HIGH-03 | NAV分類とRuntime入力の契約不一致を静的確認 | VisionはFL_CLIENTとのOR条件。FL_FAKECLIENTだけが消えた場合と両方消えた場合を分ける。 |
| MEDIUM-01 | 妥当な診断候補 | TeamInfoの未取得・Unknown・世代不一致を区別。Unknownを推測でT/CTへ補正しない。 |
| MEDIUM-02 | capabilityと診断の整備対象 | readWeaponはnullを検査している。attachで未検査だからnull dereferenceする、という指摘ではない。 |
| MEDIUM-03 | 分類表の点検は必要。対応範囲を分ける | GALILのRifle分類漏れと、Shotgun/M249/Knifeの未実装semanticsを一括修正しない。 |
| MEDIUM-04 | 現段階の明示的制約 | single-primaryは現契約と一致するが、複数Bot自律プレイ完成の証拠にはならない。 |
| MEDIUM-05 | 入力成立条件の列挙は妥当 | 正当なJump/Ladder実行中にcurrentAreaが一時不明となるケースも追加確認する。 |
| LOW-01 | 原因未確定。根拠の追加が必要 | model空文字初期化だけでは同一モデルの原因は説明できない。addBotCommandにはclass 1〜4の循環選択がある。 |
| LOW-02 | 妥当だが影響を過小評価 | 時刻だけでなくBotの帰属も混在。engineCall=falseの拒否イベントはMovementログに出ない。 |

参照: 台帳2、3、4、5、7節。ソースは末尾S1〜S13。

## 3. HIGH-01: neutral stamp欠落は修正対象。ただし単独の根因とはしない

`RuntimeOrchestrator::run()`のobjective unavailable分岐は`teamDecision_ = {}`の後にaccepted等を設定するが、sharedのmap/round/tick/timeは設定しない。一方、`NavConsole::applyRuntimeNavigation()`は`decision.team.shared.map`と現在mapの一致を要求する。[S1][S2]

この条件下でNAV適用が拒否されることは、実機を起動しなくてもコードから確認できる。

ただし、`LifecycleCoordinator::startFrame()`末尾にはNAV適用と別にCombat decisionを中立movementへ合成する経路がある。既存の明示gotoセッションも別に追う必要がある。したがって、HIGH-01を直せばすべてのIdleが解消する、とはまだ言えない。[S3]

### 最小修正と境界

- neutral team decisionにも必要なidentityを保持する。
- objective不明を「既知のBomb objective」などへ偽装しない。
- より根本的には、Navigation配信のidentityを任意のTeamDecision内だけから取り出さない契約が望ましい。
- 戦略判断の生成時刻と、現在のcommandを配信する時刻を混同しない。cached team decisionを毎frameの新観測として扱わない。
- NAV適用は成功／拒否／変更不要を区別して診断できるようにする。

### 必須回帰

production input builder → neutral objective分岐 → runtime decision → NAV適用 → command受付まで通す。テスト側で完成済みTeamDecisionへ正しいmapを手で書いてから渡すだけでは、欠陥経路を検証したことにならない。

## 4. HIGH-03: managed identityとEngine flagを区別する

Runtime入力は`FL_FAKECLIENT`を必須としない一方、NAV snapshotは同flagなしをHumanへ分類する。[S2][S4]

Visionの接続判定は`FL_CLIENT | FL_FAKECLIENT`のどちらかが存在すれば通るため、NAVの問題と同条件ではない。[S8]

| FL_CLIENT | FL_FAKECLIENT | Visionのflag条件 | NAVの分類 |
|---:|---:|---|---|
| あり | あり | 通る | ManagedBot |
| あり | なし | 通る | Human |
| なし | あり | 通る | ManagedBot |
| なし | なし | 通らない | Human |

これはflag部分だけの表であり、entity identity、alive、registry等の他の条件は別途必要。

修正はAstraBot所有binding、PlayerId/generation、edict同一性・serial、removal状態を使う。人間、別pluginのBot、再利用slotまで無条件でAstraBot管理下と認めてはいけない。

また、AstraBot内部の所有判定をflagから独立させることと、GameDLL/Engineが必要とするfake-client flagを維持することは別課題。既存Botのflag復元だけを見て、Engine側のflag契約を不要とは結論しない。

## 5. 追加指摘A — map-session ProfileをonDeathで削除する

台帳4.1は`onDeath()`によるprofile消去も安全動作として記載している。しかし、合意済みの「roundを跨いでmap session内に保持する」方針と整合しない部分がある。

- `OpponentProfileModel::beginRound()`はprofileを保持する。[S7]
- `RuntimeOrchestrator::onDeath(player)`は`opponentProfiles_.forget(player)`を呼ぶ。[S1]
- lifecycleは実際の死亡だけでなく、runtime readiness失敗とround境界にも`onDeath()`を利用する。[S3]

該当PlayerIdのprofileが存在すれば、死亡、または一時的な入力不成立等によって削除され得る。全profileが常に消える、とは言わない。また、現在のproduction入力から学習観測がまだ供給されていない場合、この問題は未発現の統合不整合である。

### 推奨

- 戦闘／行動／経路の取消と、map-session学習の忘却を分離する。
- 一時的なRuntime input不成立をdeathイベントとして処理しない。
- 本当の死亡時は行動stateを停止し、map-session集計は保持する。
- disconnect／identity退役では対象profileを削除し、map終了では全体を退役する。
- deadflagの毎frame検査と、alive→dead遷移イベントを区別する。

### 回帰

profileを蓄積した対象について、死亡、respawn、round開始、weapon/NAV入力の一時不成立では保持され、disconnectとmap changeでは消えることを統合経路で確認する。旧generationの遅延観測が削除済みprofileを復活させないことも検証する。

## 6. 追加指摘B — pending拒否後の中立更新が抜ける

`MovementCoordinator::dispatchAtFrameEnd()`はpendingがない場合にのみIdle経路へ入る。pendingがある場合は消費して`dispatchOne()`の結果を返す。[S5]

従って、pendingがあるがweapon selection等で拒否されたフレームでは、同じ関数内でIdle更新へフォールバックしない。継続して同様の拒否が発生する状況では、古い「通常入力が作れないと物理更新も進まない」問題が残り得る。

これは「拒否された古いcommandを送ってよい」という意味ではない。

### 推奨

action commandの採否と、中立simulation commandを送れるactorの生存・接続identityを別に検証する。正常な同一actorがまだ存在し、その更新区間でengineへ未送信なら、新規に作ったゼロ入力を検討する。disconnect、map退役、edict再利用後に旧対象へ送信してはいけない。

### 回帰

- pendingなし、正常pending、同一actorに対するcommand拒否を区別。
- weapon selection失敗後も、正当なactorには必要な中立更新が継続する。
- stale identity／map変更では旧actorへ0回送信。
- 同一区間で通常入力と中立入力が二重送信されない。
- 死体対応を追加する場合も古いAttack/Use/Jump/移動入力を再利用しない。

実際のanimation改善は、引き続き実機受入れで確認する。

## 7. 追加指摘C — currentArea必須と特殊Traversalの整合

台帳のMEDIUM-05はspawn位置の問題に着目しているが、正常な移動中のNAV外区間も確認対象に含めたい。

`runtimeActorReady()`は`currentArea`を必須とし、失敗するとlifecycleが経路を無効化する。[S3][S4] `runtimeState()`のarea検出は`containing(position, 72.0)`で、72はAPI上の`maxVerticalDistance`である。水平距離を含む「半径72以内のnearest検索」ではない。[S2][S13]

Jump/Ladder/Drop等の途中でこのqueryが一致を失った場合、正当なTraversalまで入力不成立として中断される可能性がある。全てのjumpで再現するとの断定ではない。

### 推奨回帰

床Areaから離陸／梯子へ移行し、一時的にcontaining areaがない状態でも、有効なroute/traversal identityと運動観測により処理を継続できるか確認する。不明なfloor supportを捏造したり、単に許容距離を巨大化したりして解決しない。

## 8. HIGH-02の因果関係とMEDIUM-02/03の修正範囲

### 死亡アニメーション

死亡後に当該dispatch経路からRunPlayerMoveが呼ばれなくなる条件は確認できる。[S5] それが震えの根因であるという台帳の留保は正しい。モデル／sequence／frame／framerate等の実ログがない以上、修正済み判定はできない。

必要なのは「deadなら一切呼ばない」から「行動入力は禁止し、正当な死体には必要な中立更新を送る」への分離の検証であり、通常DeadPlayer gateの一括削除ではない。

### Weapon callback capability

`readWeapon()`はcallbackのnullを検査している。[S4] 従って「Meta_Attachで検査していない」というだけでメモリ安全上のバグとは言えない。

attach成功、物理更新可能、NAV使用可能、Combat観測可能を別capabilityとして診断し、必要機能が欠落したときはcallback名と理由を出す。Combat未対応だけでplugin全体をロード不能にするかは製品方針であり、今回の監査だけで強制しない。[S9]

### Weapon分類

GALILのID 14が現Rifle分類から漏れていることは分類表から確認できる。Shotgun、M249、Knifeを既存Rifleへまとめるのは別の問題を作る。[S4]

Knifeはclip/reload/射程/攻撃方式が銃器と異なるため、未対応理由の明示と個別の対応設計が必要。「現在時刻+60秒」をunsupported capabilityの代用にするより、理由を分ける方が診断しやすい。

## 9. LOW-01/02の根拠補強

### モデル

model空文字初期化だけを同一モデルの原因としない。`ConsoleDebug::addBotCommand()`には要求classを1〜4へ循環させる処理がある。[S6]

要求team/class、実際に送られた選択、GameDLL確定team/model、クライアント表示を別々に記録する。現時点で「モデル選択コードが全くない」とは言えない。

### ログ

Movementログは前frameのruntime情報を表示し得るだけでなく、全Bot共通の`runtimeInputBuildStatus()`を各Botの行へ添付している。また`engineCall=false`を出力しない。[S6]

したがって、secondaryのIdle行にprimaryのruntime理由が表示される、または拒否そのものが見えない可能性がある。

推奨相関情報:

```text
map / round
actor PlayerId+generation / BotAgentId
inputTick / decisionTick / dispatchTick
input build reason / runtime rejection
NAV apply result / command queue result / dispatch result
source / buttons / forward / side / up
```

次tick dispatchという現設計を踏まえ、すべて同一tickに書き換えるのではなく、関連づけられる別stampを保持する。通常ログは状態変化・初回拒否を中心に間引き、bounded counterで件数を補完する。

`source=Command`だけでは実移動や射撃の成功証拠にならない。実際の非ゼロ入力、位置変化、弾数変化、観測可能な被弾等と結び付ける。

## 10. 手続き上の追加指摘 — Finish条件が循環している

台帳1.3はFinish未確認のため実機起動しなかったと記録する。この判断は現AGENTSの「実機確認はFinish後」という規約と整合する。[S10]

しかしP13 Final GateはLive acceptance、Performance、Stability等がPASSになった後にのみProject-wide Finishを宣言できるとする。[S12]

```text
Liveを実施するにはFinishが必要
FinishにするにはLive PASSが必要
```

この依存関係は解消が必要。Codexの無断起動で迂回させるのではなく、ユーザー承認のもとで規約・P12・P13・manifestの意味を統一する。

推奨整理:

```text
Offline implementation / CI complete
↓
P12 live validation authorized
↓
Live smoke / movement / combat / stability acceptance
↓
Release-ready / final Finish
```

以前こちらが作成した計画にも、このFinish前後の混同を招く部分があった。実機確認を止め続ける理由にせず、実装完了と最終受入れを別状態へ整理する。

## 11. 「全ソース監査完了」の証拠範囲

台帳のgraph 274 files、FocalSpan 408 files、index fresh/errors=0は、索引処理の状態を示す。数え方が異なるためファイル数の差を即エラーとしない説明は妥当。

ただし、これらの数値から全ファイルの意味的レビュー完了やCore計算の正しさは証明できない。特に台帳5.1末尾の「高位AIの計算が正しい」旨は強すぎる。

推奨する結論表現:

> 主要なruntime経路の静的照合から統合契約の不整合を特定した。現在の症状についてCoreアルゴリズムを一次原因とする証拠は得ていないが、Core全体の正しさや実機受入れを証明したものではない。

全ソース網羅を主張する場合は、追跡ファイルの母集団、対象／対象外、全文確認／部分確認／索引のみ、関連テストと実行結果をファイル別に残す。参照BotにもSHAとdirty状態を記録する。今回未実行のテストをPASSと扱わない。

## 12. 修正・再検証の優先順位

1. **診断の相関修正とoffline regression追加** — actor、各tick、入力／decision／NAV／queue／dispatchの失敗地点を区別する。
2. **HIGH-01 + HIGH-03** — neutral identity保持とmanaged分類を、それぞれ単独および組合せで検証する。
3. **simulation heartbeat** — Idle、死亡、pending拒否、通常送信を分け、二重送信も防止する。
4. **Profile lifecycle** — 学習保持と行動取消を分離し、death/round/input-invalid/disconnectを区別する。
5. **Traversal中のcurrentArea不明** — Jump/Ladder途中の合法状態がretireされないか確かめる。
6. **Weapon/Team capabilityと分類** — callback単位、TeamInfo、対応武器ごとに診断・回帰を追加する。
7. **Live許可とFinishの整合** — 正式な規約を修正したうえで、新規HLDSプロセス・特定DLLの受入れを行う。

AstraBotなしでも発生した起動クラッシュは別トラックで保管する。同じ例外コードだけで同一原因と断定しない。既存の無関係なworktree変更を保持する。full suiteは変更のまとまりが揃った段階で一度実行し、同一build inputへの重複実行を避ける。

## 13. Codexへの短文引き継ぎ

```text
p12-live-source-audit.mdを次の点で補強してください。

- HIGH-01/03は条件付きの静的契約不整合として扱い、Idle全件の根因とは分離。
- production input→runtime→NAV→queue→dispatchの相関回帰を追加。
- onDeathがOpponent Profileをforgetし、round変更や一時input不成立にも使われる点を修正候補に追加。
- pending command拒否後のneutral simulation欠落を確認。
- Jump/Ladder中のcurrentArea一時不明で有効なTraversalが中断されないか確認。
- Movementログは共通runtime statusではなくactorとinput/decision/dispatch tickを対応付ける。
- Weapon callback不足はcapability診断として扱い、attach必須化を自動的な正解にしない。
- model問題は既存のclass循環選択まで追う。
- 全ソース網羅とテストPASSの表現を、実際のcoverage/evidenceに合わせる。
- Finish後にのみLive可／FinishにはLive PASS必須という規約の循環を、無断実行せず修正案として整理。

既存変更を保護し、最小修正とfocused testを先行。
未実行・仮説・静的確認・実機再現済みを分けて報告してください。
```

## 14. ソース参照

以下はすべてAstraBotの同一SHA固定。行番号だけに依存せず、記載symbolから確認する。

- S1: `src/adapter/metamod/runtime_orchestrator.cpp` — `run`, `onDeath`, `onDisconnect`, `reset`
- S2: `src/adapter/cstrike/nav/console.cpp` — `applyRuntimeNavigation`, `snapshotFor`, `runtimeState`
- S3: `src/adapter/metamod/lifecycle.cpp` — `startFrame`, `handleMessage`
- S4: `src/adapter/metamod/runtime_input.cpp` — `weaponClass`, `readWeapon`, `runtimeActorReady`, `buildRuntimeInputs`
- S5: `src/adapter/metamod/movement.cpp` — `dispatchAtFrameEnd`, `dispatchOne`, `dispatchJoinProgress`
- S6: `src/adapter/metamod/console_debug.cpp` — `movementTrace`, `addBotCommand`
- S7: `src/core/p11_learning.cpp` — `OpponentProfileModel::beginRound`, `forget`
- S8: `src/adapter/cstrike/vision.cpp` — `connected`, `synchronize`, `frame`
- S9: `src/adapter/metamod/plugin_entry.cpp` — `hasRequiredFakeClientHookTable`, `Meta_Attach`
- S10: `AGENTS.md` — Finish gate and post-Finish validation
- S11: `docs/plans/phase-12-final-integration-and-live-acceptance.md`
- S12: `docs/plans/phase-13-source-cleanup-and-release.md` — P13-06 Final Gate
- S13: `src/nav/query/spatial_index.hpp` — `containing(point, maxVerticalDistance)`
- S14: `src/adapter/metamod/runtime_orchestrator.hpp` — `RuntimeActorInput`, `RuntimeDecision`
- S15: `tests/adapter/runtime_input_tests.hpp`
- S16: `tests/adapter/runtime_orchestrator_tests.cpp`

参照基点:

```text
https://github.com/AoiKagase/AstraBot/tree/fc34d9beef64720e5fa752f0bc0c8d3f442af33b
https://raw.githubusercontent.com/AoiKagase/AstraBot/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/
```

末尾のraw基点にS1〜S16のpathを付加すれば対象ソースへ到達する。これらを読んだことは、そのテストを実行したことを意味しない。
