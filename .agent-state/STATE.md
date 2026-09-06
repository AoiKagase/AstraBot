# State
Status: P4-04 complete; P4-05 through P4-09 pending. P4 goal remains active.
Goal: complete approved P4; no project-wide Finish or live checks.
Main: .worktrees/main-integration, implementation837e0d3 (base a4f7c50).
Dedicated branch/worktree codex/p404-anonymous-sound retained.
Plan: docs/plans/phase-4-perception-world-model.md.
Report: docs/reports/p4-04-anonymous-sound.md.
Next: P4-05, new dedicated branch/worktree from current main. Read its complete
plan, source smoke event/ScreenFade path and settings before implementation.

P4-04: SDK-free anonymous sound memory,256-unit regions,0.5/3s original-time
decay; queue256,32 events/frame,32 receivers,16 retained each. Emission-time
listener snapshots/epochs prevent queued revival. No added engine trace.
Dynamic precache mapping, pre/post hook revision/serial validation, six exports.
Clock recovery preserves Core time/sequence guards. See source capability table.
Pre-merge Windows46/56, Linux45; merged main Windows46/56, Linux45 all passed.
Main Metamod50.36s, portable29.34s, Linux17.82s. Release PE32/x86 exactly6 exports.
Main DLL SHA256 071c8b8d3e20c190ca10d45906a3ff758eeaf68d8f8d50764eab25ed27cb5339.
Final verification commit changes plan/report/STATE only.

SDK7ec9b014f8c0a947a724644aebe34eb33706e44b;
ReGameDLL_CS679973265e1ac99a43193119e0da212ee568f5f9 (MIT).
PM_PlaySound footstep coverage and HE TE_EXPLOSION remain unsupported/unproven.
No inferred sound or emitter identity is exported to Core. P4-05 reuses the
createsmoke.sc mapping; P4-04 does not implement smoke visibility effects.
Graph lacked usable call/test context; source fallback used. FocalSpan updated.

No subagents, push or cleanup. Preserve root codex/p307-progress-recovery edits.
Never stage local .focalspan index/config. Every shell command starts with rtk.
Capture and poll long command sessions including commits; verify exit codes.
Do not edit tracked files during full gates: P3 replay checks source dirty state.
Use ff-only main integration then rerun every gate for each remaining P4 item.
