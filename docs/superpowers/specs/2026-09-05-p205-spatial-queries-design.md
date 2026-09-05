# P2-05 spatial queries — design for review

Status: proposed, not approved for implementation.
Base: main f510dca (P2-04), merged portable Debug CTest 8/8 passed.

## Goal and alternatives

Build an immutable, SDK-free spatial index over a validated NavMeshSnapshot.
Queries locate a containing area or nearest geometry candidate without engine
traces, mutable snapshot caches, routes or runtime grounding.

Recommended: a deterministic binary BVH. Area size and distribution need no cell
size configuration and each area appears in one leaf. A uniform grid needs cell
size and multi-cell duplication caps. A linear scan is simple but provides no
spatial acceleration; retain it only as the independent test oracle.

## Distance decision requiring review

Recommended geometry contract: clamp the query X/Y into the area's rectangle,
evaluate the surface Z by bilinear interpolation at that X/Y, then rank candidates
by squared 3D distance to that projected point. This is not the mathematically
closest point on a sloped or saddle-shaped bilinear surface.

Use an explicitly named projectedPoint field and document the convention on
nearestGeometry. The alternative is a true global minimum over each bilinear
surface, requiring a separate numerical solver and convergence/error contract.
Do not silently substitute one definition for the other.

## Surface and query semantics

- Corners are NW=(minX,minY,NW.z), NE=(maxX,minY,NE.z),
  SW=(minX,maxY,SW.z), SE=(maxX,maxY,SE.z). Convert float inputs to double before
  subtraction; use convex linear interpolation twice to compute height.
- containing requires X/Y inside the closed rectangle. At the projected height,
  apply a caller-specified finite nonnegative maximum absolute vertical distance.
  For overlapping floors, choose the smallest absolute vertical distance, then
  the lowest area ID on exact equality.
- nearestGeometry clamps X/Y, applies the same vertical filter, and requires
  the projected-point 3D distance to be within an explicit finite nonnegative
  maximum radius. Boundaries are inclusive. Rank by squared distance then ID.
- Use double point coordinates and distance arithmetic internally and in results;
  retain NavVector3 input. Reject negative/nonfinite query limits and nonfinite
  input points before traversal. No epsilon equality grouping.
- Use hypot for the radius acceptance check to avoid squaring a huge caller
  radius. Distances between float-origin coordinates remain representable in
  double. A radius of zero accepts only exact zero distance.
- No match is a successful empty optional; invalid input is a typed failure.
  Do not return the globally nearest area outside the supplied limits.

## Ownership and interface proposal

Namespace nav::query, primarily spatial_index.hpp/.cpp and geometry.hpp/.cpp.

NavSpatialIndex owns shared_ptr<const NavMeshSnapshot> and private BVH storage.
build(snapshot, NavSpatialIndexLimits) returns a typed result containing
shared_ptr<const NavSpatialIndex>; a null snapshot is InvalidInput.

For an empty-index test/use case, a public default constructor creates an empty
immutable index with no snapshot. Queries on it return successful no-match.
This does not relax NavMeshLoader's rejection of files with zero areas.

NavSpatialIndexLimits contains maxAreas, maxNodes and maxIndexBytes, all explicit;
zero is not unlimited. NavQueryLimits contains maxRadius and maxVerticalDistance.
containing takes the vertical limit directly; nearestGeometry takes both limits.
Both const noexcept methods return ReadResult<optional<NavAreaMatch>>.
NavAreaMatch contains NavAreaId, a double-precision projected point, and squared
3D distance. No raw area pointer outlives the index owner.

Invalid query arguments use InvalidInput / RawInput / RawBytes at offset zero:
these are query diagnostics, not source-file offsets. Build count/byte rejection
uses CountLimitExceeded, checked arithmetic overflow OffsetOverflow, and caught
allocation failure AllocationFailure. Failed construction publishes no index.

## Deterministic BVH and allocation contract

- One area per leaf; nonempty trees contain exactly 2*N-1 nodes. Charge the index
  object, node storage and N area-index entries with checked size arithmetic
  before allocation. Snapshot memory is shared and is not charged again.
- Node bounds contain the XY rectangle and min/max of all four corner heights.
  Bilinear interpolation stays within those height bounds.
- For each range choose the longest X/Y/Z AABB axis, breaking ties X then Y then Z.
  Sort by doubled box-center coordinate on that axis, then unique area ID;
  split at count/2. Record child indices in deterministic preorder.
- Use in-place std::sort and a balanced recursive builder: O(N) allocated
  storage, O(log N) recursion, O(N log-squared N) construction worst case.
- Query traversal is allocation-free and uses balanced recursion. Prune by
  conservative AABB lower bounds; prune only on strict greater-than so equal
  distances can still improve the ID tie-break. Visit equal-bound children
  by stored child order. Filters and final ranking are always evaluated on
  individual projected points.
- Query state is local; concurrent const queries never modify snapshot or index.
  Keep _ITERATOR_DEBUG_LEVEL consistent with the Nav target.

## Verification and implementation sequence

1. TDD for bilinear height and XY projection using independent literal expected
   points on flat, sloped and noncoplanar patches; include a slope case proving
   the projected-point contract differs from the true nearest surface point.
2. TDD for full linear semantics: containment boundaries, stacked floors,
   radius/vertical limits at equality and just outside, exact ties by ID,
   empty index, invalid inputs and extreme finite coordinates.
3. Implement the bounded immutable BVH and test actual indexed queries against
   an independently authored linear oracle over seeded generated fixtures.
   Reorder identical area sets and require identical IDs and projected results.
4. Inject allocation failures through index construction and verify no partial
   publication or damage to an existing index. Reject insufficient nodes/bytes
   before large allocations. Exercise many concurrent queries for state isolation.
5. Portable x64 Debug NMake CTest and separate MSVC /analyze; update/query
   FocalSpan, review exact diff, stage explicit files, cached diff check, commit.

Expected CTest additions: geometry and spatial index executables (8 -> 10).
Fixture provenance remains independent of upstream code/assets. P2-06 A*,
nearestGrounded, Linux/live validation and automatic merge are excluded.
