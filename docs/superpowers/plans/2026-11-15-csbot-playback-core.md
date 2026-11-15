# CSBot playback Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans inline; one fresh final source review, one Important/Critical RED→GREEN fix pass, no re-review.

**Goal:** Complete repair unit01 as an independently tested SDK-free playback state machine.
**Architecture:** Inline max4 ChatterProgram and mutable ChatterPlaybackState; value decisions and state-scoped opaque step receipts, no callbacks. Existing queue/actor qualification and transport consume it in03; Place definitions/history are02.
**Tech Stack:** C++14; Windows/Linux x86.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-playback-core-design.md

## Global Constraints
No private/SDK Core dependencies, new exports/assets, user overwrite, frozen baseline regeneration, push/deployment. Preserve pending02/03 evidence under parent compound session. Established shared checkout/caches/explicit paths/fingerprints retained. Full native and live goal remains active.

## Review Focus
- Four-part append/null/implicit-place payload and program/state lifetime assumptions cannot lose or read beyond the intended program.
- Strict zero/duration/completion/accumulation boundaries and one part per selected invocation match native Update, with counts evaluated at start.
- Short/long delay bounds reach the float32 RNG boundary exactly; cancellation retries context without accepting stale completion.
- No-friends versus Emote, invalid/nonfinite timing, null dynamic contexts and empty programs have safe deterministic outcomes.
- Bare step tokens are state-scoped; future owner/node wrapping is explicit, with no premature runtime acceptance claim.

### Task1: Core progression RED→GREEN
Files: include/astrabot/compat/chatter_playback.hpp, tests/chatter_playback_tests.cpp, CMakeLists.txt/docs/source-manifest.json.
Interfaces: ChatterProgram.append(ChatterPart) max4; advanceChatterPlayback(program,state,input) returns Waiting/Phrase/Delay/Null/Finished and part/count/RNG bounds/step. finishChatterPlaybackPart(state,step,duration,skip), finishChatterPlaybackDelay(state,step,duration), cancelChatterPlaybackStep(state,step) require the in-flight matching revision. Program immutable/state scoped as spec.
- [x] Pure tests written before behavior; interface stub fails first phrase/logical speaking.
- [x] Diagnose/fix delay test at actual float32 boundary and run astrabot_chatter_playback GREEN. Preserve old RED/diagnostic evidence.
- [x] Test remaining review input classes with literal native expectations, including missing/null and stale in-flight steps; production code only with prior relevant RED.

### Task2: Review, complete gates and local integration
- [x] Fresh whole-unit source review after holding inputs; reproduce/fix Important/Critical once, no re-review.
- [x] Complete VS18 Hostx86/x86 and WSL builds before full CTests; serialize suites. Independent PE/ELF exact7, manifest and final source/artifact/user checks.
- [ ] Parity status/backlog records01 complete/02 onward open; explicit stage/commit/local FF, postmerge refs/identity and RESULT.json.
