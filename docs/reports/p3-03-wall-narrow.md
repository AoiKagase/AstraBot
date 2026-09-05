# P3-03 wall avoidance and narrow passages

Walk now measures lateral clearance, slows and recenters in narrow passages,
and tries a bounded supported sidestep around confirmed walls. Together with the
ground, Motor/host, stair and door reports, this completes the P3-03 implementation
checklist and its offline verification. Live acceptance remains post-Finish.

## Behavior

Two stamped hull sweeps measure up to 12 units of free movement to the left and
right of the desired path. Unknown, malformed or start-solid results stop the
controller. When either side has less than 8 units, the speed cap interpolates
between 40 and the ordinary 160 units/second. The steering offset is biased toward
the freer side, capped at 4 units and reduced near the target.

The adjusted endpoint stays within the active portal's tangent bounds and is
fully inspected for floor and hull clearance before movement. Motor receives
the corresponding view-relative lateral correction, so its normalized velocity
matches the inspected segment; the host's existing segment guard remains active.
Arrival is still later measured support at the goal, not the adjusted endpoint.

On a blocked forward probe, a bounded Blocker query distinguishes world BSP and
recognized `func_wall` / `func_wall_toggle` solids from doors and unknown actors.
Only confirmed geometry may enter wall avoidance. Door handling retains its
separate use/touch path. Actor yielding and short-lived overlays belong to P3-04.

Avoidance chooses the freer side (right on an exact tie), keeps that side for the
blocked attempt and inspects one lateral segment. It never alternates sides in
that attempt. Missing floor, insufficient clearance, exhausted budget or an
unusable portal-bound endpoint stop it. The profile permits at most 25 consecutive
blocked decisions; a fully verified forward path or corridor advancement resets
the attempt. This is local avoidance, not a general maze solver or the dispatched-
progress recovery controller planned for P3-07.

All side, blocker and alternate inspection queries share the existing 21-query
budget, including a reserved touch-dispatch guard. Samples remain capped at four.
There is no budget increase or automatic retry after a limit failure. Status and
MotionTrace include both clearances, narrow/avoidance state and lateral correction.

## Verification

- Windows x86 NMake Debug adapter + portable CTest: 32/32 PASS.
- WSL Debian GCC `-m32` Debug portable CTest: 27/27 PASS.
- Windows x86 Release DLL: PASS, exactly six required exports.
- All builds use warnings-as-errors. No hosted CI result is inferred from WSL.

The shared test-only collision model supplies independent swept center-space
geometry; production does not call it. Portable and fake-engine scenarios at
8/16/100 ms cover a 36-unit-wide physical passage with off-center start, an
avoidable wall, a wall closing the corridor, and missing floor at the side exit.
They verify speed/lateral changes, supported goal arrival, no collision along
emitted movement, actual host dispatch, and bounded query/sample counts. Portable
tests also reject unknown clearance, invalid profiles and exhausted query or
avoidance budgets. Existing door, stair and host guard regressions pass.

## Remaining work

P3-04 must implement reactive player blockers/overlay expiry and the actual
per-player adapter seam. P3-05 through P3-08 remain pending. The live matrix,
including real door/stair/narrow-passage physics, is not accepted by these
synthetic results. No server was started or project-wide Finish declared.
Real NAV compatibility remains partially validated.
