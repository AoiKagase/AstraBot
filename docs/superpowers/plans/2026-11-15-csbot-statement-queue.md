# Native common pending statement queue Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline, one fresh final source reviewer and one Critical/Important RED→GREEN fix pass.

**Goal:** Replace split single-slot pending stores with native ordered common admission/storage/maintenance for currently represented statements.
**Architecture:** ChatterPendingQueue linked ownership inside OutgoingRadioScheduler; fixed/current represented report payloads, immutable owner-qualified node handles and monotonic mutation revision. Pure event constructors supply delay/phrase, streaming native team selection reads actual list order without allocation/candidate caps. Confirmed speech and direct VIP retain independent receipt state.
**Tech Stack:** C++14 SDK-free Core; public HLSDK/Metamod; Windows/Linux x86.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-statement-queue-design.md

## Global Constraints
No private symbols/layouts/ReAPI, new exports/assets, baseline regeneration, user-file overwrite or push/deployment. Preserve existing shared checkout/cache/evidence with exact owned paths and fingerprints. Full compound/conversion/meme/unrepresented round-end/dead/trigger/transport-retry parity and live acceptance remain active follow-ups.

## Review Focus
- Native selected action precedes maintenance for every updating owner, including an unselected owner; strict expiry does not interrupt retained speaking.
- Multiple exempt and distinct-topic statements coexist; shared own pending/current redundancy crosses all former stores; IDs mint only after successful admission.
- No captured node pointer crosses Engine/RNG/transport callbacks; owner/connection/lifetime/node token rejects old completion without deleting fresh inserted speech.
- Team scan streams actual list order and fences every peer inclusion/exclusion and queue/speech mutation; empty queues cause no extra Engine calls and scans allocate nothing per frame.
- o/m admission-only and r/R transport parsing, default catalog vs profile clips, constructor RNG order, direct VIP independence and lifecycle/death/icon safeguards survive migration.

### Task 1: Actual loss regression and Core collection
Files: create tests/statement_queue_runtime_tests.cpp, tests/chatter_statements_tests.cpp, include/astrabot/compat/chatter_catalog.hpp, include/astrabot/compat/chatter_statements.hpp; modify outgoing_radio.hpp/chatter_schedule.hpp, CMakeLists.txt/source manifest.
Interfaces: preserve ChatterPhrase/ChatterRadioCatalog/RadioMenuChoice names in extracted catalog header. ChatterStatementRequest carries source/topic, fixed or represented context payload, created/start/expiry/ready time, place/subject/conditions. ChatterStatementHandle carries immutable owner-qualified monotonic admission ID. ChatterPendingQueue admits/removes by handle, exposes revision and allocation-free list visitor; ChatterTeamSelection consumes slot/list-ordered views preserving native compare-start/store-created/future/speaking rules.
- [x] Register actual public human Go + observed enemy visibility fixture; require both acknowledgement and Help audio rather than silently dropping one. Observe semantic RED with current runtime, validate acknowledgement/vision controls.
- [x] Extract catalog declarations without behavior change and use existing parser/voice tests; add collection tests for >6 exempt entries, all native redundancy exceptions/place/subject, equal-start insertion, strict lifetime/selected-before-maintenance, retained own speaking, admission mode/catalog and stale node/owner removal. Observe RED then implement minimal linked collection and streaming selector GREEN.

### Task 2: Migrate producers and actual dispatch
Files: outgoing_radio.hpp, enemy_count_chatter.hpp, kill_praise.hpp, friend_death_chatter.hpp/chatter_voice.hpp as needed; runtime behavior radio/death/friend/perception/runtime/voice cpp/hpp; tests existing event/voice/praise/death fixtures plus ownership JSON.
Interfaces: prepareRemainingEnemyStatement/prepareKillPraiseStatement return stateless request payloads using existing native semantic RNG IDs/ranges before admission gates; friend event constructor uses existing KilledFriend delay helper. OutgoingRadioScheduler.prepare admits every ordinary trigger, decide resolves qualified selected node, maintain runs native pending validity→speaking skip→strict obsolete→friend redundancy after action. Direct confirmation/discard uses separate token.
- [x] Capture immutable event admission ownership before RNG/catalog/engine callbacks; verify afterward, do not recapture fresh life for an old event. No profile bank/sample I/O at event admission; cached default metadata loading remains explicit.
- [x] Replace actor count/praise/friend pending fields with common scheduler collection; remove copying fill/reset call sites, preserve monotonic queue revision/IDs across same-owner reset/cancellation.
- [x] Stream actual shared list and retained speaking in public slot order; replace fixed snapshot arrays with pure revision proofs for all peers. Keep prior excluded-peer, queue-change, direct, empty-characterization safeguards.
- [x] Route selected node readiness/transport confirmation/discard by handle; native pending maintenance follows owner action/second team selection even when blocked. Preserve strict completion gap and selected-only speech contexts. Admission-only first-letter modes, fixed undefined-place criteria, death/pain/icon/direct behavior.
- [x] GREEN actual multi-statement transport plus existing event/voice/callback/VIP/frozen/ownership gates; correct only expectations demonstrably contrary to native source, never regenerate frozen baselines.

### Task 3: Final review, complete gates and local integration
- [x] Hold final inputs, single fresh final review; Important/Critical reproduced and fixed once with RED→GREEN, no re-review.
- [x] Complete both x86 build directories/suites (correct VS Hostx86/x86, WSL); serialize OS suites; independently exact7 PE/ELF, source manifest/ownership, source/artifact/user hashes; report known Linux combat_policy baseline by name.
- [ ] Update parity status/backlog, explicit owned-path stage/commit, local FF master, postmerge identities/RESULT.json. Keep whole goal active and live acceptance separate.
