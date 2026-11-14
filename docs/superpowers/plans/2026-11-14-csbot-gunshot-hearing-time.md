# CSBot gunshot hearing time implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Observe native gunshots and make hearing time independent of server FPS.
**Architecture:** Public event index catalog feeds shared bounded sound ring; optional seconds context drives perception memory.
**Tech Stack:** C++14, public HLSDK/Metamod, Windows/Linux x86, CMake/CTest.
**Spec:** docs/superpowers/specs/2026-11-14-csbot-gunshot-hearing-time-design.md

## Global Constraints
Seven exported ABI symbols; no private offsets; preserve unrelated edits and frozen baseline; offline evidence does not establish live acceptance.

## Review Focus
- Map precaching precedes activation; map event indices must not leak.
- Engine override results and event flags must not misclassify custom events.
- Low/high FPS, delayed publication, rewind and time boundaries.
- Same-frame dual feed and long-range AWP/silencer behavior.
- Moving observer, self/friendly sources and source generation reuse.

### Task 1: Seconds-based perception hearing
Files: perception.hpp/perception.cpp, world_snapshot.hpp, perception_tests.cpp.
Interface: PerceptionInput::setHearingTime(float nowSeconds,float reactionSeconds); AudibleEvent::hearingRange (0 legacy).
- [x] Add and run failing seconds/FPS/boundary tests.
- [x] Implement optional seconds clock, 3-second replacement/current observer distance, 20-second expiry and invalid/rewind handling.
- [x] Run perception regression tests.

### Task 2: Native public gun event feed
Files: compat/gunshot_events.hpp; plugin_runtime.hpp/.cpp; observation runtime; tests/gunshot_event_tests.cpp and gunshot_runtime_tests.cpp; CMake, source manifest, ownership fixture.
Interface: GunshotEventCatalog::observe(index,script), range(index,bparam1,bparam2), reset(); runtime engine post table with PrecacheEvent callback and pre PlaybackEvent.
- [x] Add and run failing catalog and actual callback tests.
- [x] Share sound ring receipt insertion/dedup, public source validation, native range; connect real-time/profile context.
- [x] Test lifecycle/flags/teams/dedup/long-range and existing noise look.

### Task 3: Review and delivery
- [x] Fresh independent read-only review per executing-plans.
- [x] Full x86 builds, sequential OS CTest, separate PE/ELF, manifest/contracts/diff; keep pre-existing Linux combat failure distinct.
- [x] Update parity/evidence; explicit path commit, local FF merge to master, no push/deploy.

## Completion ledger
Task1: complete — perception RED (missing API) to GREEN,20/100/500FPS and reaction/replacement/expiry boundaries.
Task2: complete — catalog missing-header RED and actual missing-post-hook RED to GREEN; map/dual-feed/range/team/serial tests pass.
Task3: complete — independent review corrected host-only playback and explicit AWP validation, plus invalid source serial; all regression cases RED→GREEN. Full Windows98/98, Linux96/97 with unchanged pre-existing combat policy failure; independent PE/ELF gates PASS.
Final review: /root/gunshot_hearing_review, gpt-6-astra high; no unresolved findings.
Final rulings and costs: NAV randomization/exact investigate remain existing tactical differences; PM_Move footsteps/custom delayed events remain unimplemented intake; live acceptance is pending and cannot be inferred from offline tests.
