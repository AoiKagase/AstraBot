# CSBot Noise Look Implementation Plan

> Use superpowers:executing-plans for inline implementation.

**Goal:** React visually to recent heard public sounds using pinned CSBot timing and look priorities.
**Architecture:** SDK-free noise-look policy/state; public sound receipt timestamps; behavior-owned request/angle state consumed at existing command cadence. Reuse adapter line-bending geometry from guard look.
**Tech Stack:** C++14, public GoldSrc/Metamod-P, Windows/Linux x86.
**Spec:** docs/superpowers/specs/2026-11-14-csbot-noise-look-design.md

## Constraints and review focus
Preserve unrelated edits, oracle read-only, no private offsets/deployment/push. Keep seconds separate from frame IDs; no world scans. Verify enemy/objective/breakable priorities, duration-after-alignment, movement projection, stale/future receipt times, blocked/no-NAV cases and actor/round reuse.

## Task 1: Clock and policy
- [x] Add failing pure noise-look tests and perception receipt-time retention test.
- [x] Implement include/astrabot/compat/noise_look.hpp; append explicit timestampSeconds to sound/noise memory, populate/copy public time and known-shot priority.

## Task 2: Command integration
- [x] Add failing actual EmitSound-to-RunPlayerMove regression with no invented fire and preserved movement.
- [x] Share bounded look-bending geometry, add behavior noise owner and narrow command service; gate/reset it through existing lifecycle and look priorities.

## Task 3: Verification
- [x] Independent read-only review; update source/ownership manifests.
- [x] Complete both x86 builds, sequential CTests, PE/ELF and exact source/evidence records. Keep native acceptance separate and known Linux combat-policy baseline failure explicit.

Final: Windows96/96 PASS, Linux94/95 PASS (pre-existing combat_policy only), PE/ELF x86 seven exact exports PASS. Frozen baselines unchanged; no native acceptance/deployment/push. See docs/parity/STATUS.md and session receipts for public-feed limitations.
