# State

Status: in_progress
Goal: entire Phase 3; P3-03 implementation checklists have offline evidence.
Main: fast-forward integrated d32cc21 -> 1df97cd on user request; not pushed.
Integrated main verification: Windows x86 Debug adapter+portable32/32 PASS;
WSL Debian GCC -m32 Debug portable27/27 PASS.
Current branch: codex/p304-reactive-blockers, based on 1df97cd; not merged or pushed.
Current boundary: portable BlockerWait foundation, one expiring route-step fact,
stable generation-safe PlayerId priority and finite yield/attempt limits.
Refresh/replacement/expiry cannot restart the deadline. Clear passage requests
reinspection; advice never permits translation. No mesh mutation or allocation.
Tests: expiry/deadline, priority, clear/unavailable/malformed facts, stale tick,
identity invalidation, cancellation and replay including UINT64_MAX.
Report: docs/reports/p3-04-blocker-wait.md.
Verification: Windows x86 NMake Debug adapter+portable33/33 PASS; WSL Debian
GCC -m32 Debug portable28/28 PASS. Release x86 DLL with six exports PASS.
All warnings-as-errors. Final diff reviewed; FocalSpan updated before commit.
Next: integrate actual player observations and this controller into Walk with
shared query budget, inspected avoidance, finite failure/replan consumption.
Then separate per-player adapter entity/join/dispatch mapping commit.
Both P3-04 implementation checklist slices remain open. Then P3-05..P3-08.
Current host remains single managed actor. P3-03 live acceptance is post-Finish.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial. No real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI for this branch pending.
