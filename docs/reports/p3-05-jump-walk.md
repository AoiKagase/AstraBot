# P3-05 Simple Jump owned by Walk

Built from `ebe2000` on `codex/p305-jump-walk`. This slice connects the portable
route owner to Jump. The host does not yet supply the profile/physics or dispatch
guards, so P3-05's Simple Jump implementation checklist remains open.

Walk accepts optional `WalkJumpLimits` and a current actor/map/route/step/tick-bound
`JumpPhysics` with each Jump update. Missing, stale or changed physics cannot
launch a transition. Each selected step owns one SimpleJump instance, derived
geometry, inspection state and dispatch acknowledgement. Primitive entry has a
separate tick; grounded phases use prepare/launch/landing inspection, while
Takeoff/Airborne do not manufacture ground observations.

`reportJumpDispatch` accepts only the first result for the active step's exact
Press command. Other actors/steps/commands and duplicate reports cannot overwrite
it. The controller validates the dispatch timestamp and success; missing, rejected
or stale dispatch followed by airborne observation fails without retry. Cancellation
and failure clear the controller and request stationary Jump release.

The existing per-decision budget includes host-reserved queries. Jump helper
ordinals map into that enclosing sequence without revalidating stale responses by
accident. A Takeoff/Airborne update can consume no new queries even when the host
already used the entire21-query budget; grounded proof cannot bypass exhaustion.

Only target support followed by grounded cooldown advances the corridor cursor.
Completion belongs to the exact step and requires the actor hull inside the target
extent. The final step's already-consumed Jump hint does not request another jump
while finishing the goal inside that same area. Subsequent transitions retain
their own hints and lifecycle. Consecutive Jump steps clear old Press identity.

Before entering Jump, an enabled crouch gate confirms standing posture. A crouched
actor waits for verified standing clearance and the observed standing hull; a low
ceiling retains Duck at zero speed until timeout. Old crouch intent cannot leak
into a Jump command. Ordinary Walk and unsupported traversal behavior remain
covered by their existing tests.

## Offline verification

- Windows x86 NMake Debug with warnings-as-errors: 40/40 tests passed.
- WSL Debian GCC `-m32` Debug with warnings-as-errors: 35/35 portable tests passed.
- Windows x86 Release DLL rebuilt; exact six undecorated exports verified.
- The final Press-identity cleanup was rebuilt and the Jump/Walk test rerun on
  both platforms after those full runs.
- The25Hz IntentPump plus synthetic Motor/ballistic simulation completes a single
  Jump route and two consecutive Jumps followed by ordinary Walk at8/16/100ms.
  Press occurs once per step; cached frames do not repeat it. No cursor advancement
  occurs during recovery; supported cooldown precedes every Jump completion.
- Tests reject missing/rejected/stale/wrong-actor dispatch, changed physics,
  invalid actor and wrong landing. They cover query exhaustion, full reserved
  budget in Takeoff/Airborne, abort release and standing-clearance wait/timeout.

The physics in these fixtures remain synthetic (gravity800, impulse268.3281573).
Actual MovementCoordinator dispatch is not simulated by reporting a successful
portable acknowledgement. Next integration must establish the host physics model,
call `reportJumpDispatch` from the real result seam, validate each queued Jump
command and handle observed airborne frames without removing grounded Walk guards.
Host trace output must expose the new Jump diagnostics. Live validation remains
post-Finish and real NAV compatibility remains partial.
