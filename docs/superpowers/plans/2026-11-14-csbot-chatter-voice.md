# CSBot Chatter Voice Implementation Plan

> Use superpowers:executing-plans for inline implementation.

**Goal:** Play existing outgoing chatter through host-owned voice banks with CSBot transport and fallback behavior.
**Architecture:** SDK-free bounded voice catalog, existing profile/scheduler selection, public adapter SendAudio/BotVoice transport. Behavior owns catalogs/feedback; observation forwards public radio preference receipts.
**Tech Stack:** C++14, public GoldSrc/Metamod-P, Windows/Linux x86.
**Spec:** docs/superpowers/specs/2026-11-14-csbot-chatter-voice-design.md

## Constraints and review focus
Preserve unrelated files; read-only oracle; no private layouts/deployment/push. Check named-bank inheritance, Count/Place matching, absent files/messages, recipient/ignore/observer isolation, feedback cancellation and no asset I/O per frame.

## Task 1: Voice catalog and profile
- [x] Add failing voice catalog tests for BaseDir, criteria, cyclic variants and malformed/oversized data; add VoiceBank profile inheritance regression.
- [x] Implement include/astrabot/compat/chatter_voice.hpp and profile parsing/copy/validation. Existing radio catalog remains the source of fallback metadata.

## Task 2: Public transport
- [x] Add actual adapter regression for normal/minimal/radio/off, named bank/pitch, same-team/dead/observer recipients, ignore receipt, duration/team blocking and feedback cleanup.
- [x] Implement behavior voice owner and wire outgoing scheduler readiness without requiring a radio mapping when a voice exists. Observe public preference receipts; update feedback from existing frame boundary with zero-active fast exit.

## Task 3: Verification
- [x] Read-only review against pinned CSBot; update source/ownership inventories.
- [x] Complete Windows and Linux x86 builds, sequential CTests, PE/ELF checks and evidence records. Native audio acceptance remains open.

Final gates: Windows 94/94 PASS; Linux 92/93 PASS with the documented pre-existing combat-policy failure only. PE/ELF PASS. Installed host database parse PASS. Both reviewer findings reproduced RED then fixed GREEN. Native audio acceptance remains open; see docs/parity/STATUS.md.
