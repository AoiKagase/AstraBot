# P3-03 Walk host integration

This boundary connects `b7fdef6`'s portable controller to the existing adapter.
It completes the Walk/motor/scheduling/observability checklist inside P3-03.
Doors, stairs, wall avoidance and narrow-passage correction remain open; this
does not complete P3-03 or declare project-wide Finish.

## Behavior

`astrabot_goto <area-id>` builds an owned corridor from the selected complete
route and the observed hull. The final point is the actor's XY projected into
the goal area's hull-safe interior (one extra unit on each side), with NAV floor
Z. Intermediate targets remain selected portals. Invalid corridor/goal/hull or
unsupported traversal produces typed motion failure; route readiness alone
does not establish executable local movement or arrival.

Frame order is now: observe validity and guard the previous NAV command;
dispatch the previous transport queue; record its result; observe/decide Walk;
convert the intent with Core Motor and submit for a later tick. Status only
observes and reports, never schedules or submits movement. A route request's
tick is reserved for its own ground query; Walk begins on a later tick.

Decisions use the 25 Hz IntentPump with at most one decision per frame; missed
deadlines are counted without catch-up. Commands are produced per positive
simulation frame, with the transport's measured duration still authoritative
at dispatch. Zero-duration frames do not replay a queued NAV command.

Before dispatch, NAV commands must still match the actor/map, joined/alive/
grounded observation and hull. The larger of measured transport delta and
simulation delta is checked against remaining intent freshness. Exactly 120 ms
is permitted for a new intent; greater age is rejected. Accumulated measured
wall age also limits cached intents. Movement must remain within the inspected
segment (0.5 unit lateral, 4 unit vertical tolerance), and its measured msec
travel must fit the remaining segment. Reduced speed limits also reject stale
commands. These checks guard reuse; they do not replace fresh world queries or
predict moving obstacles.

Cancellation/replacement removes only the matching player/map/command-tick
queue entry. Cancellation schedules one neutral command for the still-valid
same actor, on a later frame. Death, disconnect and map changes cannot send
that command to an invalid or reused actor. A replacement route supersedes the
old stop request. Submission/dispatch rejection clears the cached intent until
a fresh decision. Query-time invalidation is deferred until borrowed calls
return. Dispatch tickets preserve old-route feedback across engine callback
invalidation or replacement without applying it to the new route. Transport
traces retain the duration actually used even if a callback resets the clock.

## Observability and offline evidence

`astrabot_nav_status` includes the latest motion decision and actual queued
command, selected edge, support/target, cursor, typed reasons, tick/route,
query/sample counts, intent age and queued/dispatched/rejected/missed counters.
The adapter retains 128 chronological motion records, with sequence/omitted
counts. Record kinds distinguish a new decision from queue/dispatch feedback;
decision fields on feedback records describe their originating decision.
Terminal decisions, cancellation and rejection are printed as events; normal
per-frame traffic stays in the bounded history. Route-only diagnostic output
continues to say `arrival=unverified`; the separate Walk state records measured
`Arrived` and support.

The fake-engine test uses actual registered commands, lifecycle scheduling,
ground/hull conversion, Motor, command submission and `pfnRunPlayerMove`.
Flat scripted physics reaches same-area, straight, L-shaped and zigzag goals
at 8/16/100 ms frames, with nonzero yaw. It checks later-tick movement, final
neutral output, no repeated action buttons, supported arrival, query budgets
and bounded history. Further scenarios cover cancellation, goal replacement,
zero delta, 120/120.001 ms freshness boundaries, position deviation, death,
ground loss, queue conflict, unavailable transport, query-time invalidation,
disconnect, map change, and map/goal changes reentered from RunPlayerMove.
Exact transport cancellation rejects wrong actor generations, maps and ticks.

Verification passed Windows x86 NMake Debug with warnings-as-errors and the full
30-test suite; WSL Debian GCC `-m32` Debug portable tests passed 25/25. The x86
Release adapter builds against pinned SDK
`7ec9b014f8c0a947a724644aebe34eb33706e44b` and retains exactly the six approved
undecorated exports. Hosted CI for this branch is pending.

No live server or real-device validation was run. The physics model is a flat
offline seam, not GoldSrc movement proof. Real NAV compatibility remains
partial. Ordinary doors/stairs/wall/narrow behavior, P3-04..P3-08 and the
post-Finish live matrix remain acceptance work.
