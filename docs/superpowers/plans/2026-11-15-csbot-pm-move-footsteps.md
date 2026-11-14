# PM_Move Footsteps Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline.

**Goal:** Hear real PM_Move footsteps through public SDK callbacks while preserving audio and callback ownership.
**Architecture:** Bounded adapter-owned MovementSoundBridge wraps PM_PlaySound for paired PM_Move scopes; PluginRuntime validates lifetime/source and sends explicit-origin footsteps to observation. Existing Core hearing/equipment remains SDK-free.
**Tech Stack:** Existing C++14, HLSDK/Metamod-P, CMake MSVC/GCC x86.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-pm-move-footsteps-design.md

## Global Constraints
Pinned references read-only; no private layouts/offsets/physics copy. Preserve original callback arguments and seven exports, user edits/profile and frozen fixtures. Same VS HostX86/x86 complete build/test; OS CTests serialized.

## Review Focus
- Original audio still called exactly once on rejected AI input and both nesting forms.
- Callback restoration, foreign replacement, overflow/unmatched post and active detach.
- Pre/callback source reuse, dead/spectator/client prediction, map/round/clock identity.
- Live movement origin without edict mutation; dual-feed dedup/raw sniper distance.
- Existing audio/perception/ports/ownership and frozen characterization.

### Task1: Scoped callback bridge
Files: new adapter bridge/movement_sound_bridge.hpp and tests/movement_sound_bridge_tests.cpp; CMake/source manifest.
Interface: begin(move,source,map,round,server,thunk), end(move), reset(), active(), current(), forward(channel,sample,volume,attenuation,flags,pitch).
- [x] Write scoped callback tests for forwarding/nesting/overflow/restore/foreign/reset.
- [x] Build focused target; Expected missing bridge RED.
- [x] Implement SDK-only bridge and run focused tests Expected PASS.

### Task2: Runtime intake
Files: plugin runtime header/cpp, observation header/cpp, ownership manifest; new pm_move_footstep_runtime_tests.cpp and CMake target.
Interfaces: HookPmMove/HookPmMovePost/thunk; onPmMove/End, onMovementSound. Observation onEmitSoundAt(...,world position) reuses sound path.
- [x] Write actual registered callback runtime tests; Expected missing pre/post hooks RED.
- [x] Wire scoped source/lifecycle validation and explicit-origin known footsteps, restore on lifecycle events and refuse active detach.
- [x] Runtime intake/noise/equipment/duplicate and bridge tests Expected PASS.

### Task3: Review and delivery
- [x] Independent final read-only review; Important/Critical fixes RED→GREEN.
- [x] Complete both x86 builds; sequential full CTests; exact PE/ELF7exports, unchanged baseline, manifest/ownership/ports/whitespace. Expected Windows all PASS; Linux only known combat_policy baseline.
- [x] Fingerprint tested sources/artifacts and userprofile, document limits and outcomes; explicit-path commit/local FF, no push/deploy.

## Ledger
Ruling: user continuation authorizes execution and established local commit/merge workflow without repeat approval. Existing checkout/caches retained; all unrelated changes preserved.

Review: one Important (nested foreign saved-thunk audio chain), reproduced RED and fixed GREEN with wrapper/original call-count and restoration regression, including an extra own nested scope. Zero Critical/Minor and no deferred findings.

Final gates: Windows104/104, Linux102/103 (only unchanged combat_policy baseline), exact x86 PE/ELF7exports, manifest270/263, source273 and preserved user/profile identity all confirmed. Local commit/FF delivery; no push/deployment or live acceptance.
