# Phase 2 to Phase 3 planning gate

Date: 2026-09-05. Audited HEAD: `b49f4da6e20beec43aa1c47678725eb1865aacf6`.
Branch: `main` (also origin/main at inspection). Initial tracked tree clean;
untracked `.focalspan.json` and `.serena/` preserved. This report's containing
commit changes documentation only. No Phase 3 production source was written.

## Repository review

Reviewed status, full HEAD, branch, last 30 commits, diff stat and tracked-file
inventory, AGENTS.md, CMakeLists.txt, LICENSE, NOTICE.md, architecture, existing
Phase 2/3 plans, offline evidence, Nav/host/adapter sources and test contracts.
README.md does not exist; no replacement README is needed for this scoped audit.
Existing Phase 3 plan is `docs/plans/phase-3-nav-movement.md`, P3-01 through P3-08.
No additional task number or duplicate phase plan was created.

FocalSpan CLI status was ready/fresh at the repository root (133 files, 1509
symbols, revision `64ba50c238a30f52`), and focused queries located route tests and
MovementCoordinator/LifecycleCoordinator command timing. Source checks refine
the retrieved evidence; the current checkout is authoritative.

## Phase 2 verdict: PASS (offline only)

| Required result | Current HEAD evidence |
|---|---|
| Portable Nav Core | CMake `astrabot_nav`, only SDK-free Nav sources and C++17 headers |
| Safe bytes/header/v1-v5 reader | io/byte_reader, nav_reader, area_reader; nav.byte_reader/reader/area_records tests |
| Immutable snapshot and validation | io/mesh_loader.cpp, model/mesh_snapshot.hpp, mesh_validator.cpp; nav.mesh/corruption |
| Nearest-area queries | query/geometry, spatial_index; nav.geometry/spatial/differential |
| Deterministic A* and selected edges | query/route_search, graph, route_types; nav.route/graph/differential |
| Traversal enrichment and synthetic ladder | enrichment/traversal_link.hpp, graph composition; nav.enrichment |
| Corruption/fuzz evidence | tests/nav/corruption_tests.cpp, fuzz_tests.cpp, corpus/README.md |
| Independent query/route oracle | differential_tests.cpp and evidence/scene.hpp; linear geometry and Dijkstra comparisons |
| ASan evidence | tracked phase-2-offline-gate.md and verify-nav-evidence.ps1; separate Windows ASan configuration |
| Fixture provenance | fixtures/README.md, evidence-manifest.json; original encoders, no upstream assets |
| Offline gate report | phase-2-offline-gate.md; P2-01 through P2-08 recorded passed |

Fresh current-checkout verification in this audit:

- `rtk proxy powershell -NoProfile -File tools/verify-nav-evidence.ps1 -Mode Debug`
  exited 0: Windows x64 NMake Debug configure/build, **16/16 CTest passes**,
  generated manifest checks and script checks succeeded.
- Separate `tools/check-nav-manifest.ps1 -FixtureDirectory build-portable-test/evidence-fixtures`
  exited 0, with all ten generated fixtures checked.
- Final documentation review checks: all eight existing task headings retained,
  every task contains the requested eleven planning fields; relative document
  references checked and `git diff --check` / cached diff checked before commit.

Historical HEAD-contained evidence, **not rerun in this planning audit**:
Windows /analyze 16/16, dedicated ASan 3/3, long fuzz 100000 cases, 2465 typed
corruption rejections including 1788 prefixes, 9216 nearest oracle comparisons
and 1024 Dijkstra pairs. These bounded checks are not universal compatibility,
absence-of-UB or live-server proof. Hosted CI was not executed here.

No unfinished P2-01–P2-08 offline implementation was identified. Real NAV,
Linux and live acceptance remain outside that verdict. Historical feature-branch
wording in the earlier report/state is not current branch state: inspected main
already contains P2-08 at b49f4da. No new P2-09 was created.

## Real NAV readiness: Not yet validated

Only generated synthetic/benchmark/fuzz NAV files were found under this checkout
and its build worktrees. No lawful real file plus independent expected output
was provided. No asset was added or live server started. See
[the read-only protocol](../research/real-nav-compatibility.md) for provenance,
wire/structural fields, geometry nearest/route, limits and unchanged-input proof.
P3-01 owns this prerequisite; portable contracts can proceed while it is pending,
but real-map readiness cannot pass.

## Architecture and implementation decision

Keep NavMesh connectivity, selected-edge Traversal and per-tick Motion Primitive
separate. Reuse NavRouteResult rather than create another route type. Adopt
portable RouteSession/corridor/LocalNavigator/tagged primitive state/motor and
adapter-only world queries/command delivery. The full contract, alternatives,
status policy, lifecycle, future hooks and scheduling are in
[architecture.md](../architecture.md#phase-3-local-navigation-decision-2026-09-05).

Walk/Crouch/Simple Jump/Ladder are active targets. Drop is recognized but
unsupported, not implicitly walked. Goal arrival requires physical support and
goal-area confirmation; Complete route status alone is insufficient.
ExpansionLimit prefixes are diagnostic-only. Stuck detection requires dispatched
movement and failed expected progress; deterministic recovery carries finite
attempts through replans. Observability is implemented with the first sessions.

Current actor mapping is single-primary at fake-client/lifecycle/dispatch seams.
Per-PlayerId portable state avoids coupling; P3-04 reserves a narrow separately
reviewable adapter commit before two-Bot acceptance and 8/16-Bot runtime loads.
25 Hz local decisions feed per-frame fresh commands through next-tick dispatch;
the existing adapter quantizes actual frame delta to 1..255 ms.

## Linux and Finish decision

Recommend pre-Finish Linux portable/Nav CI in P3-01, contingent on explicit
AGENTS.md/policy revision; consolidate at P3-08. No policy exception was enacted,
CI changed or Linux build run here. Current Linux/live prohibition remains.
Offline implementation completion and post-Finish live acceptance are separately
recorded to remove the old plan's circular live-only completion wording.
Project-wide Finish still requires every project plan, never just Phase 3.

## Risks and next task

**Blocking for the corresponding gates:** lawful real NAV/independent structural
expectations missing; live ladder/contact/ground/door behavior unverified;
pre-Finish Linux activation needs a policy revision; real multi-Bot loads need
the adapter seam; all live checks need project-wide Finish and a pinned runtime.
None of these blocks completion of this planning-only audit.

**Non-blocking for portable planning:** fixture/parser shared assumptions,
opaque attributes versus supported motion semantics, strict reference/trailing
rules on old files, geometry-only nearest, uncertain real portal clearance,
frame quantization and untuned recovery thresholds. Follow the planned probes;
do not silently weaken validation or present synthetic movement as live proof.

Advanced GapJump/LongJump/EdgeTraverse/Boost, wall-edge balance, demonstration/
trajectory learning, RL/LLM, Tactical Planner/Combat AI, Experience DB, successRate,
adaptive path costs and crowd prediction remain deferred. Only typed terminal
outcome identity is reserved for a future Experience consumer.

Next session: **P3-01 first commit — SDK-free bounded read-only NAV inspector
with synthetic tests**. The detailed [existing task map](../plans/phase-3-nav-movement.md)
defines subsequent slices; no production implementation is part of this report.
