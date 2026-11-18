# Remaining CSBot implementation work

Completed repair units01–06 and the07A NAV-center and07B public-ladder travel prerequisites are recorded below and in STATUS.md, including production compound playback and native one-shot standard/direct radio plus delayed acknowledgement reset and same-plan agreement conversion. Original07 Help transmission/reaction and actual native ladder assistance are implemented and offline verified;07A/07B supplied the required route geometry/topology and read-only range queries. Each unit retains its exact source/test/artifact/commit identity in its session RESULT.json.
The user goal remains active: proceed through remaining features, commit and locally merge each.
This is an evidence-driven queue, not a complete parity/completion claim. Historical P00 matrices are not current feature inventories.

| Order | Current source/reference evidence | Work and acceptance still required |
|---|---|---|
|1|DeathMsg previously only reset victim state; native cs_bot_event.cpp159–169 / cs_bot_chatter.cpp1884–1893|Remaining enemy-count statements implemented and offline verified: event conditions/RNG/dynamic0..3/queue/lifecycle/host transport; review1 Important fixed RED→GREEN; Windows106/106 and Linux104/105 (known baseline). Commit/local merge receipt is in the session RESULT.json; live acceptance stays open|
|2|cs_bot_event.cpp71–194; current behavior lacks corresponding event reactions|NiceShot/NiceShotSir/NiceShotCommander implemented and offline verified (Windows109/109, Linux107/108 known baseline); fixed death voice criteria repaired for count reports too. Friend-death retaliation/Hunt reconsideration/attack reposition are now implemented with qualified fixed-goal pursuit and actual command tests; final full verification is recorded in STATUS.md. CommanderDown/KilledFriend are implemented with pending/current intention redundancy and actual voice/radio transport; final verification is recorded in STATUS.md. Speaking-at-death pain is implemented with native verbosity/default catalog, qualified cancellation/feedback and actual transport tests; final results are in STATUS.md. FriendlyFire/injury still needs exact receipt intake: native CCSBot::TakeDamage uses direct pevInflictor before base rules, while public Damage HUD aggregates save/take/bits/origin without that attempted-damage identity/timing stream. Current public intake evidence and non-equivalence of Damage/entvars/log/animation candidates are documented in PUBLIC_EVENT_INTAKE_AUDIT.md. Investigate an exact public bridge; no approximation, universal impossibility or parity waiver is claimed. Other reports remain|
|3|cs_bot_event.cpp267–273,305–354; public gun/step intake already implemented|Knife ForceRun for mapped public enemy gunfire and exact standard fire/dryfire samples implemented and offline verified: Windows110/110, Linux108/109 (known combat_policy baseline), frozen inputs unchanged and exact7x86 exports verified. Native reload/knife/grenade animation event intake remains unimplemented. Hostage help/used auditory tactical decisions and associated chatter still require actual public receipts|
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
|11C-2b（設置前）|爆破地点の警備選択と落下報告への応答|士気・安全時間・チームのラッシュ判断から警備を選ぶ。C4の直接目撃で警備を終える条件と、味方の無線で落下を知った場合の応答を検証する。|
|12|人質状況報告と共有|警備・連れ去り・回収・呼び掛け等|
|13|追加戦闘報告|撃破・釘付け・狙撃警告等|
|14|終了・勝利発話|死亡Bot／REPORT_ROUND_END／終了発話の保持|
|15|その他Say／Encourage|任意既定フレーズ・Negative・意図表明・励まし|

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|16|味方撃ち抗議|正確な公開受信経路の調査が必要|
|17|被弾警戒・パニック|正確な加害元・時刻・呼出順の公開受信経路が未確立|
|18|武器アニメーションイベント|reload／knife／grenadeのネイティブ対応|
|19|人質救助要請へのCT対応|受信経路／回収判断|
|20|人質使用へのT対応|受信経路／救出地点警備|

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|21|音声候補の乱数整合|Engine RNG shuffle近似とnative CRT順の差分|
|22|Linux combat_policy|既知のweapon/skill/recoil chest閾値失敗|
|23|全13状態と周辺の現行監査|NAV／戦闘／武器／目的／管理／profile／command。既存noise ground probeとnative ground-layer規則の差、chatter NAV線形検索の空間索引／性能、他の経路コンテキストの参照中心選択も確認。追加差分は明示的に追加|
|24|実参照との差分リプレイ|判断／event／RNG消費順の比較|
|25|両OS実機受入れ|C4／視認後Aim／旋回／多人数FPS・lag|
