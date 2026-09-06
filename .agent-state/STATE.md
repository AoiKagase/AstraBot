# State
Status: P4-07 complete; P4-08 and09 pending. P4 goal remains active.
Main: .worktrees/main-integration, implementation5fb6000 from3f9af62.
Dedicated: codex/p407-nav-distribution, .worktrees/p407-nav-distribution retained.
Plan: docs/plans/phase-4-perception-world-model.md.
Report: docs/reports/p4-07-nav-distribution.md.
Next: P4-08 dedicated branch/worktree from current main; Graph/FocalSpan then
inspect TeamRoster, WorldSnapshot and operator command contracts before editing.

WorldModel owns canonical memories and candidate distributions. New
WorldSnapshot distributions[i] corresponds to visual->memories[i], borrowed const.
DistributionTopology builds sorted unique destination lists on NAV publication.
200ms exact contribution merging, top32+unknown,256 connections/frame,
32 mappings/frame,2048 active job visits/frame; persistent round-robin cursor.
NAV retirement withdraws public candidates immediately. No extra engine traces.
Fixed x86 sizes: WorldModel890104, DistributionModel1761384 bytes.
Targeted8/8 passed. Pre-merge Windows50/63/Linux49 passed; main same totals.
Main ReleasePE32/x86/exact6exports; DLL SHA256:
da6c13464e3856a895624ee625b79997495252a1dc0dba9636a9efd0d626dc7b.
Linux iterator sign conversion fixed and gates repeated; fixture dangling edge fixed.
Graph lacked useful function coverage; source fallback. FocalSpan updated.

No subagents, push, cleanup, project-wide Finish or live HLDS/ReHLDS checks.
Preserve root codex/p307-progress-recovery edits. Never stage .focalspan files.
Every shell command starts with rtk. Git metadata writes require escalation.
Capture/poll command sessions until completion, including commit commands.
Do not edit tracked files during full replay gates (dirty state is checked).
Each P4 item: all pre-merge gates, narrow commit, main ff-only, all main gates,
then report/plan/STATE commit. SDK7ec9b014f8c0a947a724644aebe34eb33706e44b.
