# CSBot team statement arbitration Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline; one fresh final source reviewer, one Critical/Important RED→GREEN fix pass.

**Goal:** Authorize existing queued voice/radio statements by the native team-wide active-statement algorithm.
**Architecture:** Qualified SDK-free candidate views preserve per-owner start/insertion ordering; runtime scans managed teammates in slot order. Specialized event admission remains, direct VIP radio bypasses the statement queue.
**Tech Stack:** C++14; public HLSDK/Metamod; Windows/Linux x86.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-team-chatter-design.md

## Global Constraints
No private symbols/layouts/ReAPI, new exports or assets; no baseline regeneration; preserve unrelated user edits/profiles/evidence. Shared checkout/cache with exact source/artifact/user fingerprints and serialized OS suites. Full remaining CSBot objective stays active.

## Review Focus
- Candidate creation/start timestamps differ: preserve native compare-start/store-created behavior and final future gate.
- A pending view never invokes a peer update/perception/RNG/I/O or consumes unavailable generic speech.
- Every successful insertion receives the actor-wide monotonic order, including both friend-death slots; inspect all entries.
- Retained speaking survives nominal duration for peers; owner strict-after completion consumes one update, no immediate next send.
- Slot order/name/team/serial/connection/map/round/time and callback replacement determine peer eligibility; direct VIP dispatch remains independent.

### Task 1: Differential regressions and shared selector
Files: modify tests/friend_death_chatter_runtime_tests.cpp; create include/astrabot/compat/chatter_schedule.hpp and tests/chatter_schedule_tests.cpp; register CMakeLists.txt and docs/source-manifest.json.
Interfaces: ChatterScheduleEntry contains slot, topic, creation/start times, actor admission order and speaking; selectActiveChatter(entries,count,now) returns selected entry or none after sorting native slot/list order. ChatterStatementTopic follows native report types; redundancy helper uses topic/place/subject with four native exceptions.
- [x] Add actual registered DeathMsg/SendAudio sender capture. Target older KilledFriend to slot5, later to slot1; at readiness expect sender5 despite slot1 updating first. Watch actual RED. Add future-selected-blocker case (older delayed slot1, later immediate slot5): neither emits until older is due. No source-text assertions.
- [x] Add pure selector table: reverse input order, equal-start insertion, speaking priority, creation/start counterexample, selected future blocker, strict equal scan tie, nonfinite/invalid input and native redundancy truth table. Watch RED then implement pure algorithm and GREEN.

### Task 2: Qualified metadata and actual team authorization
Files: modify include/astrabot/compat/outgoing_radio.hpp, enemy_count_chatter.hpp, kill_praise.hpp, friend_death_chatter.hpp; runtime behavior bot_behavior_radio.cpp, bot_behavior_death.cpp, bot_behavior_friend_death.cpp, bot_behavior_runtime.hpp; separate schedule implementation file only if needed and register it. Tests: same runtime fixture plus death_interrupt_tests.cpp / existing Core tests as required.
Interfaces: expose read-only schedule entries qualified to current receipt; creation times already stored for count/praise, add actual per-slot creation for friend-death; assign actor-wide insertion order at successful admission. Generic separates update maintenance/admission from selected transport readiness.
- [x] Pin callback/identity/no-extra-RNG/profile query behavior and generic blocked-missing/direct-VIP controls as executable RED cases.
- [x] Stamp actual admitted entries and enumerate every pending topic, not only the local first friend-death slot; qualify peer owner/serial/connection/map/round/time without peer updates.
- [x] Replace local arbitration with native selected slot/topic. Maintain native retained speaking and strict completion update; use active teammate topic/place/subject for represented redundancy. Preserve current transport ownership/revision and callback cancellation safeguards.
- [x] Run focused scheduler/Core plus multi-bot/death/ordinary voice/VIP/frozen tests and observe GREEN. Inspect failures against current native behavior, never regenerate frozen evidence.

### Task 3: Review, offline identity and integration
- [x] Hold final source inputs, single fresh final read-only reviewer; resolve Important/Critical once with reproduced RED→GREEN; no re-review.
- [x] Complete x86 OS builds/CTest directories under correct VS/WSL environments, suites serialized; named known Linux combat_policy baseline stays open. Separate exact7 PE/ELF, source manifest and user/source/artifact hashes.
- [x] Update parity STATUS/backlog, inspect diff/explicit owned-path staging, commit feature and local FF master; postmerge hashes and RESULT.json. No push/deployment/live or whole-goal completion claim.

Final evidence: .agent-state/sessions/csbot-team-chatter-8e4e8c08-f815-4e9d-8642-6aca3a713d48/; complete Windows120/120 PASS9.91s, Linux118/119 PASS12.62s (only unchanged combat_policy baseline). Separate exact7 PE/ELF x86, manifest291/283, held source294/artifact2/user4. Single final review0Critical/2Important/0Minor fixed actual runtime RED→GREEN7/7; full-gate repair4/4 preserves frozen characterization. Local integration receipt RESULT.json; full CSBot goal/live acceptance remain active.
