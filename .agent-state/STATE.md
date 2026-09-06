# State
Status: P4-07 implemented; all pre-merge gates passed; integration pending.
Main: .worktrees/main-integration at3f9af62, P4-06 complete.
Dedicated: codex/p407-nav-distribution, .worktrees/p407-nav-distribution.
Plan: docs/plans/phase-4-perception-world-model.md.
Report: docs/reports/p4-07-nav-distribution.md.
Next: FocalSpan, review/commit, ff-only main and all gates again, report/plan/STATE commit.
Pre-merge: Windows50/63, Linux49, Release PE32/six exports passed.
P4-08 and09 remain pending.

WorldModel owns canonical visual/sound reducers; legacy APIs alias same memory.
StartFrame withdraws publication, revalidates, decays, sorts inputs, publishes.
Fixed32 visual+1024 sound inputs; no extra engine traces.
P4-07: exact sorted destination aggregation, top32+unknown,200ms steps,
256 connections/32 mappings/2048 job visits per frame, persistent fairness cursor.
Read-only topology built at NAV publication; immediate candidate retirement.
Targeted8/8 passed1.33s. WorldModel890104 bytes, DistributionModel1761384 bytes.
Following P4-06 gate evidence is historical:
Pre-merge Windows49/61, Linux48 passed. Merged ad010bc: Windows49/61,
Linux48 passed. Main Release PE32/x86, exactly6 exports; DLL SHA256:
7685064f68709ae3836a7145bd754ee5bc05a0665baa952ceabc61e2d6c5e014.
Graph lacked useful function coverage; source fallback. FocalSpan updated.

No subagents, push, cleanup, project-wide Finish or live HLDS/ReHLDS checks.
Preserve root codex/p307-progress-recovery edits. Never stage .focalspan files.
Every shell command starts with rtk. Git metadata writes require escalation.
Capture/poll command sessions until completion, including commit commands.
Do not edit tracked files during full replay gates (dirty state is checked).
Each P4 item: all pre-merge gates, narrow commit, main ff-only, all main gates,
then report/plan/STATE commit. SDK7ec9b014f8c0a947a724644aebe34eb33706e44b.
