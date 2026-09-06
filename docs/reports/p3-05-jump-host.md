# P3-05 guarded Jump host integration

The host enables Walk-owned Simple Jump for supported native cardinal portals.
Pending commands carry the selected step, plan, physics and exact Press tick.
The dispatch guard rechecks current identity, standing hull, physics and movement
before calling RunPlayerMove. Only the actual transport result acknowledges a
Press; immediate submission rejection is accepted at the command tick, whereas
a successful acknowledgement must come from a later tick.

## Physics and bounds

The explicit standard-CS profile reads public sv_gravity, entity gravity and
mp_jump_height (ordinary-CS fallback 45). The independently inspected pinned
ReGameDLL revision b0889847fe6d03898be88acc9e366660efb40ab5 supplies model evidence:
pm_shared.cpp PM_JumpHeight/PM_Jump and game.cpp's default cvar.
The vertical impulse is sqrt(1600 * height); no upstream implementation or
private data access is included. Private per-player jump-height and movement
hook overrides are outside this profile and are not verified.

Water, non-WALK movement, base velocity, moving support, invalid gravity/height,
active stamina and held old Jump are rejected at the applicable guard.
Ground preparation and launch use bounded support/clearance queries. Launch
reserves a query for the actual dispatched frame; total host queries per actor
and frame stay within 21. Airborne sweeps use observed velocity and rounded
transport msec. Landing contact is checked against the selected target; cursor
advance still requires later measured grounding and the 1.5-second cooldown.
Cancellation releases buttons, including in flight. No automatic Jump retry,
air-control planner, gap jump or serialized NAV change is added.

## Verification

- Windows x86 NMake Debug, warnings as errors: 40/40 tests passed.
- WSL Debian GCC -m32 Debug portable tests: 35/35 passed.
- Windows x86 Release DLL: exactly the six required undecorated exports.
- Fake engine integration covers single/consecutive Jump followed by Walk at
  8/16/100 ms; actual Press counts, landing, cooldown and per-frame query bounds.
- Faults cover blocked launch, absent takeoff, wrong landing, airborne cancel,
  query-time map invalidation, changed launch velocity/gravity/stamina/water,
  and an obstacle appearing in flight.
- Portable regression distinguishes same-tick failed submission from invalid
  same-tick success acknowledgement.

P3-05's implementation slices have offline evidence. Real engine physics,
mod/plugin overrides and live transition acceptance remain post-Finish.
Cross-primitive and multi-player acceptance belongs to the P3-08 matrix.
Real NAV compatibility stays partial. This report does not declare Phase 3 or
project-wide Finish complete; P3-06, P3-07 and P3-08 remain.
