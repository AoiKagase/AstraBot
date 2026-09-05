# State

Status: in_progress
Goal: entire Phase 3; P3-03 has offline implementation evidence.
Main: fast-forward integrated 1df97cd -> 4e6a7d2 on user request; not pushed.
Main worktree: .worktrees/main-integration (clean).
Current branch: codex/p304-multi-nav, based on 4e6a7d2; next boundary not merged.
Current boundary: per-player NAV sessions share immutable navigation and isolate
RouteSession/Walk/IntentPump/pending commands/guards/history in up to32 slots.
Commands use optional slot:generation, required for multiple managed actors.
Generation reuse resets actor state; cancel/disconnect is actor-specific;
map publication/reset invalidates all. Synchronous query invalidation is deferred
without deleting active actor storage. Removal-pending ingress is rejected.
Report: docs/reports/p3-04-multi-nav.md.
Verification: Windows x86 NMake Debug adapter+portable34/34 PASS; expanded
fake-client regression rerun PASS after adding slot reuse/reentry assertions.
WSL Debian GCC -m32 Debug portable29/29 PASS. Release x86 DLL/six exports PASS.
All warnings-as-errors. Two independent lanes at8/16/100ms cover arrival,
cancel/disconnect/map invalidation, stale/ambiguous selectors, query/history
isolation, slot reuse and cross-actor invalidation during query.
Next: bounded automatic replan consumption with expiring edge facts and carried
attempts. DynamicBlocked/Replan currently stops the host. P3-04 reactive
checklist remains open; per-player host/navigation implementation slice checked.
Then P3-05 crouch/jump, P3-06 ladders, P3-07 finite recovery, P3-08 offline matrix.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial. No real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI for this branch pending.
