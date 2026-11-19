# Remaining CSBot implementation work

Completed repair units01–06 and the07A NAV-center and07B public-ladder travel prerequisites are recorded below and in STATUS.md, including production compound playback and native one-shot standard/direct radio plus delayed acknowledgement reset and same-plan agreement conversion. Original07 Help transmission/reaction and actual native ladder assistance are implemented and offline verified;07A/07B supplied the required route geometry/topology and read-only range queries. Each unit retains its exact source/test/artifact/commit identity in its session RESULT.json.
The user goal remains active: proceed through remaining features, commit and locally merge each.
This is an evidence-driven queue, not a complete parity/completion claim. Historical P00 matrices are not current feature inventories.

| Order | Current source/reference evidence | Work and acceptance still required |
|---|---|---|
|1|DeathMsg previously only reset victim state; native cs_bot_event.cpp159–169 / cs_bot_chatter.cpp1884–1893|Remaining enemy-count statements implemented and offline verified: event conditions/RNG/dynamic0..3/queue/lifecycle/host transport; review1 Important fixed RED→GREEN; Windows106/106 and Linux104/105 (known baseline). Commit/local merge receipt is in the session RESULT.json; live acceptance stays open|
|2|cs_bot_event.cpp71–194; current behavior lacks corresponding event reactions|NiceShot/NiceShotSir/NiceShotCommander implemented and offline verified (Windows109/109, Linux107/108 known baseline); fixed death voice criteria repaired for count reports too. Friend-death retaliation/Hunt reconsideration/attack reposition are now implemented with qualified fixed-goal pursuit and actual command tests; final full verification is recorded in STATUS.md. CommanderDown/KilledFriend are implemented with pending/current intention redundancy and actual voice/radio transport; final verification is recorded in STATUS.md. Speaking-at-death pain is implemented with native verbosity/default catalog, qualified cancellation/feedback and actual transport tests; final results are in STATUS.md. FriendlyFire/injury still needs exact receipt intake: native CCSBot::TakeDamage uses direct pevInflictor before base rules, while public Damage HUD aggregates save/take/bits/origin without that attempted-damage identity/timing stream. Current public intake evidence and non-equivalence of Damage/entvars/log/animation candidates are documented in PUBLIC_EVENT_INTAKE_AUDIT.md. Investigate an exact public bridge; no approximation, universal impossibility or parity waiver is claimed. Other reports remain|
|3|cs_bot_event.cpp267–273,305–354; public gun/step intake already implemented|Knife ForceRun for mapped public enemy gunfire and exact standard fire/dryfire samples implemented and offline verified: Windows110/110, Linux108/109 (known combat_policy baseline), frozen inputs unchanged and exact7x86 exports verified. Native reload/knife/grenade animation event intake remains unimplemented. Hostage-use chatter accepts the 12A receipts. Hostage-help intake, inaudible repeat uses and tactical reactions remain open in 12B-3b, 19 and 20|
|4|Current ChatterPhrase list implements a subset; native BotStatement carries lists, contexts, subject/place and team arbitration|Ordinary pending/delayed/direct speech death cancellation and actual VIP direct Follow me dispatch/consumption are now covered. Team-wide GetActiveStatement selection for currently represented pending/speaking entries is implemented with native slot/list ordering, actual actor-wide insertion stamps, compare-start/store-creation/future gate, retained speaker/strict owner completion, represented native topic/place/subject redundancy and independent direct VIP transport. Final gates are recorded in STATUS.md. Common represented pending storage/admission is now a linked owner list, with no six-view candidate cap, successful-admission IDs, shared own/current redundancy, default metadata and admission-only verbosity. Represented selected action → second selection → owner maintenance now follows native order, including an unselected owner; final verification is recorded in STATUS.md. Compound place/count/delay composition, current-context playback, production EnemySpotted combat conditions, one-shot standard/direct transport, same-owner execution-reset delayed Ack cancellation and same-plan agreement conversion are implemented and offline verified in01–06. Memes, new report subjects/constructors including dead/REPORT_ROUND_END and reference-wide trigger/lifetime/scheduler equivalence still require implementation/audit. Death-only native Radio attachment/UI effects are now implemented through public SVC_TEMPENTITY/TE_PLAYERATTACHMENT with host model/cvar, per-recipient audio pairing and callback identity guards; ordinary custom statements retain native showIcon=false and mapped standard radio remains GameDLL-owned. Final verification is in STATUS.md|
|5|STATUS/repair ledger separate offline results from acceptance; initial matrices are historical|Current-source reference-wide audit across all13 states, navigation/combat/weapons/objectives/manager/profile/commands, plus real reference replay and live Windows/Linux/multiplayer/FPS acceptance. Genuine public-information limits need explicit evidence. Known unchanged Linux combat_policy failure is still open, not waived as complete|

PM_Move actual footstep intake was implemented and locally merged at7c7de92. Native animation GameEvents and full scheduling equivalence were not established by that bridge.

Each completed item must cite its source/test/artifact identity in STATUS.md. Preserve unrelated edits and user profiles; no push or deployment is implied by local merge authorization.

## User-approved repair-unit sequence — 2026-11-15

The user requests01 onward in order, each repaired unit committed and locally FF merged.22 repair/investigation candidates plus3 audit/live gates; this is not a final exhaustive total before23. NAV authoring/learning remains outside the current milestone (.planning/PROJECT.md).

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|01|複数パーツ再生Core|本節のCore実装。最終ゲート／統合記録はunit01 session RESULT.json|
|02|場所音声・共有20秒抑制|既定Place ID／属性・プロフィール候補・数値条件・共有64件20秒履歴を実装。全体ゲート／統合記録はunit02 session RESULT.json。03が本番接続を担当|
|03|本番複数パーツ・文脈判定|実装・オフライン検証済み。場所→待機→動的敵数／Help、最多敵場所、戦闘条件、無音死亡割込み、共有抑制とコールバック取消。Windows135/135、Linux133/134（既知22のみ）。統合記録はunit03 session RESULT.json|
|04|標準無線の送信回数|実装・オフライン検証済み。受信確認非依存の1回試行と2+.1待機、直接VIPの共有時計／無音時間分離。Windows135/135、Linux133/134（既知22のみ）。統合記録はunit04 session RESULT.json|
|05|同一Botリセット時の遅延応答|実装・オフライン検証済み。予約済みAckと未処理命令を破棄し、resetを跨ぐ受信完了を拒否。新規命令は保持。Windows135/135、Linux133/134（既知22のみ）。統合記録はunit05 session RESULT.json|

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|06|同計画への同意|実装・オフライン検証済み。同じ場所はAgreeWithPlanへ変換して0.5〜1秒待機、別の場所は元の計画を3〜4秒延期。Windows135/135、Linux133/134（既知22のみ）。統合記録はunit06 session RESULT.json|
|07A（前提）|NAV経路の参照中心|実装・オフライン検証済み。CompatibilityのA*をCSBotの領域中心へ合わせ、距離0を保持。Windows135/135、Linux133/134（既知22のみ）。本体07は下記で実装・オフライン検証済み。統合記録はunit07A session RESULT.json|
|07B（前提）|公開ラダーの復元・到達距離|実装・オフライン検証済み。公開bounds／traceから復元し、CSBotの経路コストで選んだ経路の中心間距離を返す。Windows135/135、Linux133/134（既知22のみ）。本体07は下記で実装・オフライン検証済み。統合記録はunit07B session RESULT.json|
|07|援護要請の行動連動|実装・オフライン検証済み。自然完了Help伝達、共有Rogue／任務／距離／Place判断、固定援護目標と任務保持、公開ラダー経路／実コマンドを接続。Windows137/137、Linux135/136（既知22のみ）。旧トレースは保持し、独立参照ビルド由来の厳密補正で両OS149075行一致。統合記録はunit07 session RESULT.json。実機25は未実施|
|08|追従要請の行動連動|実装・オフライン検証済み。設置計画の自然完了からFollowを伝達し、Rogue・Busy・SAFEST距離を判定。追従相手の移動と接続IDを追跡し、CoveringFriendで応答。Windows138/138、Linux136/137（既知22のみ）。Follow状態全体の監査は23、実機受入れは25|
|09|防衛要請の行動連動|設置報告完了時にDefendHereを伝達。発話時の位置から防衛地点を選び、地点指定と近傍探索の応答、移動、到着、待機期限を検証。Windows139/139、Linux137/138（既知22のみ）。Hide中の追従再開や経路再計算を含む状態全体の監査は23、実機受入れは25|
|10|状況報告応答|人間のReportInとBotのRequestReportから戦況を発話。位置、交戦状態、味方の人数、直近の敵の記録、設置・解除・護送任務を反映。Windows140/140、Linux138/139（既知22のみ）。落下C4と人質の警備任務に固有の報告は11・12で接続。実機受入れは25|

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|11A|C4捜索時のサイト確認報告|CTが未確認サイトの範囲内に入るとBombsiteClearを発話。受信側は確認済みとして記憶し、捜索中なら応答して捜索先を選び直す。2体のCTによる音声と目標変更を両OSで検証。Windows141/141、Linux139/140（既知22のみ）|
|11B|設置サイトの問い合わせ・発見・共有|最初のビープ音で設置サイトを発見したCTが報告する。サイトを知る味方は問い合わせにも応答し、聞き手はサイト中央へ移動する。2体のCTによる自発報告と問い合わせを別々の統合テストで確認。|
|11C-1|C4搬送者・落下・回収の報告|目撃位置を味方に伝達する。搬送者は前回報告から1000単位以上、落下C4は視野・遮蔽・10秒の再報告間隔、回収音はCTから1000単位未満で判定。2体のCTで報告と重複抑制を検証。|
|11C-2a|落下C4の警備|Hunt中の協調的なCTがC4を視認すると、ドロップ時のNAVエリア付近に隠れる。状況照会に現在のC4位置を伝え、警備期限で場所を選び直し、回収に気づくと警備を終える。|
|11C-2b（設置後）|人間に解除を任せるときの爆破地点警備|CTがC4の位置または設置サイトを知り、生存中の人間の味方に解除を任せると、NAVの隠れ場所で警備する。警備期限で最寄りのサイトを選び直し、解除を担当できるようになると警備を解く。|
|11C-2b（設置前）|爆破地点の警備選択と落下報告への応答|非ラッシュ時の士気・敵の目撃時刻に応じた警備、狙撃銃所持時の優先判断、落下C4を直接見たCTの離脱と味方の報告完了後の応答を2体のCTで検証。実機受入れは25。|
|12A|人質状況報告と共有・連れ出し|追従開始音または初回接触ログから `HostagesBeingTaken` を共有し、対応可能な味方が応答|
|12B-1|人質状況報告と共有・目視での確認|T側Botが元の位置を確認し、最後の人質を見失うと `HostagesTaken` と報告して待機状態へ戻る|
|12B-2|人質と救出地点の警備|序盤・連れ出しの記憶・抽選で警備先を選び、NAVの隠れ場所へ向かう。救出地点警備の更新と状況照会時の発話を追加|
|12B-3a|全員救出の認識|全員救出をラウンド内で記憶し、人質警備を終了。通常のCT勝利・個人宛ての通知と区別する|
|12B-3b|人質の追従状態・残項目|無音の再追従、目の前の未追従人質への警備切り替え。公開候補の調査は PUBLIC_EVENT_INTAKE_AUDIT.md|
|13A|狙撃警告|目視した敵が狙撃銃を構えていることを公開GameDLLで確認し、味方へ警告。本人の再確認と受信側の抑制を追加|
|13B-1|最後の1人が数で劣ったときの発話|援護の代わりに恐怖を発話。初回と再発話の間隔と交戦終了時の取り消しを確認|
|13B-2|釘付け・恐怖と戦闘撤退|発話の抑制、撤退再試行、敵の視線と占有状況で選ぶ退避先を接続。実機確認は25。|
|13B-3a|再装填時の退避|弾倉と交戦状態で再装填を判断し、移動入力で要求する。適格なら1000単位以内の遮蔽へ移動する。実機確認は25。|
|13B-3b|敵を見失った際の待ち伏せ|視認が途切れるたびに1回だけ抽選し、200単位以内の遮蔽を探索。再視認後の反応時間と、より近い銃声への切り替えを接続。実機確認は25。|
|13B-3c|撤退命令への応答|味方の人間からTeam Fall Backを受け、条件が合えば撤退して肯定応答。忙しい場合、非協調、退避先がない場合を区別。実機確認は25。|
|14|終了・勝利発話|勝利Emoteをラウンド終了境界で引き継ぎ、実際のリセットではREPORT_ROUND_ENDを保持。実機確認は25。|
|15A|Sayの既定フレーズ・意図表明|名前で音声定義を解決して意図表明として登録。Negativeは既存の確認応答を利用。|
|15B-1|C4警備中のEncourage|味方の人間に解除を任せるCTが隠れ場所から残り時間に応じて発話し、間隔を共有する。実機確認は25。|
|15B-2a|自動追従開始時のSay|通常戦とキャリア戦の発話を使い分け、追従を開始した時点で1回登録する。実機確認は25。|
|15B-2b|追跡経路終了時のLostEnemy|経路の終点到達または失敗で追跡を終え、対象が生存していれば発話する。実機確認は25。|
|15B-2c|安全時間終了間際のWaitingHere|士気・残り安全時間・防衛方針に応じて隠れ場所への移行と発話を行う。実機確認は25。|
|15B-2d|解除直後のBarelyDefused|残り2秒未満で解除した場合にCTが発話を登録し、ラウンド終了後の再生判定へ渡す。実機確認は25。|
|15B-2e|ReGameDLLでのBlinded発話|公開の失明フックで対象Botを照合し、CSBotの抽選条件で発話する。実機確認は25。|
|15B-2f|人質警備のEncourageと標準GameDLLのBlinded|自由な人質の視認には追従状態、標準GameDLLのBlindedには閃光による失明の受信経路が必要。|

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|16A|ReGameDLLでの味方撃ち抗議|公開ダメージフックで直接の加害元を確認し、CSBotの遅延抽選と重複判定を接続する。実機確認は25。|
|16B|標準GameDLLでの味方撃ち抗議|ダメージ判定前に直接の加害元を確定できる公開経路の調査が必要。|
|17A|ReGameDLLでの敵被弾記録とHide解除|公開ダメージフックで敵の加害元と時刻を記録し、安全時間の短縮と到着済みのHide解除に接続する。実機確認は25。|
|17B|ReGameDLLでの被弾パニック・視線と攻撃先の再選択|直接の加害元を視認できない場合のパニック、発砲遅延、警戒視線を接続。交戦中の敵を見失った場合は、視認できた直近の加害元へ切り替える。実機確認は25。標準GameDLLの被弾受信経路は16Bと共通の残件。|
|18|標準GameDLLの武器アニメーションイベント|ReGameDLL 5.30では発砲・空撃ち・リロードのネイティブ通知を接続済み。ナイフ音と投擲の無音扱いを反映。標準GameDLLの同等通知と実機確認（25）が残る。|
|19|人質救助要請へのCT対応|参照版既定の `REGAMEDLL_FIXES` では人質の通知を回収処理の手前で除外する。互換モードで標準の救助要請音5種を調査音に入れないようにした。実機確認は25。|
|20|人質使用へのT対応|12Aの通知から、視認できる未追従の人質がいないと確認できる場合に救出地点の警備へ切り替える。交戦終了と既存警備の維持を含む。無音の通知と、視認中の別の人質の追従状態は12B-3bに残る。実機確認は25。|

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|21|音声候補の乱数整合|Engine RNG shuffle近似とnative CRT順の差分|
|22|Linux combat_policy|既知のweapon/skill/recoil chest閾値失敗|
|23|全13状態と周辺の現行監査|NAV、戦闘、武器、目的、管理、プロファイル、コマンドの照合箇所と追加差分は [CSBot参照実装との差分監査](CSBOT_SOURCE_AUDIT.md)。状態ごとの残件と実装済みの修正を追跡する。|
|24|実参照との差分リプレイ|[固定版との比較](TRACE_REPLAY.md) で聴覚判断・乱数順と時刻0の視線反応を確認。Windows x86 HLDSで武器イベント分類とNAV経路の一部を比較済み。残件は目標選択・後続経路・移動入力と実機乱数順。|
|25|両OS実機受入れ|C4／視認後Aim／旋回／多人数FPS・lag|
