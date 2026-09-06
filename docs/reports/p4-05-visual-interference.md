# P4-05 — Visual interference

Status: implementation and pre-merge gates complete; main integration pending.
Base main2818a3a; branch codex/p405-visual-interference.
No live acceptance or project-wide Finish is declared.

## Source-backed input contract (recorded before implementation)

SDK pin7ec9b014f8c0a947a724644aebe34eb33706e44b.
ReGameDLL_CS pin679973265e1ac99a43193119e0da212ee568f5f9, MIT LICENSE.
Implementations are original; external source provides behavioral evidence.

| Input / setting | Evidence and interpretation |
|---|---|
| Smoke event | `dlls/wpn_shared/wpn_smokegrenade.cpp:34` precaches events/createsmoke.sc; resolve dynamic index through P4-04 catalog |
| Initial smoke | `dlls/ggrenade.cpp:616-630`, SG_Detonate: null invoker, delay0, detonation origin, zero angles/fparams, iparam1=0, iparam2=1, bparam1 boolean light-smoke, bparam2=0; flags0 or FEV_RELIABLE |
| Recurring smoke puffs | `ggrenade.cpp:493-530`, iparam2=4 and bparam2=6; excluded from new regions to avoid extending lifetime |
| Radius | `game_shared/bot/bot_manager.cpp:106`, smokeRadius115.0; default115 units, configurable. Our contract rejects any segment/sphere intersection, rather than copying that bot's integrated-length threshold |
| Duration | `ggrenade.cpp:526-543,629`, first think+0.1s, one-second cycles through counter20 and retirement on following think. Conservative default22,000,000 microseconds from initial event, configurable; not a client-rendering equivalence claim |
| ScreenFade | `dlls/util.cpp:546-573`: duration/hold/flags as3 shorts, RGBA as4 bytes; duration/hold unsigned16 with4096 units/second; MSG_ONE to entity |
| Flash shape | `dlls/combat.cpp:3-5,165-166`, RadiusFlash passes white and PlayerBlind sends flags0. White255/255/255, nonzero alpha, flags0 only; suppress through duration+hold, including fade tail conservatively |

Black/nonwhite fades, nonzero flags, broadcast fades, generic smoke effects and
unknown playback forms are excluded and diagnosed. A plugin can emit an identical
white ScreenFade: the wire does not prove grenade causation. Capability means the
registered supported input path is available, not complete live effect coverage.
Region positions remain privileged Adapter state, never knowledge of player positions.

## Implementation

`VisualEffects` is SDK-free privileged Adapter state with32 smoke regions and32
receiver-specific flash deadlines. Settings default to115 units and22 seconds.
Overflow conservatively blocks all new visual samples until the newest omitted
region expires. Equal-time/equal-position initial smoke repeats do not add regions;
different-time identical wire events cannot be proven duplicates and are treated
as new. Recurring smoke particle events do not extend initial-region lifetime.

ScreenFade accepts exactly3 shorts and4 bytes, including signed representations
of unsigned16 durations. Optional missing/colliding ScreenFade IDs disable that
capability. `smokeCapability()` reflects the dynamic precache mapping; both
capabilities and effect rejection/expiry/overflow diagnostics are read-only.

StartFrame advances effects before vision. Each geometric sight sample checks
effects before and after the engine trace; no extra trace is added. An effect
revision change during a scan discards that scan's observations while existing
memory still decays. Smoke/flash never supplies or updates a target position.
Map/round changes clear all effects; receiver retirement clears its flash state.
Clock rewind/invalid time blocks new visual samples and ingestion; recovery
preserves existing effect deadlines rather than prematurely erasing the smoke.

## Verification evidence

Portable geometry/lifetime/overflow/clock/generation tests; decoder wire-shape
tests; fake-engine actual hooks through StartFrame to observation and memory,
including effect injection during traces, head/body samples, decay and recovery.
1/8/16 observers with8/16/100ms frames preserve the existing four-scans/frame
budget; blocked samples perform no engine trace. The new tests and existing
vision/memory/identity/sound targeted regressions passed7/7.
An initial fake-engine test dereferenced an unpublished revived observer; the
fixture now waits for the existing staggered schedule and separately asserts that
flash state was retired. The production schedule and acceptance were not relaxed.

| Gate | Pre-merge | Merged main |
|---|---|---|
| Windows x86 portable Debug /WX, inspector ON | 48/48 passed,30.44s | pending |
| Windows x86 Metamod Debug /WX, inspector ON | 59/59 passed,52.62s | pending |
| Linux x86 portable Debug /WX, inspector ON | 47/47 passed,18.92s | pending |
| Release PE32/x86, exactly six exports | passed | pending |

Graph returned no usable call/test relationships; actual sources/tests were
reviewed. FocalSpan was initialized and queried before changes. Full gate logs
will be in each worktree's build-*-x86-test/Testing/Temporary/LastTest.log.
Live render equivalence, event coverage and tuning remain post-Finish acceptance.
