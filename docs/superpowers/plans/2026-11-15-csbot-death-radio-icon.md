# Death Radio icon Implementation Plan

> Use superpowers:executing-plans inline, with one fresh final read-only reviewer.

**Goal:** Native death-only Radio attachment payload/routing/cvar behavior.
**Architecture:** Extend existing explicit DeathInterrupt sendChatterVoice purpose; public model/cvar metadata and receipt guards.
**Tech Stack:** C++14/public HLSDK/Metamod Windows/Linux x86.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-death-radio-icon-design.md

## Global Constraints
No private/ReAPI/new exports/assets, preserve user changes and frozen baselines; complete OS gates distinct from live.

## Review Focus
- Normal voice must never emit an attachment even with all public API capabilities; mapped standard radio stays GameDLL-owned.
- Pain icon follows accepted SendAudio to self/dead teammate/matching spectator only; ignored/dormant/enemy recipients excluded; original slot/pitch retained.
- Optional mp_show_radioicon presence/value and model/WriteCoord availability control only icon, not pain/audio feedback cleanup.
- Model/cvar lookup or packet callback reset/reconnect/round replacement cannot emit old follow-up icon/feedback/silence.
- SDK constants/payload types35/15 and model index are host-derived; no extra wave/RNG/precache, no baseline churn.

## Tasks
1. [x] Persist exact public damage/animation intake audit with current source/SDK identities and counterexamples; no source-text mirror tests for human documentation.
2. [x] RED→GREEN packet regressions in tests/death_interrupt_runtime_tests.cpp (and ordinary chatter fixture if needed), src/adapter/metamod/runtime/behavior/bot_behavior_voice.cpp only production owner. Assert actual temp-entity fields/recipient/order, normal omission, cvar/model/transport gates, callback cancellation.
3. [x] Single fresh final review and one Important/Critical fix pass; full builds/suites and separate PE/ELF/manifest/source/artifact/user identity gates, parity docs, explicit-path commit/local FF master. Full user goal active.

Evidence: .agent-state/sessions/csbot-death-radio-icon-3d5c41c6-07ea-438e-93a9-d7596fb72fa1/; final Windows119/119, Linux117/118 (only unchanged combat_policy baseline), exact7x86 exports PE/ELF, manifest288/281, held source291/artifact2/user4. Final reviewer original0Critical/0Important/1Minor; parent regraded one Important wire-acceptance gap, fixed in one typed-recorder/control-RED/restore-GREEN pass, no re-review. Local integration receipt in RESULT.json. Live and full user goal remain open.
