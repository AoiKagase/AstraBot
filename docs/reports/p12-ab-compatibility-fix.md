# P12 A/B compatibility correction

## Local main integration (2026-09-11)

- User requested main merge and cleanup of this task worktree/branch.
- Integrated implementation `7130889` with main `967d75c`; retained main's
  Roam arrival cancellation, route-style state, diagnostics and economy fixes.
- Integration x86 Release adapter build passed with tests OFF; PE32/x86 and
  exactly six required exports verified. No tests/CTest/canonical or live run.
- Merged DLL SHA-256:
  `42a5d6d33a8a1fda73a806537704a704a38ca066d4e0c0e9f8bc32675aa024a0`.
- This merged DLL was not deployed. The earlier deployment below remains
  distinct and has no live PASS.
- Cleanup archive: `C:/Users/SandS/AppData/Local/Temp/astrabot-p12-ab-merge-20260911`.
  It retains the task's local configuration/index, inherited patch, Release
  output and the root's previously untracked plan file. Other worktrees and
  unrelated root changes remain untouched.

Scope: the approved A/B correction plan from 2026-09-11. C runtime-added links
and damage-bearing shortcuts are out of scope. Reference: ReGameDLL_CS
`b0889847fe6d03898be88acc9e366660efb40ab5`.

## Baseline and ownership

The isolated `codex/p12-ab-compat-fix` worktree starts at `8807783` and includes
the existing four-file movement/cooldown working changes. The original worktree
is preserved. `.gitignore`, local MCP/FocalSpan configuration and temporary files
are not part of the implementation commit.

## Implementation ledger

- Stair collision: always inspect lift, forward and landing, including level
  floor samples; retain the last validated step and supported landing evidence.
- Dispatch: keep XY route bounds and fresh actor/route/step identity, but prove
  physical ground and stair movement each frame instead of interpolating height.
- Jump: shared physics capabilities, source/flight/landing posture transitions,
  observed hull validation, external origin conversion, and reuse of the running
  Walk primitive when a measured geometry obstruction selects a Jump candidate.
- Narrow NAV: center membership for goals, crossings and recovery; physical hull
  sweeps and floor support remain mandatory; precise movement suppresses lateral
  recovery.

## Verification gate

The user explicitly approved pre-Finish live comparison for this A/B correction
only. This does not declare project Finish. Before explicit live PASS, **no test
configure, test build/run, CTest or canonical** is allowed. Regression source is
written now, execution is deferred. Only the x86 Release adapter with tests OFF
and its six exports may be built/checked before that gate.

Server stop/restart still requires separate user authorization. Preserve the
running DLL in a timestamped backup and record hashes before deployment.

Live acceptance remains outstanding: identical NAV/start/goal/physics against
Zbot, de_dust edges 5->70, 5->1665, 2036->141 and stairs/narrow/crouch-jump/doors/
ladders; five consecutive successes each. Then 2v2 and configured count for at
least ten minutes and three rounds, plus lifecycle and combat regressions.
After explicit PASS, run affected regressions and one canonical All gate.

## Status

Implementation and tests-OFF Release verification completed; live acceptance and
test execution are pending. No automated test pass or Finish is claimed.

## Build and deployment evidence

- MSVC 14.51, VS 18 Community developer environment, x86 target/x64 host,
  NMake Makefiles, Release, tests OFF, warnings-as-errors ON.
- SDK: `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
- Initial build failed with C4456/C2220 at jump_motion.cpp (local `flight`
  shadowing). The boolean was renamed; the affected adapter target and its
  dependencies rebuilt successfully. Final target build exited 0.
- `dumpbin /headers`: 14C machine (x86), PE32.
- `dumpbin /exports`: exactly GetEngineFunctions, GetEntityAPI2,
  GiveFnptrsToDll, Meta_Attach, Meta_Detach, Meta_Query (6 names/functions).
- Artifact: `build-metamod-x86-release/astrabot_mm.dll`.
- Artifact/deployed SHA-256:
  `99632a565db99a5258b03bb6b682905a510255bb3a164b49aa473232813319a1`.
- Deployed to `D:/SteamCMD/cstrike_rehlds/cstrike/addons/astrabot/dlls/astrabot_mm.dll`.
- Verified backup: `astrabot_mm.dll.pre-p12-ab-20260911-141810.bak` in the same
  directory; SHA-256 `2cae8b0fce4b8c94e241d103cb8683215cc1e357a2593778e6607a10a555ec75`.
- No hlds.exe process was listed immediately before deployment. No server
  start, stop, restart, map change, or gameplay comparison was performed.

## Regression source and review

Regression sources cover same-height step collision/ceiling/budget, physical
frame checks, narrow goal/recovery/Drop, precise final segment, attributed and
observed-obstacle Jump, standing-to-air-duck landing observations, stale physics,
standard/nonstandard hulls, and transient edge cooldown isolation/expiry.
Tests have not been configured, built or run under the explicit live-PASS gate.

Independent static reviews found and resolved final-segment PRECISE leakage,
Walk primitive re-entry during Jump fallback, and external Jump landing origin
conversion. Adding the mixed-posture regression also exposed and corrected an
airborne Duck Hold overwrite in the Walk wrapper. Static review is not runtime
acceptance. The fake adapter simulator still does not model mixed air duck
physics faithfully; that path is covered with core observation fixtures and
requires the specified real-server comparison.

Only the pinned standard CS standing/duck hulls are recognized by the adapter.
Gravity/impulse and crouch capability are checked against current observations;
unrecognized hulls or changed physics do not authorize launch.
