# P2-06 — Immutable connectivity and deterministic A*

Status: P2-06 complete; T1-T4 and final whole-branch review approved.
Base: `6ea1e29` on `codex/p206-route-search`.

The user approved the proposed design and explicitly selected opt-in partial
routes on expansion-limit termination. This document makes its boundary and
observable rules concrete. Completion evidence below records P2-06 acceptance,
not project-wide Finish or live movement acceptance.

## Scope and architecture

Build an immutable graph from a shared `const NavMeshSnapshot`. Keep the snapshot
alive without modifying it. Canonicalize graph vertices by ascending area ID;
enumerate outgoing edges by cardinal direction, target ID, then traversal kind.
Snapshot wire order is unchanged. Reject null snapshots and unknown traversal
kinds before publishing a graph. Preserve directedness: never infer reverse edges.

An edge carries source ID, target ID, N/E/S/W direction and traversal. Connections
sharing endpoints but having different directions remain distinct. Parent state,
cost-policy context and returned routes all retain the selected edge. Do not add
future enrichment identity to static connections. Walk decoded from NAV remains
a default interpretation, not proof that movement is safe.

Search takes a const graph, start and goal area IDs, explicit limits, a partial
route option and a pure cost policy. Area IDs must be nonzero and belong to the
graph. P2-05 point queries compose externally using an index built from the same
snapshot; no point overload or implicit nearest-area fallback is added here.

All open/closed, parent-edge, g/f and queue state belongs to one query. Use an
indexed binary heap with at most one entry per vertex; decrease-key and reopening
must not create unbounded stale queue entries. Concurrent queries may share the
graph but not mutable policy context. Graph construction and search publish
results transactionally through typed result APIs with noexcept boundaries.

## Costs and heuristic

An area center is its XY midpoint with bilinearly interpolated Z, calculated in
double precision using the P2-05 geometry contract. The standard edge cost is
Euclidean 3D center distance plus nonnegative traversal, danger and experience
components. The default additional components are zero; no raw attribute or
tactical byte is interpreted as a penalty or traversal instruction.

The policy receives the complete selected edge and const source/target records.
It may prohibit the edge or return distance, traversal, danger and experience
components. Every component and the fixed-order sum must be finite and
nonnegative. Reject invalid values and accumulated-cost overflow as typed errors,
not blocked edges. Preserve the accepted component values on parent improvement
and in the final route; do not reevaluate the policy while reconstructing it.

Standard costs use center distance to goal as the heuristic. A custom cost policy
defaults to zero heuristic (Dijkstra), since arbitrary costs need not dominate
Euclidean distance. An explicitly supplied custom heuristic must be pure, finite,
nonnegative, zero at the goal and admissible for the supplied costs. Admissibility
is a caller precondition, not something local numeric checks can establish.
Inconsistent admissible heuristics are supported through reopening. No optimality
claim applies to a caller-supplied overestimating heuristic.

Queue ordering is exactly `(f, h, area ID)`. Relax only strictly smaller g; an
equal-g alternative keeps the first canonical parent. No epsilon is used. The
same binary, inputs and pure policy must produce identical edges, costs and
metrics; bitwise floating-point identity across different platforms is not claimed.
Zero-cost edges and cycles are permitted without cyclic parent chains.

## Termination and route publication

Return a status of Complete, Unreachable or ExpansionLimit for valid searches.
These are distinct from typed failures for invalid requests, budgets, numeric
errors or allocation failure. A complete route includes both endpoint IDs;
start equals goal returns one ID, no edges, zero cost and zero expansions.

One expansion means removing a non-goal vertex from the queue and processing its
outgoing edges. Reopening and expanding again counts again. Goal detection occurs
before the expansion-cap check: a queued goal may complete without expanding it.
An empty frontier is Unreachable. Otherwise, when the next expansion would exceed
the cap, stop with ExpansionLimit. A zero cap permits no expansions, not unlimited
work. Report expansion, examined-edge, successful-relaxation, reopen and peak-open
counts with precisely these events counted; reconstruction changes no metrics.

Only ExpansionLimit can produce a partial route, and only when explicitly enabled.
Choose among discovered vertices with finite g, including the start, by exact
`(3D center distance to goal, g, area ID)` ordering. This is independent of the
search heuristic. Return the currently known parent chain to that candidate;
it is not a completed or necessarily shortest route to the candidate. Without
opt-in, or for Unreachable, return no corridor/edges. Never return a partial route
for a typed error. A discovered but not selected goal does not imply completion.

Routes own corridor IDs, selected edges, per-edge cost evidence, fixed-order
component totals and total cost, plus termination status and metrics. A route
contains one fewer edge than area IDs, with matching adjacent endpoints. Separate
component totals may differ in the last bits from accumulated edge totals due to
floating-point association; total route cost is the sum of edge totals in route
order. For partial routes, an improved ancestor may not yet have propagated its
lower g to descendants: candidate ranking uses stored g, but returned totals use
the reconstructed edge evidence, not a stale candidate g. Tests cover this case.
Tests use exactly representable costs
for exact component identities and explicit tolerances only for geometric sums.

## Bounds and diagnostics

Graph limits explicitly bound areas, edges and logical graph bytes. Search limits
explicitly bound expansions and logical working bytes, including candidate result
storage. Zero is never interpreted as unlimited. Charge object storage and
element-count times sizeof before each corresponding allocation; check arithmetic
before multiplying or adding. Preflight fixed query arrays and worst-case route
storage using validated vertex/edge counts. Bound temporary construction storage
by those same validated counts. Budgets describe logical data, not allocator
overhead, excess capacity, shared-pointer control blocks or policy-owned memory.

Use CountLimitExceeded for logical budget violations, OffsetOverflow for checked
size arithmetic overflow, and AllocationFailure for caught allocation failures.
Invalid endpoints, unknown traversal and invalid cost/heuristic values receive
distinct appropriate typed diagnostics. Query diagnostics must not pretend a
runtime graph index is a NAV file byte offset; use zero offset and explicit
query/graph diagnostic context. Keep existing reader diagnostics unchanged.

Policy callbacks must not let exceptions escape the public noexcept boundary;
translate bad_alloc to AllocationFailure and other callback exceptions to a typed
policy failure. Numeric/policy errors in evaluated edges abort transactionally.
Prohibited edges are not relaxed. Policies are evaluated only on expanded edges;
search does not promise validation of unreachable policy outputs. Previously
published graphs, snapshots and routes remain usable after any failed attempt.

## Alternatives and selected approach

- Selected: immutable canonical graph, bounded indexed heap and query-local A*.
  This makes deterministic ordering and memory accounting explicit.
- Direct snapshot traversal saves a graph copy but couples search to input order
  and repeats lookup work; it is not selected.
- Always-zero heuristic is safe for every valid nonnegative cost policy but loses
  standard geometric guidance; retain it as the default for custom costs only.

## Verification and completion boundary

Use independently generated synthetic NAV fixtures and existing loader contracts;
do not import Valve/ReGameDLL files or implementation. TDD covers graph identity,
trivial/directed/disconnected routes, equal optima, direction-distinct parallel
edges, improved parents, reopened closed vertices under an admissible inconsistent
heuristic, prohibited edges, zero costs/cycles, component evidence, invalid
endpoints/traversal/numeric values and callback exceptions.

Fix expansion precedence and zero/exact caps, partial opt-in/ranking, repeated
results and metrics, parallel independence, graph/query byte boundaries and size
overflow. Sweep allocation failures through graph build, query preparation and
result reconstruction, proving no termination or partial publication and retaining
previously successful objects. Assert const publication and snapshot lifetime.

Run portable x64 Debug NMake /W4 /WX CTest and a separate MSVC /analyze build.
Retain the Nav PUBLIC `_ITERATOR_DEBUG_LEVEL=0` ABI setting required for reliable
allocation-failure tests. Refresh/query FocalSpan, inspect the final diff, stage
only intended files, check the staged diff, commit and verify hash/status.

P2-07 enrichment, P2-08 fuzzing, movement, Linux/live validation, main merge and
remote operations are excluded. Preserve existing worktrees and untracked local
FocalSpan/Serena data. This task does not declare the project-wide Finish gate.

## P206-T4 verification evidence — 2026-09-05

Verified in `H:/sourcecode/003.Game/amxmodx/AstraBot/.worktrees/p206-route-search`
on the reviewed T1-T3 source at `29d159a`, with T4 additions confined to tests and
documentation. Controller review approvals for T1-T3 are retained in STATE.

| Contract | Evidence |
| --- | --- |
| Graph ownership, canonical directed identity, center geometry and bounds | `graph_tests.cpp`; route tests retain graph/snapshot/previous results through allocation sweeps. |
| Costs, heuristic, complete routes and deterministic queue behavior | `deterministicDiamond`, `selectedEvidence`, `heapOrderingAndDecreaseKey`, `reopeningAndBlocking`, `zeroCyclesAndCustomDefault`. |
| Termination, partial ranking, budgets and transactional diagnostics | `capPrecedence`, `partialRanking`, `staleAncestorEvidence`, `queryByteBoundaries`, `allocationFailureSweeps`, `callbackFailureStages`, `numericFailureStages`, `metricSizeBoundaries`. |
| Concurrent independence and externally composed endpoints | `concurrentSearches`: two synchronized threads, 100 searches each, pure separate policy contexts, full result comparison against hand-checked serial baselines. `snapshotLifetimeAndSpatialComposition`: containing/nearest endpoints from the same retained snapshot, graph and route lifetime assertions; compile-time const publication/access checks. |

Canonical commands (the existing external helper was read only; cwd above):

```powershell
rtk proxy powershell -NoProfile -File H:/sourcecode/003.Game/amxmodx/AstraBot/.worktrees/p205-spatial-queries/build-portable-test/verify.ps1
rtk proxy powershell -NoProfile -File H:/sourcecode/003.Game/amxmodx/AstraBot/.worktrees/p205-spatial-queries/build-portable-test/verify.ps1 -Analyze
```

Both commands exited 0. Portable x64 Debug passed 12/12 (CTest 0.37 seconds);
the separate MSVC `/analyze` build passed 12/12 (0.73 seconds). Actual compiler
and analyzer output contained no warnings/errors. Generated flags retain `/W4
/WX` and the Nav PUBLIC `_ITERATOR_DEBUG_LEVEL=0` setting; the second build adds
`/analyze` in `build-portable-analyze`. The route test's output in both
`Testing/Temporary/LastTest.log` files confirms the 2 x 100 full-result comparisons
and snapshot/endpoint composition cases ran with the allocation failpoint disabled.

T4 regression tests were **initial-green** against existing production code on
their first Debug execution. No missing behavior, production correction or RED
failure was observed or claimed. Existing allocation-failure sweeps still report
four graph allocation sites, two query-preparation sites, and two reconstruction
sites for routes with corridors; exact query bytes are 792 for four vertices.

The scoped integration commit is `4eaaf95 docs: record nav route search verification`.
Full local command output and review reports are retained in the ignored
`build-portable-test/p206-review-evidence/` archive. Linux/live validation, main
merge, remote operations and project-wide Finish were not performed or declared.

## Final whole-branch acceptance

All four task reviews approved their scoped implementation and quality. The T1
SAL annotation finding was fixed in `648f7ce` and its scoped re-review passed.
The independent final review of `6ea1e29..4eaaf95` found no Critical, Important or
Minor issues, with no deferred or parked findings. The parent independently ran
the same canonical configure/build/CTest commands on `4eaaf95`: Debug12/12
(0.57 seconds) and separate analyze12/12 (0.58 seconds), both exit 0. The final
completion commit changes only STATE and documentation; code/tests remain those
reviewed and verified. Branch `codex/p206-route-search` and its worktree remain
available; main stays at `6ea1e29`.

Evidence limits remain explicit: the concurrent regression is not a race-detector
run or exhaustive schedule exploration; caller-supplied heuristic admissibility
and purity remain preconditions. Unknown traversal is covered by the production
validation helper, while current loader-derived end-to-end fixtures use Walk.

Controller process decisions: task headings were aligned with the brief extraction
tool without changing P206 task IDs or behavior; subagents inherited the model
under the tool's no-unsolicited-override rule; empty-input coverage preserves null
rejection and a valid edgeless singleton rather than inventing a zero-area snapshot.
If these interpretations need revision, the impact is respectively tool headings,
execution cost, or graph-input tests, not a silent change to the loader contract.
