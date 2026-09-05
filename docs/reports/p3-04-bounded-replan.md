# P3-04 bounded automatic replan

Built from `7a0ef6c` on `codex/p304-bounded-replan`. This closes the remaining
P3-04 implementation slice with offline evidence; live acceptance remains open.

A dynamic-blocker timeout with a fresh valid stamped observation may schedule
one automatic replan. Missing capability, invalid/stale observations and absent
selected edges fail closed. Walk retires its cached blocker fact on timeout;
only the current query reply may justify the new exclusion.

Each actor owns one ReplanAttempt per explicit goal. It stores one directed
edge, identity, observation time and pending tick. A later frame consumes it
once; repeated schedule calls cannot refresh the pending fact, and replacement
route generations cannot replenish the attempt. The policy snapshot is owned
for one synchronous RouteSession request, excludes the selected edge for one
second and preserves distance/external-link costs. The standard graph is never
mutated. Reverse edges and different external generations remain available.
No expired fact is consumed and no failure automatically restarts a search.
Cancellation, goal replacement and actor/map invalidation retire pending state.

The terminal decision produces neutral movement before the later replan. The
replacement uses the existing grounded-area observation, fixed 100,000 expansion
and 256 MiB search limits, allowPartial=false and normal corridor/Walk startup.
It retains the goal area; its hull-safe target point is projected again from
the newly observed position. The 21-query per-actor decision budget remains;
replanning occurs on a separate tick with one grounded-area query. Diagnostics
include DynamicObstacle, attempt count/limit, fact lifetime and edge/link identity;
the existing route report records search status, cost and selected steps.

Portable tests prove direct 1->2 changes to 1->3->4->2 with cost 300, reverse
edge preservation, exact expiry, stale map/actor rejection, same-tick rejection,
no fact refresh, no second retry across route generations, external provenance
and additional cost, and unchanged graph results without the overlay.

Adapter tests at 8/16/100 ms frame intervals prove detour arrival, failure of a
second blocked route without a third generation, Unreachable without a partial
corridor, cancellation while pending and expiry before consume. Synthetic
physics verifies actual dispatched movement and arrival in the goal area.
Existing door reopening, player yield/avoidance, observation failures, per-player
isolation and lifecycle tests remain active. Graph reachability does not promise
local clearance: a geometrically blocked replacement stops at the finite bound.

Verification: Windows x86 NMake Debug adapter/portable 35/35 PASS; WSL Debian
GCC -m32 Debug portable 30/30 PASS; warnings-as-errors enabled. Windows x86
Release DLL builds with exactly the six contracted exports. Diff reviewed and
FocalSpan updated before commit. Hosted CI remains unverified, no push performed.

P3-05 crouch/jump, P3-06 ladders, P3-07 dispatched-progress recovery and P3-08
matrix remain. This is not Phase 3 or project-wide Finish. No live server,
deployment or real NAV/BSP modification occurred; real compatibility stays partial.
