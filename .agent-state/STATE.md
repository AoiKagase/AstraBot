# State
Status: in progress - P4-03 implementation integrated locally; main merge pending.
Task: complete P4-03 through P4-09 in approved order; not project-wide Finish.
Branch: codex/p403-perception-identity; base main c2ca0f9.
Worktree: .worktrees/p403-perception-identity.
Plan: docs/plans/phase-4-perception-world-model.md.
Next: commit intended files, ff-only main, repeat
Windows portable/Metamod Debug, Linux portable Debug and Release PE32/six exports.
Then record merged evidence, mark only P4-03 complete, and start P4-04.

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
Release passed: PE32/x86, exactly six required exports. Main merge and post-merge gates pending.
P3 fixture stale connected entities corrected; movement expected outcomes intact.
Graph review metadata lacks call/test coverage; actual source/tests reviewed.

Historical P4-02 evidence: implementation 3b9923e, report 5c3a0be; Windows x86
portable 43/43, Metamod 51/51, Linux portable 42/42, Release PE32/six exports.
See docs/reports/p4-02-visual-memory.md. These are not fresh verification results.

No subagents, push, cleanup, live checks or project-wide Finish.
Preserve original root user changes and exclude FocalSpan local index/config.
