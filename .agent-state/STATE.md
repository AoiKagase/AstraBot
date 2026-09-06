# State
Status: P4-06 complete; P4-07 through09 pending. P4 goal remains active.
Main: .worktrees/main-integration, implementation ad010bc from abad680.
Dedicated: codex/p406-world-snapshot, .worktrees/p406-world-snapshot retained.
Plan: docs/plans/phase-4-perception-world-model.md.
Report: docs/reports/p4-06-world-snapshot.md.
Next: P4-07 dedicated branch/worktree from current main; Graph/FocalSpan then
inspect WorldSnapshot and read-only NAV contracts before implementation.

WorldModel owns canonical visual/sound reducers; legacy APIs alias same memory.
StartFrame withdraws publication, revalidates, decays, sorts inputs, publishes.
Fixed32 visual+1024 sound inputs; x86 sizeof251128; no extra engine traces.
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
