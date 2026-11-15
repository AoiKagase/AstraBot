# CSBot same-plan conversion implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans inline; one fresh most-capable final reviewer, no implementer delegation.

**Goal:** Convert same-plan pending statements and reschedule them exactly as native maintenance.
**Architecture:** Store first phrase identity in the common schedule, add AgreeWithPlan metadata/profile selection, and mutate only stored first part/start during qualified maintenance. Runtime supplies callback-safe RNG/current receipt.
**Tech Stack:** SDK-free C++14 Core; public HLSDK/GameDLL/Metamod-P adapter; Windows/Linux x86 CMake/CTest.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-plan-agreement-design.md

## Global Constraints
SDK-free C++14/public SDK only; no native bodies/private data/ReAPI/new services/exports/assets; NAV read-only. Preserve shared checkout/caches/user files/generated evidence. Explicit owned paths and per-unit localFF, no push/deploy. Complete VS Hostx86/x86 NMake and WSL builds before serialized suites; independent exact7PE/ELF/manifest/identities. Keep known22 and live25 distinct.

## Review Focus
1. First-part identity must come from the actual prepared phrase and distinguish default chatter IDs from same-name Place criteria lookup.
2. Same Place0 converts too; different place is repeatedly delayed, without reordering the linked list or changing creation/expiry/remaining parts.
3. Speaking, invalid, obsolete, different-topic/phrase and already-agreed entries must consume no conversion draw; missing agreement must become a delayed null part.
4. RNG callbacks can reset/recreate own queue or mutate the selected peer/catalog/frame/team; stale completion must not publish or delete fresh nodes.
5. Actual selected-action then second-selection maintenance must deliver the converted audio after delay; Core-only metadata cannot establish production integration.

### Task 1: Stored plan conversion and production maintenance
**Files:** include/astrabot/compat/chatter_catalog.hpp, chatter_schedule.hpp, chatter_statements.hpp, outgoing_radio.hpp; src/adapter/metamod/runtime/behavior/bot_behavior_radio.cpp and bot_behavior_runtime.hpp; tests/chatter_statements_tests.cpp, statement_queue_runtime_tests.cpp; this spec/plan; docs/parity/STATUS.md and IMPLEMENTATION_BACKLOG.md.
**Interfaces:** schedule.firstPhrase uint32 default-catalog identity appended without changing existing aggregates; catalog.phraseId(ChatterPhrase) chatter-only. ChatterPendingMaintenance adds optional random source/timing/agreementPhraseId, queue.maintain(in,current) template with existing one-argument wrapper. OutgoingRadioScheduler qualified maintenance forwards optional context with existing API wrapper. No new virtual/product test API.
- [x] Write real queue tests for native short/long boundaries/repetition/no-reconvert/undefinedPlace, first-only preservation/list order/lifetime, exclusion conditions, missing-agreement/null and RNG unavailable/reset/current predicate. Introduce only compile-time interface declarations/no-op behavior as necessary, then build astrabot_chatter_statements and run ctest -R '^astrabot_chatter_statements$': expected semantic RED before conversion implementation.
- [x] Extend existing registered statement_queue_runtime with a heap real Behavior component bound realPluginservices, public managed peer creation and existing typed escort-decision input. Existing Ack/Help/reset controls remain; expect old repeated escort/no agreement and no proper delay semantic RED. Test callback reset through Engine RNG boundary; no product-only methods.
- [x] Implement catalog identity/Agree metadata, queue stored conversion and revision fences, runtime own/peer/catalog qualification and RNG forwarding in native maintenance order. Format changed hunks. Build focused targets; ctest -R '^astrabot_(chatter_statements|outgoing_radio|chatter_schedule|chatter_statement_playback|statement_queue_runtime|runtime_ports_contract|runtime_ownership_contract)$' expected all PASS. One fresh most-capable read-only review; one Important-Critical RED→GREEN fixpass if needed, no re-review.
- [x] Complete both x86 full target builds and serialized full CTest, independent exact7PE/ELF/manifest/identities. Expected Windows allPASS, Linux known22 only; never weaken assertions/baselines. Update parity scope/results and plan. Explicit commit12ownedpaths, postcommit task-done focused test, localFFmaster and postmerge identity/test receipt. Fullgoalactive07+.

Pre-flight: first phrase identity produced at admission and consumed by both team selection and maintenance; runtime uses the same snapshot. Queue callback cannot retain borrowed iterator across RNG without a revision check. Existing continuous inline/integration authority replaces handoff/approval menus; no new worktree/cache duplication.

Verification receipt: Core15semanticRED before conversion; actual runtime normal2RED with real plan control. Correctly bound Engine peer-guard control1RED then proper source restored/recompiled7/7PASS2.08s. Review0/0/0; Windows135/135PASS12.88s, Linux133/134PASS15.44s(known22only); independent7PE/ELF/manifest PASS. Per-unit commit/localFF receipt is session RESULT.json.
