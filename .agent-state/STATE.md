# AstraBot active state

- Repository: `H:\sourcecode\003.Game\amxmodx\AstraBot`
- Branch/HEAD at implementation start: `main` / `d5f7959`
- Active work: P12 ZBot-style Jump firing and movement continuity.
- User-owned `.gitignore` and untracked files remain untouched.

## Implemented, awaiting live acceptance

- Added actor-local bounded `JumpAttemptRegistry`; keys contain agent/player generations, map, source, target and Jump traversal, but not route/goal generation.
- `SimpleJump` now separates proof, readiness, terminal reason and recovery disposition.
- Under-speed, lateral error and over-speed remain in `Accelerate`; Run input points along the velocity error. They no longer immediately become `Blocked`.
- `JumpProbe::launch` validates the intended launch velocity and reuses an existing sweep slot to validate the current-frame alignment command. No diagnostic-only world query was added.
- Jump press is recorded once per attempt. Attempt start/press state survives route regeneration through `NavConsole::ActorState`.
- Sweep provenance distinguishes verified world/static BSP from dynamic/unknown blockers. Only verified static proof requests structural exclusion.
- Jump failure bypasses goal/source aggregation, so only the exact directed edge receives transient 2-second cooldown. Sibling Jump exits remain searchable.
- `astrabot_debug 2` includes attempt ID/start, proof/provenance, readiness, axis, velocity components, Run command direction, proof distance/lifetime and command/dispatch ticks.

## Verification completed

- FocalSpan Ready before edit; post-edit index update completed.
- tests-OFF x86 Release `astrabot_mm` build passed.
- `dumpbin /exports` found exactly the six required undecorated exports.
- `git diff --check` passed after the test-source additions; rerun after this state update.
- Test source was extended, but test targets, CTest and canonical were not configured, built or run.

## Required next actions

1. User restarts and runs `de_dust`, 1v1, `astrabot_debug 2`, for 60 seconds or until both actors complete Jump.
2. Require attempt-correlated `Accelerate -> Takeoff -> IN_JUMP dispatch -> Airborne -> Recover/Complete`, and actor 1 leaving area 5 plus actor 2 leaving area 11.
3. Before explicit live PASS, do not build/run test targets, CTest or canonical, and do not commit.
4. After explicit live PASS, run focused tests and canonical All once, update FocalSpan, stage explicit paths only, diff-check and create the limited commit.

## Previous deployed artifacts

- Runtime DLL before this task: SHA-256 `C6090A2FCB9DBD2B6F1BC3F221548A0DF4A18D9FFBC0EC7483AD7F54FC93043F`.
- Backup: `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.pre-jump-attempt-20260911-204957.dll`, same SHA-256 as above.
- Intermediate candidate backup: `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.pre-final-jump-attempt-20260911-205700.dll`, SHA-256 `0B74813BAAC24EA8621237FBF6D3602758B537BDD04853421F2FEC2ABAE63A0E`.
- Newly deployed runtime DLL: SHA-256 `0487247B73423B273A0B3F85F78738F8B99323AC51B802594B06F1DB7856BBF4`.
- Server was not restarted by Codex.

## Current P12 measurement candidate

- Rebuilt tests-OFF x86 Release artifact at 2026-09-12 11:17:11. SHA-256 `05DC0223D8438241B0343F8C4AE13396B5CFB989657FCDB85D475FDEE3F5229C`.
- Exactly six required undecorated exports verified with `dumpbin /exports`.
- Deployed to `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll`.
- Backup: `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll.pre-p12-nosupport-rebuild-20260912-111711`, SHA-256 `DF1E7BFC1C01E4B417A777CE5E83452FE94055A2952A3965CE4D2BF3D8FCDA9A`.
- Server was not restarted by Codex; live acceptance, CTest, canonical, and commit remain pending.

## P12 micro-NAV Jump candidate deployment (2026-09-12)

- `JumpLandingEnvelope` now carries the target centre-safe region and, only for a direct ordinary route continuation, one successor landing region.  `JumpPlan` records its landing area and one- or two-transition cursor advance.
- Candidate construction no longer reimposes full-hull NAV containment on micro patches and filters candidates through the shared deterministic trajectory calculation.
- `JumpProbe::launch` uses measured horizontal velocity for touchdown prediction.  A `LandingRadius` mismatch preserves current source support and lets `SimpleJump` continue Run alignment; no Jump press is allowed until full trajectory proof is ready.
- `Cursor::advanceLanding` accepts exactly the supported target or validated immediate Walk/Crouch successor, never arbitrary area skipping.
- `astrabot_debug 2` now emits candidate area/advance, landing envelope bounds, predicted touchdown, landing error and trajectory-ready state.
- tests-OFF x86 Release `astrabot_mm.dll` built and six exports verified. Runtime deployed SHA-256: `0E250F689E8A96B67FE64DF81C0A87C1FAB14308CE199BA630336AC0573256EA`.
- Runtime backup: `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll.pre-p12-micro-jump-20260912-142500` (SHA-256 `576305CEF67EF5103A9B535E644933932FD6F4275CDCF849313F6EA870AB7CF4`).
- Required next action: user restart, then 2BOT `astrabot_debug 2` live measurement.  Do not run test targets, CTest, canonical, or commit until explicit live PASS.
