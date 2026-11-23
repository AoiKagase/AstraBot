# 23　CSBot参照実装との差分監査

## C02-2b 手榴弾視線中の音要求・抑制時計を部分修正（2026-11-23）

Windows176/176＋PE7、Linux175/175＋ELF7合格。最終DLL `257a64a4530f…` を既存実機へ再配備し、公開の危険銃声が視線受付で拒否されてもRNGと2〜4秒の抑制時計が進むことを読取りCDB・実入力で確認した。C4実機0/7/8も逐次合格、追加サーバーコピー0。Hunt/Guardの後続順・stationary0 Clear・moving encounterはC02-2cとして残し、49歴史項目／47残件を維持する。詳細は [GRENADE_NOISE_ADMISSION_C02_2B.md](GRENADE_NOISE_ADMISSION_C02_2B.md)。以下のC02-2a/C02-1は当時の範囲を保持する。

## C02-2a 手榴弾視線中の破壊物割込みを部分修正（2026-11-23）

Windows175/175＋PE7、Linux174/174＋ELF7合格。最終DLL `6d662b9aee0a…` を既存実機へ再配備し、実マップ破壊物のTouchで手榴弾視線が上書きされないこと、保持入力と装備要求が残ることを読取りCDBと実入力で確認した。C4実機0/7/8も逐次確認。追加サーバーコピー0。C02は部分対応、49歴史項目／47残件を維持する。noise/Hunt/Guard等の受付とRNG、経路投擲、C10の共通装備時計・接触処理時刻は未完。詳細は [GRENADE_OBSTACLE_ADMISSION_C02_2A.md](GRENADE_OBSTACLE_ADMISSION_C02_2A.md)。以下のC02-1記録は当時の範囲を保持する。

## C02-1 手榴弾保持・認識敵への投擲を部分完了（2026-11-23）

Windows174/174＋PE7、Linux173/173＋ELF7合格。最終DLL `77b92327d98d…` を既存実機へ再配備し、敵不在の保持38入力、敵認識後の待機と接触喪失後のstrict 3秒解放を読取りCDB/実入力で確認した。C4実機0/7/8も直列確認。サーバーコピー追加0。C02は部分対応のまま残す。低優先度視線の受付/RNG・障害物との競合（C02-2）、安全時間終了後の経路向け投擲（C02-3）、C08の照準所有は未受入れ。49歴史項目／47残件。詳細は [GRENADE_LIFECYCLE_C02_1.md](GRENADE_LIFECYCLE_C02_1.md)。

## C16 full移動の視線公開を限定完了（2026-11-23）

Windows173/173＋PE7、Linux172/172＋ELF7合格。最終DLL `456ccc43…95610` を既存実機へ再配備し、同一Actor・実ゲーム時刻のfull移動598/598で共有視線と実RunPlayerMoveの一致を確認した。C16を限定完了へ更新。49歴史項目／47残件。C08・全参照リプレイ・両OS実機/継続性能は未完。詳細・指紋・除外区間は [FULL_LOOK_PUBLICATION_C16.md](FULL_LOOK_PUBLICATION_C16.md)。次はC02の手榴弾開始/投擲経路。

## C01共有角速度の限定完了（2026-11-23）

Windows173/173＋PE7、Linux172/172＋ELF7合格。既存実機の最終DLLで2000 step、327非ゼロowner変更、164同時刻置換、1843実RPP角一致を確認した。C01を限定完了へ更新した。共有resultと最終RPPの不一致86件を記録し、既存full公開経路を新残件C16として追加した。C08/V01/V02を閉じない。C01時点では49歴史項目／48残件。詳細・限界・SHA・証拠は [SHARED_LOOK_C01.md](SHARED_LOOK_C01.md)。当時の次単位C16は後続で限定完了（[記録](FULL_LOOK_PUBLICATION_C16.md)）。現行の次単位はC02。


## 2026-11-22 全ソース監査による現行判定

従来の残件5群には具体的な漏れがあった。製品ソース198本をファイル単位で照合した現行正本は [FULL_SOURCE_AUDIT.md](FULL_SOURCE_AUDIT.md)、全パス・SHA・担当・根拠と残件IDは [FULL_SOURCE_INVENTORY.json](FULL_SOURCE_INVENTORY.json)。一般GameDLL全行や全分岐の一致を保証する監査ではなく、full/focused/依存棚卸しを区別した。

残件は、視線/戦闘/武器、状態/目的/人質/VIP、NAV/移動/知覚、公開観測/イベント/発話/管理/profile、固定参照RNG/両OS実機/継続性能の5群へ具体化した。C01の共有ばねは限定完了（[記録](SHARED_LOOK_C01.md)）。full公開C16も後続で限定完了（[記録](FULL_LOOK_PUBLICATION_C16.md)）。次単位はC02。実機は既存再配備、隔離が必要でも固定1コピーを再利用する。

旧記録のCRT音声shuffle（21）とLinux胸部照準の既知失敗（22）はオフライン解消へ更新した。ClearLook、早期Plant判断、MoveTo→Plant順、Plant入場NAV入力/Run、Plant前Idle経路抑止、cold Idle/Buy視線供給も完了した限定範囲を現行未完一覧から外す。未実装だった視認/音/状態/発話等のP00/P04/P06記録は、その後の実装と区別する。過去の失敗・当時の実行結果は履歴として保持する。

この2026-11-22文書監査時点の製品変更・build/CTest再実行・実機起動/再配備・サーバーコピーは0。既存最終製品と合格ログの同一性、ソースとユーザーファイルの保持、全ファイルの担当/根拠/リンクを検証した文書監査である。過去の実機smokeを各新規差分の受入れへ流用しない。pushなし。

## 通常Idle/Buyの初回からの視線供給（2026-11-22）

基点 `b75be3a`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。`cs_bot_update.cpp:135–183` のNOT_LOOKING_AT_SPOTは毎Upkeepに保持Ahead・pitch0を供給し、drift後にUpdateLookAnglesを一度積分する。従来adapterはClearLook後だけ既定lookを再開し、coldなIdle/Buyでは供給されなかった。

互換Idle/Buyで初回ownerを初期化し、実managed Actor/entity/generationとserial/lifecycleに結び付ける。優先注視・Attack・blind・stop/freezeを既存境界で尊重し、同時刻は一度だけ積分する。neutral fullによる再接続では現在の既定springを保持し、実UseEntity解放時だけそのspringを引き継ぐ。梯子のfullでは既定ownerを解除し、次lightで既定と梯子の二重積分を行わない（参照 `cs_bot_vision.cpp:93–159`）。全Combat/Guard/梯子ownerの角速度統一を完了とはしない。

正規回帰はcold Idle/Buyの実cached RunPlayerMove、同時刻重複、freeze、serial失効後の復活抑止、新neutral fullの速度保持を確認した。実cached対照が再接続時のspringゼロ化をREDで検出し、実梯子対照が競合ownerをREDで検出した。最終重点7/7合格。既存Plant敵無視対照は実FOV内の敵を-45度に置き、20軽更新の全てでPlant/Attack保持、終点で敵方向への接近を確認する。既定Ahead0や後方の床注視では通過できない。Defuse入場はUpkeep後・NAV前の実視線をtraceで採録し、移動軸と次lightの旋回を検証する。胸部照準は射線を味方が塞がない対照の実発砲時pitchを判定し、長い非発砲待機後の既定driftと混同しない。履歴fixtureのタイミングは変更していない。

最終全targetビルド後、Windows x86 **171/171・PE7**、続いてLinux x86 **170/170・ELF7**を直列で確認。DLL SHA-256 `1bdef02f4a3086c5d12c10ee4632ea6de40395fcd72ec17f721d063cc37f1807`、PDB `ead662289491d33e066a57c391ab9b7ecadc4862d200f743e1ffd313549d3a27`、ELF `705c14bbb9eb31d5091696f0d50f1ca54287b70d533bf8d6ecbe5f7b74603557`。candidate1は最終梯子修正前のためオフライン・実機とも除外して保持。最終v2の補正前170/171は履歴差だけであり、最終合格ではない。Ubuntuのcmake不在試行も除外し、既存Debianで全ビルドした。

固定baselineは再生成せず、親補正後151999行を維持する806個のexact offset補正を追加した。差812行はmove779、ground lookahead9、avoidance floor24。moveのactor/roll/up/msecと方向以外のbutton bit、probeの種類/件数/Zは不変。変更フィールド件数はpitch744・yaw428・forward12・side27・方向buttons36。保持Aheadからの角度と既存の水平投影・floor probe経路に整合する。残り151187行とRNG510行は同じ位置/順序/値で完全一致し、各OSのbefore/after SHAと全byte再構成を検証した。任意の移動差を許容するマスクではない。製品差・試験前提・raw履歴はread-onlyレビュー済み、重要な指摘なし。

最終DLL/PDBの隔離HLDSは0/7/8/11と読取りCDB0を直列実行し、全実PID終了0・forced=false。Idleのpath0/input0を維持し、通常/自然NAVの設置、区域外、実dropを確認した。DLL module-load後のCDBは最初のIdle→Plant `10.49649239` 秒/full `0x3e` の保持Ahead0と、初期化済み・有効な通常look ownerを直接採録した。cold Idle/Buyの連続cached角度/springの厳密な回帰はオフライン証拠、実機はこの入場前ownerと経路/入力/設置の受入れとして区別する。明示C4在庫bit同期を含む隔離fixtureであり、標準GameDLL自然拾得・実水没・全軌跡同一性へ拡張しない。

証拠は `artifacts/local/debug/csbot-idle-buy-look-supply-20261122/evidence-final-v2/acceptance.json` と同ディレクトリの最終ログ・raw履歴監査・実機manifest。元の失敗・旧候補も上書きしていない。push・実環境配布なし。

当該修正単位完了時の残件要約（履歴。現行正本は [全ソース監査](FULL_SOURCE_AUDIT.md)）：

1. Combat/Guardを含むowner間の角速度共有。
2. C4初回未装備と実水没条件の直接受入れ。
3. Hunt/NAV、同一area/fallback、味方待機、cs_assault梯子、経路失敗と同条件timeout比較。
4. 視認敵をAimせず通過する問題、旋回カクつき、多人数継続性能。
5. 標準GameDLL公開観測経路と未網羅のCSBot差分。

参照は ReGameDLL-CS `b0889847fe6d03898be88acc9e366660efb40ab5`。AstraBotの当初の監査起点は `8662286`。現行全ソース監査の基点は先頭の記録を参照。公開エンジン情報と所有するBotの状態で再現できる範囲を、現行の実装・テストと照合した。

## 全13状態の照合

CSBot側のファイルは `regamedll/dlls/bot/states/cs_bot_<略称>.cpp`。AstraBot側の状態遷移は `src/core/compat/state_machine.cpp` に集約されている。

| 状態 | CSBot | AstraBotで確認した経路・残件 |
|---|---|---|
| Attack | `attack` 32、156、276–308、565行 | 攻撃オーバーレイと戦闘更新。盾所持・実際の閉状態と遅延認識した敵のリロードを公開観測し、継続開閉・入力後の期限反転・終了時の開要求を実装。通常のFFA索敵と敵の盾を考慮した脅威選択は残件。 |
| Buy | `buy` 48、238、542行 | 購入コントローラ・公開購入エリア・武器プリセット・盾購入経路は実装済み。Career/購入進入時刻/在庫観測と実機はS11に残る。 |
| DefuseBomb | `defuse_bomb` 32、40、91行、`cs_bot_event.cpp` 221–262行、`cs_bot_pathfind.cpp` 1296–1328、1399–1458行、`game_shared/bot/bot.cpp` 71行 | 設置・近距離ビープによるCTの記憶、最終区間の歩行を照合。解除へ向かうMoveToは現在視線で移動軸を決め、次の軽更新で経路先読みに向く。解除進入では経路視線を保持し、最初のUSE更新でC4への視線を設定する。隔離ReHLDSで通常設置から解除開始・完了まで確認。 |
| EscapeFromBomb | `escape_from_bomb` 31、40、68行 | 退避・武器持ち替え、終端後のcache解除と次更新の逃走先再選択は現行実装あり。選定metric/実機同条件比較は全ソース監査に従う。 |
| FetchBomb | `fetch_bomb` 32–76行、`hunt` 47–81行、`idle` 205–219行、`cs_bot.cpp` 513行、`cs_bot_pathfind.cpp` 1608–1741行 | Tの専用状態で落下C4への安全経路を保持し、拾得・消失・移動終了でIdleに戻る。経路作成失敗時はHuntを経由する。Idleの攻撃側狙撃抽選の優先順位と、視認もレーダー受領もないTでの再現は残件。 |
| Follow | `follow` 32、103、214、220、287行。`cs_bot_pathfind.cpp:1582`、`:1608`。`cs_bot_statemachine.cpp:224`、`cs_bot_hide.cpp:115` | 周辺NAVの候補選択と経路計算を既存経路の進行後に行う。状態進入時のアイドル乱数、外側0.5秒と共有0.4–0.6秒の再試行待ち、最後のポータルに沿う接地終点、到達不能時の最寄り到達可能領域への部分経路を実装。停止時は遮蔽への経路を計算してからHideに入り、その経路を保持する。再試行の抑制中はFollowを維持する。Hideへの遷移と乱数の順序、リーダーの再移動で追従を再開する条件を回帰テストで確認。 |
| Hide | `hide` 33、71、452行 | 守備、被弾による解除、C4や救出地点の警備を確認。警備先の全選定条件の比較は残件。 |
| Hunt | `hunt` 32、47、219行 | 状態の30秒境界とローミング。参照版の巡回先とC4分岐の網羅照合は残件。 |
| Idle | `idle` 37、52行 | 状態のリセット、Huntへの遷移、シナリオの優先順位。狙撃地点の割り当ては残件。 |
| InvestigateNoise | `investigate_noise` 52、58、135行 | 音源の接地判定とNAV距離を修正。記憶の消費、接近、再選択は既存実行時テストで確認。 |
| MoveTo | `move_to` 32、66、317行 | Plant発話→経路更新→非PROGRESSING時のzone/C4再確認→Plant入場を照合。互換旧PlantAttemptの先行装備/停止を除去。Plant入場の期限切れNAV入力/Duck/Runとcallback失効は入場receiptで検証済み。全タスク遷移順とHunt/NAV全体は残件。 |
| PlantBomb | `plant_bomb` 32、45、75行、`cs_bot_weapon.cpp:411–470` | 公開弾薬・許可クラス・水没条件と共通5秒装備時計を使用。末尾はPrimaryAttack/即時SelectC4→所持喪失→strict5秒超、退出前EquipBest→起立→ResetStuck→Engage→注視取消の順へ接続した。喪失による新Hide timestampを古いPlant ageで上書きしない。通常設置・区域外timeout・実C4ドロップでAKに残る最終Attackと次fullまでの保持、退出fullのUpkeep実角保持を確認。次light通常look再開とIdle前段NAV抑止は後続修正で限定完了。共有角速度C01とfull公開C16は後続で限定完了、私有複数slot先頭C12、初回未装備/水没C14は残件。 |
| UseEntity | `use_entity` 33、38、69行、`player.cpp:9415` | 状態に入ったときの人質を保持する。コマンド更新ごとに視線を動かし、現在のyawとpitchが両方とも許容角20度未満のときにUSEを送る。コマンドが受理されると試行を記録してIdleへ移る。ネイティブの人質護送カウンターと追従者の照合は残件。 |

## ClearLook後の軽更新（2026-11-22追補）

固定 `cs_bot.h` のClearLookAtはspot状態/説明だけを消し、`cs_bot_update.cpp:120–183` と `cs_bot_vision.cpp:57–166` が残存Aheadと角速度から次Upkeepを積分する。command runtimeは優先注視へ入る前の実ばねと既定Aheadを分けて保持し、解除後の通常lightへ引き継ぐ。Aheadの供給は元方向からengine VecToYaw相当の整数角、round resetではAhead保持/速度0、新規個体ではAhead0。通常UseEntity/Defuse解除、新しい優先owner、scope/Enhancedの境界も回帰で確認した。

Windows170/170・Linux169/169、PE/ELF公開7関数と直列隔離実機11試行を確認。検証済みCDBの実goal/spring-beforeから次の実viewを独立計算し、誤差.0001度未満。直前のPlant退出full/末尾入力の修正を保つ。詳細はSTATUS先頭と `artifacts/local/debug/csbot-clear-look-upkeep-20261121/README.md`。

上のPlant行に残していた「次lightの通常look-goal/spring再開」は、このowner解放後の復帰範囲で確認済み。当時残ったPlant前path/Ahead75差とcold Idle/Buy供給は後続95aec579/b75be3a/6fdadbdで限定完了。全Combat/Guard owner間の単一速度C01は後続で限定完了。full公開C16は後続で限定完了。絶対視線軌跡/更新順C08は残る。

## Plant入場判断の優先順位（2026-11-22追補）

固定 `b088984` の `states/cs_bot_idle.cpp:52`以降はNAV/ゾンビ/round/追従の前提を確認し、C4所持・zone内なら直接Plantと発話、区域外で共有期限後なら最寄りzoneへMoveTo。`cs_bot_follow.cpp`はleaderが有効ならzone内で直接Plantする。`cs_bot_hunt.cpp:47`以降は30秒超、または共有期限/zone内かつ実敵接触からstrict>3秒でIdleへ戻りreturnする。`cs_bot_manager.h:172`の共有期限は>=。`cs_bot_update.cpp:441–446`は取得済み敵が可視なら、Attack中でなくても最後の接触時計を更新する。候補を見ただけの時計とは異なる。

今回のadapterはこの早期分岐をTeamwork・objective target・NAV移動より前へ接続した。Idleから直接Plant、有効leader Follow陽性、Hunt→Idle→次更新のPlant/期限後site選択を正規実行で確認。公開 `client.cpp:5108–5109`の `UpdateClientData.iuser3` bomb-zone bitを優先し、API既知falseをgeometryで覆さず、不明時のregistry発見を到達可能にする。Buy上位要求で入場receipt/chatterを取消す。core任意ガードはcallback後の同一Actor/full-update/epochを検証し、ResetPath後の旧taskとOnExit後の旧ClearLookを再入先へ持ち越さない。

最終v2はWindows171/171+PE7、Linux170/170+ELF7、隔離HLDS0/7/8と読取専用CDB7が合格。初回Plant10.50985813秒の保持Aheadを最終DLL/PDBから0°と直接採録し、旧75°差を解消した。旧段落の「Plant進入前path/Ahead」は不要なHunt供給を除去したこの範囲で確認済みとなる。当時未完だった直前Idleのpath1準備はb75be3a、MoveToの経路更新後のPlant判断は6546606で限定完了。目標選定/RNG差S01/S02は別件。標準GameDLLでの直接受入れ、全視線ownerの共有速度、全経路/RNG同等性へ拡張しない。詳しい試行・指紋・履歴補正はSTATUS先頭とTRACE_REPLAYの同日追補を参照。

## 周辺機能

| 分野 | 照合箇所と結果 |
|---|---|
| NAV | `game_shared/bot/nav_area.cpp:4318,4773,4823`。音源とChatterで接地の規則を共有し、通常の床検索をスナップショット単位の300ユニットの空間索引に変更した。メッシュ外での最近傍検索は参照版の定義順を使う。 |
| 経路コスト | `cs_bot_listen.cpp:60`、`cs_bot.h:1577`、`game_shared/bot/nav_area.h:860`。音源との高低差が120ユニットを超える場合に、最後のNAVエリアから最も安全な経路を引き、はしごと参照版のエリア中心を使って距離を合計する。Helpは既に同じNAV距離APIを使う。 |
| 戦闘・武器 | `game_shared/bot/bot.h:431`、`states/cs_bot_attack.cpp:276–308,467,578–580`、`cs_bot_weapon.cpp:849`。FFAで視認できる同チームの加害者に再照準し、その相手への発砲を許可する。本人の盾継続制御は実装済み。一般のFFA索敵、FFA以外の `mp_friendlyfire` 判定と発砲線、敵の盾を考慮した脅威選択は残件。 |
| 管理・コマンド | `cs_bot_manager.cpp:423–512`。4種の武器プリセットが公開CVarに反映される。個別kickのfillモードと、対象が空の場合を含む全体kickでのquotaを修正。自動quota調整の死亡Bot優先と生成失敗時のquota補正は残件。 |
| プロファイル | `game_shared/bot/bot_profile.h:133–170`。各テンプレートは `Default` から生成し、適用時は `Default` と異なる値を順番に継承する。武器候補の継承は `Default` との件数差で判定する。 |

実参照での判断・イベント・乱数順の再生は24、実機の照準・多人数FPS・ラグの受入れは25で扱う。

## MoveToのPlant開始順序（2026-11-22追補）

基点 `95aec579`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。CSBot MoveToの発話は経路更新より前に行い、UpdatePathMovementがPROGRESSING以外になってからzone内・C4所持ならPlantへ入る。互換adapterのphaseだけによる早期要求を除去し、進行中／観測不能はMoveToを保持、終端・失敗時には公開zoneとC4所持を取り直す。Actor/edict serial・frame/time・full-update・revision・normal/Attack epoch・target generationを検証し、callbackによる再入後は旧NAV終端処理を続けない。新しいPlantのOnUpdateは同更新で呼ばない。床の視線点は入場時の実yawから設定する。

通常Plant専用処理がある互換モードで旧PlantAttemptを開始しない。これにより、MoveTo中のphase到達だけでC4を選択し移動を停止する裏経路も除去した。正規fixtureのREDでこの停止を確認後に修正した。発話のstrict >2秒境界は維持し、到着fixtureはNAV区間を通過し、actor hullを含め区域外に保った後で2秒ちょうど／2.125秒に到着する。防衛fixtureのC4除去はPlant到達後、後半のFollow移動検証は新しいPlant報告が割り込まない構成にする。自然Follow成立直後の別ケースでFollow→Plant陽性も保持した。

最終オフラインはWindows x86 **171/171・PE7**、終了後の単独Linux x86 **170/170・ELF7** 合格。DLL SHA-256 `f176ab5079cc2e6b4debbe27c64e95c76d73b3675c727ee30bbb5661d1dbc003`、ELF `609201ff9e46859d202be10691f3f6bf4d6cfdf513949f7461480e1093860aef`。製品・PDB・採録・保持指紋は `artifacts/local/debug/csbot-move-plant-order-20261122`、全ログと実行手順は `C:/Temp/astrabot-move-plant-order`。一時LNK1163は既存incremental .ilkを証拠へ退避して再リンクし、ソース/SDK/cache設定変更は行っていない。

最終DLLの隔離HLDS0/7/8/11と読取専用CDB11は全て実PID終了0・forced=false。0は14.546655秒、自然NAV11は74.637466秒に実設置成功。7は11.629729秒に区域外へ移動、8は11.489011秒に実drop entity99・hasC4=0。CDB11はmodule-load後にBPを置き、60.29094315秒・full0x1d9でNAV終端outcome2→旧MoveToからPlantの順を直接採録した。固定nativeは実UpdatePathMovementのPATH_FAILURE=2→Plantを58.207436秒に採録。旧95aec579は64.39331818秒に経路評価前のPlant入場を採録した。軌跡や絶対時刻を同値とは主張しない。fixtureの明示的C4在庫bit同期は残り、自然拾得や標準GameDLL全体の受入れではない。

固定履歴baselineは再生成せず、両OS同じ19か所のoffset/前後hash限定補正で152248→152258行を全行照合。終端後の公開zone再観測で32行増え、旧PlantAttemptの重複geometry走査を除去して22行減る。RNG510行は各OSの親履歴と順序・値とも全一致。途中候補・失敗CDBは最終合格に流用せず保持する。

## Plant入場更新のNAV入力・Runとcallback失効（2026-11-22）

基点 `6546606`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。参照の経路進捗5秒期限切れは移動入力を作った後にRun/StandUp/DestroyPathを行いPATH_FAILUREを返す。続くPlant OnEnterはしゃがみ・自己防衛・床視線を設定し、既存の移動/ジャンプを消さない。C4選択・PrimaryAttack・通常の移動停止は次のPlant OnUpdateに属する。

互換MoveTo/Plant taskに限定して期限切れで既に作ったNAV intentを保持する。Actor/edict serial・frame/time・full-update・revision・normal/Attack epochが一致するNAV入場receiptだけ、plugin終端中立分岐を通さずmovement commandへ渡す。入場更新は既存入力にDuckと床視線を加え、C4選択・手榴弾取消しを先行しない。期限切れRunは近い最終areaの余分な0.4倍歩行を抑止し、次fullでreceiptとともに消える。EndOfPathで入力がゼロなら前のコマンドは捏造しない。draft作成前に入場epochを保存し、draft後・action準備後に保存epochと完全なreceiptを再検査して、callback後の旧NAV入力を破棄する。

実onStartFrame回帰は4area内の局所移動で速度stuckを避け、strict >5秒の経路期限切れ→PathFailure→Plant→実RunPlayerMoveの移動+Duck、入場C4選択なし、次fullの停止/C4選択1回を確認する。別の実コマンド対照でJumpと最終areaのRun速度を確認する。追加回帰はpost-entry collectActorのtrace callbackで同じActorをPlant→Huntへ正規遷移させ、古いNAVコマンドが送られないことを確認する。修正前 `red-entry-callback.log` はcallback発火の前提合格後に旧コマンド送信だけ失敗し、`green-entry-callback-isolated.log` は重点3/3合格。旧fixtureの時刻使切り・2area/区域外site・後続goalcache汚染を含む試行は合格証拠から除外した。

最終v2オフライン：Windows x86 **171/171・PE7**、Linux x86 **170/170・ELF7**。全targetをビルド後、CTestはWindows→Linuxの直列。DLL SHA-256 `1db9c898ad18a8adfce9f9e2594fc4b76347f8b0311c5aae87a0d6edb54322e0`、PDB `3960ba34acd4b34cfde2836f2c55269f983653894ff23cd14c5716d5c842b2e3`、ELF `ef0852bf37f0aa0a8a6adf077516adeea9f555d2b52531ebc6229869ce85eee5`。v1候補はcallback対策前として別保存し、最終合格に流用していない。初回v2 ELFの改行不備とリンク途中の読取りは除外し、完了製品を独立検証した。

固定DLL/PDBの隔離実機0/7/8/11と読取りCDB11は、次の起動前に実PID終了0・forced=falseを逐次確認した。

- final-v2-natural-0: PID36436 exit0/forced=false; plant-fixture planted time=14.653230; plant-fixture complete mode=0 planted=1
- final-v2-natural-7: PID39116 exit0/forced=false; terminal-fixture leave-zone now=11.544034 has=1; plant-fixture complete mode=7 planted=0
- final-v2-natural-8: PID2300 exit0/forced=false; terminal-fixture actual-drop now=11.745628 entity=99 has=0; plant-fixture complete mode=8 planted=0
- final-v2-natural-11: PID43620 exit0/forced=false; plant-fixture planted time=63.578010; plant-fixture complete mode=11 planted=1
- final-v2-route-debug-11: PID35256 exit0/forced=false; plant-fixture planted time=63.467300; plant-fixture complete mode=11 planted=1

CDBはDLL module-loadを待ってBPを設定。最初のPlantは `59.37293243` 秒/full `0x1d3`、MoveTo→EndOfPath2→Plant。生成NAV入力ゼロのためDuck4/AKで入り、その後C4へ移る。固定nativeのPATH_FAILURE時buttons14とは終端結果が異なり、同条件14対4比較とは主張しない。タイムアウト入力保持・callback失効は実フレームのオフライン回帰、実機は到着/区域外/実drop/設置の受入れとして区別する。fixtureの明示的C4在庫bit同期は残る。

固定baseline・既存補正を再生成せず、両OSとも親の152258行と全byte一致。RNG510行も各OSの親と順序・値まで一致し、新規補正なし。既存のOS間丸め差は残る。証拠と指紋は `artifacts/local/debug/csbot-plant-entry-nav-input-20261122/evidence-final-v2/acceptance.json`、製品は同root `final-v2/`、raw試行は `C:/Temp/astrabot-plant-entry-nav-input`。push・実環境配布なし。

## Plant前Idleの不要な経路準備（2026-11-22）

基点 `a924873`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。参照Idle OnEnterはDestroyPathを行い、次のIdle OnUpdateが次状態を選ぶ。Buy→Idleが完了した更新で後続状態の経路更新を先行しない。旧最終実機ではTのBuy終了更新がIdleのままpath1と前進88.4を作っていた（前単位final-v2-natural0、frame817）。Hunt→Idleの設置待ちは既存receiptで止まっており、これと混同していない。

互換モード・非Attack IdleではNAV用objective target選択を呼ばず、movementSuppressedによってground lookaheadとcore経路処理を抑止する。known NAV areaはstate更新前のobserveFullUpdateMotionが独立して更新するため、area発見の循環を作らない。neutral actionにも残っていた旧PlantAttempt専用の目標選択は、実際に消費する非互換モードだけに限定した。互換Plant処理は既存の正規PlantBomb OnUpdateを使う。

正規targetの実onStartFrame回帰は、実runtimeのBuy完了→Idle→次fullのPlantを通し、Idle更新のpathなし・active pathなし・実RunPlayerMove移動ゼロ・cold目標cache未生成を確認する。`red-idle-after-buy.log` はIdle成立後に経路/移動だけ失敗、NAV抑止後の `red-idle-action-target.log` は次fullPlantと経路/移動ゼロが成立して目標生成だけ失敗した。最終 `green-idle-entry.log` はPlant entry/terminal/ClearLookの3/3合格。製品差と履歴補正は別途read-onlyレビュー済み。

最終オフラインはWindows x86 **171/171・PE7**、Linux x86 **170/170・ELF7**。各全targetビルド後、OS間CTestを直列で実行した。初回Windows全CTestとLinux重点履歴が検出した契約差は最終合格と区別する。DLL SHA-256 `a6a1b4ce7db967a90d958f7d0706461ce1ae130795bdc8bb425a10f98197ee08`、PDB `6595a235dad33a183765708264e4065255706d9c79074964150194930a21db47`、ELF `5151074f75de4d3c9bcbfcd9f959b31050a3c663491eee4d7ee73d7b0d3d198f`。

Windows再構築では進行役NMakeだけを一時停止し、既存child targetの完了を待ってから、OBJ/PDBが独立した22targetを4本で処理した。共有lib・固定DLL/PDBの不変と全22件exit0を確認してから進行役を再開し、正規全ビルドと最終全CTestを通した。同じtargetを通常buildと重ねていない。制御・時刻・指紋はcoordinated-build.jsonに記録した。初回snapshotの文字コード失敗は停止操作前、ELFのリンク途中読取りは除外し、全ビルド終了後のelf-final.logを採用した。

両OSとも固定baselineは保持し、既存補正後の152258行から151999行への差だけを105個のoffset編集（629削除・370挿入）として追加した。読取りcallbackの位置移動も含み、単なる削除のみとはしない。純減259行はindexOfEdict98、entityOfIndex72、szFromIndex84、mp_roundtime取得2、Idleで不要になったground lookahead trace3（fixtureのログ名はtraceObjectiveVisibility）。fixture内の読取りcallback以外の3128行と、RNG510行は各OSの親と順序・値まで完全一致し、全byteを親offset・before行・前後SHAで再構成検証した。この読取り分類は当該fixtureの実装に対するもので、任意GameDLLの副作用なし保証ではない。既存OS間の数値表記/丸め差は保持した。

最終DLL/PDBの隔離実機は全て実PID終了0を確認してから次を起動した。

- final-natural-0: PID44112 exit0/forced=false; plant-fixture planted time=14.614705; plant-fixture complete mode=0 planted=1
- final-natural-7: PID31424 exit0/forced=false; terminal-fixture leave-zone now=11.677335 has=1; plant-fixture complete mode=7 planted=0
- final-natural-8: PID18608 exit0/forced=false; terminal-fixture actual-drop now=11.850727 entity=99 has=0; plant-fixture complete mode=8 planted=0
- final-natural-11: PID19236 exit0/forced=false; plant-fixture planted time=70.292488; plant-fixture complete mode=11 planted=1
- final-idle-debug-0: PID41332 exit0/forced=false; plant-fixture planted time=14.435778; plant-fixture complete mode=0 planted=1

全5試行でTのIdleはpath0・入力(0,0,0)。最終case0のIdle frame742/update70はgoal_generation0、移動ゼロを実RunPlayerMoveで確認した。読取専用CDBはDLL module-load後にBPを置き、初回Idle→Plant `10.36654758` 秒/full `0x41` に保持Ahead **0°** を型付きで直接採録した。入場時はDuck/AK、次の2cached commandもAK、10.498880秒からC4となる。固定参照の初回保持Ahead0°と整合するが、絶対時刻・全経路・全視線軌跡の一致とは主張しない。fixtureの明示的C4在庫bit同期、自然拾得・実水没・標準GameDLL全体の未受入れ範囲は残る。

固定製品は `artifacts/local/debug/csbot-idle-navigation-hold-20261122/final`、受入れ・前後指紋は同root `evidence-final`、全raw試行は `C:/Temp/astrabot-idle-navigation-hold`。push・実環境配布なし。
