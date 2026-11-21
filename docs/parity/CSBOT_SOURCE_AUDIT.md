# 23　CSBot参照実装との差分監査

参照は ReGameDLL-CS `b0889847fe6d03898be88acc9e366660efb40ab5`。AstraBotの監査起点は `8662286`。公開エンジン情報と所有するBotの状態で再現できる範囲を、現行の実装・テストと照合した。

## 全13状態の照合

CSBot側のファイルは `regamedll/dlls/bot/states/cs_bot_<略称>.cpp`。AstraBot側の状態遷移は `src/core/compat/state_machine.cpp` に集約されている。

| 状態 | CSBot | AstraBotで確認した経路・残件 |
|---|---|---|
| Attack | `attack` 32、156、276–308、565行 | 攻撃オーバーレイと戦闘更新。盾所持・実際の閉状態と遅延認識した敵のリロードを公開観測し、継続開閉・入力後の期限反転・終了時の開要求を実装。通常のFFA索敵と敵の盾を考慮した脅威選択は残件。 |
| Buy | `buy` 48、238、542行 | 購入コントローラと公開購入エリア。武器プリセットを修正。盾の購入可否は別途確認が必要。 |
| DefuseBomb | `defuse_bomb` 32、40、91行、`cs_bot_event.cpp` 221–262行、`cs_bot_pathfind.cpp` 1296–1328、1399–1458行、`game_shared/bot/bot.cpp` 71行 | 設置・近距離ビープによるCTの記憶、最終区間の歩行を照合。解除へ向かうMoveToは現在視線で移動軸を決め、次の軽更新で経路先読みに向く。解除進入では経路視線を保持し、最初のUSE更新でC4への視線を設定する。隔離ReHLDSで通常設置から解除開始・完了まで確認。 |
| EscapeFromBomb | `escape_from_bomb` 31、40、68行 | 退避・武器持ち替え。失敗または到達時の逃走先再選択は残件。 |
| FetchBomb | `fetch_bomb` 32–76行、`hunt` 47–81行、`idle` 205–219行、`cs_bot.cpp` 513行、`cs_bot_pathfind.cpp` 1608–1741行 | Tの専用状態で落下C4への安全経路を保持し、拾得・消失・移動終了でIdleに戻る。経路作成失敗時はHuntを経由する。Idleの攻撃側狙撃抽選の優先順位と、視認もレーダー受領もないTでの再現は残件。 |
| Follow | `follow` 32、103、214、220、287行。`cs_bot_pathfind.cpp:1582`、`:1608`。`cs_bot_statemachine.cpp:224`、`cs_bot_hide.cpp:115` | 周辺NAVの候補選択と経路計算を既存経路の進行後に行う。状態進入時のアイドル乱数、外側0.5秒と共有0.4–0.6秒の再試行待ち、最後のポータルに沿う接地終点、到達不能時の最寄り到達可能領域への部分経路を実装。停止時は遮蔽への経路を計算してからHideに入り、その経路を保持する。再試行の抑制中はFollowを維持する。Hideへの遷移と乱数の順序、リーダーの再移動で追従を再開する条件を回帰テストで確認。 |
| Hide | `hide` 33、71、452行 | 守備、被弾による解除、C4や救出地点の警備を確認。警備先の全選定条件の比較は残件。 |
| Hunt | `hunt` 32、47、219行 | 状態の30秒境界とローミング。参照版の巡回先とC4分岐の網羅照合は残件。 |
| Idle | `idle` 37、52行 | 状態のリセット、Huntへの遷移、シナリオの優先順位。狙撃地点の割り当ては残件。 |
| InvestigateNoise | `investigate_noise` 52、58、135行 | 音源の接地判定とNAV距離を修正。記憶の消費、接近、再選択は既存実行時テストで確認。 |
| MoveTo | `move_to` 32、66、317行 | Plant発話→経路更新→非PROGRESSING時のzone/C4再確認→Plant入場を照合。互換旧PlantAttemptの先行装備/停止を除去。入場更新のNAV末尾入力と全タスク遷移順は残件。 |
| PlantBomb | `plant_bomb` 32、45、75行、`cs_bot_weapon.cpp:411–470` | 公開弾薬・許可クラス・水没条件と共通5秒装備時計を使用。末尾はPrimaryAttack/即時SelectC4→所持喪失→strict5秒超、退出前EquipBest→起立→ResetStuck→Engage→注視取消の順へ接続した。喪失による新Hide timestampを古いPlant ageで上書きしない。通常設置・区域外timeout・実C4ドロップでAKに残る最終Attackと次fullまでの保持、退出fullのUpkeep実角保持を確認。次lightの通常look-goal/spring再開と後段NAV供給、私有複数スロット先頭、Plant初回C4未装備の直接実機は残件。 |
| UseEntity | `use_entity` 33、38、69行、`player.cpp:9415` | 状態に入ったときの人質を保持する。コマンド更新ごとに視線を動かし、現在のyawとpitchが両方とも許容角20度未満のときにUSEを送る。コマンドが受理されると試行を記録してIdleへ移る。ネイティブの人質護送カウンターと追従者の照合は残件。 |

## ClearLook後の軽更新（2026-11-22追補）

固定 `cs_bot.h` のClearLookAtはspot状態/説明だけを消し、`cs_bot_update.cpp:120–183` と `cs_bot_vision.cpp:57–166` が残存Aheadと角速度から次Upkeepを積分する。command runtimeは優先注視へ入る前の実ばねと既定Aheadを分けて保持し、解除後の通常lightへ引き継ぐ。Aheadの供給は元方向からengine VecToYaw相当の整数角、round resetではAhead保持/速度0、新規個体ではAhead0。通常UseEntity/Defuse解除、新しい優先owner、scope/Enhancedの境界も回帰で確認した。

Windows170/170・Linux169/169、PE/ELF公開7関数と直列隔離実機11試行を確認。検証済みCDBの実goal/spring-beforeから次の実viewを独立計算し、誤差.0001度未満。直前のPlant退出full/末尾入力の修正を保つ。詳細はSTATUS先頭と `artifacts/local/debug/csbot-clear-look-upkeep-20261121/README.md`。

上のPlant行に残していた「次lightの通常look-goal/spring再開」は、このowner解放後の復帰範囲で確認済み。一方、Plant進入前のpath/Ahead供給が参照0・Astra75となる時刻/経路差は独立残件。全Idle/Buyでの既定goal常時供給や全Combat/Guard owner間の単一速度・絶対視線軌跡の一致まで確認済みとはしない。

## Plant入場判断の優先順位（2026-11-22追補）

固定 `b088984` の `states/cs_bot_idle.cpp:52`以降はNAV/ゾンビ/round/追従の前提を確認し、C4所持・zone内なら直接Plantと発話、区域外で共有期限後なら最寄りzoneへMoveTo。`cs_bot_follow.cpp`はleaderが有効ならzone内で直接Plantする。`cs_bot_hunt.cpp:47`以降は30秒超、または共有期限/zone内かつ実敵接触からstrict>3秒でIdleへ戻りreturnする。`cs_bot_manager.h:172`の共有期限は>=。`cs_bot_update.cpp:441–446`は取得済み敵が可視なら、Attack中でなくても最後の接触時計を更新する。候補を見ただけの時計とは異なる。

今回のadapterはこの早期分岐をTeamwork・objective target・NAV移動より前へ接続した。Idleから直接Plant、有効leader Follow陽性、Hunt→Idle→次更新のPlant/期限後site選択を正規実行で確認。公開 `client.cpp:5108–5109`の `UpdateClientData.iuser3` bomb-zone bitを優先し、API既知falseをgeometryで覆さず、不明時のregistry発見を到達可能にする。Buy上位要求で入場receipt/chatterを取消す。core任意ガードはcallback後の同一Actor/full-update/epochを検証し、ResetPath後の旧taskとOnExit後の旧ClearLookを再入先へ持ち越さない。

最終v2はWindows171/171+PE7、Linux170/170+ELF7、隔離HLDS0/7/8と読取専用CDB7が合格。初回Plant10.50985813秒の保持Aheadを最終DLL/PDBから0°と直接採録し、旧75°差を解消した。旧段落の「Plant進入前path/Ahead」は不要なHunt供給を除去したこの範囲で確認済みとなる。一方、直前Idleの一般path1準備、MoveToの経路更新後にPlantを判断する順序は未完。標準GameDLLでの直接受入れ、全視線ownerの共有速度、全経路/RNG同等性へ拡張しない。詳しい試行・指紋・履歴補正はSTATUS先頭とTRACE_REPLAYの同日追補を参照。

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
