# Phase 2 plan — Independent Nav Core

## Goal

Without GoldSrc or a server process, load defensively parsed Counter-Strike
`.nav` versions 1–5, expose immutable area/connection metadata, perform pure
nearest-area/connectivity queries and deterministic A*, reject malformed input,
and prove that separately supplied ladder enrichment participates in routes.

No upstream nav code, nav generation, live entity scan, local movement,
Experience DB, or adapter dependency is in scope.  See
[the extraction decision](../research/nav-extraction.md).

## Planned modules

```text
src/nav/io/*              bounded byte reader and v1-v5 decoder
src/nav/model/*           immutable IDs, area/static metadata
src/nav/query/*           spatial index, nearest/connectivity, A*
src/nav/enrichment/*      immutable traversal links such as ladders
src/nav/diagnostics/*     typed errors and route evidence
tests/nav/fixtures/*      provenance-recorded generated/redistributable fixtures
tests/nav/corpus/*        malformed mutations and fuzz seeds
```

## Commit-sized tasks

### P2-01 — Nav value model and bounded byte reader

- **Goal:** create the SDK-free data and error foundation.
- **Files/modules:** `nav/model`, `nav/io/byte_reader`, unit tests.
- **Implementation outline:** explicit little-endian `u8/u16/u32/f32`, checked
  offset/length arithmetic, finite vectors/extents, value IDs and typed error
  `{kind, offset, record, field}`.  No native struct reads.
- **Dependencies:** Phase 1 portable identity conventions; documented format.
- **Tests:** endian values, exact EOF, truncation at every width, overflow,
  non-finite float, deterministic errors.
- **Acceptance:** portable target includes no SDK; sanitizer/static-analysis pass;
  invalid reads never allocate or publish partial model.
- **Risk:** float/endianness assumptions.  Freeze bytes in generated fixtures.

### P2-02 — File header and Place dictionary v1–v5

- **Goal:** decode versioned file envelope.
- **Files/modules:** `nav/io/nav_reader`, fixture generator/test data.
- **Implementation outline:** validate magic/version, optional v4 BSP size,
  optional v5 `uint16` Place dictionary with bounded NUL strings, area count.
- **Dependencies:** P2-01.
- **Tests:** one minimal fixture per version; wrong magic, 0/6 version, zero/
  excessive counts, unterminated/oversized/invalid Place entries.
- **Acceptance:** result reports version/fingerprint inputs/Places exactly and
  every header truncation produces the expected offset.
- **Risk:** fixture licensing.  Prefer tiny independently generated fixtures and
  record generator + field provenance; do not commit Valve map assets casually.

### P2-03 — Area and tactical static records

- **Goal:** decode all serialized per-area records.
- **Files/modules:** area/connection/hiding/approach/encounter decoders and values.
- **Implementation outline:** version-aware hiding and encounter layouts,
  directional connections, attributes, extents/corner heights, approach and
  Place entry.  Enforce per-section and total allocation limits.
- **Dependencies:** P2-02.
- **Tests:** v1 vector hiding, v2 object hiding, v3 ID encounter, v4 BSP field,
  v5 Place; zero/max small counts and truncation at each field.
- **Acceptance:** golden expected values match all fixtures; no ladder is claimed
  from file bytes; serialized/static data stays immutable.
- **Risk:** legacy v1–2 discard layout.  Test byte consumption even for data not
  retained semantically.

### P2-04 — Transactional validation and snapshot publication

- **Goal:** reject corrupt graphs before exposing a `NavMeshSnapshot`.
- **Files/modules:** `nav/model/builder`, validator, diagnostics.
- **Implementation outline:** unique/nonzero IDs, finite/non-degenerate geometry,
  valid bits/directions, all cross-reference resolution, Place bounds, count/
  memory caps, explicit trailing-byte policy; publish only after full validation.
- **Dependencies:** P2-03.
- **Tests:** duplicate IDs, dangling/self/duplicate connections according to
  policy, bad approach/encounter/hiding IDs, inverted extent, allocation bombs,
  deterministic diagnostic ordering.
- **Acceptance:** corrupt corpus cannot yield a partial snapshot; valid snapshot
  is immutable and stable when loaded twice.
- **Risk:** rejecting tolerated historical oddities.  Separate structural error
  from warning and document compatibility exceptions with a fixture.

### P2-05 — Spatial index and pure nearest-area queries

- **Goal:** locate containing/nearest static area without engine traces.
- **Files/modules:** `nav/query/spatial_index`, geometry queries.
- **Implementation outline:** deterministic grid/BVH choice, bilinear area height
  where required, closest point, maximum radius/vertical policy and stable
  lowest-ID tie-break.  Define `containing`, `nearestGeometry` separately from
  future adapter-assisted `nearestGrounded`.
- **Dependencies:** P2-04.
- **Tests:** inside/boundary/outside, stacked floors, sloped corners, equal
  distance, empty mesh, radius/vertical filters, property tests vs linear scan.
- **Acceptance:** indexed result equals reference linear implementation and is
  deterministic across insertion order.
- **Risk:** selecting the wrong stacked floor without traces.  API name/policy
  must make the limitation explicit.

### P2-06 — Connectivity and deterministic A*

Pre-A* connection contract: cardinal buckets now hold `NavConnection` values
(`target`, `traversal`) rather than bare IDs. See
[the traversal contract](../superpowers/specs/2026-09-05-pre-astar-traversal-design.md).
Search must preserve the chosen directed edge (including its source area and
cardinal direction), not just a parent area ID, and expose that edge to its pure
cost policy and route result. Unknown traversal kinds must fail closed.

- **Goal:** return an observable area corridor and cost evidence.
- **Files/modules:** graph query, A* search record, route/cost result.
- **Implementation outline:** query-local open/closed/parent/g/f state, stable
  queue ordering, pluggable pure cost components, directed edges, partial/failure
  reason, expansion limit and metrics.  Never mutate an area.
- **Dependencies:** P2-04; P2-05 for point endpoints.
- **Tests:** trivial/disconnected/directed/multiple optimal paths, reopen/lower
  cost, prohibited edge, expansion limit, repeatability, route/component sums.
- **Acceptance:** fixed fixture returns fixed IDs/cost/reason for same inputs and
  seed/config; parallel queries do not interfere.
- **Risk:** heuristic inconsistency and float ties.  State admissibility policy,
  epsilon and stable tie order in tests.

### P2-07 — Traversal enrichment and synthetic ladder route

Implementation evidence and API contract:
`docs/superpowers/specs/2026-09-05-p207-traversal-enrichment-design.md`.
Caller-supplied BSP SHA-256 equality, source-generation isolation, immutable
composition and selected-link evidence are covered by the dedicated enrichment
test target. Live discovery/movement remain out of scope.

Keep enrichment separate from file-derived cardinal connections. Validate
`isKnownTraversalKind` when accepting supplied links; reject unsupported kinds
without publishing a partial graph. Do not cast raw area attributes or approach
traversal bytes to the runtime enum. Preserve selected-link identity/provenance
when composing routes; do not merge distinct links merely by endpoint pair.

- **Goal:** prove non-file traversal data composes without GameDLL types.
- **Files/modules:** `nav/enrichment/traversal_link`, snapshot composition.
- **Implementation outline:** immutable typed links with source/generation,
  endpoints, connected areas, direction and cost metadata; merge validation
  against map fingerprint and area IDs.  Create an independently authored
  synthetic ladder fixture.
- **Dependencies:** P2-06.
- **Tests:** up/down directed ladder route, missing area, wrong fingerprint,
  duplicate/conflicting links, static-only route unchanged, deterministic merge.
- **Acceptance:** `.nav` reader still emits zero ladders; supplying valid
  enrichment changes connectivity/A* exactly as expected.
- **Risk:** overfitting to ReGameDLL's ladder object.  Store traversal facts only,
  not engine entity pointers or local-movement state.

### P2-08 — Corruption/fuzz/differential evidence and gate report

- **Goal:** harden the parser and freeze reproducible Phase 2 evidence.
- **Files/modules:** fuzz target, mutation corpus, test manifest, benchmark/report.
- **Implementation outline:** fuzz byte reader/loader with allocation/time caps;
  mutate every fixture section; where legally redistributable local fixtures are
  available, compare observed metadata with the pinned server/tool output without
  embedding upstream implementation.
- **Dependencies:** P2-07.
- **Tests:** corpus regression, seeded fuzz smoke in CI, longer sanitizer job,
  load/query microbenchmarks.
- **Acceptance:** all valid fixtures and invalid corpus pass; no crash/OOM/UB;
  report lists fixture hash/provenance, version, areas, links and expected routes.
- **Risk:** differential oracle may share upstream bugs.  The AstraBot validation
  contract remains authoritative for safety.

## Offline gate

- v1–v5 valid fixtures load and expose expected area, directional connection,
  hiding, approach, encounter and Place values;
- pure containing/nearest queries pass stacked/boundary tests;
- connectivity and A* return exact routes, total/component cost and reason;
- synthetic ladder enrichment changes the expected directed route;
- wrong magic/version, truncation, malformed counts/strings/floats/references and
  fingerprint mismatch are rejected with stable diagnostics;
- the test process links no GoldSrc, Metamod-P, ReGameDLL or ReAPI library.

This gate does not claim live ladder discovery or Bot movement; those are Phase 3.
