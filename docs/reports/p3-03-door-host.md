# P3-03 Use-door host integration

Subsequent [touch-door work](p3-03-touch-doors.md) adds contact-based activation.
The remaining-touch statements below describe this historical Use-door boundary.

Ordinary use-only `func_door` / `func_door_rotating` observations now connect
DoorWait to Walk and the queued host movement path. This is offline evidence for
Use doors. Touch-to-open doors, wall avoidance and narrow-passage control remain
open within the existing combined P3-03 checklist.

## Observations and control

The adapter uses one hull trace per Door query and returns an entity index plus
serial number within the stamped map. A retained value ID must still resolve to
the same live door. Clearance is physical passage clearance, not velocity or a
private toggle-state inference. No SDK pointer or private data enters Nav.

Player-use selection uses the pinned ReGameDLL use-only flag, 64-unit engine
sphere enumeration and a view toward `absmin + size/2` from the player's eye.
There are at most 33 enumeration calls per selection check. Unknown competitors
(including buttons, hostages, other doors or players) reject Use: their private
ObjectCaps are not assumed. Missing APIs, out-of-range entities, malformed bounds
and generation changes also fail closed. Master locks are not inferred; a door
that does not open times out.

Walk enters stationary waiting only after current ground is verified and a
typed door observation identifies the obstruction. This also handles a future
floor probe starting inside a tall door. A floor gap without a door remains a
failure. Door queries share the existing 21-query decision budget and consecutive
ordinals. Waiting uses two queries (ground plus door). Stair/sample limits remain
unchanged.

The console chooses a 1,000,000-microsecond door timeout and supplies the intent
pump's accumulated simulation clock, including skipped decision frames. One Use
Press is queued for a later tick; the stationary command is rechecked immediately
before dispatch for the same target generation and view. This trace-free guard
is bounded to one check per queued press/frame, counted separately as
`use_checks`; together with a decision's selection the cap is 66 enumeration
calls per frame. Rejected presses are never reissued. Cached frames release Use.

A fresh clear observation produces a neutral decision. A later full ground and
segment inspection must succeed before movement resumes. Clearance never means
arrival. An immediately reblocked last door fails rather than starting another
press/wait cycle. Actor displacement during stationary waiting, lost ground,
route cancellation and invalid actor identity stop the attempt.

## Verification

- Windows x86 NMake Debug adapter + portable CTest: 31/31 PASS.
- WSL Debian GCC `-m32` Debug portable CTest: 26/26 PASS.
- Windows x86 Release DLL: builds with exactly the six required exports.
- All builds use warnings-as-errors. Final diagnostic/finite-value review edits
  are rechecked with the affected Walk, DoorWait and fake-client tests.
- Fake-engine scenarios at 8/16/100 ms cover Use, delayed opening and supported
  goal arrival; locked doors; competing Use candidates; generation replacement
  and competition introduced between queue/dispatch; cancel/death; unsupported
  touch-only capability; and malformed sphere enumeration. Tests compare
  observed query counts with actual hull calls and independently check the Use
  radius/direction predicate at RunPlayerMove.
- Portable tests cover the missing future-floor case, monotonic ordinals,
  exhausted query budget, stale/throwing door replies, exact timeout, replacement,
  reblocking and mandatory reinspection after clearance.

The initial host test incorrectly searched a bounded history after its rejection
record had been evicted. It now observes the event while frames run; production
history remains capped at 128 records.

## Reference and remaining acceptance

Independent implementation informed by ReGameDLL_CS commit
`679973265e1ac99a43193119e0da212ee568f5f9`: `doors.h` (`ObjectCaps`), `player.cpp`
(`PlayerUse`), `player.h` (use radius) and `bmodels.cpp` (`VecBModelOrigin`). The
referenced files had no local modifications; upstream implementation was not
copied. The adapter SDK remains the pinned Metamod-P checkout.

No live server was started. Real NAV compatibility remains partial. Touch-door
contact handling, additional local obstacle control and the remaining Phase 3
plans are still pending; neither P3-03 nor project-wide Finish is declared.
