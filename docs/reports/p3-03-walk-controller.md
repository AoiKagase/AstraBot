# P3-03 Walk controller offline evidence

This is a commit boundary inside the existing Walk/motor checklist, not P3-03
completion. Main was fast-forwarded from `5a34298` to `ef16095`; this work starts
from that integrated state on `codex/p303-walk-controller`.

## Implemented contract

- `GroundProbe::locate` validates one measured ground observation, including
  actor/map/tick/route identity, floor normal, feet height and containing NAV.
  It makes no assumption about the area before a portal crossing.
- `Walk` owns its corridor, cursor and one primitive lifecycle per transition.
  Each decision reports binding, tick, state/reason, primitive event, measured
  support, inspected endpoint, neutral or movement intent and query/sample counts.
- Targets follow the selected portal and extend past its boundary by the
  observed hull size plus an explicit positive crossing margin. Advancement
  requires measured target-area support with the whole hull inside the target
  extent. At most one transition advances per decision. Exhaustion does not
  establish arrival; same-area and final arrival require observed support and
  distance/height agreement with the explicit goal floor point.
- Only ordinary Walk with zero area attributes is supported in this boundary.
  External traversals and special/unknown area hints stop explicitly. The
  corridor retains start attributes even for a same-area route. P3-05 will
  interpret those constraints; no generic Walk fallback is introduced here.
- Each decision reuses its exact ground query synchronously during segment
  inspection. Actual host queries have consecutive unique ordinals and remain
  within the explicit budget. Intermediate floor samples cannot leave the
  active source/target area pair. There is no nearest lookup or query retry.
- Float endpoints round toward the observed position to preserve the strict
  distance budget. The direction uses the inspected endpoint, and speed is
  capped so 120 ms of ideal motion cannot pass it. This is not proof against
  external forces, teleportation or stale queued adapter commands.
- Duplicate/stale ticks produce no intent and no query. Actor/map invalidation
  permanently aborts the instance; failures and cancellation are neutral with
  a single terminal event. The caller must retire it on goal/route replacement.

## Offline verification

`astrabot.nav.walk` couples the controller with IntentPump, Core Motor and a
later-tick synthetic command queue. Flat scripted physics reaches same-area,
straight, L-shaped and zigzag goals at 8/16/100 ms frames, with exact repeated
decision/position traces. Reverse travel checks the other cardinal directions.
Tests also cover straddling versus fully entering a portal, corridor exhaustion,
missing ground at the goal, blocked hull/ceiling, missing floor, unsafe drop,
stale query identity, host exceptions, query budgets, invalid speed/goal, actor
reuse, map mismatch, off-corridor support, cancellation and unsupported hints.
The diagonal simulation reproduced a float endpoint exceeding the distance
budget before the inward-rounding correction.

Validation commands use Windows x86 NMake Debug and WSL Debian GCC `-m32`
Debug, both warnings-as-errors. Final results: Windows adapter/portable 30/30,
Windows portable-only 26/26 plus fixture/manifest verification, and Linux
portable 25/25 passed. Hosted CI for this branch is pending. No real map input
is modified or added to Git.

## Remaining acceptance at this boundary

Host wiring subsequently landed in [the host report](p3-03-walk-host.md).
The following records what remained when this controller boundary was verified.

The console/adapter still does not drive Walk. Next: wire decisions and motor
submission after dispatch; cancel queued navigation commands on replacement;
prevent a long hitch from dispatching stale movement before observation; bound
reuse to the inspected segment; expose command rejection/dispatch in traces;
verify the complete fake-engine goto/RunPlayerMove path. Door use/wait, stairs,
wall avoidance, narrow-passage correction and the later P3 controllers remain.
Discrete floor samples do not prove continuous support. Flat synthetic physics
does not establish HLDS behavior. Live validation remains post-Finish; real NAV
compatibility remains partial, and project-wide Finish has not been declared.
