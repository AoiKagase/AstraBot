# State

Status: complete
Task: P3-01 console goto/status/cancel slice (no movement)
Branch: codex/p301-nav-console, base main 03ee7d9 (integration complete).
Plan: docs/plans/phase-3-nav-movement.md, existing P3-01 console checklist.
Implemented: explicit bounded NAV load, managed actor selection,
ground trace plus containing query, RouteSession lifecycle, bounded console trace.
Portable20/20 and adapter24/24 tests pass on x86; Release six exports verified.
Core/Nav STL debug ABI aligned; SDK macro collisions isolated.
User approved six exports including GiveFnptrsToDll. Bootstrap table used for
Metamod-tracked registration; incorrect hook-table registration is regression-tested.
Report: docs/reports/p3-01-nav-console.md.
Next: P3-02 corridor/portals. Real NAV provenance/detail acceptance remains partial.
Main baseline x86 verification passed. FocalSpan queried in this worktree.
No subagents, Linux, live server, real input writes or Finish.
