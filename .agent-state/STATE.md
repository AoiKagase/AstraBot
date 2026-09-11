# State Status: verifying — P12 movement implementation and debug-level follow-up pending live acceptance

Milestone: P12
Task: ZBot-equivalent NAV movement with bounded shortcut, Jump/Drop, collision recovery, combat regression, and controllable diagnostics
Goal: Every managed BOT independently roams, follows a validated route, handles stairs/Jump/Drop/obstacles, and resumes after round/map transitions. Damage/death/combat remain live acceptance requirements.

Relevant:
- docs/plans/p12-zbot-movement-implementation.md
- docs/reports/p12-console-debug.md
- src/adapter/cstrike/nav/console.cpp
- src/adapter/metamod/console_debug.cpp
- src/adapter/metamod/console_debug.hpp

Done:
- P12 ZBot-compatible NAV/movement implementation committed as 7e316c5.
- NAV diagnostics were gated by astrabot_debug in 67b6f56; the follow-up changes now define level 0 as all diagnostics off, level 1 as general diagnostics, and level 2 as general plus NAV diagnostics.
- Tests-OFF x86 Release adapter rebuilt and six exports verified.
- Deployed DLL: D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll.
- FocalSpan is fresh/ready after the current source update.

Verified:
- Previous deployed DLL hash: F34CB22B14A66C171A50CA31EF9C0225560406EDD76DDE66286773A5723AC918.
- Existing unrelated .gitignore and untracked work files remain unstaged and preserved.

Next:
- Rebuild/redeploy after the debug-level 2 change, then restart/changelevel ReHLDS and capture qconsole.log with astrabot_debug 0, 1, and 2.
- After explicit user-confirmed live PASS only, build/run focused regression tests and then the canonical gate.

Blocked:
- CTest, test-target builds/runs, and canonical verification remain prohibited until explicit real-device PASS.
- P12 live acceptance is not yet established; do not report P12 or project Finish as complete.
