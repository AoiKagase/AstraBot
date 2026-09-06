# P4-04 — Anonymous sound observations

Status: implementation and pre-merge gates complete; main integration pending.
Base main: `a4f7c50`. Branch: `codex/p404-anonymous-sound`.

## Contract and implementation

`SoundMemoryModel` owns anonymous observations independently of visual memory.
The published value contains observation provenance, sound kind, a 256-unit grid
cell and confidence. Neither emitter ID nor exact emitter position crosses the
Adapter boundary. Sound never updates a person's lastSeen or identifies a target.
The initial confidence is 0.5, linearly expiring three seconds after the original
event, including time spent queued. The Core retention setting is nonzero and
configurable; the Adapter uses the approved three-second default.

The Adapter queues 256 events FIFO and drops new arrivals when full. It processes
at most 32 events and 1,024 audience checks per frame. Each event snapshots up to
32 eligible listeners at ingestion, so movement before the next StartFrame does
not change historical audibility. Each observer retains the newest 16 sounds;
Core decay visits at most 512 entries per frame. Capture itself visits at most
32 listeners and 256 duplicate keys per callback; callback frequency is supplied
by the engine and is not a claimed fixed per-frame CPU budget. Event mapping is
bounded to 128 names; hook nesting retains at most 32 validation records.

Map/round boundaries retire queued sounds and memory. Receiver death, spectator
state, team change, removal, disconnect and generation/agent changes retire its
memory and audience epoch. Anonymous sound has no emitter lifecycle association.
Invalid clocks clear pending state and memory, retaining time/sequence guards.
Source sequences continue across transient resets. Core retains rejection guards
when clearing snapshots, preventing replay of accepted sound after clock recovery.

The read-only API is `LifecycleCoordinator::sound().memory().latest(observer)`;
the snapshot remains owned by the model and should be read before its next mutation.
Adapter and Core diagnostics expose queue size, per-frame work, accepted/expired/
evicted/retired observations, invalid/unknown/duplicate input and audience rejection.

## Source and capability table

Metamod-P SDK: `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
ReGameDLL_CS: `679973265e1ac99a43193119e0da212ee568f5f9`, MIT LICENSE.
The latter differs from historical research pins. This is source-backed offline
compatibility evidence, not a claim of live standard-CS event coverage.

| Input | Support and evidence |
|---|---|
| EmitSound / EmitAmbientSound | SDK `hlsdk/engine/eiface.h:136-137`; matching sample names only |
| PrecacheEvent / PlaybackEvent | SDK `eiface.h:245-246`; dynamic returned index mapped to exact precache name |
| Footstep names | `pm_shared/pm_shared.cpp:199-318`; step/metal/dirt/duct/grate/tile/slosh/wade/ladder/snow families. These PM footsteps use **PM_PlaySound**; complete receipt through EmitSound is **not established** |
| Server weapon events | `dlls/wpn_shared/wpn_ak47.cpp:39,141` and other `wpn_*.cpp`; 25 exact gun event names enumerated in `sound_profile.cpp`, including elite_left/right and mp5n |
| Flashbang audio | `dlls/ggrenade.cpp:77-78`, flashbang-1/2.wav through EmitSound |
| C4 audio | `ggrenade.cpp:178`, c4_explode1.wav through EmitSound |
| Smoke grenade detonation audio | `ggrenade.cpp:594`, sg_explode.wav through EmitSound; this adds no smoke visibility behavior |
| HE explosion | `ggrenade.cpp:268-286`, SVC_TEMPENTITY/TE_EXPLOSION audio: **unsupported** by these hooks. createexplo.sc precache alone is insufficient evidence |
| Smoke event mapping | createsmoke.sc name retained for P4-05; no smoke effect implemented here |
| Unknown, delayed, local-only or altered events | Diagnosed/excluded; no inferred audio from movement or weapon state |

Pre hooks retain lifecycle revision and emitter serial. Post hooks require the
same revision/serial and an unsuppressed engine call before ingestion. This rejects
an event spanning map/round/identity retirement. Precache uses the original return
index and rejects overridden/superceded results. Hook rejection is diagnosed.
Post hooks use the existing META_FUNCTIONS slot; no DLL export is added. Detach
restores the previous callback pointers.

Duplicate detection uses the same simulation time, callback kind, emitter serial,
sample/event, source and acoustic parameters within a 256-key window. GoldSrc
does not supply an audio event sequence here: a later identical callback is a new
event, and identical simultaneous legitimate sounds may be coalesced. This is a
bounded heuristic, not complete network retransmission identification.

Audibility approximates Euclidean distance with range
`min(8192, 1024 * volume / attenuation)`; zero attenuation uses 8192. Volume must
be finite in (0,1], attenuation finite and nonnegative. Gunshot events use volume
1 and attenuation 0.8. Silencer-specific acoustics, PAS, walls/reflections and
client rendering are not reproduced. Sound flags that change/stop an existing
sound are excluded; PlaybackEvent accepts zero delay and 0/FEV_NOTHOST only.
The conventional all-zero weapon event origin falls back to the invoker origin
inside the Adapter. Water sample classifications may include non-step water audio.

## Verification

Focused tests cover original-time decay/expiry, delay, duplicate/reordered input,
invalid metadata, clock recovery, receiver separation, 32x16 capacity, negative
quantization, acoustic boundaries, dynamic event IDs, mapping conflicts/overflow,
FIFO queue overflow, receiver death/resurrection and StartFrame publication.
Fake engine exercises 1/8/16 observers with 8/16/100ms frames, existing movement,
and sound capture during vision traces followed by disconnect/map retirement/death.
Pre/post hook tests reject map/round changes and emitter serial reuse. No extra
engine trace is used by sound processing.

| Gate | Pre-merge | Merged main |
|---|---|---|
| Windows x86 portable Debug /WX, inspector ON | 46/46 passed, 32.40s | pending |
| Windows x86 Metamod Debug /WX, inspector ON | 56/56 passed | pending |
| Linux x86 portable Debug /WX, inspector ON | 45/45 passed, 24.05s | pending |
| Release PE32/x86, exactly six exports | passed: machine 14C, magic 10B, six names | pending |

The required exports are GetEngineFunctions, GetEntityAPI2, GiveFnptrsToDll,
Meta_Attach, Meta_Detach and Meta_Query. SoundMemoryModel measured 39,656 bytes
on Windows x86 and 35,420 bytes on Linux x86; both visited at most 512 entries.

Logs: each worktree's `build-*-x86-test/Testing/Temporary/LastTest.log`.
Graph returned no usable function/flow/test context, so actual source and tests
were reviewed. FocalSpan was queried before editing and must be updated after gates.
Live audio coverage/tuning remains post-Finish acceptance. P4-05 through P4-09
remain pending; neither P4 completion nor project-wide Finish is declared here.
