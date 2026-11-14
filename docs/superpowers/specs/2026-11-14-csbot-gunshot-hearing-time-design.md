# CSBot gunshot ingestion and hearing clock

User requests implementation matching pinned CSBot (ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5). Continue existing local commit/merge workflow.

## Design
Use public PrecacheEvent post-hook to map standard gun event scripts to engine-assigned indices, then observe server PlaybackEvent without modifying its arguments or result. Retain seven public exports by registering the engine post table through META_FUNCTIONS. No private weapon fields or offsets. Register before ServerActivate; clear catalog on detach/deactivation, preserve it over rounds. Unknown/custom/non-gun events and invalid invokers fail closed. Zero delay is the supported native gun firing path; delayed custom events are not guessed.

All supported guns are high priority: ordinary 2000 units, TMP 1000, AWP 99999; M4A1 bparam1 and USP bparam2 indicate 1000 with silencer. Source is public invoker origin, not zero event origin. Same-frame duplicate EmitSound/PlaybackEvent receipts coalesce without reducing known event range. Friendly/self filtering stays per observer. No trace or per-bot scan in hook.

Supply public global time and profile ReactionTime to perception input. Real-time noise is ready at age >= ReactionTime, recent if age <3 seconds, expires at age >20 seconds. Recent replacement requires priority >= old and strictly closer than remembered position measured from current observer. Keep tick age for diagnostics and legacy clock-less fixtures only. Invalid/future/stale receipt times cannot renew memory. Clock rollback clears old real-time memory. Actual accepted receipt time is retained, not publication time.

## Approaches
Chosen: public engine event mapping plus optional real-time hearing context; preserves current observation owner and deterministic tick-only offline consumers. EmitSound-only misses native shots; replacing frame IDs with milliseconds would break frame identity. These implementation choices follow the requested CSBot behavior and existing architecture.

## Scope and acceptance
Gunshots and sound memory clock only. PM_Move footsteps, NAV position randomization, exact native investigate policy and custom delayed events remain separate. Prove 20/100/500 FPS equivalent seconds, 3/20 second boundaries, profile delay, observer movement, hook post result/flags/invalid inputs, silencer and long range, duplicate and map/round lifecycle. Full Windows and Linux x86 builds/CTest, separate PE/ELF seven-export gates, unchanged frozen characterization. Existing Linux combat policy failure is tracked separately; live HLDS acceptance remains pending.
