# State

Status: in_progress
Goal: entire Phase 3; P3-03 implementation checklists have offline evidence.
Main: fast-forward integrated d32cc21 -> 1df97cd on user request; not pushed.
Integrated main verification: Windows x86 Debug adapter+portable32/32 PASS;
WSL Debian GCC -m32 Debug portable27/27 PASS.
Current branch: codex/p304-player-host, based on 1a3d8d7; not merged or pushed.
Current boundary: per-player host mapping/join/decoder/dispatch. Up to32 client
states; createBot/requestJoin(player)/remove(player)/entityFor(player).
Mapped PlayerId+map+serial validated before commands. Each dispatch consumes
only its slot. Interleaved ShowMenu uses per-client decoders; message-begin
generations reject stale TeamInfo across reuse. Occupied edicts are never deleted.
Tests cover two creations, different join states/teams, distinct engine commands,
death/serial reuse, replacement, disconnect/map change and detach cleanup.
Report: docs/reports/p3-04-player-host.md.
Verification: Windows x86 NMake Debug adapter+portable34/34 PASS; WSL Debian
GCC -m32 Debug portable29/29 PASS. Release x86 DLL/six exports PASS.
All warnings-as-errors. Diff reviewed; FocalSpan updated before commit.
Next: NavConsole still has one route/motion owner and unique-managed-actor guard.
Add explicit per-actor navigation sessions/selection before two-Bot routing.
Bounded automatic replan consumption with expiring edge facts and carried
attempts is still pending. Current DynamicBlocked/Replan stops the host.
Both P3-04 implementation checklist slices remain open. Then P3-05..P3-08.
Host supports multiple managed actors; navigation integration remains single.
P3-03 live acceptance is post-Finish.
Keep goal active. No subagents, live server or project-wide Finish.
Real NAV compatibility remains partial. No real input writes or Git additions.
Preserve untracked .focalspan.json and .serena/. Hosted CI for this branch pending.
