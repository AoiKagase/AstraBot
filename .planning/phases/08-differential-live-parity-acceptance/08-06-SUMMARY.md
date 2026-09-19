---
phase: 8
plan: 06
subsystem: navigation-start-comparison
requirements-progress: [PAR-01, PAR-03, PAR-04, PAR-06, TEST-04]
production-commit: uncommitted-working-tree-checkpoint
---

# Plan 08-06 checkpoint: navigation-start comparison

## Scope

Compared the current AstraBot navigation-start path with the registered YaPB
and RealBot sources, and with the ReGameDLL-CS CSBot implementation used as
the ZBot/CSBot comparator. No production source change was made in this
checkpoint.

## Evidence

- CRG registry contains YaPB, RealBot, ReGameDLL-CS, and AstraBot. Their
  incremental graphs were refreshed to current HEADs; AstraBot's graph now
  matches `b5a497c`.
- FocalSpan is fresh for the AstraBot checkout (`245` files, `2576` symbols,
  `stale=false`).
- The live server is `hlds.exe` PID `47836`; the deployed DLL hash is
  `5FC11FDB0F8CA791904AAAE83130491321F3F3DD10AD56504A04BD4BD13DB321`.
- Offset-scoped verification from qconsole line `77406` remains failed:
  `ReadySamples=16`, `MovingSamples=0`, `BotAttackLines=0`,
  `PlantLines=0`, `DefuseLines=0`, and `IdleKickLines=4`.

## Confirmed navigation-start gaps

1. `spawnReadiness()` accepts a live entity while `grounded=0` and
   `groundentity=0` when team/dead/solid/movetype/health are otherwise valid.
   The live run reached `ready=1` at frame 21 while still airborne; after the
   two-frame warmup, navigation input began at frame 24.
2. `NavPathFollower::update()` targets the center of the current/next nav area
   and only advances after a one-unit horizontal center tolerance. The
   comparators use path/portal lookahead and feet/ground-aware movement, so
   center-seeking is a behavioral difference and a candidate contributor to
   the live `Stuck` result.
3. `LocomotionController::recordProgress()` reaches `Stuck` after eight
   no-progress observations; `NavRoamController` resets the route and the
   adapter sends neutral movement. YaPB/ReGameDLL retain a stuck monitor and
   perform wiggle/recovery/repath behavior instead of terminating the first
   route without a recovery action.

## Not yet proven

- The live log does not record the first target coordinates or command yaw,
  so it does not yet prove that center-seeking aimed into a wall or that the
  first input was geometrically invalid.
- `frametime`-based msec is less robust than the comparators' elapsed-time
  accumulator, but the live sample proves that engine movement/vertical
  settling occurred; this remains a secondary timing risk, not the primary
  confirmed cause.

## Next action

Add a focused RED contract for spawn-settled navigation start and first
feedback, including target/intent diagnostics. Then test the smallest fix for
the confirmed grounded/start and bounded stuck-recovery gaps. Keep live
combat, C4, Linux, PAR-06, TEST-03, and TEST-04 pending until fresh log gates
pass.

## Movement gate closure update

- Added the grounded readiness gate and verified its RED/GREEN regression.
- Changed live physics evidence to retain a post-ready sample window instead
  of consuming the quota on pre-landing neutral samples.
- Changed Movement verification to require an eight-sample cumulative
  horizontal progress window while rejecting teleport-sized jumps.
- Rebuilt and deployed x86 DLL SHA-256
  `EF5C80401C1295D64CDC3343890861F2246A1753308721E8E704B8F74DE7FCF5`.
- Fresh offset-scoped live result from qconsole line `79240`: Movement
  `Passed=true`, `ReadySamples=256`, `MovingSamples=3`, `IdleKickLines=0`.
  Bot-to-Bot combat remains `0` attacks and C4 remains `0/0`; those gates are
  not promoted by this Movement fix.
