# P3-03 motor and per-frame intent pump

Status: implemented internal commit boundary, 2026-09-06. The P3-03 Walk/motor
checklist remains open until Walk controller, console/host wiring and its
simulation/observability evidence exist. This is not Phase 3 completion.

`core/motor.*` owns SDK/Nav-independent MovementIntent and action vocabulary.
Nav retains aliases for its existing primitive API; its validator uses the
same inline Core contract without adding a library dependency back into Core.
Motor rotates world direction into GoldSrc forward/right axes using output
view, clamps speed to actor and 400-unit bounds (including combined magnitude),
validates/clamps view, and quantizes measured frame duration without overflow.
Press is emitted only on first intent consumption; Hold repeats and None/Release
clear buttons. The existing adapter measured-dispatch duration remains final.

`nav/local/intent_pump.*` binds actor/map/route generation, schedules 40 ms
decisions with at most one update per frame, counts missed deadlines without
catch-up bursts, and emits once per nonzero-time eligible frame. It preserves
per-frame commands between decisions rather than under-driving at 25 Hz.
Intent age strictly greater than 120 ms emits neutral/released intent. Zero
elapsed time emits no command; stale ticks/invalid actor snapshots are rejected.
Invalid actor/map retires the instance. Rejected submission clears the cached
intent; a later fresh decision is required. No wall clock or host pointer is
retained. This component does not itself submit or dispatch commands.

Tests failed to link before implementation. Final Windows x86 NMake Debug,
warnings-as-errors, adapter+portable CTest passed **29/29**. Windows portable
baseline plus fixture/manifest verification also passed. Final WSL Debian 13
GCC14 -m32 Debug warnings-as-errors CTest passed **24/24**. Rate replays use
5/10/16.667/40/100 ms frames and check actual motor commands on every frame,
one press per decision, no duplicate output, zero/long deltas, the 120 ms boundary,
binding mismatch, invalid observations/intents, overflow and rejected submission.

No adapter scheduling modification, new movement dispatch, hosted CI, live
acceptance or Finish is claimed. Next: Walk controller using corridor and ground
probes; supported arrival; console/lifecycle integration after existing dispatch;
immediate pending-command cancellation on invalidation; end-to-end replay.
