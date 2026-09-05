# P3-03 portable door wait boundary

This historical boundary implements the SDK-free `DoorWait` lifecycle only.
The subsequent [Use-door host integration](p3-03-door-host.md) connects it to Walk.
At the portable boundary, the existing
P3-03 ordinary door/stairs/wall/narrow checklist remains open. Walk and the
adapter do not invoke this controller yet; `QueryKind::Door` is still unavailable
from the adapter. This is not end-to-end door passage evidence.

## Contract

- One attempt is bound to agent, player generation, map, route and corridor step.
  Requests/results must match the full query stamp, including ordinal and tick.
- A fresh, usable closed-door observation with a host-proven use view emits one
  stationary `Press` intent. The existing Motor clears Press on cached frames.
  Later decisions are neutral; a lost command is not automatically retried.
- The caller supplies an explicit positive timeout and monotonic simulation
  clock including time between decisions. Regressing/stopped clocks on a new
  tick fail. Expiry wins at the exact deadline, with subtraction preventing
  overflow near the maximum clock value. No timeout profile is selected yet.
- Unknown, unusable or replaced door observations fail. Binding changes abort;
  duplicate/old ticks emit nothing and cannot reset the deadline. Terminal events
  occur once. Owners must abort on actor health or route invalidation.
- `DoorObservation.open` denotes fresh physical clearance of the requested
  passage, not private door state. `Clear` only permits Walk to inspect ground
  and clearance again. DoorWait never emits translation or declares arrival.
- `canUse` and the view require host proof of capability, range and target
  selection. This boundary does not fabricate that proof from a classname.

## Independent upstream reference

ReGameDLL_CS commit `679973265e1ac99a43193119e0da212ee568f5f9`:
`regamedll/dlls/doors.h` (`CBaseDoor::ObjectCaps`) identifies use-only doors as
impulse-use capable; `regamedll/dlls/player.cpp` (`CBasePlayer::PlayerUse`)
selects usable entities within a radius by view direction and tests the pressed
Use edge. The referenced files had no local modifications. Implementation was
written independently; no upstream code was copied.

## Verification

The portable suite covers initial clearance, closed/unusable doors, later
clearance, never-opening timeout, exact deadline, skipped time, clock overflow,
clock regression, missing/malformed/stale observations, replacement, binding
invalidation, cancellation and terminal-event uniqueness. Motor replay checks
at 8/16/100 ms verify a single Use press and zero translation.

Windows x86 NMake Debug adapter + portable tests: 31/31 PASS. WSL Debian GCC
`-m32` Debug portable tests: 26/26 PASS. Both builds use warnings-as-errors.
The integrated main was also verified before this boundary: Windows 30/30 PASS.
Live HLDS/ReHLDS acceptance remains post-Finish. Remaining work includes adapter
door identification and use-selection proof, bounded queries in Walk, measured
host-clock wiring, touch-door handling and fake-engine passage scenarios.
