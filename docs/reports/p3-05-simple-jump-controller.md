# P3-05 Simple Jump controller foundation

Built from `c65bed9` on `codex/p305-simple-jump`. The Simple Jump checklist is
still open: this boundary implements its portable controller, not host routing.

SimpleJump owns Approach, Align, Accelerate, Takeoff, Airborne, Recover,
Complete/Failed/Aborted states. An explicit source/target, takeoff/landing origin,
bounded speed/position/facing profile and finite phase timeouts are required.
Unknown/conflicting constraints (including NoJump and duck-jump) cannot launch.
Ground support, actual velocity and facing must agree before one Press is emitted.
Approach/acceleration commands are bounded; the takeoff-region cap accounts for
the forward distance remaining inside its circle rather than radial distance.

The controller accepts a trusted current-stamped JumpInspection value containing
the observed origin/hull, plan geometry, bounded query count, support and explicit
approach/takeoff/flight/landing clearance. Absent or stale proof is not clearance.
This is a contract for a future world-query producer: the controller checks
ownership and supplied evidence but cannot establish geometric safety by itself.
The source fields and false-clearance tests must not be presented as completed
world-query/planner integration. NAV attributes alone never supply these proofs.

Takeoff has an exact command/dispatch tick and full actor/map/route/step binding.
Only successful matching dispatch followed by observed airborne state enters
flight. Waiting for takeoff does not add ground translation. A rejected/missing
dispatch, takeoff timeout or flight timeout terminates without another Press.
The airborne intent keeps one heading, without an air-control planner. Completion
requires current supported target-area observation inside the landing region,
then uninterrupted grounded cooldown. Wrong/missing landing or lost support
fails; every terminal result releases Jump at zero requested movement once.

Tests drive Motor outputs through an independent idealized ballistic fixture at
8/16/100ms (test gravity800 and vertical impulse268.3281573), checking one Press,
cached-frame suppression, actual airborne/landing states and cooldown. These
parameters are synthetic and do not validate live GoldSrc physics. Fault cases
cover NoJump, unknown/false clearance, stale proof geometry/tick, wrong actor,
missing/rejected/stale dispatch, wrong/missing landing support, airborne/takeoff
timeout, lost support, repeated tick and abort. Limits in this fixture are
approach120, speed100..180, radius16, facing5deg, max distance96/rise32,
query count21, approach2s, takeoff200ms, flight1.5s and cooldown200ms.

Remaining work: derive a bounded takeoff/landing plan from a supported selected
transition and observed physics; generate genuine support/flight clearance
queries, connect Walk and command guards, deliver actual dispatch feedback, and
verify missed/wrong landings through the adapter. Recheck flight safety at the
dispatch boundary and carry recovery/cooldown ownership across transitions.
The existing host still rejects Jump hints. No live jump or Finish is claimed.

Verification: Windows x86 NMake Debug adapter and portable tests 37/37 passed;
WSL Debian GCC `-m32` Debug portable tests 32/32 passed. Both builds use
warnings-as-errors. Windows x86 Release DLL build and its exact six exports
passed. The Windows 37-test suite was rerun successfully before integration.
These are offline controller checks; hosted CI and live acceptance are separate.
