# P4-01 — Visible observation evidence

Status: implementation and applicable offline verification complete.
Base: main `7158917`; branch `codex/p401-visible-observation` in its dedicated
worktree. No merge, push, server execution or project-wide Finish transition.
Contract: [approved plan](../plans/p4-01-visible-observation.md).

## Delivered boundary and data flow

`GiveFnptrsToDll`'s existing bootstrap supplies the borrowed globalvars pointer
to LifecycleCoordinator. Its StartFrame path runs vision after existing movement
dispatch/decisions. Vision never submits a command or mutates engine vectors.
The adapter's private roster borrows edicts synchronously and compares registry
IDs, map/tick and edict serials before/after trace and before publication.

Only observation-eligible foreign players acquire a new PlayerRegistry ID;
dead/dormant slots do not preempt pending registration. Existing managed bot
identity remains lifecycle-owned; only joined, alive, non-spectator bots observe.
Humans and other bots can be targets regardless of team. Public-entvars health,
deadflag, FL_CLIENT/FL_FAKECLIENT, FL_SPECTATOR and iuser1 provide eligibility.

Portable `core/perception.hpp` provides VisionSettings, Point, Stamp,
VisibleObservation, ObservationBatch, Diagnostics, InputFrame/PlayerSample,
SightRequest, IVisibilityQueries and Vision. InputFrame is a privileged,
transient perception input, **not** a World Model snapshot. Engine/private data
and invisible candidate positions are absent from ObservationBatch and diagnostics.
Access is `LifecycleCoordinator::vision().observations().latest(player)` and
`diagnostics(player)`; the adapter also exposes a frame-level error. These are
borrowed const views; copy a batch if retaining it across a frame/lifecycle call.
No public DLL export or console command is added.

Observations carry the successfully tested eye/body-center sample and target ID;
the enclosing stamp carries observer agent/player, map, tick and simulation time.
Target contact is sufficient to prove that sample's target visible. A batch is
the last scan, not a live omniscient roster; consumers must use its timestamp.
Non-scan frames do not refresh that timestamp. Retirement removes invalid data
immediately at a lifecycle callback or the next frame, even before the next scan.

Defaults are a 90-degree circular cone (pitch and yaw), 4096 units and 100 ms.
Agent-ID phases and round-robin scheduling bound each frame to four observers,
31 candidates and 62 rays per observer (248 rays/frame). Counters expose actual
interval, lateness and deferred frames. Low FPS intentionally reduces cadence;
these are finite work bounds, not measured live CPU/capacity guarantees.
Diagnostic reasons/counts describe point-level rejections; a rejected eye can
coexist with a successful body-center observation. No rejected positions are logged.

## Verification

All builds below use x86 and warnings-as-errors; Debug retains assertion tests.
Windows used VS 2026/MSVC 19.51.36256.0, NMake Makefiles and CMake 3.31.4.
Linux used WSL Debian GCC 14.2.0 with `-m32`, Metamod OFF. SDK SHA is
`7ec9b014f8c0a947a724644aebe34eb33706e44b`.

| Gate | Result |
| --- | --- |
| Windows portable Debug, inspector ON | 42/42 |
| Windows Metamod Debug, inspector ON | 49/49 |
| Linux portable Debug, inspector ON | 41/41 |
| Windows Metamod Release, tests OFF | Build passed; PE32/x86 confirmed |
| DLL exports | Exactly GetEngineFunctions, GetEntityAPI2, GiveFnptrsToDll, Meta_Attach, Meta_Detach, Meta_Query |

The vision-specific tests are `astrabot.perception` and
`astrabot.adapter.vision` (the existing fake-engine executable with
`--p401-tests`). They cover distance/cone boundaries, head-to-body fallback,
31 successful outputs/62 blocked rays, invalid values/results/settings,
missing TraceLine, stale generations even at newer ticks, slot/serial reuse,
death/spectators, disconnect and map retirement during trace, time preservation,
1/8/16 actor 8/16/100 ms host matrices, 32-observer portable fairness, both human
and managed-bot targets, and goto arrival while vision is active. Existing
fake-client and P3-08 movement replay regressions pass. The final success-capacity
and mixed-target assertions also passed targeted Windows/Linux reruns.

Retained final logs are in `build-portable-x86-test/p401-ctest.log`,
`build-metamod-x86-test/p401-ctest.log`, and `build-linux-x86-test/p401-ctest.log`.
Builds are local ignored artifacts. The Release DLL SHA-256 is
`f89268807c6e97b279d336f1c14e08d5b0e41c8b06412fe67bc14124efbd1d21`.

WSL needs process-local `GIT_DIR=/mnt/h/sourcecode/003.Game/amxmodx/AstraBot/.git/worktrees/p401-visible-observation`
and `GIT_WORK_TREE=/mnt/h/sourcecode/003.Game/amxmodx/AstraBot/.worktrees/p401-visible-observation`
for the existing P3-08 checker: Windows-created worktree metadata otherwise
contains a Windows path Linux Git cannot resolve. The first replay failure was
this environment mismatch; rerunning with those variables passed. No Git
configuration or repository metadata was rewritten for Linux.

Windows final CTest runs use the same VsDevCmd environment as the build,
through the repository's documented RTK PowerShell invocation. A separate
plain-process log-capture attempt could not launch its configured WindowsApps
Python interpreter (BAD_COMMAND); the documented invocation rerun passed.

Graph tools were consulted first, but reported no indexed symbols/communities
for this worktree and did not cover new files. Their zero risk score is not
review evidence. Scoped source/diff review, fresh FocalSpan update/query and
executable tests establish the offline findings above.

## Remaining acceptance

Smoke, flash, sound, lost-enemy beliefs/confidence and combat are deliberately
absent. World Model consumers remain future work. Live HLDS/ReHLDS behavior,
32-slot/16-bot capacity and smoke/flash perception accuracy are unvalidated.
Run live acceptance only after the separate project-wide Finish decision.
This slice neither completes Phase 4 nor changes existing real-NAV/live gates.
