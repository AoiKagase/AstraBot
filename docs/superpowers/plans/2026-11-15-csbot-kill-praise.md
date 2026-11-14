# Kill Praise Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline.

**Goal:** Add all three native kill-congratulation variants through validated public death receipts and existing speech transport.
**Architecture:** Core KillPraisePolicy retains a single native redundant topic. Metadata-only load preserves phrase definitions/importance without consuming shuffle RNG. Adapter point visibility/public Career and event sequence create receipts; a three-way arbiter preserves generic/count/praise scheduling and transport ownership.
**Tech Stack:** C++14, HLSDK/Metamod-P, MSVC/GCC x86, CMake.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-kill-praise-design.md

## Global Constraints
Pinned references read-only, no SDK Core/private data/new exports, existing checkout/caches, preserve user edits/profile/frozen inputs. Same VS HostX86/x86 full build before CTest. OS CTests serialized. Explicit-path commit/local FF per user; no push/deploy. Full goal stays active beyond this item.

## Review Focus
- Count draw precedes praise delay/chance; lazy asset admission consumes no shuffle RNG.
- Victim-point FOV/LOS/smoke/blind and public source/recipient generation guards; dead victim is valid.
- Original strict3 lifetime and duplicate retention, death/revival/round/map/clock/serial cancellation.
- All three waiting topics maintain lifetime before earliest-start selection; no premature context consumption or loss of unselected statements.
- Metadata known vs actual wave availability, MINIMAL Important, transport failures, team topic redundancy and no stale replay.

### Task1: Core policy and phrase metadata
Files: new include/astrabot/compat/kill_praise.hpp, tests/kill_praise_tests.cpp; outgoing_radio.hpp three phrase names/helper/default-definition query, chatter_voice.hpp native unspecified-count normalization and tests; CMake/source manifest.
Interfaces: KillPraisePolicy::observe(KillPraiseDeathInput,ICompatibilityRandomSource&), update(KillPraiseSpeechInput)->KillPraiseDecision, active/scheduledStart/discard/reset. Core event captures immutable phrase/delay with actor/frame/entity generation.
- [x] Write eligibility/draw/admission/time/duplicate/lifetime policy tests and confirm missing-header RED.
- [x] Implement policy plus defined/important phrase metadata and run policy/catalog GREEN.

### Task2: Public runtime integration
Files: behavior death/radio/voice/runtime/perception/header, ownership fixture; new kill_praise_runtime_tests.cpp/CMake/manifest.
Interfaces: unchanged playback chatterBank(index), ensureOutgoingChatterCatalog(), allowedKillPraisePhrases(verbosity) using default metadata only, point visibility private helper; existing observePlayerDeath port unchanged. ActorState::killPraise holds policy. Three-way start-time selection owns confirm/discard for exactly one selected topic.
- [x] Write registered DeathMsg-to-SendAudio test; confirm no-praise RED.
- [x] Add public point visibility and Career/admission inputs after existing count event draw; lifecycle clearing, metadata-only default-catalog admission, three-way speech arbitration and common transport.
- [x] Runtime GREEN for variants/RNG/visibility/admission/lifetime/coexisting topics and unchanged generic/count behavior.

### Task3: Review and delivery
- [x] One independent final read-only review; Important/Critical RED→GREEN in one fix pass, record all findings.
- [x] Full x86 builds and sequential suites, exact7exports, manifest/ports/ownership/architecture/whitespace, unchanged baseline/frozen inputs/profile/source/artifact identity.
- [x] Update STATUS/backlog, exact-path commit and local FF; confirm byte identity after merge, rerun only if bytes change. Leave full goal active.

Review:0 Critical,3 Important,1 Minor. Corrected native undefined-count sentinel/undefined place/default metadata authority; event admission no longer loads profile audio. Named NAV/conflicting flags/default-missing tests RED then GREEN7/7. Same fixed death criteria also repair remaining-count transport. No deferred findings or re-review.

Final gates: Windows109/109, Linux107/108 (unchanged combat_policy baseline), source280/artifact/user fingerprints, PE/ELF7exports and source277/270/ports/ownership/architecture all confirmed. Test-only MSVC environment reader uses owned CRT copy, no warnings disabled. Local commit/FF reuses byte-identical tested inputs; full goal remains active.
