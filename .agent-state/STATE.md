# State
Status: in progress - P4-03 Core identity foundation; Adapter integration pending.
Task: complete P4-03 through P4-09 in approved order; not project-wide Finish.
Branch: codex/p403-perception-identity; base main c2ca0f9.
Worktree: .worktrees/p403-perception-identity.
Plan: docs/plans/phase-4-perception-world-model.md.
Next: connect global TeamInfo, round lifecycle and observation metadata; fake-engine
tests, all platform gates, report, then ff-only main and repeat full gates.

Current slice: SDK-free TeamRoster, RoundGeneration, ObservationIdentity and
portable tests. TeamRoster has 32 generation-bound entries, explicit unknown
relations, map/retirement high-water guards; observation metadata retains original
and receipt time. Types are not yet connected to production Adapter flow.
Existing handleMessage routes TeamInfo only to managed ClientState; VisionAdapter
discovers foreign identities only for alive slots. Share identity binding without
preempting FakeClient registration; preserve serial checks, message-begin identity
snapshots and trace reentrancy invalidation.

Round source: ReGameDLL_CS HEAD 679973265e1ac99a43193119e0da212ee568f5f9,
multiplay_gamerules.cpp:1723-1733 RestartRound emits MSG_SPEC HLTV byte(0),
byte(0) all-player FOV reset, distinct from byte(0), byte(100|DRC_FLAG_FACEPLAYER).
Resolve message ID and validate destination/shape; ResetHUD alone is insufficient.
Duplicate notification policy and old-round rejection still need implementation.
Graph has empty/stale architecture; source fallback used. FocalSpan initialized
and queried in dedicated worktree before edits.

Approved: full Perception/World Model scope, anonymous sound regions, explicit
teammate reports only. P4-03 -> 04 -> 05 -> 06 -> 07 -> 08 -> 09, one dedicated
branch/worktree per task, pre-merge verification and tests again after main merge.
Plan includes interfaces, unnumbered commit slices, tests, acceptance, risks,
dependencies, resource limits, compatibility evidence and post-Finish matrix.
Verification: Windows portable x86 Debug /W4 /WX, inspector ON: 44/44 CTest
passed (31.02s), including new perception_identity test. Evidence:
build-portable-x86-test/Testing/Temporary/LastTest.log.
Other platform gates and main merge pending; P4-03 checkboxes remain unchecked.

Historical P4-02 evidence: implementation 3b9923e, report 5c3a0be; Windows x86
portable 43/43, Metamod 51/51, Linux portable 42/42, Release PE32/six exports.
See docs/reports/p4-02-visual-memory.md. These are not fresh verification results.

No subagents, push, cleanup, live checks or project-wide Finish.
Preserve original root user changes and exclude FocalSpan local index/config.
