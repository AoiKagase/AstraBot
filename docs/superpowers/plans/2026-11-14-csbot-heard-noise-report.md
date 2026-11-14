# CSBot HeardNoise Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline.

**Goal:** Report investigated low/medium noise to teammates with pinned CSBot timing and place.
**Architecture:** SDK-free report admission feeds existing bounded outgoing scheduler; adapter supplies observed timing and NAV place. Extend existing host voice transport only.
**Tech Stack:** existing C++14, Metamod-P public SDK, CMake/MSVC and GCC x86.
**Spec:** docs/superpowers/specs/2026-11-14-csbot-heard-noise-report-design.md

## Global Constraints
Read-only pinned oracle; preserve SDK-free Core and seven exports. Public engine observations only, no pdata or offsets. Host-owned assets only. Preserve unrelated edits, BotProfile.db and frozen characterization. Windows complete build within VsDevCmd HostX86/x86; CTests serialized across OS.

## Review Focus
- Failed chance/off mode must still consume cooldown; High noise draws nothing.
- Report place must survive speaker movement and expire at strict >5 seconds.
- Same/different/undefined team place duplication.
- Actor/map/round/clock/NAV/death invalidation even during combat or stopped AI.
- Existing voice fallback, recipient isolation and characterization remain intact.

### Task 1: Admission and scheduler
Files: new include/astrabot/compat/noise_report.hpp, tests/noise_report_tests.cpp; modify outgoing_radio.hpp, chatter_voice.hpp as needed, CMakeLists.txt, source-manifest.json.
Interfaces: NoiseReportPolicy::attend(input,rng)->bool; HeardNoiseReport carrying actor/frame/created/place/nav revision, OutgoingRadioInput::noiseReport and navRevision/roundOver, OutgoingRadioDecision::place.
- [x] Write policy/scheduler tests for >30 enemy gap, priority, round-over, cooldown20 including failed chance, 33 threshold, invalid RNG, reset, original expiry5 and place dedup.
- [x] Run focused target; Expected: feature missing RED.
- [x] Implement policy and HeardNoise enum/catalog/scheduler integration; preserve old enum values.
- [x] Focused policy/outgoing/chatter suites; Expected: PASS.

### Task 2: Runtime attendance and voice
Files: investigation/radio/voice/runtime behavior source/header, plugin_runtime.hpp public service getter, round_lifecycle_tracker.hpp, service/fixture ports, relevant runtime tests/ownership manifest.
Interfaces: successful consume publishes report event; service_roundOver()->bool uses public tracker. sendChatterVoice accepts explicit report place only for HeardNoise.
- [x] Add actual attendance/transport test with low/medium vs High, original place, cooldown and Round_End suppression; Expected: RED before wiring.
- [x] Wire current public timings, RNG request, NAV place/revision, resets and outgoing payload.
- [x] Focused runtime and unchanged characterization; Expected: PASS.

### Task 3: Review and delivery
- [x] Independent read-only whole-change review; fix Important/Critical with RED→GREEN.
- [x] Complete Windows/Linux x86 builds; sequential CTests; exact PE/ELF7 exports, source/ownership/ports/whitespace gates. Expected: Windows all PASS, Linux only known combat_policy baseline.
- [x] Record hashes/source inputs/profile preservation and limits in STATUS; explicit-path commit and local FF merge, no push/deploy.

## Execution ledger
Ruling: continuing implementation request supplies execution authorization; no repeated design/merge approval. Established current-checkout branch workflow preserves existing caches and user edits.

Task1: complete — missing-header RED, policy/outgoing/voice3/3 PASS; clock catch-up behavioral RED→GREEN.
Task2: complete — absent runtime report RED, actual attendance/place/High/off-on/round transport GREEN.
Task3: complete — fresh independent review3 Important fixed RED→GREEN; final Windows102/102 and Linux100/101 (only known baseline) plus artifact/ownership/source gates. Final explicit commit/local FF is the enclosing delivery operation.
Final: Ruling: production lifecycle observes every frame and advances round generation on clock rollback, resetting the attendance policy on next call; standalone callers must supply current generations. Cost if wrong: old20-second cooldown may remain. No deferred production minors.
