# P2-05 implementation plan

Spec: ../specs/2026-09-05-p205-spatial-queries-design.md.

1. Verify the inherited 8-test portable Debug baseline.
2. Add geometry tests with literal expected projections, boundary membership,
   saddle height and extreme coordinates. Observe missing API failure. Implement
   double-precision XY projection and bilinear surface interpolation.
3. Add indexed query tests using loader-produced independent snapshots. Observe
   missing index failure. Implement bounded BVH construction and allocation-free
   containing/nearest traversal, deterministic exact distance/ID ranking.
4. Check ordered/reversed fixtures and seeded points against a separate linear
   oracle; add build-failure sweeps, invalid inputs, caps and concurrent queries.
5. Portable Debug and /analyze 10-test suites, FocalSpan update/query, diff review,
   explicit staging/cached diff check and implementation commit. Keep P2-05
   unmerged; no remote, Linux/live or P2-06 work.

Public API: NavQueryPoint (three doubles), NavAreaMatch (area ID, projectedPoint,
distanceSquared), NavQueryLimits (maxRadius/maxVerticalDistance),
NavSpatialIndexLimits (maxAreas/maxNodes/maxIndexBytes). All limits are explicit.
NavSpatialIndex::build returns ReadResult<shared_ptr<const NavSpatialIndex>>;
containing(NavVector3,double) and nearestGeometry(NavVector3,NavQueryLimits)
return ReadResult<optional<NavAreaMatch>>. Const methods allocate no memory.

## Completed verification

- Geometry and spatial tests each first failed on their missing API header,
  then passed with the implementation.
- Literal geometry covers closed bounds, sloped XY projection, saddle height
  and extreme finite float coordinates using double arithmetic.
- 64 independent areas, 1,000 seeded points and reversed area order compare
  containing and nearest against a separate linear/four-weight oracle.
- Exact ID ties, stacked floors, radius/vertical boundaries, empty index,
  invalid/nonfinite inputs, zero limits and pre-allocation cap failures pass.
- Successive construction allocation failpoints return typed failure with no
  value; queries succeed while allocations are disabled. Four concurrent
  workers each perform 100 const queries.
- Portable x64 Debug NMake /W4 /WX CTest: 10/10 passed.
- MSVC /analyze x64 Debug NMake /W4 /WX CTest: 10/10 passed, no warnings.
  The analysis configuration preserves /DWIN32 /D_WINDOWS /GR /EHsc and adds
  /analyze in a separate build directory.
- FocalSpan update/query locates NavSpatialIndex, nearestGeometry, containing
  and projectToArea. No upstream implementation or assets were introduced.
- P2-04 is merged into main at f510dca; P2-05 remains on its worktree branch.
  No Linux/live, remote operations or P2-06 implementation were performed.
