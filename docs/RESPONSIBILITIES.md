# AstraBot 責任表

対象は自作ソース・ヘッダーと関連するビルド・検証。外部SDK、生成された成果物、NAV/BSPデータは変更しない。
本表は2026-11-12の責任分離の管理表であり、CSBotとの互換性や既知不具合の修正を認定するものではない。

2026-11-13更新: 所属の解決はObservationの`resolvePlayerTeam`に集約し、
TeamInfoを生のEntity値より優先する。目的地キャッシュは所属情報を所有しない。
`collectManagedPerception`は観測を更新し、`readManagedWorldSnapshot`は公開済みSnapshotを参照する。
反応キュー・標的認識・攻撃待ち時間はBehavior、入力送信を起点とする設置試行は
Coreの`PlantAttemptController`が所有する。設置状態への遷移を起点とする互換タイマーは別責務である。
購入条件と購入実行はbuying、行動全体の優先順位と音源調査はprioritiesに配置する。

## 担当と受け渡し

| 担当 | 保持状態・変更権限 | 入力 | 出力 | 初期化・寿命 |
|---|---|---|---|---|
| BotObservationRuntime | 公開メッセージ、武器・音・知覚・観測キャッシュ。推定とUnknownを維持 | 元の観測位置で受け取るEngine値、メッセージ、世代 | CompatibilityObservation、物理状態、公開知識の読み取り | actor bind/切断、ResetHUD、ラウンド/マップ切替の既存条件 |
| BotBehaviorRuntime | 状態機械、戦術目的、シナリオ・戦闘・購入・連携判断、目的キャッシュ | 観測、移動結果、送信受付、モード | 行動要求、目標、経路破棄・移動抑制要求 | actor生成/死亡/再出現、既存ラウンド/マップ条件 |
| BotNavigationRuntime | 巡回先、経路、通過動作、味方待ち、再探索・復帰、移動診断 | 既存NavRoamObservation、行動目標、NAV | NavRoamResult、NavRoamDecision、LocomotionIntent、NavigationStep | actor/map/round/goal/path世代と既存reset条件 |
| BotCommandRuntime | 最終入力、テンプレート、送信系列、jump/crouch、接触回避 | 行動・移動要求、実行制約、現在の時刻と世代 | BotCommand、CommandReceipt、行動受付通知 | actor生成/切断、テンプレート失効、既存freeze条件 |
| PluginRuntime | 接続、フック、ライフサイクル、Bot登録、更新スケジュール | Engine/Metamod通知 | 担当呼出しと世代・時刻の通知 | attach/activate/round/deactivate/detach |

行動側のHunt等の戦術目的と、ナビゲーション側の具体的なNAV巡回先は別の責任である。
共有するのは既存の値型と読み取りビューであり、全担当が自由に書ける共通ActorStateは置かない。
担当間の変更は名前のある操作で受け渡す。ルートの初期化通知は担当のライフサイクル操作へ伝える。
観測の一括先読み、追加キャッシュ、乱数呼出しの移動、タイマーやフラグ消費順の変更は行わない。

## 更新順序

1. PluginRuntimeが既存BotTimingSchedulerの30Hz入力/10Hz判断イベントを処理する。
2. 行動判断、移動更新、同一更新内の移動結果反映を既存順序で実行する。
3. 行動入力を判断し、入力担当が最終入力を確定・送信する。
4. 送信受付と前後の物理観測を各担当へ通知し、診断は返された結果を記録する。

送信受付は物理的前進の証明ではない。Progressing等の既存列挙値・ログの意味をこの整理で変更しない。


## ライフサイクルと失敗の責任

| イベント | 観測・行動 | 移動・入力 | 統合・対応テスト |
|---|---|---|---|
| 生成・登録 | `onActorRemembered`が公開観測のactor/frameをbind。行動はプロフィール、戦闘、目的、購入、状態機械、無線を既存値で準備 | `resetCreatedActorRoute/Sampling/Readiness`と入力の`resetCreatedActorCommand/GateDiagnostic`を元の位置で実施 | Rootがactor登録・join・入力bind・スケジュールを管理。`compat_actor_command`、`fake_client_isolation` |
| 死亡・再出現 | DeathMsgは公開観測slotと反応キューを解除。ResetHUDはslot再bind。行動の死亡遷移は`updateManagedBotCompatibilityState`、再出現/round世代変化は同処理の既存分岐。プロフィール・チームまで一括初期化しない | `prepareMovement`が初回死亡で経路・着地追跡・warmupと行動のbomb approachを解除。死亡/settle/warmup時のneutral送信とその前後のreset位置を維持。入力系列の既存継続条件を保持 | 送信受付は生存/物理成功の代用にしない。`compat_actor_command`、`movement_physics`、`jump_crouch_sequencer` |
| 切断・slot再利用 | `clearManagedBot`→各`onActorRemoved`。観測はチーム・公開観測・知覚・目的知識を解除。行動は目的・戦闘・購入・状態機械・戦術記憶を解除。切断時は武器キャッシュとlookaheadの保存値を保持し、再利用時は既存のactor/frame世代照合で不一致として扱う | 移動は経路・着地・復帰・診断、入力はテンプレート・系列・回避を解除。外部解除役leaseは共有状態として既存の世代照合/lease期限と観測通知の分岐で失効・更新し、切断時の追加resetは行わない | Rootがslot登録、handle、join、timingを解除。`fake_client_isolation`、`compat_actor_command` |
| round・map・detach | round通知は武器/公開知識/無線/音/煙/認識を所定順で解除し再bind。行動は購入/plant乱数/戦術/連携を同じ位置で初期化。mapはprovider probeとmap共有観測を準備、deactivateでproviderを解除 | roundの経路世代確認はNavRoamControllerの既存更新に任せる。attach/deactivate/detachの`resetSharedExecutionState/Actor`は元のreset集合を保持。死亡danger受付はround、danger記憶はmapで解除 | Rootがmap/round世代・freeze・NavSnapshot・入力bind・Bot登録を管理。重複round通知の扱いも維持。`metamod_hooks`、`compat_actor_command`、`round_lifecycle_tracker` |
| 失敗・受付 | 行動が経路失敗を連携要求に反映。reload/C4選択/攻撃の受付は専用通知で観測/行動に戻す | 行動の`NavigationRequest`をRootが同期適用し、経路破棄・step公開・failure解除を元の位置で実施。移動更新結果は`NavigationUpdate`値で返す。入力は受付を`CommandReceipt`で返す | `NavigationUpdate`はその更新だけの戻り値。共有の可変ActorStateではない。`runtime_characterization`と各Coreテスト |

担当別の`BotObservationServices`、`BotBehaviorServices`、`BotNavigationServices`、
`BotCommandServices`はインフラの読み取りと名前付き操作のポートであり、ActorStateへの可変参照を返さない。
経路要求を更新末尾へ遅延すると同じ更新の判断が変わるため、型付き要求は同期適用する。
通常移動・待機・join heartbeat・購入・無線の送信入口は`BotCommandRuntime`に集約。
Rootの`dispatchClientCommand`は既存GameDLL hook/argvエミュレーションの転送先として残す。

## NavRoamController内部

`RoamGoalState`（巡回目標）、`RouteState`（経路）、`TraversalState`（通過動作）、
`FriendWaitState`（味方待ち）、`RecoveryState`（失敗・復帰）をprivateな参照ビューで分離する。
公開C++クラスのABIを保持するため、格納メンバーの順序とコンストラクター初期化は移動しない。
ビューは外に返さず、状態・キャッシュを追加しない。タイマー/フラグ消費順と乱数位置は従来どおり。

## 状態ごとの対応

移動元は凍結入力の位置、現在位置は分離後の定義を示す。各ActorStateは担当クラスのprivateストレージ。
初期化欄は実際に状態を初期化する通知操作を示す。各操作は従来の初期値・条件を移したもの。
条件付きの死亡・再出現処理は下記寿命表と担当メソッドを併せて参照する。列挙されないイベントで追加の一括リセットは行わない。

| 保持状態 | 担当 | 保持単位 | 変更できる担当 | 入力 | 出力 | 初期化・保持条件の実装 | 移動元 | 現在の実装位置 | 対応テスト |
|---|---|---|---|---|---|---|---|---|---|
| `state_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach / onServerActivate / onServerDeactivate | `src/adapter/metamod/plugin_runtime.hpp:422` | `src/adapter/metamod/plugin_runtime.hpp:141` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `metaGlobals_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach | `src/adapter/metamod/plugin_runtime.hpp:423` | `src/adapter/metamod/plugin_runtime.hpp:142` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `gameDllFunctions_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach | `src/adapter/metamod/plugin_runtime.hpp:424` | `src/adapter/metamod/plugin_runtime.hpp:143` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `hookedGameDllFunctions_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach | `src/adapter/metamod/plugin_runtime.hpp:425` | `src/adapter/metamod/plugin_runtime.hpp:144` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `engineFunctions_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach / giveEnginePointers | `src/adapter/metamod/plugin_runtime.hpp:426` | `src/adapter/metamod/plugin_runtime.hpp:145` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `globals_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / detach / giveEnginePointers | `src/adapter/metamod/plugin_runtime.hpp:427` | `src/adapter/metamod/plugin_runtime.hpp:146` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `roundFreezeScheduled_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / onServerActivate / onServerDeactivate / onRoundLifecycleMessage | `src/adapter/metamod/plugin_runtime.hpp:428` | `src/adapter/metamod/plugin_runtime.hpp:147` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `roundFreezeStartTime_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / onServerActivate / onServerDeactivate / onRoundLifecycleMessage | `src/adapter/metamod/plugin_runtime.hpp:429` | `src/adapter/metamod/plugin_runtime.hpp:148` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `roundFreezeUntil_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / onServerActivate / onServerDeactivate / onRoundLifecycleMessage | `src/adapter/metamod/plugin_runtime.hpp:430` | `src/adapter/metamod/plugin_runtime.hpp:149` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `roundFreezeGeneration_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / onServerActivate / onServerDeactivate / onRoundLifecycleMessage | `src/adapter/metamod/plugin_runtime.hpp:431` | `src/adapter/metamod/plugin_runtime.hpp:150` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `roundLifecycleTracker_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / onServerActivate / onServerDeactivate | `src/adapter/metamod/plugin_runtime.hpp:432` | `src/adapter/metamod/plugin_runtime.hpp:151` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `lifecycle_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach / beginRoundGeneration / onServerActivate / onServerDeactivate / onStartFrame | `src/adapter/metamod/plugin_runtime.hpp:433` | `src/adapter/metamod/plugin_runtime.hpp:152` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `actorRegistry_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach / onServerDeactivate | `src/adapter/metamod/plugin_runtime.hpp:434` | `src/adapter/metamod/plugin_runtime.hpp:153` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `fakeClientManager_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / configureFakeClientManager | `src/adapter/metamod/plugin_runtime.hpp:435` | `src/adapter/metamod/plugin_runtime.hpp:154` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `inputDispatcher_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach / onServerDeactivate / configureFakeClientManager | `src/adapter/metamod/plugin_runtime.hpp:436` | `src/adapter/metamod/plugin_runtime.hpp:155` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `compatibilitySurface_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター | `src/adapter/metamod/plugin_runtime.hpp:437` | `src/adapter/metamod/plugin_runtime.hpp:156` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `compatibilityRandomSource_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / giveEnginePointers | `src/adapter/metamod/plugin_runtime.hpp:438` | `src/adapter/metamod/plugin_runtime.hpp:157` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `observationAdapter_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:439` | `src/adapter/metamod/bot_observation_runtime.hpp:254` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicObservations_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered / rebindRoundObservations / resetMapPublicObservations | `src/adapter/metamod/plugin_runtime.hpp:440` | `src/adapter/metamod/bot_observation_runtime.hpp:255` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageName_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:441` | `src/adapter/metamod/bot_observation_runtime.hpp:256` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageTarget_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:442` | `src/adapter/metamod/bot_observation_runtime.hpp:257` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageDestination_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:443` | `src/adapter/metamod/bot_observation_runtime.hpp:258` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageActor_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:444` | `src/adapter/metamod/bot_observation_runtime.hpp:259` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageFrame_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:445` | `src/adapter/metamod/bot_observation_runtime.hpp:260` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageValues_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:446` | `src/adapter/metamod/bot_observation_runtime.hpp:261` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageCoords_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:447` | `src/adapter/metamod/bot_observation_runtime.hpp:262` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageCoordMask_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:448` | `src/adapter/metamod/bot_observation_runtime.hpp:263` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicBombRadars_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved | `src/adapter/metamod/plugin_runtime.hpp:449` | `src/adapter/metamod/bot_observation_runtime.hpp:233` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `bombGuardPlanners_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved | `src/adapter/metamod/plugin_runtime.hpp:450` | `src/adapter/metamod/bot_behavior_runtime.hpp:202` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `publicMessageStringMask_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:451` | `src/adapter/metamod/bot_observation_runtime.hpp:264` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageFieldCount_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:452` | `src/adapter/metamod/bot_observation_runtime.hpp:265` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageMalformed_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:453` | `src/adapter/metamod/bot_observation_runtime.hpp:266` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageString_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:454` | `src/adapter/metamod/bot_observation_runtime.hpp:267` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicMessageStrings_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:455` | `src/adapter/metamod/bot_observation_runtime.hpp:268` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `scenarioRadio_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundRadioAndSound / resetMapPublicObservations | `src/adapter/metamod/plugin_runtime.hpp:456` | `src/adapter/metamod/bot_observation_runtime.hpp:234` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `radioSequence_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetRoundRadioAndSound / resetMapPublicObservations | `src/adapter/metamod/plugin_runtime.hpp:457` | `src/adapter/metamod/bot_observation_runtime.hpp:269` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `outgoingChatterCatalog_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:458` | `src/adapter/metamod/bot_behavior_runtime.hpp:239` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingRadioSchedulers_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:459` | `src/adapter/metamod/bot_behavior_runtime.hpp:203` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingRadioContactMemory_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:460` | `src/adapter/metamod/bot_behavior_runtime.hpp:204` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingRadioReceipts_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:461` | `src/adapter/metamod/bot_behavior_runtime.hpp:205` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingRadioAttemptGenerations_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:462` | `src/adapter/metamod/bot_behavior_runtime.hpp:206` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingRadioAttempts_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:463` | `src/adapter/metamod/bot_behavior_runtime.hpp:207` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingRadioAttemptTimes_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:464` | `src/adapter/metamod/bot_behavior_runtime.hpp:208` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingRadioAttemptChoices_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:465` | `src/adapter/metamod/bot_behavior_runtime.hpp:209` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingTeamLastSpeech_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:466` | `src/adapter/metamod/bot_behavior_runtime.hpp:240` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingTeamBusyUntil_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:467` | `src/adapter/metamod/bot_behavior_runtime.hpp:241` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingTeamCurrentPhrase_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:468` | `src/adapter/metamod/bot_behavior_runtime.hpp:242` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingMissingPhraseLogged_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:469` | `src/adapter/metamod/bot_behavior_runtime.hpp:244` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingRadioMap_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:470` | `src/adapter/metamod/bot_behavior_runtime.hpp:245` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `outgoingRadioRound_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:471` | `src/adapter/metamod/bot_behavior_runtime.hpp:246` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `teamScores_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetMapPublicObservations | `src/adapter/metamod/plugin_runtime.hpp:472` | `src/adapter/metamod/bot_observation_runtime.hpp:270` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `teamScoresKnown_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetMapPublicObservations | `src/adapter/metamod/plugin_runtime.hpp:473` | `src/adapter/metamod/bot_observation_runtime.hpp:271` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicScoreStatuses_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundObjectiveKnowledge | `src/adapter/metamod/plugin_runtime.hpp:481` | `src/adapter/metamod/bot_observation_runtime.hpp:235` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `bombNoticeKnown_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundObjectiveKnowledge | `src/adapter/metamod/plugin_runtime.hpp:482` | `src/adapter/metamod/bot_observation_runtime.hpp:236` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `bombNoticePlanted_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundObjectiveKnowledge | `src/adapter/metamod/plugin_runtime.hpp:483` | `src/adapter/metamod/bot_observation_runtime.hpp:237` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicBombClocks_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundObjectiveKnowledge | `src/adapter/metamod/plugin_runtime.hpp:484` | `src/adapter/metamod/bot_observation_runtime.hpp:238` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `scenarioEscapeAreas_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundTactics | `src/adapter/metamod/plugin_runtime.hpp:485` | `src/adapter/metamod/bot_behavior_runtime.hpp:210` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `bombSearchMemory_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundTactics | `src/adapter/metamod/plugin_runtime.hpp:486` | `src/adapter/metamod/bot_behavior_runtime.hpp:211` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `bombSearchTargets_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundTactics | `src/adapter/metamod/plugin_runtime.hpp:487` | `src/adapter/metamod/bot_behavior_runtime.hpp:212` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `teamworkPolicies_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundTactics | `src/adapter/metamod/plugin_runtime.hpp:488` | `src/adapter/metamod/bot_behavior_runtime.hpp:213` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `teamworkRadioApplied_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundTactics | `src/adapter/metamod/plugin_runtime.hpp:489` | `src/adapter/metamod/bot_behavior_runtime.hpp:214` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedProfiles_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:490` | `src/adapter/metamod/bot_behavior_runtime.hpp:215` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `reactionQueues_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered / rebindRoundObservations | `src/adapter/metamod/plugin_runtime.hpp:491` | `src/adapter/metamod/bot_behavior_runtime.hpp:239` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `recognizedTargets_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered / rebindRoundObservations | `src/adapter/metamod/plugin_runtime.hpp:492` | `src/adapter/metamod/bot_behavior_runtime.hpp:240` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `recognizedPoints_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:493` | `src/adapter/metamod/bot_behavior_runtime.hpp:241` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `recognitionSequences_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemembered / rebindRoundObservations | `src/adapter/metamod/plugin_runtime.hpp:494` | `src/adapter/metamod/bot_behavior_runtime.hpp:242` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `lastCombatTimes_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemembered | `src/adapter/metamod/plugin_runtime.hpp:495` | `src/adapter/metamod/bot_behavior_runtime.hpp:216` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `lastVisibleEnemyTimes_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:496` | `src/adapter/metamod/bot_behavior_runtime.hpp:243` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `attackReadyTimes_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:497` | `src/adapter/metamod/bot_behavior_runtime.hpp:217` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `buyControllers_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered / beginRoundPlantDecision | `src/adapter/metamod/plugin_runtime.hpp:498` | `src/adapter/metamod/bot_behavior_runtime.hpp:218` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `buyingComplete_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemembered / resetRoundBuyingState | `src/adapter/metamod/plugin_runtime.hpp:499` | `src/adapter/metamod/bot_behavior_runtime.hpp:219` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `escapingWithKnife_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemembered / resetRoundBuyingState | `src/adapter/metamod/plugin_runtime.hpp:500` | `src/adapter/metamod/bot_behavior_runtime.hpp:220` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `buyReceiptTimeoutLogged_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemembered / resetRoundBuyingState | `src/adapter/metamod/plugin_runtime.hpp:501` | `src/adapter/metamod/bot_behavior_runtime.hpp:221` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `weaponActions_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved | `src/adapter/metamod/plugin_runtime.hpp:502` | `src/adapter/metamod/bot_behavior_runtime.hpp:222` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `nextPopulationUpdate_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / onServerActivate / maintainPublicBotPopulation | `src/adapter/metamod/plugin_runtime.hpp:503` | `src/adapter/metamod/plugin_runtime.hpp:159` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `populationAdjustment_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / maintainPublicBotPopulation | `src/adapter/metamod/plugin_runtime.hpp:504` | `src/adapter/metamod/plugin_runtime.hpp:160` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `smokeObservations_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | rebindRoundObservations / resetMapSoundAndSmoke | `src/adapter/metamod/plugin_runtime.hpp:505` | `src/adapter/metamod/bot_observation_runtime.hpp:272` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `optionalObservations_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetMapPublicObservations / deactivateMapProvider | `src/adapter/metamod/plugin_runtime.hpp:506` | `src/adapter/metamod/bot_observation_runtime.hpp:273` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `soundEvents_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetRoundRadioAndSound / resetMapSoundAndSmoke | `src/adapter/metamod/plugin_runtime.hpp:507` | `src/adapter/metamod/bot_observation_runtime.hpp:274` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `soundTeams_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:508` | `src/adapter/metamod/bot_observation_runtime.hpp:275` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `soundSequence_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundRadioAndSound / resetMapSoundAndSmoke | `src/adapter/metamod/plugin_runtime.hpp:509` | `src/adapter/metamod/bot_observation_runtime.hpp:276` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `deliveredSoundSequences_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundRadioAndSound / resetMapSoundAndSmoke | `src/adapter/metamod/plugin_runtime.hpp:510` | `src/adapter/metamod/bot_observation_runtime.hpp:244` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `investigatedNoiseTicks_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundNoiseInvestigation | `src/adapter/metamod/plugin_runtime.hpp:511` | `src/adapter/metamod/bot_behavior_runtime.hpp:223` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `publicSmokeEntities_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | rebindRoundObservations / resetMapSoundAndSmoke | `src/adapter/metamod/plugin_runtime.hpp:517` | `src/adapter/metamod/bot_observation_runtime.hpp:277` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `nextSmokeEntity_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | rebindRoundObservations / resetMapSoundAndSmoke | `src/adapter/metamod/plugin_runtime.hpp:518` | `src/adapter/metamod/bot_observation_runtime.hpp:278` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `runtimeProfiler_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター | `src/adapter/metamod/plugin_runtime.hpp:519` | `src/adapter/metamod/plugin_runtime.hpp:162` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `navLoader_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター | `src/adapter/metamod/plugin_runtime.hpp:520` | `src/adapter/metamod/plugin_runtime.hpp:163` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `navPublisher_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター | `src/adapter/metamod/plugin_runtime.hpp:521` | `src/adapter/metamod/plugin_runtime.hpp:164` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `navLoadDiagnostic_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach / onServerDeactivate / loadCurrentMapNavigation | `src/adapter/metamod/plugin_runtime.hpp:522` | `src/adapter/metamod/plugin_runtime.hpp:165` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `adapterFrameCount_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach / onServerActivate / onServerDeactivate / onStartFrame | `src/adapter/metamod/plugin_runtime.hpp:523` | `src/adapter/metamod/plugin_runtime.hpp:166` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `pluginId_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach | `src/adapter/metamod/plugin_runtime.hpp:524` | `src/adapter/metamod/plugin_runtime.hpp:167` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `nativeBotGuard_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター | `src/adapter/metamod/plugin_runtime.hpp:525` | `src/adapter/metamod/plugin_runtime.hpp:168` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `nativeGuardDecision_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / updateNativeBotGuard / armNativeBotGuard / resetNativeBotGuard | `src/adapter/metamod/plugin_runtime.hpp:526` | `src/adapter/metamod/plugin_runtime.hpp:169` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotSlots_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / createFakeClient / rememberManagedBot / clearManagedBot | `src/adapter/metamod/plugin_runtime.hpp:527` | `src/adapter/metamod/plugin_runtime.hpp:170` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `playerAvoidance_` | BotCommandRuntime | Botごとのprivate ActorState | BotCommandRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered | `src/adapter/metamod/plugin_runtime.hpp:528` | `src/adapter/metamod/bot_command_runtime.hpp:92` | compat_actor_command / fake_client_isolation / metamod_hooks / action_adapter / input_dispatcher / jump_crouch_sequencer |
| `managedBotCreationIds_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / rememberManagedBot / clearManagedBot | `src/adapter/metamod/plugin_runtime.hpp:529` | `src/adapter/metamod/plugin_runtime.hpp:172` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `nextBotCreationId_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター | `src/adapter/metamod/plugin_runtime.hpp:530` | `src/adapter/metamod/plugin_runtime.hpp:173` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `teamInfoTeams_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered | `src/adapter/metamod/plugin_runtime.hpp:531` | `src/adapter/metamod/bot_observation_runtime.hpp:245` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `publicTeamKnown_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered | `src/adapter/metamod/plugin_runtime.hpp:532` | `src/adapter/metamod/bot_observation_runtime.hpp:246` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `managedBotHandles_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / rememberManagedBot / clearManagedBot | `src/adapter/metamod/plugin_runtime.hpp:533` | `src/adapter/metamod/plugin_runtime.hpp:175` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotNames_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / clearManagedBot | `src/adapter/metamod/plugin_runtime.hpp:536` | `src/adapter/metamod/plugin_runtime.hpp:178` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `joinControllers_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / clearManagedBot / processJoinControllers | `src/adapter/metamod/plugin_runtime.hpp:538` | `src/adapter/metamod/plugin_runtime.hpp:180` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `pendingJoinActions_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター | `src/adapter/metamod/plugin_runtime.hpp:539` | `src/adapter/metamod/plugin_runtime.hpp:181` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `pendingJoinActionOverflow_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / onServerDeactivate / deferJoinAction / processDeferredJoinActions | `src/adapter/metamod/plugin_runtime.hpp:540` | `src/adapter/metamod/plugin_runtime.hpp:182` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `userMessageKind_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:541` | `src/adapter/metamod/bot_observation_runtime.hpp:279` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageTarget_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:542` | `src/adapter/metamod/bot_observation_runtime.hpp:280` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageFieldCount_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:543` | `src/adapter/metamod/bot_observation_runtime.hpp:281` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageWeaponState_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:544` | `src/adapter/metamod/bot_observation_runtime.hpp:282` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageWeaponId_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:545` | `src/adapter/metamod/bot_observation_runtime.hpp:283` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageWeaponMessageValid_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:546` | `src/adapter/metamod/bot_observation_runtime.hpp:284` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageMenuType_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:547` | `src/adapter/metamod/bot_observation_runtime.hpp:285` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageNeedMore_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:548` | `src/adapter/metamod/bot_observation_runtime.hpp:286` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageValidSlots_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:549` | `src/adapter/metamod/bot_observation_runtime.hpp:287` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageTeamSlot_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:550` | `src/adapter/metamod/bot_observation_runtime.hpp:288` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageShowFragmentActive_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:551` | `src/adapter/metamod/bot_observation_runtime.hpp:289` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageText_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:552` | `src/adapter/metamod/bot_observation_runtime.hpp:290` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `userMessageTextLength_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:553` | `src/adapter/metamod/bot_observation_runtime.hpp:291` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `managedBotAreaVisitHistory_` | BotNavigationRuntime | 共有状態（Bot配列と分離） | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:554` | `src/adapter/metamod/bot_navigation_runtime.hpp:220` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `managedBotRoamReservationBoard_` | BotNavigationRuntime | 共有状態（Bot配列と分離） | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:555` | `src/adapter/metamod/bot_navigation_runtime.hpp:221` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `managedBotMovement_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetActorExecutionState / resetCreatedActorRoute | `src/adapter/metamod/plugin_runtime.hpp:558` | `src/adapter/metamod/bot_navigation_runtime.hpp:196` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `managedNavigationFailures_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:566` | `src/adapter/metamod/bot_navigation_runtime.hpp:197` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `managedNavigationSteps_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:567` | `src/adapter/metamod/bot_navigation_runtime.hpp:198` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `managedStateInputs_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:568` | `src/adapter/metamod/bot_behavior_runtime.hpp:224` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedMoveSpeeds_` | BotCommandRuntime | Botごとのprivate ActorState | BotCommandRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:569` | `src/adapter/metamod/bot_command_runtime.hpp:93` | compat_actor_command / fake_client_isolation / metamod_hooks / action_adapter / input_dispatcher / jump_crouch_sequencer |
| `managedCommandDiagnosticUpdates_` | BotCommandRuntime | Botごとのprivate ActorState | BotCommandRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:570` | `src/adapter/metamod/bot_command_runtime.hpp:94` | compat_actor_command / fake_client_isolation / metamod_hooks / action_adapter / input_dispatcher / jump_crouch_sequencer |
| `managedBotCombat_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered / resetActorExecutionState | `src/adapter/metamod/plugin_runtime.hpp:573` | `src/adapter/metamod/bot_behavior_runtime.hpp:225` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotObjectives_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered / resetActorExecutionState | `src/adapter/metamod/plugin_runtime.hpp:576` | `src/adapter/metamod/bot_behavior_runtime.hpp:226` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotObjectiveTargets_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetActorExecutionState | `src/adapter/metamod/plugin_runtime.hpp:579` | `src/adapter/metamod/bot_behavior_runtime.hpp:227` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBombApproaches_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetActorExecutionState | `src/adapter/metamod/plugin_runtime.hpp:581` | `src/adapter/metamod/bot_behavior_runtime.hpp:228` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `roundPlantDecision_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | beginRoundPlantDecision / beginMapPlantDecision | `src/adapter/metamod/plugin_runtime.hpp:582` | `src/adapter/metamod/bot_behavior_runtime.hpp:248` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `dangerMemory_` | BotNavigationRuntime | 共有状態（Bot配列と分離） | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetMapDanger | `src/adapter/metamod/plugin_runtime.hpp:583` | `src/adapter/metamod/bot_navigation_runtime.hpp:222` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `deathDangerRecorded_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetRoundDeathReceipts | `src/adapter/metamod/plugin_runtime.hpp:584` | `src/adapter/metamod/bot_navigation_runtime.hpp:199` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `managedTeamObjectiveCoordinator_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:585` | `src/adapter/metamod/bot_behavior_runtime.hpp:249` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `compatibilityObjectiveCoordinators_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:586` | `src/adapter/metamod/bot_behavior_runtime.hpp:229` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `objectiveKnowledge_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetRoundObjectiveKnowledge / resetMapObjectiveKnowledge | `src/adapter/metamod/plugin_runtime.hpp:592` | `src/adapter/metamod/bot_observation_runtime.hpp:247` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `managedTeamObjectiveAssignmentContext_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:593` | `src/adapter/metamod/bot_behavior_runtime.hpp:250` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedTeamObjectiveAssignments_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:594` | `src/adapter/metamod/bot_behavior_runtime.hpp:251` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedTeamObjectiveAssignmentMapGeneration_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:595` | `src/adapter/metamod/bot_behavior_runtime.hpp:252` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedTeamObjectiveAssignmentRoundGeneration_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:596` | `src/adapter/metamod/bot_behavior_runtime.hpp:253` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedTeamObjectiveAssignmentFrame_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:597` | `src/adapter/metamod/bot_behavior_runtime.hpp:254` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedTeamObjectiveAssignmentsValid_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState / resetRoundObjectiveCoordination | `src/adapter/metamod/plugin_runtime.hpp:598` | `src/adapter/metamod/bot_behavior_runtime.hpp:255` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `externalDefuserEntityIndex_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:599` | `src/adapter/metamod/bot_observation_runtime.hpp:292` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `externalDefuserEntitySerial_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:600` | `src/adapter/metamod/bot_observation_runtime.hpp:293` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `externalDefuserLeaseUntil_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:601` | `src/adapter/metamod/bot_observation_runtime.hpp:294` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `externalDefuserMapGeneration_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:602` | `src/adapter/metamod/bot_observation_runtime.hpp:295` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `externalDefuserRoundGeneration_` | BotObservationRuntime | 共有状態（Bot配列と分離） | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:603` | `src/adapter/metamod/bot_observation_runtime.hpp:296` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `managedTeamObjectivePathFailurePending_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState / resetRoundObjectiveCoordination | `src/adapter/metamod/plugin_runtime.hpp:604` | `src/adapter/metamod/bot_behavior_runtime.hpp:256` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedTeamObjectivePathFailureActor_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState / resetRoundObjectiveCoordination | `src/adapter/metamod/plugin_runtime.hpp:605` | `src/adapter/metamod/bot_behavior_runtime.hpp:257` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedTeamObjectivePathFailureGeneration_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState / resetRoundObjectiveCoordination | `src/adapter/metamod/plugin_runtime.hpp:606` | `src/adapter/metamod/bot_behavior_runtime.hpp:258` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedTeamObjectivePathFailureStartArea_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState / resetRoundObjectiveCoordination | `src/adapter/metamod/plugin_runtime.hpp:607` | `src/adapter/metamod/bot_behavior_runtime.hpp:259` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedTeamObjectivePathFailureFrame_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState / resetRoundObjectiveCoordination | `src/adapter/metamod/plugin_runtime.hpp:608` | `src/adapter/metamod/bot_behavior_runtime.hpp:260` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotObjectiveActions_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState / resetActorExecutionState / resetRoundObjectiveCoordination | `src/adapter/metamod/plugin_runtime.hpp:610` | `src/adapter/metamod/bot_behavior_runtime.hpp:230` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotCurrentWeapons_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetActorExecutionState / resetRoundWeapons | `src/adapter/metamod/plugin_runtime.hpp:612` | `src/adapter/metamod/bot_observation_runtime.hpp:248` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `managedObjectiveSiteRegistry_` | BotBehaviorRuntime | 共有状態（Bot配列と分離） | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:613` | `src/adapter/metamod/bot_behavior_runtime.hpp:261` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotStateMachines_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered / resetActorExecutionState | `src/adapter/metamod/plugin_runtime.hpp:616` | `src/adapter/metamod/bot_behavior_runtime.hpp:231` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `scenarioPlanners_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:617` | `src/adapter/metamod/bot_behavior_runtime.hpp:232` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `scenarioDecisions_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:618` | `src/adapter/metamod/bot_behavior_runtime.hpp:233` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotPerception_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetActorExecutionState | `src/adapter/metamod/plugin_runtime.hpp:621` | `src/adapter/metamod/bot_observation_runtime.hpp:249` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `managedBotPerceptionFullUpdates_` | BotObservationRuntime | Botごとのprivate ActorState | BotObservationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:624` | `src/adapter/metamod/bot_observation_runtime.hpp:250` | compat_actor_command / fake_client_isolation / metamod_hooks / observation_adapter / public_observation / perception_adapter |
| `managedBotCommandSequences_` | BotCommandRuntime | Botごとのprivate ActorState | BotCommandRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorCommand | `src/adapter/metamod/plugin_runtime.hpp:627` | `src/adapter/metamod/bot_command_runtime.hpp:95` | compat_actor_command / fake_client_isolation / metamod_hooks / action_adapter / input_dispatcher / jump_crouch_sequencer |
| `managedBotFullUpdateSequences_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / updateManagedBots / resetManagedBotExecutionState / rememberManagedBot / clearManagedBot | `src/adapter/metamod/plugin_runtime.hpp:630` | `src/adapter/metamod/plugin_runtime.hpp:186` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotStateRounds_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:633` | `src/adapter/metamod/bot_behavior_runtime.hpp:234` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotWasDead_` | BotBehaviorRuntime | Botごとのprivate ActorState | BotBehaviorRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / onActorRemembered / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:635` | `src/adapter/metamod/bot_behavior_runtime.hpp:235` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotTiming_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / resetManagedBotTiming / resetManagedBotExecutionState / createFakeClient / clearManagedBot | `src/adapter/metamod/plugin_runtime.hpp:638` | `src/adapter/metamod/plugin_runtime.hpp:190` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `managedBotJumpCrouch_` | BotCommandRuntime | Botごとのprivate ActorState | BotCommandRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetActorExecutionState | `src/adapter/metamod/plugin_runtime.hpp:641` | `src/adapter/metamod/bot_command_runtime.hpp:96` | compat_actor_command / fake_client_isolation / metamod_hooks / action_adapter / input_dispatcher / jump_crouch_sequencer |
| `managedBotCommandTemplates_` | BotCommandRuntime | Botごとのprivate ActorState | BotCommandRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:644` | `src/adapter/metamod/bot_command_runtime.hpp:97` | compat_actor_command / fake_client_isolation / metamod_hooks / action_adapter / input_dispatcher / jump_crouch_sequencer |
| `managedBotCommandTemplateValid_` | BotCommandRuntime | Botごとのprivate ActorState | BotCommandRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:646` | `src/adapter/metamod/bot_command_runtime.hpp:98` | compat_actor_command / fake_client_isolation / metamod_hooks / action_adapter / input_dispatcher / jump_crouch_sequencer |
| `managedBotActionStopsMovement_` | BotCommandRuntime | Botごとのprivate ActorState | BotCommandRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:648` | `src/adapter/metamod/bot_command_runtime.hpp:99` | compat_actor_command / fake_client_isolation / metamod_hooks / action_adapter / input_dispatcher / jump_crouch_sequencer |
| `movementDiagnosticSamples_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorSampling | `src/adapter/metamod/plugin_runtime.hpp:651` | `src/adapter/metamod/bot_navigation_runtime.hpp:200` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementDiagnosticAttempts_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorSampling | `src/adapter/metamod/plugin_runtime.hpp:654` | `src/adapter/metamod/bot_navigation_runtime.hpp:201` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementDiagnosticUnavailable_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorReadiness | `src/adapter/metamod/plugin_runtime.hpp:657` | `src/adapter/metamod/bot_navigation_runtime.hpp:202` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementResumeFrames_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorReadiness | `src/adapter/metamod/plugin_runtime.hpp:660` | `src/adapter/metamod/bot_navigation_runtime.hpp:203` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementLastDeadFrames_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorReadiness | `src/adapter/metamod/plugin_runtime.hpp:663` | `src/adapter/metamod/bot_navigation_runtime.hpp:204` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementSettledDeadFrames_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorReadiness | `src/adapter/metamod/plugin_runtime.hpp:666` | `src/adapter/metamod/bot_navigation_runtime.hpp:205` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementWarmupFrames_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorReadiness | `src/adapter/metamod/plugin_runtime.hpp:669` | `src/adapter/metamod/bot_navigation_runtime.hpp:206` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementGroundLookaheadRoutes_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:672` | `src/adapter/metamod/bot_navigation_runtime.hpp:207` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementPhysicsSamples_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:675` | `src/adapter/metamod/bot_navigation_runtime.hpp:208` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementReadyLogged_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorSampling | `src/adapter/metamod/plugin_runtime.hpp:677` | `src/adapter/metamod/bot_navigation_runtime.hpp:209` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementGoalDiagnosticSeconds_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorReadiness | `src/adapter/metamod/plugin_runtime.hpp:680` | `src/adapter/metamod/bot_navigation_runtime.hpp:210` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementGateDiagnosticTimes_` | BotCommandRuntime | Botごとのprivate ActorState | BotCommandRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorGateDiagnostic | `src/adapter/metamod/plugin_runtime.hpp:682` | `src/adapter/metamod/bot_command_runtime.hpp:100` | compat_actor_command / fake_client_isolation / metamod_hooks / action_adapter / input_dispatcher / jump_crouch_sequencer |
| `movementTraversalDiagnosticSeconds_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorReadiness | `src/adapter/metamod/plugin_runtime.hpp:685` | `src/adapter/metamod/bot_navigation_runtime.hpp:211` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementTraversalPending_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState / resetCreatedActorReadiness | `src/adapter/metamod/plugin_runtime.hpp:688` | `src/adapter/metamod/bot_navigation_runtime.hpp:212` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementDispatchFrames_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:691` | `src/adapter/metamod/bot_navigation_runtime.hpp:213` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementWasAirborne_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:693` | `src/adapter/metamod/bot_navigation_runtime.hpp:214` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementFallLandingTrackers_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetActorExecutionState | `src/adapter/metamod/plugin_runtime.hpp:695` | `src/adapter/metamod/bot_navigation_runtime.hpp:215` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementReversalDiagnostics_` | BotNavigationRuntime | Botごとのprivate ActorState | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | onActorRemoved / resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:697` | `src/adapter/metamod/bot_navigation_runtime.hpp:216` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementDiagnosticRound_` | BotNavigationRuntime | 共有状態（Bot配列と分離） | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | resetSharedExecutionState | `src/adapter/metamod/plugin_runtime.hpp:698` | `src/adapter/metamod/bot_navigation_runtime.hpp:223` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `movementDiagnosticGlobal_` | BotNavigationRuntime | 共有状態（Bot配列と分離） | BotNavigationRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言・元の条件付き更新で保持（下記寿命表参照） | `src/adapter/metamod/plugin_runtime.hpp:699` | `src/adapter/metamod/bot_navigation_runtime.hpp:224` | compat_actor_command / fake_client_isolation / metamod_hooks / nav_roam_controller / locomotion / jump_drop |
| `originalCommandArgs_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / giveEnginePointers | `src/adapter/metamod/plugin_runtime.hpp:700` | `src/adapter/metamod/plugin_runtime.hpp:192` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `originalCommandArgv_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / giveEnginePointers | `src/adapter/metamod/plugin_runtime.hpp:701` | `src/adapter/metamod/plugin_runtime.hpp:193` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `originalCommandArgc_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / giveEnginePointers | `src/adapter/metamod/plugin_runtime.hpp:702` | `src/adapter/metamod/plugin_runtime.hpp:194` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `clientCommandContextActive_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / dispatchClientCommand | `src/adapter/metamod/plugin_runtime.hpp:703` | `src/adapter/metamod/plugin_runtime.hpp:195` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `clientCommandArgumentCount_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / dispatchClientCommand | `src/adapter/metamod/plugin_runtime.hpp:704` | `src/adapter/metamod/plugin_runtime.hpp:196` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `clientCommandArgv0_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / dispatchClientCommand | `src/adapter/metamod/plugin_runtime.hpp:705` | `src/adapter/metamod/plugin_runtime.hpp:197` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `clientCommandArgv1_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / dispatchClientCommand | `src/adapter/metamod/plugin_runtime.hpp:706` | `src/adapter/metamod/plugin_runtime.hpp:198` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `clientCommandArgs_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / dispatchClientCommand | `src/adapter/metamod/plugin_runtime.hpp:707` | `src/adapter/metamod/plugin_runtime.hpp:199` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `nativeGuardEnabled_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach / armNativeBotGuard / resetNativeBotGuard | `src/adapter/metamod/plugin_runtime.hpp:708` | `src/adapter/metamod/plugin_runtime.hpp:200` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `compatibilityCvarOwned_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / registerCompatibilityCvars | `src/adapter/metamod/plugin_runtime.hpp:709` | `src/adapter/metamod/plugin_runtime.hpp:201` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `compatibilityRegistrationInProgress_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / attach / detach / onServerDeactivate / registerCompatibilityCommands | `src/adapter/metamod/plugin_runtime.hpp:710` | `src/adapter/metamod/plugin_runtime.hpp:202` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `nativeControlsCaptured_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / armNativeBotGuard / resetNativeBotGuard | `src/adapter/metamod/plugin_runtime.hpp:711` | `src/adapter/metamod/plugin_runtime.hpp:203` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `performanceDisablePathSearch_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / synchronizeCompatibilityCvars | `src/adapter/metamod/plugin_runtime.hpp:712` | `src/adapter/metamod/plugin_runtime.hpp:204` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `originalBotEnable_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / armNativeBotGuard / resetNativeBotGuard | `src/adapter/metamod/plugin_runtime.hpp:713` | `src/adapter/metamod/plugin_runtime.hpp:205` | compat_actor_command / fake_client_isolation / metamod_hooks |
| `originalBotQuota_` | PluginRuntime | 共有状態（Bot配列と分離） | PluginRuntimeのみ（他担当は通知・要求） | 既存呼出し時点の値・通知 | constビュー/担当の結果 | 宣言/PluginRuntimeコンストラクター / armNativeBotGuard / resetNativeBotGuard | `src/adapter/metamod/plugin_runtime.hpp:714` | `src/adapter/metamod/plugin_runtime.hpp:206` | compat_actor_command / fake_client_isolation / metamod_hooks |

## 自作ファイル全体の担当

既に責任が明確なCore等は既存実装を維持する。SDK/vendor、ビルド成果物、生成ヘッダーは本棚卸しの改変対象外。

| ファイル | 担当 | 対応方針 |
|---|---|---|
| `src/adapter/metamod/action_adapter.cpp` | BotCommandRuntimeの既存アダプター | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/action_adapter.hpp` | BotCommandRuntimeの既存アダプター | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/astrabot_mm.def` | PluginRuntimeの接続・管理・診断基盤 | 責任分離・境界検証 |
| `src/adapter/metamod/astrabot_mm.map` | PluginRuntimeの接続・管理・診断基盤 | 責任分離・境界検証 |
| `src/adapter/metamod/bot_behavior_runtime.cpp` | BotBehaviorRuntime | 責任分離・境界検証 |
| `src/adapter/metamod/bot_behavior_runtime.hpp` | BotBehaviorRuntime | 責任分離・境界検証 |
| `src/adapter/metamod/bot_command_runtime.cpp` | BotCommandRuntime | 責任分離・境界検証 |
| `src/adapter/metamod/bot_command_runtime.hpp` | BotCommandRuntime | 責任分離・境界検証 |
| `src/adapter/metamod/bot_navigation_runtime.cpp` | BotNavigationRuntime | 責任分離・境界検証 |
| `src/adapter/metamod/bot_navigation_runtime.hpp` | BotNavigationRuntime | 責任分離・境界検証 |
| `src/adapter/metamod/bot_observation_runtime.cpp` | BotObservationRuntime | 責任分離・境界検証 |
| `src/adapter/metamod/bot_observation_runtime.hpp` | BotObservationRuntime | 責任分離・境界検証 |
| `src/adapter/metamod/bot_runtime_services.hpp` | アダプター内部の読み取り契約/共通値・純粋ヘルパー | 責任分離・境界検証 |
| `src/adapter/metamod/bot_runtime_support.hpp` | アダプター内部の読み取り契約/共通値・純粋ヘルパー | 責任分離・境界検証 |
| `src/adapter/metamod/bot_runtime_types.hpp` | アダプター内部の読み取り契約/共通値・純粋ヘルパー | 責任分離・境界検証 |
| `src/adapter/metamod/compat_surface.cpp` | PluginRuntimeの接続・管理・診断基盤 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/engine_random_source.cpp` | PluginRuntimeの接続・管理・診断基盤 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/engine_random_source.hpp` | PluginRuntimeの接続・管理・診断基盤 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/fake_client_manager.cpp` | PluginRuntimeの接続・管理・診断基盤 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/input_dispatcher.cpp` | BotCommandRuntimeの既存アダプター | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/join_controller.cpp` | PluginRuntimeの接続・管理・診断基盤 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/map_fingerprint.cpp` | ナビゲーション入出力 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/movement_execution_gate.cpp` | BotCommandRuntimeの既存アダプター | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/native_bot_guard.cpp` | PluginRuntimeの接続・管理・診断基盤 | 責任分離・境界検証 |
| `src/adapter/metamod/nav_loader.cpp` | ナビゲーション入出力 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/observation_adapter.cpp` | BotObservationRuntimeの既存アダプター | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/observation_adapter.hpp` | BotObservationRuntimeの既存アダプター | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/plugin_exports.cpp` | PluginRuntimeの接続・管理・診断基盤 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/plugin_runtime.cpp` | PluginRuntimeの接続・管理・診断基盤 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/plugin_runtime.hpp` | PluginRuntimeの接続・管理・診断基盤 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/profile_loader.cpp` | 行動設定の入出力 | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/regamedll_observation_provider.cpp` | BotObservationRuntimeの既存アダプター | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/regamedll_observation_provider.hpp` | BotObservationRuntimeの既存アダプター | 既存契約維持/関連時のみ変更 |
| `src/adapter/metamod/runtime_profiler.cpp` | PluginRuntimeの接続・管理・診断基盤 | 既存契約維持/関連時のみ変更 |
| `src/core/behavior/behavior_state.cpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/behavior/objective_proposal.cpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/build_identity.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/combat/combat_intent.cpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/combat/weapon_state.cpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/compat/bot_configuration.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/compat/buy_controller.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/compat/command_registry.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/compat/cvar_state.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/compat/observation.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/compat/profile_catalog.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/compat/random_source.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/compat/runtime_mode_policy.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/compat/state_machine.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/nav/jump_drop.cpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/nav/legacy_nav_reader.cpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/nav/locomotion.cpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/nav/nav_model.cpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/nav/nav_query.cpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/nav/nav_snapshot.cpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/nav/special_traversal.cpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/objectives/objective_state.cpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/objectives/round_objectives.cpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/perception/perception.cpp` | 観測・世界モデル契約（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/runtime/actor_registry.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/runtime/bot_command.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/runtime/bot_timing_scheduler.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/runtime/command_queue.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/runtime/jump_crouch_sequencer.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/runtime/lifecycle.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/runtime/movement_physics.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/runtime/nav_roam_controller.cpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `src/core/team/radio_intent.cpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/team/round_objective_coordinator.cpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/team/team_report.cpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `src/core/world/world_snapshot.cpp` | 観測・世界モデル契約（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/behavior/behavior_state.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/behavior/objective_proposal.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/build_identity.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/combat/combat_intent.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/combat/weapon_state.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/bomb_guard.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/bomb_navigation.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/bomb_search.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/bot_configuration.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/buy_controller.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/combat_behavior.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/command_registry.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/cvar_state.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/grenade_trajectory.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/navigation_step.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/observation.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/outgoing_radio.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/profile_catalog.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/public_bomb_clock.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/public_bomb_radar.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/public_observation.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/random_source.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/random_trace.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/runtime_mode_policy.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/scenario_behavior.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/smoke_observation.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/state_machine.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/teamwork_policy.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/compat/weapon_actions.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/abi_contract.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/compat_surface.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/fake_client_manager.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/input_dispatcher.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/join_controller.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/map_fingerprint.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/movement_execution_gate.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/native_bot_guard.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/nav_loader.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/profile_loader.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/round_lifecycle_tracker.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/metamod/runtime_profiler.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/nav/danger_memory.hpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/nav/jump_drop.hpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/nav/legacy_nav_reader.hpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/nav/locomotion.hpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/nav/nav_model.hpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/nav/nav_query.hpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/nav/nav_snapshot.hpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/nav/special_traversal.hpp` | Navigation Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/objectives/bomb_site_route_selection.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/objectives/objective_state.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/objectives/round_objectives.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/perception/perception.hpp` | 観測・世界モデル契約（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/runtime/actor_registry.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/runtime/bot_command.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/runtime/bot_timing_scheduler.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/runtime/command_queue.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/runtime/jump_crouch_sequencer.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/runtime/lifecycle.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/runtime/movement_physics.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/runtime/nav_roam_controller.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/runtime/player_avoidance.hpp` | 既存Core/公開契約（契約維持） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/team/radio_intent.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/team/round_objective_coordinator.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/team/team_report.hpp` | Behavior Core（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `include/astrabot/world/world_snapshot.hpp` | 観測・世界モデル契約（SDK非依存） | 既存契約維持/関連時のみ変更 |
| `tests/action_adapter_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/actor_registry_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/behavior_state_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/bomb_navigation_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/bot_configuration_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/build_identity_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/combat_intent_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/command_queue_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/compat_actor_command_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/compat_command_registry_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/compat_cvar_state_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/compat_state_machine_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/compat_surface_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/compatibility_rng_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/engine_random_source_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/fake_client_isolation_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/fake_client_manager_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/grenade_trajectory_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/input_dispatcher_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/join_controller_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/jump_crouch_sequencer_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/jump_drop_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/legacy_nav_reader_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/lifecycle_generation_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/locomotion_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/map_fingerprint_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/metamod_abi_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/metamod_hook_table_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/movement_execution_gate_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/movement_physics_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/native_bot_guard_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/nav_danger_memory_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/nav_diagnostics_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/nav_external_performance_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/nav_legacy_limit_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/nav_loader_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/nav_model_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/nav_query_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/nav_roam_controller_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/nav_snapshot_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/objective_proposal_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/objective_state_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/observation_adapter_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/observation_model_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/observation_trace_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/outgoing_radio_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/p076_performance_contract.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/p076_production_runtime_docs.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/perception_adapter_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/perception_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/phase6_verification.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/phase7_scenarios.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/phase8_action_adapter_boundary_test.ps1` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/phase8_differential.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/phase8_live_log_tests.ps1` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/phase8_objective_gate_test.ps1` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/phase8_slow_movement_gate_test.ps1` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/player_avoidance_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/profile_catalog_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/profile_loader_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/public_bomb_clock_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/public_observation_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/radio_intent_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/regamedll_observation_provider_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/round_objective_coordinator_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/round_objectives_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/runtime_characterization.hpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/runtime_characterization_contract.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/runtime_mode_policy_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/runtime_movement_integration_contract.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/runtime_ownership_contract.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/runtime_profiler_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/runtime_timing_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/scenario_behavior_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/source_manifest_checker_tests.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/special_traversal_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/team_report_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/teamwork_policy_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/test_compare_parity_traces.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/vanilla_behavior_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/vanilla_management_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/weapon_actions_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/weapon_state_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/world_snapshot_tests.cpp` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |
| `tests/x86_artifact_verifier_tests.py` | 検証（製品状態の変更権限なし） | 既存契約維持/関連時のみ変更 |


| `CMakeLists.txt` | ビルド定義（DLLと3統合テストへ同じ4実装を追加） | x86/C++14、SDK分離、既存exportを維持 |
| `cmake/AstraBotBuild.cmake` | 既存コンパイラー/SDK/成果物契約 | 変更なし |

## 検証と制限

- 同一スタブEngine入力・同一ツールチェインで乱数要求、コマンド、状態記録を分離前後で比較する。
- callbackの時刻/順序、actor/map/round/goal/path世代、Unknown/推定の区別を維持する。
- 落下再試行・姿勢・終点・上下階判定などの既知不具合は別作業。現行比較記録を正解仕様へ昇格させない。
- 完全Windows/Linux x86ビルド・CTestとPE/ELFのexport検証は別々のゲートとして記録する。
- HLDS/ReHLDSでの実機受入れ、配備、設定変更は今回実行しない。

比較基準は`tests/fixtures/runtime_characterization/provenance.json`と圧縮記録に保存する。
Windows/Linuxそれぞれ132,153記録。乱数の型/範囲/順、Engine provider呼出し、状態結果、最終コマンドを同一fixtureで比較する。
これはfixtureが到達する挙動の同値性であり、全状態空間やCSBot実機との同値性の証明ではない。
構造検査・比較検査はCMake/CTestへ登録済み。書式に依存した補助検査も担当ファイルを参照する。
比較記録は全引数を網羅しない。例えばindexOfEdictは呼出し名のみ、traceは選択した座標のみを記録する。未記録の引数・fixture未到達の分岐はソース照合と別の受入れで補う。
code-review-graphは古いHEAD/不足するedgeがあるため索引の0件を影響なしの根拠にせず、FocalSpanは探索に限定し、現行ソースとビルド・比較記録で照合する。
