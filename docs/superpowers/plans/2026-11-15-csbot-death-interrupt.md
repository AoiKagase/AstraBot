# Death interruption audio Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline.

**Goal:** Match native speaking-at-death pain audio from actual public DeathMsg.
**Architecture:** Qualified speaking snapshot before reset, explicit death transport purpose reusing current voice recipients.
**Tech Stack:** C++14 SDK-free Core, public HLSDK/Metamod, Windows/Linux x86.
**Spec:** docs/superpowers/specs/2026-11-15-csbot-death-interrupt-design.md

## Global Constraints
Preserve unrelated files/baselines; no private SDK/ABI changes. Current shared checkout/caches retain explicit owner and fingerprints. Full goal active.

## Review Focus
- Death during callback/reset/reconnect cannot overwrite fresh speech, feedback, silence or team lock.
- Death pending vs already speaking, retained speaking between clip end and next update, round/clock/serial cancellation.
- Native prefix verbosity and default catalog vs profile, default ANY place/CountMany, missing sample no fallback/duration probe.
- Recipient filtering dead teammate/spectator/ignored radio, already dead sender, no pain BotVoice start and old feedback stop.
- Death clears only own lock/pending and duplicate death never repeats; actual DeathMsg transport ordering and frozen callback contracts.

## Tasks
1. [x] Core RED→GREEN: extend ConfirmedChatterSpeech with wasTalking/ownsSpeech supporting dead receipt but retaining owner/epoch/time qualification. Tests actual statement record→death lifecycle and ownership; append Pain enum/catalog name, native prefix verbosity helper. Files friend_death_chatter.hpp/outgoing_radio.hpp; tests/death_interrupt_tests.cpp; CMake/manifest.
2. [x] Runtime RED→GREEN: copy previous speech in observePublicDeath, reset before callbacks, finishDeathChatter performs qualified own team lock release/silence and voice transport. Extend sendChatterVoice explicit purpose, ANY/-1, no duration/feedback; add registered DeathMsg actual SendAudio tests tests/death_interrupt_runtime_tests.cpp and default-missing mode, lifecycle/recipients/reentrancy matrix. Update runtime header and existing perception/voice files, ownership only if necessary. Run related/frozen contracts.
3. [x] One fresh final read-only reviewer; one Critical/Important RED→GREEN fix pass, minors/rulings ledgered. Freeze source identity; full x86 OS builds/suites serialized, PE/ELF7exports/manifest; docs, explicit-path commit and local FF master.

Final verification:0Critical/4Important/0Minor, all4Important reproduced as5actual runtime failures and fixed in one combined RED→GREEN pass; focused4/4 PASS0.28s, then full Windows119/119 PASS9.22s and Linux117/118 PASS13.23s (unchanged combat_policy failure). Separate PE/ELF x86 exact7exports and manifest288/281 PASS. Source291/artifact2/user4 fingerprints retained. Local commit/FF receipt in session RESULT.json. Full goal active; native Radio UI/global scheduling and exact injury intake remain open; live not executed.
