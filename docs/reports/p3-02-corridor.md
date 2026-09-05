# P3-02 corridor and primitive lifecycle evidence

## Corridor slice (2026-09-06)

Implemented SDK-free `nav/corridor/corridor.*`. Only Complete routes with
consistent area/step sequences and exact selected graph edges are accepted.
External identity includes source/generation/link, endpoints and traversal;
parallel edges are never reconstructed from area IDs alone. Transitions own
their extents, attributes, selected edge and independent support endpoints.

Cardinal Walk portals require exact shared boundaries and positive overlap
after explicit XY hull shrink. Degenerate/no-overlap/narrow/reversed geometry
returns typed `InvalidPortal`, without a center fallback. Bilinear support is
computed in double on each side, preserving slopes and vertical discontinuity.
External entry/exit must fit each area with hull clearance. Every transition
still requires world probes; static support does not authorize a step/jump.

Look-ahead reverse-projects through a caller-bounded number of gates and stops
at external links. The returned target stays on the active source-side portal;
it never skips a corner or advances the cursor. This is conservative gate
targeting, not a funnel/spline optimizer. Cursor advancement requires the exact
step plus caller-verified target-area support. Exhaustion is not arrival.
Limits bound transition storage and selected-edge membership checks, with
overflow checks before allocation. No automatic budget retry.

Verification: tests failed to link before implementation. After implementation,
Windows x86 NMake Debug /W4 /WX portable CTest passed 21/21, plus existing
fixture/manifest checks; WSL Debian 13 GCC 14.2 -m32 Debug warnings-as-errors
passed 20/20. The one-test difference is the Windows-only real-file checker
regression. Tests cover cardinal/reverse/unequal/sloped/degenerate portals,
clearance, bounds, invalid route/partial/unreachable, L/zigzag look-ahead,
jitter/replay, same-area exhaustion, external ownership and parallel identity.

No adapter or movement commands, real input writes, live server or Finish
decision. P3-01 real-file compatibility remains partially validated. Hosted CI
for this branch is not yet run.

## Primitive lifecycle slice (2026-09-06)

Implemented `nav/local/primitive.*` as one value-owned instance per transition.
Enter is accepted once; completion/failure/abort emits exactly one terminal
event. Existing Walk/Crouch/Jump/Ladder tags are retained; unknown tags and Drop
produce explicit UnsupportedTraversal failures. This is lifecycle dispatch,
not implementation of those motion controllers.

Updates are bound to agent/player generation/map/route generation/step and a
strictly later tick. Rejected updates and all terminal results return neutral
intent, clearing action requests. Running feedback validates finite direction,
speed [0,400], lateral correction [-1,1], optional view and action vocabulary.
The future motor still clamps to observed speed and command constraints.
Completion needs trusted controller feedback with verified selected-target
support. These values do not perform world queries or establish live arrival.
The session owner must abort on invalidation and create the next lifecycle.

Tests cover enter/terminal once, duplicate/stale updates, actor/map/route/step
identity, abort, missing/wrong support, controller failure, unknown/Drop tags,
external link ownership, invalid intents, and exact two-pass corridor-to-
lifecycle/cursor replay. Final Windows x86 Debug /W4 /WX: 22/22 CTest plus
fixture/manifest verification. WSL Debian 13 GCC 14.2 -m32 Debug warnings-as-
errors: 21/21. Production changes are SDK-free Nav modules only; Core command
types/constants are consumed as headers without adding a reverse dependency.

P3-02's two implementation/offline slices are complete. P3-03 owns wiring into
RouteSession/host, world probes, Walk, scheduling and motor commands. No runtime
motion, adapter change, live acceptance, real compatibility upgrade or Finish
is claimed. Hosted CI and main integration for this branch remain pending.
