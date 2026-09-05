# State

Status: in_progress
Goal: entire Phase 3; P3-03 implementation checklists have offline evidence.
Main: fast-forward integrated d32cc21 -> 1df97cd on user request; not pushed.
Integrated main verification: Windows x86 Debug adapter+portable32/32 PASS;
WSL Debian GCC -m32 Debug portable27/27 PASS.
Current branch: codex/p304-reactive-blockers, based on 1df97cd; not merged or pushed.
Current boundary: reactive Walk/player observation, built on 98c6bf9.
One expiring portal fact, stable registry PlayerId priority; unregistered public
player hits are Other with no inferred team/priority. Host profile: fact120ms,
yield400ms, timeout3s; shared21 queries/4 samples and max25 side attempts.
Refresh/replacement/expiry cannot restart the deadline. Full verified passage
invalidates the fact on a neutral tick. Side movement needs complete inspection.
Tests cover static/approaching/receding/removed/immobile blockers, replay, query
limits, stale stamps, slot generation, absent lookup and fake-engine movement.
Report: docs/reports/p3-04-reactive-walk.md.
Verification: Windows x86 NMake Debug adapter+portable34/34 PASS; WSL Debian
GCC -m32 Debug portable29/29 PASS. Release x86 DLL/six exports PASS.
All warnings-as-errors. Diff reviewed; FocalSpan updated before commit.
Next: separate per-player adapter entity/join/dispatch mapping commit and
two-client isolation; bounded automatic replan consumption with expiring edge
facts and carried attempts is still pending. Current DynamicBlocked/Replan
request stops the host; it is not automatic replanning.
Both P3-04 implementation checklist slices remain open. Then P3-05..P3-08.
Current host remains single managed actor. P3-03 live acceptance is post-Finish.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial. No real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI for this branch pending.
