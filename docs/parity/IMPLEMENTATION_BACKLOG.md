# Remaining CSBot implementation work

Source base7c7de92; enemy-count-chatter implementation/verification is recorded in STATUS.md on2026-11-15.
The user goal remains active: proceed through remaining features, commit and locally merge each.
This is an evidence-driven queue, not a complete parity/completion claim. Historical P00 matrices are not current feature inventories.

| Order | Current source/reference evidence | Work and acceptance still required |
|---|---|---|
|1|DeathMsg previously only reset victim state; native cs_bot_event.cpp159–169 / cs_bot_chatter.cpp1884–1893|Remaining enemy-count statements implemented and offline verified: event conditions/RNG/dynamic0..3/queue/lifecycle/host transport; review1 Important fixed RED→GREEN; Windows106/106 and Linux104/105 (known baseline). Commit/local merge receipt is in the session RESULT.json; live acceptance stays open|
|2|cs_bot_event.cpp71–194; current behavior lacks corresponding event reactions|Friend-death retaliation/Hunt reconsideration/attack reposition, friendly-kill reaction and native kill congratulations; preserve visibility, disposition, subject, delays and RNG order|
|3|cs_bot_event.cpp267–273,305–354; public gun/step intake already implemented|Knife ForceRun on enemy gun events; hostage help/used auditory tactical decisions and associated chatter; identify actual public receipts before implementing|
|4|Current ChatterPhrase list implements a subset; native BotStatement carries lists, contexts, subject/place and team arbitration|Other native statement concepts/composition, remaining-enemy vs unrelated statement ordering, subject-aware redundancy and reference global scheduler behavior require further implementation/audit|
|5|STATUS/repair ledger separate offline results from acceptance; initial matrices are historical|Current-source reference-wide audit across all13 states, navigation/combat/weapons/objectives/manager/profile/commands, plus real reference replay and live Windows/Linux/multiplayer/FPS acceptance. Genuine public-information limits need explicit evidence. Known unchanged Linux combat_policy failure is still open, not waived as complete|

PM_Move actual footstep intake was implemented and locally merged at7c7de92. Native animation GameEvents and full scheduling equivalence were not established by that bridge.

Each completed item must cite its source/test/artifact identity in STATUS.md. Preserve unrelated edits and user profiles; no push or deployment is implied by local merge authorization.
