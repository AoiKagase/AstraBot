# P3-04 per-player NAV sessions

Main was fast-forwarded from `1df97cd` to `4e6a7d2`. This next implementation
boundary runs on `codex/p304-multi-nav`; no push or live server was performed.

NavConsole shares immutable mesh, graph and spatial index and lazily owns at
most 32 actor states. Each slot tracks its PlayerId generation, RouteSession,
Walk, IntentPump, pending command, guard budget and 128-entry motion history.
Replacing a slot generation retires its old state. Lifecycle dispatch and world
queries resolve the addressed player through the managed entity mapping.
Removal-pending players cannot submit new commands.

Commands accept an optional `slot:generation` selector:
`astrabot_goto <area> [slot:generation]`,
`astrabot_nav_status [slot:generation]`, and
`astrabot_nav_cancel [slot:generation]`. Without it, exactly one managed actor
must exist. Malformed, stale and ambiguous selections leave existing routes
and queues intact. Loading/publication remains map-wide and invalidates every
actor. Disconnect/removal/cancel affects only the addressed actor.

Actor storage retains stable addresses across synchronous query invalidation.
Invalidating the querying actor is deferred until its query returns; another
actor can retire immediately. Map-wide invalidation and reset defer retirement
of active queries before clearing shared navigation.

Synthetic tests drive two disjoint lanes at 8, 16 and 100 ms frame intervals.
Both actors arrive independently; cancel/disconnect preserves the other route.
They check actual entity movement, per-actor query counts (at most 21), floor
samples (at most 4), history identity/capacity, map invalidation, stale selectors,
slot reuse with fresh history and cross-actor invalidation during a query.
Existing single-client, door, steering and reentrant invalidation tests pass.

Windows x86 NMake Debug adapter/portable tests: 34/34 PASS, with the expanded
fake-client test rerun after adding generation/reentry assertions. Warnings are
errors. Windows Release x86 DLL: PASS with exactly six contracted exports.
WSL Debian GCC -m32 Debug portable tests: 29/29 PASS. Hosted CI is unverified.

This completes the per-player navigation implementation boundary, not P3-04 or
Phase 3. Bounded automatic replan consumption and expiring edge overlays remain
open. Synthetic navigation is not live multi-Bot acceptance. Live validation
remains post-Finish; real NAV compatibility remains partial.
