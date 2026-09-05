# P3-05 bounded Jump launch inspection

Built from integrated main `3f85e3f` on `codex/p305-jump-inspection`.
P3-05's Simple Jump checklist remains open. This slice adds world-query launch
inspection; it does not yet enable Jump in Walk or the host command dispatcher.

`JumpProbe::launch` requires a current actor/map/route/step/tick-bound
`JumpPhysics` input with explicit gravity and vertical impulse. Its model is
constant gravity and constant horizontal velocity. It never infers these values
from NAV hints or assumes that default GoldSrc values describe the running game.
The pinned ReGameDLL `pm_shared.cpp` has jump-height overrides as well as the
ordinary jump branch; the model must therefore be established and revalidated
by the eventual host integration. No upstream implementation was copied.

The inspector checks the supported Jump constraints, standing observation,
takeoff region, actual speed/lateral velocity and facing. It measures support at
the current position and planned landing, derives descending touchdown from the
observed horizontal velocity and explicit flight model, then measures that actual
touchdown. A different landing height, wrong NAV area, missing support, or landing
outside the permitted region discards the result without retries.

Takeoff and touchdown receive stationary hull-clearance traces. The flight is
split into at most eight intervals. For each interval the constant-gravity curve
lies above its chord by at most `gravity * dt * dt / 8`; lower and upper sweeps
with overlapping vertical hull spans enclose the intervening curve. Upper
coordinates round outward, and non-overlapping or non-finite spans are rejected.
Queries retain the native hull dimensions. This avoids requesting an expanded
hull that the existing GoldSrc TraceHull adapter cannot represent. It can reject
obstacles outside the actual curve but inside the enclosing swept volumes.

Every request has a unique ordinal and full batch stamp. Unknown, malformed,
stale, exceptional, obstructed or exhausted responses discard all partial
clearance evidence. The hard limit is 21 queries, including three support queries,
two stationary hull queries and up to sixteen flight sweeps. There is no retry or
automatic budget increase. The returned launch inspection binds observed velocity
and primitive step as well as origin/hull; SimpleJump rejects mismatched evidence.
Acceleration can use takeoff clearance while below launch speed, but Press still
requires velocity-bound flight and landing clearance.

## Offline evidence

- Windows x86 NMake Debug, warnings-as-errors: 38/38 tests passed.
- WSL Debian GCC `-m32` Debug, warnings-as-errors: 33/33 portable tests passed.
- Windows x86 Release DLL rebuilt; exact six undecorated exports verified.
- Additional 16/32-unit supported rises and diagonal-velocity cases passed in
  the targeted JumpProbe test on both platforms after the full runs.
- Native adapter test uses actual `queryNavWorld` with a fake TraceHull engine;
  nineteen logical queries produce nineteen standing-hull engine calls. A solid
  takeoff and unavailable TraceHull cannot produce a launch inspection.
- Independent box/slab obstacle geometry covers a low obstacle, blocking wall
  and low ceiling. Dense analytical-trajectory samples check the paired sweep
  envelope to `1e-4` fixture coordinate tolerance. Exact budget boundaries
  (19 and 21), stale replies at every ordinal, malformed support/hull data,
  invalid physics/identity/velocity and mismatched touchdown height are tested.

The geometry fixture's gravity800 and impulse268.3281573 are synthetic. These
tests establish the bounded model's query behavior, not a live engine's physics,
integration error, air acceleration, moving-platform behavior or runtime safety.
Future host integration must account for its measured movement model and numeric
error before using this inspection to dispatch Jump. It must derive takeoff and
landing from the selected transition, inspect approach/acceleration, revalidate at
dispatch and deliver actual dispatch/airborne/landing feedback. Live validation
remains post-Finish; real NAV compatibility remains partial.
