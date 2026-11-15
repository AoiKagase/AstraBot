# CSBot delayed acknowledgement reset implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans inline; one final fresh most-capable reviewer, no implementer delegation.

**Goal:** Cancel old deferred and unprepared human acknowledgements at same-owner execution reset without losing current orders.
**Architecture:** Keep OutgoingRadioScheduler interfaces; bind reserved acknowledgement to its admission and clear it on discard/reset. Observation invalidates stored input with its own reset revision and immutable receiving-message snapshot; no new service port or test-only method.
**Tech Stack:** C++14 SDK-free Core; public HLSDK/GameDLL/Metamod-P adapter; Windows/Linux x86 CMake/CTest.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-ack-reset-design.md

## Global Constraints
SDK-free C++14 Core/public HLSDK GameDLL Metamod-P only; no native bodies/private data/ReAPI/new exports/assets; NAV read-only. Preserve shared checkout/caches/user files/generated evidence. Explicit owned paths, no push/deploy. Full VS Hostx86/x86 and WSL target builds before serialized suites; separate exact7PE/ELF/manifest/identity. Keep known Linux22 and live25 distinct.

## Review Focus
1. Armed-but-not-queued delay survives a discard today: Core old sequence stays cancelled, fresh higher sequence still acknowledges.
2. Same actor/round with changed admission lifetime, connection or entity must cancel old reservation; advancing ordinary ticks/time must preserve valid delay.
3. Unprepared command and a packet spanning reset must not transfer into the new input life; actual observation decoder/reset controls retain same actor/serial/frame.
4. Engine sender lookup can reset input and deliver a newer command; immutable message copies and source sequence baseline preserve the fresh receipt.
5. First order/normal speech completion/same-time identical new order must remain eligible; no speech-counter coupling, coordinate leak or extra callback-capable lookup.

### Task 1: Core reservation and public radio-input lifecycle
**Files:** include/astrabot/compat/outgoing_radio.hpp; src/adapter/metamod/runtime/observation/bot_observation_runtime.hpp/.cpp; tests/outgoing_radio_tests.cpp, statement_queue_runtime_tests.cpp and fixtures/runtime_ownership.json; this spec/plan and parity STATUS/backlog.
**Interfaces:** prepare/discardPending/cancelOnDeath signatures retained. Save ChatterScheduleAdmission for the armed Ack and compare actor/entity/connection/map/round/lifetime with forward tick/time. Observation adds private invalidateRadioInput(index), per-actor radio-input revision and MessageBegin receiver-revision snapshot; scenarioRadio getter API unchanged.
- [x] Write existing-API Core tests for discard-before-maturity, same-source non-rearm/fresh source, owner revision/entity/connection changes and forward-time positive controls. Run astrabot_outgoing_radio: expected semantic RED before product changes.
- [x] Extend the registered statement-queue runtime fixture after its existing Ack→Help control. Use actual heap-allocated BotObservationRuntime bound to real Plugin services, production message/reset/remember calls and unchanged actor/serial/frame; assert old unprepared and spanning packets drop, same-time fresh receipt survives, reset/newer receipt during sender lookup is preserved. Expected semantic RED on old retained input; test doubles only at Engine boundary.
- [x] Implement reservation cancellation/qualified maturity and observation input revision/reset/snapshot/callback fences, preserving default legacy admission0 and existing source sequence behavior. Update owned-state inventory only for new adapter members.
- [x] Build focused Core and actual fixture; expect all new/existing controls PASS. One final fresh most-capable read-only review, one Important-Critical RED→GREEN pass if needed; no re-review.
- [x] Full Windows/Linux x86 target builds, serialized full CTest (known22 only), independent7PE/ELF/manifest and fingerprints. Record remaining23/25 scope and evidence, update this task, explicit commit/localFF master with postmerge identity receipt; continue06/fullgoalactive.

Pre-flight: Core and input invalidation form one reset contract; no separate task interfaces. Tests exercise production components without exposing the private Plugin reset or regenerating frozen baselines. Existing continuous execution/integration authority needs no renewed handoff menu.

Verification receipt: initial Core8/runtime7 RED before production; extra mutation2 RED then restored/recompiled focused4/4PASS2.19s. Final review0/0/0; Windows135/135PASS11.18s, Linux133/134PASS15.57s (known22 only), independent7PE/ELF/manifest PASS. Commit/localFF identity is recorded in the unit session RESULT.json after integration.
