# Phase 2 offline gate evidence

Date: 2026-09-05. Base main: 4fcc3fe. Work branch: codex/p208-nav-evidence.
Status: Phase 2 offline acceptance passed (P2-01 through P2-08).
This report ships with the implementation/evidence commit; main merge is separate.
Scope: independent SDK-free Nav Core, offline only. Not project-wide Finish.

## Gate mapping

| Task / acceptance | Executable evidence |
| --- | --- |
| P2-01 bounded little-endian values, EOF/overflow and finite numbers | nav.values, nav.byte_reader; byteOracle independently checks up to256 operations for every fuzz input |
| P2-02 v1-v5 header, BSP/Place version gates, exact offsets | nav.reader, nav.corruption; ten fixture hashes and literal header/value expectations |
| P2-03 connections, hiding, approach, encounter, opaque bytes | nav.area_records, nav.corruption; full values in all versions, legacy v1/v2 discard consumption, no inferred ladder |
| P2-04 immutable owned publication, references, budgets, diagnostics | nav.mesh, nav.corruption, nav.fuzz; exact errors and all1788 prefixes, allocation injection, retained-snapshot/lifetime/repeat checks |
| P2-05 containing/XY-clamp bilinear nearest | nav.geometry, nav.spatial, nav.differential;9216 independent linear comparisons, slopes, stacks, ties, boundaries and filters |
| P2-06 directed connectivity, A*, costs, limits and concurrency | nav.graph, nav.route, nav.differential;1024 Dijkstra pairs plus existing deterministic tie/reopen/custom-cost/partial/lifetime/concurrent tests |
| P2-07 synthetic ladder composition and selected metadata | nav.enrichment; up12/down15/two-up24, standard/custom costs, fingerprint/source-generation validation, ownership and deterministic merge |
| P2-08 corruption, fuzz, reproducibility and measurement | nav.corruption/nav.fuzz/nav.differential; Windows ASan, manifest checker, microbenchmark and this report |

P2-07 expected synthetic hash is42 followed by31 zero bytes (not a real BSP hash).
The static-only fixture is disconnected before links; separate upward/downward
link IDs introduce directed connectivity. Existing tests retain link evidence
after graph destruction and inject six composition/four route allocation sites.
Those tests remain in the normal16-test suite, not the ASan allocator environment.

## Observed verification

- Baseline portable Debug13/13 passed (0.91s).
- Final portable x64 Debug16/16 passed (6.51s); /analyze16/16 (6.92s), no warnings.
- Dedicated Windows ASan3/3 passed (27.98s), including10000 mutations.
- Long ASan100000 mutations passed (275.76s), with3043 valid loads and96957
  typed rejections; smoke has299 valid loads. Both paths are exercised.
- Fixed corpus:2465 exact kind/record/field/offset rejections, including1788
  prefix truncations, plus limit-boundary checks. Valid fixtures:10/10.
- Query oracle:9216 comparisons. Route oracle:1024 start/goal pairs (507 complete,
  517 unreachable); independent
  Dijkstra compares reachability and minimum costs, not arbitrary equal-cost
  corridors. Selected-edge tie ordering remains tested by existing exact tests.
- All ten fixture and two benchmark SHA-256/length pairs match the manifest.
- Empty/missing/duplicate and hash-mismatched manifests are rejected by
  persisted script regression tests; malformed journal and existing output
  are rejected by the extractor. Case9999 replay/extraction matches169 bytes.

The reproducible fixture table (hashes, versions, areas, connections, hiding,
approach/encounter/Place counts and expected routes) is
tests/nav/fixtures/evidence-manifest.json. Generation is original authored
little-endian encoding; no upstream implementation or map data was imported.
The fixed mutation corpus is source-generated, not a downloaded corpus.

TDD checkpoints: missing fixture.hpp, fuzz.hpp and scene.hpp produced the
expected RED build failures before the helpers were implemented. Existing Nav
behavior then passed; no production source fix was needed. An initial fuzz
smoke reached9688 cases before60s because it reopened two artifact files per
case; open-once flushed journals completed10000 cases in5.85s with the same
timeout and case count. No tests/limits were relaxed.
Self-review also caught empty-manifest false success: the controlled empty
input returned0 (RED); schema/full-set checks now reject it, missing/duplicate
entries and wrong hashes (GREEN) in all three build directories.

## Reproduction

Run from the feature checkout; scripts resolve that checkout themselves:

```powershell
rtk proxy powershell -NoProfile -File tools/verify-nav-evidence.ps1 -Mode Debug
rtk proxy powershell -NoProfile -File tools/verify-nav-evidence.ps1 -Mode Analyze
rtk proxy powershell -NoProfile -File tools/verify-nav-evidence.ps1 -Mode Asan -LongFuzz
rtk proxy ./build-portable-test/astrabot_nav_benchmark.exe build-portable-test/benchmark-fixtures
rtk proxy powershell -NoProfile -File tools/check-nav-manifest.ps1 -FixtureDirectory build-portable-test/evidence-fixtures -BenchmarkDirectory build-portable-test/benchmark-fixtures
```

The scripts use VS2026 Community locally (VsDevCmd override or vswhere fallback
for CI), x64, NMake, Debug, warnings-as-errors, tests ON and Metamod OFF.
ASan and analyze use separate build directories; /EHsc and Nav's public
_ITERATOR_DEBUG_LEVEL=0 are retained. ASan Nav/test compile flags include
/fsanitize=address /Zi with no /RTC or /ZI, link ends with /INCREMENTAL:NO.
dumpbin confirms clang_rt.asan_dynamic-x86_64.dll in the fuzz executable.
Normal corruption imports only Windows/Debug CRT DLLs; generated link commands
use astrabot_nav.lib plus Windows system libraries, no SDK/GameDLL libraries.

CTest count is16 normal,3 dedicated ASan; long fuzz is a separate test directory
with one600-second test, not an extra normal test. The smoke timeout is60s.
Logs are in build-*/Testing/Temporary, long logs in build-nav-asan/long-fuzz.
Before every Nav call the journal stores the exact candidate and seed/case/
mutation recipe. See tests/nav/corpus/README.md for extraction and replay.

Fuzz input cap64KiB,128 areas;64 Places,256 bytes each including NUL,16KiB total.
Nested per-direction connections128, per-area hiding/approach/encounter64,
spots/path64; all five aggregate categories4096. Snapshot logical cap4MiB.
These are logical limits, not process heap limits. Harness generation/replay is
also bounded, one current candidate at a time. No unbounded recursive mutation.

## Microbenchmark (non-gating)

Windows10.0.26200, AMD Ryzen7 5700X, VS2026 Community18.9.2 /
MSVC14.51.36231 toolset, CMake3.31.4, x64 Debug /MDd, iterator debugging disabled.
Measurements were collected while other verification was running; they are a
reproducible local baseline, not a controlled performance comparison or budget.
Five warmups,31 samples, median index15, nearest-rank p95 index29. Load/route
batch1, nearest batch100 (reported per operation); encoding and index/graph
construction are excluded from the timed load/query/route loops.

| Areas | Operation | median us | p95 us |
| --- | --- | ---: | ---: |
|128|load|1718.200|2689.200|
|128|nearest|46.976|59.088|
|128|route|320.500|514.900|
|1024|load|13649.500|17363.300|
|1024|nearest|375.018|660.662|
|1024|route|1707.900|2529.800|

Inputs are original 2x2 sloped patches with directed +1/+4 and selected backward
arcs, reverse wire order; their exact hashes and sizes are in the manifest.
Benchmark decoder profile alone expands input to1MiB and areas to1024; its
nested and snapshot limits stay4096/4MiB. This does not change fuzz caps.

## Limitations and acceptance boundaries

- Real NAV/pinned-server differential comparison: not performed (no supplied
  lawful fixture/output pair). Independent oracle comparison is not a substitute
  claim of real-map interoperability.
- Hosted GitHub CI: configured for Windows Debug and separate ASan, not run.
  No remote operation was performed. Local verification is the evidence here.
- Linux CI was removed from the active workflow; reintroduction, Linux builds
  and live/device validation require explicit project-wide Finish.
- No live ladder detection, grounded query, Bot locomotion, combat or Phase3
  feature is claimed. nearestGeometry is XY clamp plus bilinear height, not
  exact 3D nearest-point projection or proof of traversability.
- No crash, natural OOM or ASan finding was observed in the completed bounded
  runs. ASan is not UBSan or a proof of all UB/leak/race absence; finite fuzz
  runs cannot prove absence of every malformed-input defect.
- Main merge and branch/worktree cleanup are not part of this implementation.
