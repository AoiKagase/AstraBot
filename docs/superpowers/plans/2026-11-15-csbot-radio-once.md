# CSBot one-shot radio implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline. One final fresh most-capable reviewer, no implementer delegation.

**Goal:** Make represented standard and direct VIP radio attempts follow native one-shot timing without receipt dependence.
**Architecture:** Keep03stored statement/part tokens and shared team arbitration. Remove obsolete retry/receipt owners and add the used shared per-team FollowMe event time; preserve public command callback fences.
**Tech Stack:** C++14 Core, public HLSDK/GameDLL/Metamod-P, Windows/Linux x86 CMake/NMake/CTest.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-radio-once-design.md

## Global Constraints
SDK-free C++14 Core; public HLSDK/GameDLL/Metamod-P only; no native bodies/assets/private data/ReAPI/new exports. NAV read-only. Shared checkout/caches/user evidence preserved; explicit paths only; no push/deploy. Full VS Hostx86/x86 and WSL target sets before serialized full suites; separate PE/ELF exact7 gate. Keep known Linux combat_policy22 and full goal active.

## Review Focus
1. Declined quota/no SendAudio must issue one menu/select/close and still retain2+.1; test actual compound fixture in radio and missing-custom-API modes.
2. Strict deadline and separate completion must not become immediate retry or same-update continuation; test deadline and next part via actual packets/commands.
3. Round/death/reconnect during any command must stop stale continuation and preserve fresh nodes; test command-boundary round cases plus existing death/reset fixtures.
4. Direct FollowMe clock must be team-shared, strict>30, zero-initialized/reset map/round, and independent of Chatter silence; test Core and real VIP decline.
5. Missing mapping, no listeners, all ignored, accepted receipt and reentrant admissions must preserve03 behavior; run new/existing voice/death/queue suites and full frozen/port/ownership gates.

### Task 1: One-shot public radio and ownership cleanup
**Files:** Modify include/astrabot/compat/outgoing_radio.hpp; runtime/behavior/bot_behavior_radio.cpp, bot_behavior_voice.cpp, bot_behavior_runtime.hpp/.cpp, bot_behavior_perception.cpp; runtime/shared/bot_runtime_services.hpp; runtime/observation/bot_observation_runtime.cpp; plugin/plugin_runtime.hpp under src/adapter/metamod; tests/outgoing_radio_tests.cpp, compound_chatter_runtime_tests.cpp, friend_death_chatter_runtime_tests.cpp and fixtures/runtime_ownership.json. Add this spec/plan, update parity STATUS/backlog.
**Interfaces:** OutgoingRadioInput gains float teamFollowRadioAt=0 (public Core data); prepare/confirmSent retain signatures. BotBehaviorRuntime owns std::array<float,kTeamRadioSlotCount> outgoingTeamFollowRadio_{}; no receipt observer method/virtual or retry members remain. ChatterPendingQueue::advance/currentPart/finishPart continue exact token qualification; no retry currentPart reuse.
- [x] Write actual declined mapped Place→accumulation→count tests: one radio3/menuselect4/menuselect10; repeated+.25 no retry; exact first+2.1 no next part; later accumulation/current count emits one mapped3/2. Repeat with missing custom WriteString in normal mode. Add fresh CommanderDown survival for round callbacks at each command boundary.
- [x] Add actual VIP test without public receipt, one attempt over multiple ticks; Core tests first/exact30 blocked, later allowed, a new owner honors teamFollowRadioAt, and direct confirmation does not postpone ordinary RequestReport. Add input field only before semantic Core RED; do not implement behavior before RED.
- [x] Run focused Linux targets/tests; expected semantic RED on old retries/clock/silence. Retain output and distinguish setup errors.
- [x] Remove receipt/attempt state/plumbing/ownership entries; implement one command sequence with per-boundary exact receipts, native pre/post silence order, duration2+.1 and direct consume/time behavior. Keep existing interfaces beyond explicitly removed internal observation port.
- [x] Run focused actual/Core/voice/death/queue controls; expected PASS. One final read-only most-capable review, one Important/Critical RED→GREEN pass, no re-review. Then full Windows/Linux x86 target builds, serialized full CTest (known22 only), exact7 PE/ELF, manifest and fingerprints.
- [x] Update parity records and this task, explicitly stage owned paths, commit fix: align standard radio with CSBot one-shot dispatch; localFF master, verify refs/owned/user/artifact identities and save RESULT.json. Continue05, full goal active.

Pre-flight: no separate task interfaces. Native fidelity decides callback clock/order questions; continuous inline method and existing per-unit integration authority require no renewed handoff menu.
