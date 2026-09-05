# P2-07 traversal enrichment design

Status: approved for implementation by the user (2026-09-05).

## Boundary

Compose caller-supplied directed links with an immutable snapshot into a new
immutable NavGraph. Preserve static-only build and all P2-06 route contracts.
No subagents, SDK types, ladder discovery, movement, BSP IO/hash computation,
P2-08, Linux/live validation, main merge or remote operations.

## Data and API

NavMapFingerprint is exactly 32 bytes of caller-supplied BSP SHA-256.
The caller binds the expected fingerprint to the snapshot; equality with the
link set fingerprint does not independently authenticate that binding.
NavTraversalLinkSet owns its fingerprint and vector of NavTraversalLink.
Each link has nonzero uint64 sourceId, generation, linkId; from/to area IDs;
double 3D entry/exit points; known NavTraversalKind; Forward/Up/Down direction;
and finite nonnegative additionalCost. Each record is directed: no reverse
edge generation. Up requires exit.z > entry.z, Down requires exit.z < entry.z.
Forward imposes no Z relation. Points need not lie on their areas; this is not
a proof of executability. Known non-ladder traversal kinds remain admissible.

NavGraph::compose(snapshot, expectedFingerprint, linkSet, graphLimits,
enrichmentLimits) noexcept returns ReadResult<shared_ptr<const NavGraph>>.
Enrichment limits explicitly contain maxLinks and maxWorkingBytes (zero is
not unlimited). Static build stays source-compatible.
NavDirectedEdge retains existing fields and gains optional owned external
link metadata. Its cardinal direction is meaningful only for static edges;
external edges use direction=0 as an ignored placeholder and the metadata's
typed direction. Route steps own the same metadata after graph destruction.

## Validation and determinism

Reject null snapshot, count/byte caps and overflow before allocating target
storage. Then check fingerprint, all individual links in input order, then
relational errors in identity-key order (sourceId, generation, linkId).
Individual field order: sourceId, generation, linkId, from, to, self-reference,
traversal, direction, entry, exit, direction/Z relation, additionalCost.
Zero area IDs are InvalidValue; missing IDs are DanglingReference; self-links
are InvalidValue. Unknown enum values are UnsupportedValue. Invalid points
or Z relation are InvalidGeometry; invalid cost is InvalidValue.
For each source, require a single generation. Mixed generations take priority
over duplicate keys within that source. Identical repeated keys are DuplicateId;
any differing record under a repeated key is InvalidValue/LinkConflict.
Conflict takes priority over exact duplicates in the same key group.
Distinct keys with identical endpoints or payload are retained.
Floating equality is numeric, so positive/negative zero compare equal.

Vertices sort by area ID. Outgoing static edges retain their direction/target/
traversal sort, followed by external edges in identity-key order. Equal-cost
parent selection remains strict-g improvement, hence deterministic.
Standard cost is center distance in distance + additionalCost in traversal.
Custom policy still decides the whole cost, with metadata accessible from edge;
no automatic additional-cost injection. Standard heuristic remains center
distance; custom-cost default heuristic remains zero.

## Memory and diagnostics

Preflight final graph storage with existing sizeof-based logical accounting,
including enlarged owned edge metadata. Retained snapshot memory is excluded
as before. Validation workspace is a size_t area-index array and a size_t
link-index array, count*size checked before allocating. maxWorkingBytes bounds
these arrays, not allocator overhead, fixed locals or sort stack. Temporary
storage is linear; validation uses binary search and sorted identity groups.
Working arrays are released before final graph allocations.

CountLimitExceeded / OffsetOverflow / AllocationFailure follow existing kinds.
Add dedicated Link* fields for count, working bytes, fingerprint, identity,
references, traversal, direction, entry/exit, cost, generation conflict and
payload conflict. Link errors use TraversalLink record and offset zero (not
a fabricated file offset). Graph caps retain existing graph diagnostics.
Allocation exceptions anywhere in composition use Graph/GraphBytes, matching
the existing build-wide allocation diagnostic; no guessed per-link offset.
All candidates are discarded on failure; prior published objects remain valid.
Exceptions from allocation/length limits never escape noexcept composition.

## Acceptance

Independent synthetic directed up/down ladder routes, per-direction costs,
identity selection, parallel links, static coexistence/regression, input-order
invariance, custom policies and partial routes. Reject all malformed fields,
wrong fingerprint, mixed generations, duplicate/conflicting keys. Exercise
exact and one-under budgets, checked arithmetic overflow, OOM at every observed
allocation site, lifetime, repeatability and parallel read-only queries.
Canonical portable x64 Debug/NMake and separate MSVC /analyze must pass.
Keep _ITERATOR_DEBUG_LEVEL=0. Preserve local indexes and other worktrees.

## Verification and self-review (2026-09-05)

- Baseline on e37b9e4: portable x64 Debug/NMake, CTest12/12 (0.64s).
- T1 RED: new test could not include the not-yet-implemented traversal_link.hpp.
  T1 GREEN: composition/validation plus regression CTest13/13 (0.83s).
- T2 RED: ladder expected distance10 + traversal2 = total12 assertion failed
  against the old distance-only policy. GREEN: CTest13/13 (0.84s).
- Additional safety/coverage tests were initially green, requiring no further
  production changes. Final Debug configure/build/CTest13/13 (0.42s).
- Separate MSVC /analyze configure/build/CTest13/13 (0.76s); /W4 /WX,
  /EHsc, Debug asserts and Nav PUBLIC _ITERATOR_DEBUG_LEVEL=0 confirmed.
- OOM sweep: six composition allocation sites, four complete route sites and
  four opt-in partial route sites fail transactionally before eventual success.
- Two synchronized threads each complete 100 result-equivalent searches.
- FocalSpan updated and contract queried; no waived workflow requirements.
- Inline self-review covered preflight arithmetic and array lifetimes, exact
  diagnostics and enum checks, sort strict ordering, full payload comparisons,
  static API behavior, route ownership, custom cost authority and failure cleanup.
  No remaining in-scope finding. No independent/subagent review was performed,
  as explicitly requested.
- Main/remote operations and project Finish remain outside this completion.
