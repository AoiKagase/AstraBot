# P3-06 ladder host motion

NavConsole enables the owned Walk ladder controller for selected, immutable
published links. Every decision binds the current passage again and obtains
actor/map/route/step/tick-stamped contact, physics, clearance and support.
Pending commands retain the exact ladder plan, state, target and jump press
tick. Each actual engine dispatch verifies its rounded command duration and
current one-frame motion; guard and decision queries share a ceiling of21.
Submit rejection, guard rejection and actual transport results feed the
one-shot ladder dispatch contract. Cancellation clears queued commands.

Published endpoint centers now stand49 units from the measured face instead
of33. All four standing-hull XY corners must belong to the same NAV area.
This allows the published link to satisfy Corridor's existing inset contract;
discovery still uses at most12 engine queries per passage.

Ground approach measures the whole hull path and flat floor at intervals of
at most16 units. World support outside NAV remains distinct from target-area
support. At the final short upper-mount approach, missing floor is accepted
only with a clear shallow descent and measured capture by the selected ladder
model. Other missing floor or unrelated NAV support fails. Actual contact and
FLY mode are observed afterward; capture prediction never advances the route.

Upper exit uses a bounded rise/air candidate. Lower airborne exit issues one
Jump press: pinned standard-CS physics replaces velocity with270 along the
outward face normal, changes to WALK, then applies air movement and gravity.
It does not add the ordinary ground-jump vertical impulse. Grounded lower
exit retains its independently verified floor-kick option. Grounded ladder
jump physics is not enabled. A detached exit arc with fresh landing proof can
exceed the climb-loss speed threshold; unproved falls still fail.

The exit forecaster can also prove an explicitly supplied first command. Such
a result never issues a new exit intent. Runtime guards prove only their exact
one-frame command, keeping the combined budget finite instead of repeatedly
spending a whole trajectory budget. Completing the ladder requires observed
detached target-area support, followed by ordinary final-area route movement.

## Offline evidence

- The host fake-client fixture executes mount, climb, exit, landing and route
  arrival at8,16 and100 ms in both directions. Up uses an across-top passage;
  down uses a same-face passage. The small independent planar physics fixture
  does not call the production ladder predictor. Down sends exactly one Jump;
  up sends none. Actual query counts are checked each frame against21.
- Queued lower Jump is never sent after entity replacement before dispatch,
  replacement during a trace, or cancellation. Portable tests additionally
  reject wrong/duplicate dispatch identities, missing receipts, unsupported
  contact, excessive fall, exhausted reacquisition and stale observations.
- Ground approach rejects insufficient budgets, map/entity changes at every
  query and a missing final floor. A positive outside-NAV floor never becomes
  fabricated NAV support. Published discovery is composed into a real Corridor
  in the adapter fixture.

These are synthetic offline fixtures, not real-map or live-server acceptance.
Verification on2026-09-06: Windows x86 NMake Debug /W4 /WX passed43/43 CTest;
Linux x86 Debug passed37/37. Windows x86 Release built with tests disabled and
exports exactly Meta_Query, Meta_Attach, Meta_Detach, GetEntityAPI2,
GetEngineFunctions and GiveFnptrsToDll. SDK pin:
`7ec9b014f8c0a947a724644aebe34eb33706e44b`.

Unsupported face, geometry, physics and trace results remain explicit failures.
P3-07/P3-08 and project-wide Finish are separate. Live Windows/Linux HLDS/ReHLDS
mount/climb/dismount and map-change checks remain post-Finish work.
