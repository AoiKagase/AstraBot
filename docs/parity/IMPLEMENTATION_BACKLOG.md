# Remaining CSBot implementation work

Latest implementation base18a2cc4; friend-death behavioral reactions, knife fire-run, kill-praise and earlier enemy-count verification are recorded in STATUS.md on2026-11-15.
The user goal remains active: proceed through remaining features, commit and locally merge each.
This is an evidence-driven queue, not a complete parity/completion claim. Historical P00 matrices are not current feature inventories.

| Order | Current source/reference evidence | Work and acceptance still required |
|---|---|---|
|1|DeathMsg previously only reset victim state; native cs_bot_event.cpp159–169 / cs_bot_chatter.cpp1884–1893|Remaining enemy-count statements implemented and offline verified: event conditions/RNG/dynamic0..3/queue/lifecycle/host transport; review1 Important fixed RED→GREEN; Windows106/106 and Linux104/105 (known baseline). Commit/local merge receipt is in the session RESULT.json; live acceptance stays open|
|2|cs_bot_event.cpp71–194; current behavior lacks corresponding event reactions|NiceShot/NiceShotSir/NiceShotCommander implemented and offline verified (Windows109/109, Linux107/108 known baseline); fixed death voice criteria repaired for count reports too. Friend-death retaliation/Hunt reconsideration/attack reposition are now implemented with qualified fixed-goal pursuit and actual command tests; final full verification is recorded in STATUS.md. KilledFriend/CommanderDown/pain and other reports remain|
|3|cs_bot_event.cpp267–273,305–354; public gun/step intake already implemented|Knife ForceRun for mapped public enemy gunfire and exact standard fire/dryfire samples implemented and offline verified: Windows110/110, Linux108/109 (known combat_policy baseline), frozen inputs unchanged and exact7x86 exports verified. Native reload/knife/grenade animation event intake remains unimplemented. Hostage help/used auditory tactical decisions and associated chatter still require actual public receipts|
|4|Current ChatterPhrase list implements a subset; native BotStatement carries lists, contexts, subject/place and team arbitration|Other native statement concepts/composition, remaining-enemy vs unrelated statement ordering, subject-aware redundancy and reference global scheduler behavior require further implementation/audit|
|5|STATUS/repair ledger separate offline results from acceptance; initial matrices are historical|Current-source reference-wide audit across all13 states, navigation/combat/weapons/objectives/manager/profile/commands, plus real reference replay and live Windows/Linux/multiplayer/FPS acceptance. Genuine public-information limits need explicit evidence. Known unchanged Linux combat_policy failure is still open, not waived as complete|

PM_Move actual footstep intake was implemented and locally merged at7c7de92. Native animation GameEvents and full scheduling equivalence were not established by that bridge.

Each completed item must cite its source/test/artifact identity in STATUS.md. Preserve unrelated edits and user profiles; no push or deployment is implied by local merge authorization.
