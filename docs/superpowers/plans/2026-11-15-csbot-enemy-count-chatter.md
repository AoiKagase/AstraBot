# Enemy Count Chatter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline.

**Goal:** Reproduce event-driven remaining-enemy count statements through public DeathMsg and host-owned audio.
**Architecture:** Core EnemyCountChatterPolicy owns one redundant topic; adapter validates public recipients/teams and supplies event/current counts. Existing outgoing scheduler/transport arbitrate by start time and transmit dynamic count phrases.
**Tech Stack:** C++14, HLSDK/Metamod-P, MSVC/GCC x86/CMake.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-enemy-count-chatter-design.md

## Global Constraints
Pinned oracle read-only; public interfaces only, SDK-free Core, exact7exports, no frozen fixture regeneration, explicit staging, preserve unrelated edits/profile. Full HostX86/x86 VS build before CTest; OS CTests sequential. No push/deploy. Continuation and commit/local FF explicitly authorized; existing inline execution retained without repeated approval.

## Review Focus
- Broadcast vs targeted DeathMsg; reused/invalid source/observer and no hidden target attribution.
- RNG draw before mute/duplicate filtering; delayed dynamic count and earliest statement retention.
- Strict time boundaries and cancellation across death/revival,round,map,clock and actor generations.
- Any count phrase is one shared team topic; unrelated chatter remains queued and ordered by start.
- Host metadata/modes,actual audio transport,no replay; unchanged characterization/per-frame overhead.

### Task1: policy and count phrases
Files: new include/astrabot/compat/enemy_count_chatter.hpp, tests/enemy_count_chatter_tests.cpp; outgoing_radio.hpp count phrases/name helper,cached contact counts accessor and scheduledStart metadata; CMake/manifest.
Interfaces: EnemyCountChatterPolicy::observe(EnemyCountDeathInput,ICompatibilityRandomSource&), update(EnemyCountSpeechInput) -> EnemyCountSpeechDecision, reset/discard; owns event report lifetime, earliest duplicate topic and draw ordering.
- [x] Write focused policy tests and run missing-header RED.
- [x] Implement native gates/time/dynamic selection; focused GREEN plus existing outgoing/chatter tests.

### Task2: actual death receipt and transport
Files: observation capture, IObservationRuntimeServices/PluginRuntime forwarder, behavior header/new bot_behavior_death.cpp, behavior radio/perception/runtime lifecycle, ownership fixture; new enemy_count_chatter_runtime_tests.cpp/CMake/manifest.
Interfaces: behavior_observePlayerDeath(victimSlot,killerSlot,recipient,frame), BotBehaviorRuntime::observePlayerDeath same. Public victim reset remains separate.
- [x] Write registered message to SendAudio integration test; confirm no-output RED.
- [x] Validate/route source and recipients, calculate cached nearby count and teams, queue policy; integrate dynamic topic with existing sender by start-time and clear lifecycle.
- [x] Runtime GREEN for dynamic count,recipient identity/modes/team/lifecycle and no hidden target commands.

### Task3: review and delivery
- [x] One independent final read-only review; reproduce/fix Important/Critical RED→GREEN in one pass.
- [x] Full Windows/Linux x86 builds; sequential full CTests,7export PE/ELF,manifest/ownership/ports/architecture/whitespace and unchanged baselines/profile/source fingerprints.
- [x] Document result/backlog; commit exact paths/local FF master; confirm delivered source/artifact identity; rerun tests only if delivered bytes differ.

Review:1 Important premature context consumption found; RED assertions retained a due count4 report behind an older actual generic scheduler topic, then required dynamic3 at102.4. Fix separates always-run lifetime maintenance from canStart-only speech evaluation. No Critical/Minor or deferred findings.

Verification ruling: after fast-forward, byte-identical277 inputs/artifacts retain the completed suite evidence; avoid redundant CTest runs when no content changes.

Final gates Windows106/106, Linux104/105 only unchanged combat_policy baseline; manifest274/267,source277/exports/user profile identity confirmed. Commit/local FF deliver the same tested bytes; no push/deployment/live acceptance.
