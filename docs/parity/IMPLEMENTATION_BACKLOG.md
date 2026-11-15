# Remaining CSBot implementation work

Latest implementation base7105bd88b168df8ebd1371ed006b97f3602d2ede; common represented-statement collection/admission/maintenance, team-wide represented-statement arbitration, death Radio attachment/UI, death-interruption audio, friend-death behavior/chatter, knife fire-run, kill-praise and earlier enemy-count verification are recorded in STATUS.md on2026-11-15.
The user goal remains active: proceed through remaining features, commit and locally merge each.
This is an evidence-driven queue, not a complete parity/completion claim. Historical P00 matrices are not current feature inventories.

| Order | Current source/reference evidence | Work and acceptance still required |
|---|---|---|
|1|DeathMsg previously only reset victim state; native cs_bot_event.cpp159–169 / cs_bot_chatter.cpp1884–1893|Remaining enemy-count statements implemented and offline verified: event conditions/RNG/dynamic0..3/queue/lifecycle/host transport; review1 Important fixed RED→GREEN; Windows106/106 and Linux104/105 (known baseline). Commit/local merge receipt is in the session RESULT.json; live acceptance stays open|
|2|cs_bot_event.cpp71–194; current behavior lacks corresponding event reactions|NiceShot/NiceShotSir/NiceShotCommander implemented and offline verified (Windows109/109, Linux107/108 known baseline); fixed death voice criteria repaired for count reports too. Friend-death retaliation/Hunt reconsideration/attack reposition are now implemented with qualified fixed-goal pursuit and actual command tests; final full verification is recorded in STATUS.md. CommanderDown/KilledFriend are implemented with pending/current intention redundancy and actual voice/radio transport; final verification is recorded in STATUS.md. Speaking-at-death pain is implemented with native verbosity/default catalog, qualified cancellation/feedback and actual transport tests; final results are in STATUS.md. FriendlyFire/injury still needs exact receipt intake: native CCSBot::TakeDamage uses direct pevInflictor before base rules, while public Damage HUD aggregates save/take/bits/origin without that attempted-damage identity/timing stream. Current public intake evidence and non-equivalence of Damage/entvars/log/animation candidates are documented in PUBLIC_EVENT_INTAKE_AUDIT.md. Investigate an exact public bridge; no approximation, universal impossibility or parity waiver is claimed. Other reports remain|
|3|cs_bot_event.cpp267–273,305–354; public gun/step intake already implemented|Knife ForceRun for mapped public enemy gunfire and exact standard fire/dryfire samples implemented and offline verified: Windows110/110, Linux108/109 (known combat_policy baseline), frozen inputs unchanged and exact7x86 exports verified. Native reload/knife/grenade animation event intake remains unimplemented. Hostage help/used auditory tactical decisions and associated chatter still require actual public receipts|
|4|Current ChatterPhrase list implements a subset; native BotStatement carries lists, contexts, subject/place and team arbitration|Ordinary pending/delayed/direct speech death cancellation and actual VIP direct Follow me dispatch/consumption are now covered. Team-wide GetActiveStatement selection for currently represented pending/speaking entries is implemented with native slot/list ordering, actual actor-wide insertion stamps, compare-start/store-creation/future gate, retained speaker/strict owner completion, represented native topic/place/subject redundancy and independent direct VIP transport. Final gates are recorded in STATUS.md. Common represented pending storage/admission is now a linked owner list, with no six-view candidate cap, successful-admission IDs, shared own/current redundancy, default metadata and admission-only verbosity. Represented selected action → second selection → owner maintenance now follows native order, including an unselected owner; final verification is recorded in STATUS.md. Multi-part place/count/delay composition, plan conversion (AgreeWithPlan), memes, new report subjects/constructors including dead/REPORT_ROUND_END, exact broader trigger/transport retry parity and reference-wide scheduler equivalence still require implementation/audit. Retained EnemySpotted combat-condition payload and same-owner execution-reset delayed Ack cancellation are specifically open pre-existing bridge limitations; Core maintenance coverage does not certify their production parity. Death-only native Radio attachment/UI effects are now implemented through public SVC_TEMPENTITY/TE_PLAYERATTACHMENT with host model/cvar, per-recipient audio pairing and callback identity guards; ordinary custom statements retain native showIcon=false and mapped standard radio remains GameDLL-owned. Final verification is in STATUS.md|
|5|STATUS/repair ledger separate offline results from acceptance; initial matrices are historical|Current-source reference-wide audit across all13 states, navigation/combat/weapons/objectives/manager/profile/commands, plus real reference replay and live Windows/Linux/multiplayer/FPS acceptance. Genuine public-information limits need explicit evidence. Known unchanged Linux combat_policy failure is still open, not waived as complete|

PM_Move actual footstep intake was implemented and locally merged at7c7de92. Native animation GameEvents and full scheduling equivalence were not established by that bridge.

Each completed item must cite its source/test/artifact identity in STATUS.md. Preserve unrelated edits and user profiles; no push or deployment is implied by local merge authorization.

## User-approved repair-unit sequence — 2026-11-15

The user requests01 onward in order, each repaired unit committed and locally FF merged.22 repair/investigation candidates plus3 audit/live gates; this is not a final exhaustive total before23. NAV authoring/learning remains outside the current milestone (.planning/PROJECT.md).

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|01|複数パーツ再生Core|本節のCore実装。最終ゲート／統合記録はunit01 session RESULT.json|
|02|場所音声・共有20秒抑制|既定Place ID／属性・プロフィール候補・数値条件・共有64件20秒履歴を実装。全体ゲート／統合記録はunit02 session RESULT.json。03が本番接続を担当|
|03|本番複数パーツ・文脈判定|未接続。場所→待機→敵数、最多敵場所、戦闘条件、無音中の死亡割込み。01review Minorのskip/nullと0+.1を含む|
|04|標準無線の送信回数|最大3回再試行からネイティブ1回へ|
|05|同一Botリセット時の遅延応答|古いAck破棄|

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|06|同計画への同意|AgreeWithPlan変換／開始時刻変更|
|07|援護要請の行動連動|発話完了後Help伝達／反応|
|08|追従要請の行動連動|Follow伝達／距離・任務・性格|
|09|防衛要請の行動連動|DefendHere伝達／防衛・隠れ場所|
|10|状況報告応答|RequestReport／ReportingIn|

| ID | 修正単位 | 現在の作業／残件 |
|---|---|---|
|11|C4状況報告と共有|搬送者・落下・設置場所・サイト確認等|
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
|23|全13状態と周辺の現行監査|NAV／戦闘／武器／目的／管理／profile／command。追加差分が見つかれば明示的に追加|
|24|実参照との差分リプレイ|判断／event／RNG消費順の比較|
|25|両OS実機受入れ|C4／視認後Aim／旋回／多人数FPS・lag|
