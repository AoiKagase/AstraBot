# State Status: verifying — ZBot-compatible movement implementation committed; live acceptance pending

Milestone: P12
Task: ZBot-equivalent NAV movement with bounded shortcut, Jump/Drop, collision recovery, and combat regression
Goal: Every managed BOT independently roams, follows a validated route, handles stairs/Jump/Drop/obstacles, and resumes after round/map transitions. Damage/death/combat remain live acceptance requirements.

Relevant:
- docs/plans/p12-zbot-movement-implementation.md
- src/adapter/cstrike/nav/console.cpp
- src/adapter/cstrike/nav/motion.cpp
- src/nav/corridor/corridor.cpp
- src/nav/local/walk.cpp
- src/nav/local/walk_jump.cpp
- src/nav/query/graph.cpp

Done:
- Implemented and committed the approved P12 ZBot-compatible NAV/movement plan as 7e316c5.
- Added runtime route-local overlay links with bounded physical queries, active-session graph use, route-step traffic reservation, precision traversal handling, step-up probing, external Jump/Drop endpoint handling, and bounded fall-risk checks.
- Added focused regression source changes for overlay ownership and precise/Jump corridor behavior; tests have not been built or run.
- FocalSpan status is fresh/ready after the implementation update.
- Tests-OFF x86 Release adapter rebuilt and six exports verified.
- Deployed DLL: D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll.

Verified:
- Release DLL SHA-256: 96356FA139764D02EE2C9C677F98EE75A1EB8028C5911989EF57DF701BCEC79B.
- Existing unrelated .gitignore and untracked work files remain unstaged and preserved.

Next:
- Restart/changelevel the ReHLDS server so the deployed DLL is loaded, then capture qconsole.log for 1v1, 2v2, configured BOT count, stairs/Jump/Drop/obstacle recovery, round/map transitions, damage/death, and combat regression.
- After explicit user-confirmed live PASS only, build/run focused regression tests and then the canonical gate.

Blocked:
- CTest, test-target builds/runs, and canonical verification remain prohibited until explicit real-device PASS.
- P12 live acceptance is not yet established; do not report P12 or project Finish as complete.
