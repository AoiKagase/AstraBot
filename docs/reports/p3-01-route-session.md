# P3-01 portable route session evidence

Date: 2026-09-05. Base/main: `9cc8794` (real NAV compatibility correction,
fast-forward integrated before this slice). This records only the portable
session slice, not P3-01 completion or project-wide Finish.

## Implemented contract

`astrabot_nav` now provides `runtime/route_session`, `movement_snapshot` and
`world_query` without SDK linkage. Each session binds BotAgentId, PlayerId and
MapGeneration. Requests advance a checked route generation, resolve a current
area using one same-tick GroundedArea query, and run existing bounded A*.
The caller explicitly publishes the immutable graph with its map generation.
The query host must supply actual floor support and area observations; geometric
nearest alone is not evidence of current grounded area.

Only Complete produces Ready/executable. Complete, including a same-area route,
does not mean arrival. Unreachable and ExpansionLimit are terminal diagnostic
results; partial prefixes are disabled by default and never executable.
No implicit search memory/expansion allowance or automatic retry exists.
The world-query budget is per explicit request (one query maximum in this slice),
not a frame scheduler budget. Equal-tick explicit replacement goals are allowed;
older observations/requests are rejected. elapsedUs records simulation delta
without inventing a wall clock or implementing timeouts.

Unknown movement facts use optionals. Missing life/connect/join facts fail as
InvalidSnapshot; known false values have concrete lifecycle reasons. Query replies
must match actor, agent, map, tick, route generation, ordinal and kind. Unknown
support, invalid area, unavailable/failed or exhausted queries fail closed.
Query exceptions are converted to typed reasons. The interface and route policy
are borrowed synchronously; reentrant session calls are rejected while busy.
The owner must serialize access; this is not a thread-safe controller.

Goal replacement returns at most two bounded events: the old generation's
terminal cancellation and the new generation's result. Cancellation emits one
terminal event for a Ready generation. Actor/map change observations retire the
instance, preventing revival of the old binding; the host constructs a new
session for a new identity. Death/disconnect stop the route. A later fresh explicit
request can restart a still-valid binding after health is restored. No observation
automatically starts a route. Foreign command requests are rejected without
replacing the active managed actor's goal.

The session retains its immutable graph/mesh and an immutable owned route result.
Retained traces preserve selected edges, including external link source, generation,
ID and endpoints after cancellation and graph release. Cursor advancement, motion,
arrival checks and command dispatch belong to subsequent slices.

## Verification

### x86 correction (2026-09-05)

The user clarified that all AstraBot builds are x86-only, including portable
tools/tests. The earlier x64 run below is historical evidence, not the target
acceptance run. AGENTS.md, the evidence script and CI now select x86 and separate
`build-portable-x86-test`, `build-portable-x86-analyze`, `build-nav-x86-asan`
directories. The compiler host can remain x64. CMake rejects 64-bit targets;
reconfiguring the old x64 cache confirmed that rejection.

Windows x86 NMake Debug with `/W4 /WX` and inspector ON: **20/20 PASS**
(10.93 seconds). All ten fixture hashes, 2,457 exact rejection cases plus
limit boundaries, and four manifest regressions passed. PE headers of all
generated executables report `IMAGE_FILE_MACHINE_I386` (0x14c).
An existing graph-test optional index comparison needed explicitly unsigned
`std::size_t` expectations to pass x86 warnings-as-errors; validation was not
weakened. x86 Analyze/ASan and hosted CI were not run in this correction.

### Historical x64 run

Windows x64 Visual Studio 2026, NMake, Debug, `/W4 /WX`, Metamod OFF,
tests and NAV inspector ON. Run `tools/verify-nav-evidence.ps1 -Mode Debug`.
CTest includes `astrabot.nav.session` alongside the existing portable tests.
The script also verifies v1-v5 fixture bytes/hashes and manifest regressions.
Final result: **20/20 CTest PASS** (9.45 seconds), 2,457 exact rejected cases
plus limit-boundary checks PASS, all ten fixture hashes PASS and all four
manifest rejection regressions PASS. Main integration baseline was 19/19 PASS.

Fake-host coverage includes selected directed routes and costs, same area,
invalid actor/goal/snapshot/graph/current area, unreachable and bounded prefixes,
query budget/failure/unknown ground, stale reply stamps/kind, old observations,
goal replacement, death/disconnect, actor/map retirement, independent actors and
graph/mesh/external-link ownership. Regression tests were observed failing before
the unknown-state and lifecycle fixes. Allocation fault injection, full movement
replay and real adapter queries are not acceptance claims of this slice.

## Remaining acceptance

Console goto/status/cancel, managed-actor selection and adapter Nav linkage remain
the next P3-01 slice. Real NAV compatibility remains partially validated as recorded
in `p3-01-real-nav-compatibility.md`. No real assets were modified or added here.
No Linux, live-server, movement or Finish validation was performed.
