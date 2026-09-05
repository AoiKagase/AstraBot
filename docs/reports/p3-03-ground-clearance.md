# P3-03 grounded/clearance query slice

Date: 2026-09-06. Base: main `5a34298` after fast-forward integration of
P3-02 (`8dcffd9`, `5a34298`). Work branch: `codex/p303-ground-clearance`.
Only the first P3-03 checklist slice is complete; no movement is issued.

## Implemented contract

`nav/local/ground_probe.*` accepts a managed actor snapshot, route generation,
current area, target XY, same-map immutable index, borrowed query port and
explicit limits. It rejects invalid/stale navigation and computes the required
sample/query counts before contacting the host. One grounded-area query plus
one floor and one swept-hull query per sample is the complete budget. There is
no automatic retry. Replies must match agent/player/map/tick/route/ordinal/kind.
The result retains batch identity, query/sample counts and a grounded target
only on success. Exceptions, missing/invalid support, wrong starting area,
missing containing NAV, unsafe sampled drop and hull blockage fail explicitly.

Target Z comes from the observed floor and actor hull. Containing queries at
that height distinguish stacked floors; geometric nearest is never a substitute
for support. Direction/normal/height inputs are finite-checked and float-range
checks precede narrowed trace coordinates. Ground samples are discrete, spaced
by an explicit limit. They do not prove uninterrupted support between samples;
the later Walk controller must retain per-update probes and stop on uncertainty.
Ordinary stairs/doors and actual Walk scheduling remain later P3-03 work.

`adapter/cstrike/nav/world_queries.*` converts bounded synchronous requests to
one TraceLine or TraceHull each. It exposes GroundedArea, Floor, SweptHull and
Clearance values without exposing SDK objects. TraceHull includes actors and
uses GoldSrc standing hull 1 or duck hull 3; other hull shapes are unavailable.
All-solid/start-solid never count as clear. Door/Blocker kinds remain unavailable.
The caller must validate the actor's live entity and lifetime. Existing console
ground queries use the common implementation and retain deferred-invalidation
handling. No new console commands or DLL exports were added.

## Verification

- Ground-probe tests failed to link before implementation, then passed.
- Scripted portable cases: deterministic requests/replay, upper/lower floors,
  stale reply and stale NAV, exceptions/unavailable data, budget rejection before
  any calls, unsupported/steep/nonfinite floors, unsafe drop, no containing area,
  blocked/solid hull, malformed endpoints and overflowing height limits.
- Adapter cases: standing/duck selection, unknown hull, actor-inclusive sweeps,
  all-solid/blocked/invalid fractions, vertical floor validation, unavailable
  query kinds/functions, and the portable probe through the fake engine port.
  Existing console/map-invalidation regressions also pass.
- Final Windows x86 NMake Debug /W4 /WX adapter + portable CTest: **27/27**.
  Portable-only CTest also passed **23/23**, plus fixture/manifest verification.
- Final WSL Debian 13 GCC 14.2 `-m32`, Debug, warnings-as-errors: **22/22**.
- Pinned Metamod-P SHA verified: `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
- Final x86 Release DLL builds and exports exactly `Meta_Query`, `Meta_Attach`,
  `Meta_Detach`, `GetEntityAPI2`, `GetEngineFunctions`, `GiveFnptrsToDll`.

Main integration itself passed Windows portable regression before this slice.
FocalSpan and staged-diff checks follow the repository workflow. Hosted CI for
this new branch, live traces, actual movement and Finish remain unverified.
No real NAV/BSP was modified; P3-01 real compatibility remains partial.

Next: existing P3-03 Walk/motor, 25 Hz decisions/per-frame command transport,
freshness/stop and observability slice; then ordinary doors/stairs/wall/narrow
passage handling. Completing this report does not complete P3-03 as a whole.
