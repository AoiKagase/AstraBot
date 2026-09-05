# State

Status: in_progress
Goal: entire Phase 3; P3-03/P3-04 implementation slices have offline evidence.
Main: 4e6a7d2 in .worktrees/main-integration, clean; not pushed.
Current branch: codex/p304-bounded-replan, based on 7a0ef6c; not merged/pushed.
Current boundary: one bounded automatic retry per actor/explicit goal on fresh
observed dynamic timeout. One directed-edge fact expires in1s; consume on later
tick, retain attempt count through route replacement, no automatic retry after
failure/expiry. Existing RouteSession fixed limits/allowPartial=false remain.
Explicit goal/cancel and identity/map invalidation retire pending state.
Report: docs/reports/p3-04-bounded-replan.md.
Verification: Windows x86 NMake Debug adapter+portable35/35 PASS; WSL Debian
GCC -m32 Debug portable30/30 PASS. Release x86 DLL/six exports PASS. Werror.
Portable tests cover directed detour/cost, expiry, identity, budget/external
provenance and graph immutability. Adapter8/16/100ms scenarios cover arrival,
blocked replacement, Unreachable, pending cancellation and expired pending fact.
P3-04 implementation checks are marked with offline evidence; live is post-Finish.
Next: P3-05 supported crouch/jump constraints and primitives, followed by P3-06
ladder discovery/lifecycle, P3-07 dispatched-progress finite recovery, P3-08 matrix.
Graph reachability alone does not guarantee locally executable geometry.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial; no real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI pending.
