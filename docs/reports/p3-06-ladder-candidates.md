# P3-06 ladder candidate discovery foundation

`scanLadderCandidates` independently enumerates engine edict slots and identifies
public `func_ladder` entities. Results own only values: map generation, serial/index
identity and world-space AABB. No SDK pointer is retained, no private data is read,
and no upstream implementation is copied.

The synchronous scan has fixed caps of 8192 slots and 128 candidates, with optional
tighter caller limits. It uses a fixed array and performs no dynamic allocation.
Each slot is visited once and each candidate has one additional identity lookup.
Absent ladders are successful empty discovery. Missing engine functions, invalid
identity/classname, exceeded caps and unsupported bounds/rotation/motion return
typed errors and discard the entire candidate batch, including earlier candidates.

Tests cover empty/multiple/free/non-ladder entities, serial identity changes,
value ownership, stale discovery callbacks, invalid and nonfinite geometry,
moving/rotated ladders, missing functions, invalid limits and partial-batch discard.
Windows x86 NMake Debug, warnings as errors: all 41 tests passed, including
the new ladder scanner target. The portable source and Release DLL are unchanged
in this slice; the preceding integration passed Linux -m32 35/35 and six exports.

This is a candidate discovery seam exercised by an SDK-backed test executable;
it is not yet invoked by the production host. AABB bounds prove neither the
climbable face nor supported endpoints. The caller must revalidate map/entity
identity before tracing and publication; the batch is not an atomic live-world
snapshot. No link is manufactured, published or serialized by this scan.

P3-06's first checklist item remains open: next add bounded face/contact and
endpoint support traces, resolve NAV areas, bind the actual BSP fingerprint,
and publish immutable up/down links in the same map generation. Then implement
first-class ladder motion. Live ladder acceptance remains post-Finish.
