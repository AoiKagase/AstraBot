# State
Status: P4-03 complete; P4 remains in progress (P4-04 through P4-09 pending).
Task: complete P4-03 through P4-09 in approved order; not project-wide Finish.
Branch: main; P4-03 source 3251106 (foundation 223d716), base c2ca0f9.
Worktree: .worktrees/main-integration; dedicated .worktrees/p403-perception-identity retained.
Plan: docs/plans/phase-4-perception-world-model.md.
Next: P4-04 anonymous sound, on a new dedicated branch/worktree from current main.
Read the full P4-04 plan and verify SDK event hook/source mapping before editing.

Implemented: global TeamInfo (humans/dead/spectators), geometry-free TeamRoster,
validated HLTV round boundary, source ID/time provenance in Vision and memory.
Round/team changes cancel reentrant publication; disconnected serial tombstones
prevent delayed rediscovery. Pending FakeClient registration is not preempted.
Tests include old rounds, duplicate notifications, malformed metadata, transient
reset sequences, team changes during traces, and serial reuse during a message.
Report: docs/reports/p4-03-perception-identity.md.

Round source: ReGameDLL_CS HEAD 679973265e1ac99a43193119e0da212ee568f5f9,
multiplay_gamerules.cpp:1723-1733 RestartRound emits MSG_SPEC HLTV byte(0),
byte(0) all-player FOV reset, distinct from byte(0), byte(100|DRC_FLAG_FACEPLAYER).
Resolve message ID and validate destination/shape; ResetHUD alone is insufficient.
Same time/frame repeats are duplicates; later indistinguishable wire repeats
conservatively retire memory. Missing HLTV is an explicit unsupported capability.
Graph has empty/stale architecture; source fallback used. FocalSpan initialized
and queried in dedicated worktree before edits.

Approved: full Perception/World Model scope, anonymous sound regions, explicit
teammate reports only. P4-03 -> 04 -> 05 -> 06 -> 07 -> 08 -> 09, one dedicated
branch/worktree per task, pre-merge verification and tests again after main merge.
Plan includes interfaces, unnumbered commit slices, tests, acceptance, risks,
dependencies, resource limits, compatibility evidence and post-Finish matrix.
Pre-merge verification of current source: Windows portable 44/44, Metamod 53/53
(52.10s), Linux portable 43/43 (15.18s), all x86 Debug/WX/inspector ON.
Evidence: build-*-x86-test/Testing/Temporary/LastTest.log.
Main was fast-forwarded to 3251106. Post-merge: Windows portable 44/44,
Metamod 53/53 (52.00s), Linux portable 43/43 (21.84s); Release PE32/x86,
exactly six required exports. Main Release SHA256:
352743dad41bcb078b0116f9a107376ec5512b3d4fa664cdc2d1bc467dc0d79d.
Final report/plan/STATE changes are documentation only. Do not edit even docs
during replay gates: source/build clean/dirty state is checked across the run.
P3 fixture stale connected entities corrected; movement expected outcomes intact.
Graph review metadata lacks call/test coverage; actual source/tests reviewed.

Historical P4-02 evidence: implementation 3b9923e, report 5c3a0be; Windows x86
portable 43/43, Metamod 51/51, Linux portable 42/42, Release PE32/six exports.
See docs/reports/p4-02-visual-memory.md. These are not fresh verification results.

No subagents, push, cleanup, live checks or project-wide Finish.
Preserve original root user changes and exclude FocalSpan local index/config.
