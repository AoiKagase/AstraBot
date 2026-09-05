# P2-06 Deterministic Nav Route Search Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish immutable connectivity and bounded deterministic A* routes with selected-edge cost evidence.

**Architecture:** Build a canonical adjacency graph over a retained const snapshot. Use query-local records and an indexed heap, pure cost/heuristic callbacks and transactional route publication. The approved spec is authoritative for behavior; this plan defines implementation seams and regression vectors.

**Tech Stack:** C++17, existing SDK-free astrabot_nav, CMake/NMake, VS 2026 x64 Debug, assert-based CTest.

**Spec:** [approved design](../specs/2026-09-05-p206-route-search-design.md)

## Global Constraints

- Zero is never interpreted as unlimited.
- No epsilon is used.
- Keep existing reader diagnostics unchanged.
- Retain the Nav PUBLIC `_ITERATOR_DEBUG_LEVEL=0` ABI setting required for reliable allocation-failure tests.
- P2-07 enrichment, P2-08 fuzzing, movement, Linux/live validation, main merge and remote operations are excluded.
- Preserve existing worktrees and untracked local FocalSpan/Serena data.
- Work only in `.worktrees/p206-route-search`; use RTK, explicit staging and the repository FocalSpan workflow.

## File map

| File | Responsibility |
| --- | --- |
| `src/nav/query/graph.hpp/.cpp` | immutable ID-sorted graph, selected edge, bounds and center calculation |
| `src/nav/query/route_types.hpp` | public policy context, components, request, result and metrics |
| `src/nav/query/route_search.hpp/.cpp` | public noexcept search, validation, relaxation, termination, reconstruction |
| `src/nav/query/detail/indexed_heap.hpp` | bounded heap over query records, decrease-key and stable order |
| `src/nav/query/detail/route_budget.hpp` | checked logical graph/query accounting helpers |
| `src/nav/diagnostics/error.hpp` | append runtime graph/route contexts and policy failure kind; preserve old values |
| `tests/nav/route_fixture.hpp` | independently generated v1 NAV fixture and loader helper |
| `tests/nav/graph_tests.cpp` | graph and budget contract tests |
| `tests/nav/route_search_tests.cpp` | deterministic search, policy, partial, concurrency and OOM tests |
| `tests/nav/fixtures/README.md` | generated-field provenance, no upstream fixtures/code |
| `CMakeLists.txt` | graph/route sources and two test executables |
| `.agent-state/STATE.md` | active checkpoint, not duplicated specification |

## Build recipe and per-task workflow

Use a VS developer environment for all CMake build commands. Create the following
ignored helper with apply_patch at `build-portable-test/verify.ps1` in this worktree:

```powershell
param([switch]$Analyze)
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$build = 'build-portable-test'
$flags = ''
if ($Analyze) {
    $build = 'build-portable-analyze'
    $flags = ' "-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /GR /EHsc /analyze"'
}
cmd /c ('call "' + $vs + '" -arch=x64 -host_arch=x64 && cmake -S . -B ' + $build + ' -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON' + $flags + ' && cmake --build ' + $build + ' && ctest --test-dir ' + $build + ' --output-on-failure')
exit $LASTEXITCODE
```

`VERIFY` below means:
`rtk powershell -NoProfile -ExecutionPolicy Bypass -File build-portable-test/verify.ps1`.
Run the helper after each red/green change and record the actual failing/passing
assertion or compile error. It is acceptable to run the focused CTest expression
after a successful build: `rtk proxy ctest --test-dir build-portable-test -R
astrabot.nav.route --output-on-failure`. Never count a stale binary as evidence.

Before each task, read STATE, inspect relevant source, check Git and FocalSpan
status and update/query if needed. After each green task, inspect the diff, update
FocalSpan and STATE/task checkboxes, explicitly stage its listed paths, run
`rtk git diff --cached --check`, commit with the listed message and check log/status.
Do not commit local index/configuration or the ignored helper. No new approval is
needed between the tasks once execution is selected unless a contract must change.

### Task 1: P206-T1 — Bounded immutable graph

**Files:** graph.hpp/.cpp, detail/route_budget.hpp, graph_tests.cpp,
route_fixture.hpp, diagnostics/error.hpp, fixtures/README.md, CMakeLists.txt.

**Interfaces:** In `astrabot::nav::query`, define:

```cpp
struct NavGraphLimits { std::size_t maxAreas{0}, maxEdges{0}, maxGraphBytes{0}; };
struct NavDirectedEdge {
    model::NavAreaId source{}, target{};
    std::uint8_t direction{0};
    model::NavTraversalKind traversal{model::NavTraversalKind::Walk};
};
class NavGraph final {
public:
    static diagnostics::ReadResult<std::shared_ptr<const NavGraph>> build(
        std::shared_ptr<const model::NavMeshSnapshot>, const NavGraphLimits&) noexcept;
    std::size_t areaCount() const noexcept;
    std::size_t edgeCount() const noexcept;
    std::size_t logicalBytes() const noexcept;
    std::optional<std::size_t> find(model::NavAreaId) const noexcept;
    const model::NavAreaRecord& area(std::size_t vertex) const noexcept;
    NavQueryPoint center(std::size_t vertex) const noexcept;
    std::size_t edgeBegin(std::size_t vertex) const noexcept;
    std::size_t edgeEnd(std::size_t vertex) const noexcept;
    const NavDirectedEdge& edge(std::size_t edgeIndex) const noexcept;
    std::size_t targetIndex(std::size_t edgeIndex) const noexcept;
};
```

Index accessors require valid indices from this graph. Private storage comprises
snapshot ownership, a vertex vector (snapshot index, center, edge range) and an
edge vector (selected edge, target vertex). No mutable public accessors/constructor.

- [x] Add the fixture helper `route_test::Area` with id, extent and four target-ID
  vectors, plus `snapshot(const std::vector<Area>&)`. Encode v1 header, area ID,
  attribute 0, eight extent floats, four connection counts/IDs, hiding count byte
  0, approach count byte 0 and encounter count uint32 0. Use checked loader limits
  large enough for these tiny tests. Generate little-endian fields locally using
  shifts and memcpy for float bits, as existing spatial tests do. Record provenance.
- [x] Register `astrabot_nav_graph_tests` / `astrabot.nav.graph`; run VERIFY baseline
  before adding tests (expected 10 tests), then compile failing graph API assertions.

```cpp
auto r = NavGraph::build(snapshot({area20, area10}), {2, 2, 100000});
assert(r && (*r.value)->area(0).id == model::NavAreaId{10});
assert((*r.value)->edge(0).source == model::NavAreaId{10});
assert((*r.value)->edge(0).target == model::NavAreaId{20});
assert((*r.value)->edge(0).direction == 0);
assert((*r.value)->edge(1).direction == 1);
assert((*r.value)->edgeBegin(1) == (*r.value)->edgeEnd(1));
```

Here area10 has target 20 in north and east; area20 has no outgoing edges, and
both have valid unit-square extents. Declare those fixture values in the test.

- [x] Implement checked accounting with `count > (SIZE_MAX-used)/elementSize`
  before multiplication; report OffsetOverflow first, then CountLimitExceeded if
  the checked sum exceeds the cap. Test helpers directly with SIZE_MAX so overflow
  coverage requires no huge allocations. Charge graph object and both vectors'
  logical elements, not the already-owned snapshot. Validate all traversal kinds
  via a small internal edge-validation helper also exercised for unknown enum bytes;
  do not expose a mutable snapshot or external enrichment API just for testing.
- [x] Canonicalize vertices with ID comparator, binary-search target indices and
  sort each range by direction/target/traversal. Compute midpoint XY in double and
  center Z as four quarter-weighted corners (do not narrow midpoint to float).
- [x] Run VERIFY for empty/null input, preserved wire order, lifetime after caller
  releases snapshot, two direction-distinct edges, count/byte exact boundaries,
  arithmetic overflow and unknown traversal rejection. Expected 11 tests total.
- [x] Commit `feat: add immutable bounded nav graph` using this task's explicit files.

### Task 2: P206-T2 — Complete routes, cost evidence and stable A*

**Files:** route_types.hpp, route_search.hpp/.cpp, detail/indexed_heap.hpp,
route_search_tests.cpp, diagnostics/error.hpp, CMakeLists.txt.

**Interfaces:** Consume T1 NavGraph. Define these public types in the query namespace:

```cpp
struct NavCostComponents { double distance{0}, traversal{0}, danger{0}, experience{0}; };
struct NavCostDecision { bool blocked{false}; NavCostComponents components{}; };
struct NavCostContext {
    const NavDirectedEdge& edge;
    const model::NavAreaRecord& source;
    const model::NavAreaRecord& target;
    double geometricDistance;
};
struct NavHeuristicContext {
    const model::NavAreaRecord& area;
    const model::NavAreaRecord& goal;
    double geometricDistance;
};
struct NavRoutePolicy {
    const void* context{nullptr}; // borrowed for one synchronous search
    NavCostDecision (*cost)(const NavCostContext&, const void*){nullptr};
    double (*heuristic)(const NavHeuristicContext&, const void*){nullptr};
};
struct NavRouteLimits { std::size_t maxExpansions{0}, maxWorkingBytes{0}; };
struct NavRouteRequest {
    model::NavAreaId start{}, goal{};
    NavRouteLimits limits{};
    bool allowPartial{false};
};
enum class NavRouteStatus { Complete, Unreachable, ExpansionLimit };
struct NavRouteMetrics {
    std::size_t expansions{0}, examinedEdges{0}, relaxations{0}, reopens{0}, peakOpen{0};
};
struct NavRouteStep { NavDirectedEdge edge{}; NavCostComponents components{}; double total{0}; };
struct NavRouteResult {
    NavRouteStatus status{NavRouteStatus::Unreachable};
    std::vector<model::NavAreaId> areas;
    std::vector<NavRouteStep> steps;
    NavCostComponents components{};
    double total{0};
    NavRouteMetrics metrics{};
};
class NavRouteSearch final {
public:
    static diagnostics::ReadResult<NavRouteResult> search(
        const NavGraph&, const NavRouteRequest&, NavRoutePolicy = {}) noexcept;
};
```

Callbacks may throw; the outer search catches bad_alloc separately and any other
exception as appended `PolicyFailure`. Append `Graph`/`Route` record contexts and
endpoint, cost, heuristic and budget fields without changing old enum values.
Use UnsupportedValue for unknown traversal, InvalidInput for missing endpoint,
InvalidValue for bad numeric policy output, offset zero for runtime diagnostics.

- [x] Register `astrabot_nav_route_tests` / `astrabot.nav.route`, linked with
  astrabot_nav and Threads::Threads. Write red tests through the API above:

```cpp
auto r = NavRouteSearch::search(*graph, {{1}, {1}, {0, 100000}, false});
assert(r && r.value->status == NavRouteStatus::Complete);
assert(r.value->areas == std::vector<model::NavAreaId>{model::NavAreaId{1}});
assert(r.value->steps.empty() && r.value->total == 0);
assert(r.value->metrics.expansions == 0);
```

`graph` is a T1-loaded fixture containing area 1; also request absent/zero IDs and
reverse-only routes and assert typed error versus Unreachable respectively.
- [x] Add internal query records with infinite initial g, cached h, absent parent
  edge, retained components, closed flag and heap position. Preflight record,
  heap-index, corridor and step maximum storage before allocating. Charge result
  and query control objects once. Handle vector max_size/length errors safely.
- [x] Implement standard cost, custom-cost zero heuristic, explicit custom
  heuristic and finite/negative/overflow checks. Validate heuristic goal zero even
  for start==goal. Cache heuristic on discovery; callbacks receive immutable areas.
- [x] Implement heap comparator `(f,h,area ID)`, strict-g relaxation and reopening.
  Store component evidence when assigning parent; never call cost on reconstruction.
  Reconstruct backwards with at most areaCount vertices, reverse, and accumulate
  totals in forward order. No per-edge traversal overwrites or reverse inference.
- [x] Fix deterministic diamond: edges 1->2,1->3,2->4,3->4 all custom cost 1, h=0;
  require IDs 1,2,4, total 2 and identical full metrics over 100 runs and reordered
  input records/connections. Test source/target shared in north/east with different
  policy costs; selected direction and callback context must match result evidence.
- [x] Test inconsistent admissible heuristic: S->A=3, S->B=1, B->A=1, A->G=10;
  h(S)=0,h(A)=0,h(B)=5,h(G)=0. Expect A reopened once, route S,B,A,G, total 12.
  Also block B->A and require S,A,G total 13; verify zero-cost cycle termination.
- [x] Test components (distance=1,traversal=2,danger=4,experience=8) on each of two
  selected edges: totals 2/4/8/16, overall 30, callback invoked only on expansion.
  Run VERIFY; expected 12 CTest entries. Commit `feat: add deterministic nav A star search`.

### Task 3: P206-T3 — Expansion limits, opt-in partials and failure atomicity

**Files:** route_search.cpp, detail/route_budget.hpp, route_search_tests.cpp,
graph_tests.cpp; public types remain those defined in T2.

- [x] Write red assertions for zero cap and start!=goal:

```cpp
auto r = NavRouteSearch::search(*graph, {{1}, {4}, {0, 100000}, true});
assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
assert(r.value->areas == std::vector<model::NavAreaId>{model::NavAreaId{1}});
assert(r.value->metrics.expansions == 0);
```

Repeat with allowPartial=false and assert empty areas/steps. A one-edge goal
completes with cap 1; an exhausted disconnected frontier is Unreachable even at
the cap. Detect goal before cap, but do not complete merely on discovery.
- [x] Implement cap termination in this order: frontier empty, goal at top,
  expansion budget, expand. Count examinedEdges per visited outgoing edge,
  relaxations per strict improvement, reopens only when improving a closed vertex,
  peakOpen including start. Use checked metric increments if size_t can overflow.
- [x] Select partial from discovered finite-g records by geometric goal distance,
  g and ID, independent of custom h. Test each tie level with explicit center
  coordinates and policy tables, both open/closed candidates and a goal discovered
  but not yet popped. Do not return partials for Unreachable or typed errors.
- [x] Test stale descendant g: S->A=3,S->B=1,A->D=1,B->A=1,D->G=100;
  h(S)=0,h(A)=0,h(D)=0,h(B)=5,h(G)=0. Stop after expanding S,A,D,B (cap 4).
  Place D closest to G; expect partial S,B,A,D with returned total 3, even though
  D's stored ranking g remains 4 until propagation. Include the allowed discovered
  G in ranking: block D->G in this fixture so G is not discovered; retain a separate
  unblocked complete-route test. This fixes reconstruction evidence versus stale g.
- [x] Sweep failAfter through graph construction and every query allocation until
  success, using the existing spatial-test global-new pattern. Enable injection
  only around tested calls; keep previous graph/route alive and recheck afterwards.
  Assert AllocationFailure and absent value at every failed allocation. Also throw
  bad_alloc and a nonallocation exception from both callbacks. Parallel tests run
  with fail injection disabled to avoid a shared fail counter data race.
- [x] Test NaN, infinities, negatives, component/g/f overflow, nonzero goal heuristic,
  graph/query exact logical bytes and one-byte-under, and checked size overflow
  without large allocation. Never treat malformed output as a blocked edge.
- [x] Run VERIFY and commit `test: harden nav route limits and failure atomicity`.

### Task 4: P206-T4 — Integration evidence and completion checkpoint

**Files:** route_search_tests.cpp, fixtures/README.md, approved spec completion
evidence section, this plan's checkboxes and .agent-state/STATE.md.

- [x] Add repeat/concurrency assertions using separate requests/results and pure
  policy contexts, for example launch two std::threads searching opposite requests
  over the same graph, join and compare each result to its serial baseline. Repeat
  at least 100 queries per worker. Include full edge identity and metrics comparisons.
- [x] Assert ownership after original input bytes and caller snapshot reference
  are destroyed; graph still works. Use compile-time type assertions for const
  graph publication and graph area access. Compose one P2-05 endpoint lookup with
  search over the same retained snapshot, without adding a new public point API.
- [x] Run VERIFY and `VERIFY -Analyze` (expand VERIFY to the command above). Require
  12/12 tests in each configuration. Inspect actual compiler/analyzer output.
- [x] Review staged/source diff for all spec sections, placeholders, accidental
  scope, SDK leakage and debug artifacts. Refresh FocalSpan, query graph/search
  symbols, explicitly stage only intended files and run cached diff check.
- [x] Record real red/green and final verification evidence; retain STATE verifying
  pending controller final whole-branch review (explicit T4 dispatch boundary).
  Commit `docs: record nav route search verification`.
- [x] Check `rtk git log -1 --oneline`, `rtk git status --short --branch` and report
  commit hashes, CTest/analyzer results and remaining post-Finish acceptance.

T1-T3 completion/approvals are recorded in STATE (T3 commit `29d159a`). T4 added
tests were initial-green with no production change: canonical Debug12/12 (0.37
seconds), separate /analyze12/12 (0.73 seconds), both exit 0. See the approved
spec's P206-T4 verification evidence and ignored task-4-report.md for exact output.
T4 tests/evidence are complete; final whole-branch review belongs to the controller.

## Plan self-review

All spec sections map to T1 (graph/identity/budget), T2 (policy/A*/evidence), T3
(termination/partial/error atomicity) or T4 (integration/verification). Callback
types are shared rather than redefined per task. No public mutable snapshot or
unimplemented enrichment API is introduced for testing. The baseline is 10 tests;
the two new executables yield 12, independently of internal assertion count.
