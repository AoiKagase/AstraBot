# State

Status: in_progress
Goal: entire Phase 3; never mark complete for a slice or merge.
Main: 7d6a034, fast-forward merged in .worktrees/main-integration; clean, not pushed.
Current branch: codex/p306-ladder-discovery, based on main.
Current boundary: measured ladder passage inspection, not yet host-connected.
Report: docs/reports/p3-06-ladder-passage.md.
P3-05 implementation slices have offline evidence; live acceptance is pending.
Host uses standard-CS public physics, bounded query guards, exact Press dispatch
feedback and measured landing/cooldown. Private jump overrides unsupported.
Verification: current Windows x86 Debug41/41; preceding Jump integration passed
WSL -m32 Debug35/35 and x86 Release6 exports. No portable/Release change this slice.
scanLadderCandidates returns fixed-array map/entity/AABB values; 8192 slots,
128 candidates, typed failure discards all candidates. No private data or writes.
Candidates are not contact/support proof; revalidate identity before consuming.
inspectLadderPassage now validates one cardinal face/exit variant in12 queries:
3 point-model faces,2 floor+clearance pairs,2 hull/model contacts,3 world sweeps.
Four faces and same-face/across-top fixtures, all lower budgets, map/entity
mutation at every query, missing contacts/support/area and bad trace/model pass.
Standard hull, static unrotated ladder, world BSP support; no dynamic support.
Each query rechecks map via callback, entity serial/index/bounds/model/skin.
Pinned ReHLDS TraceModel hull0/1 semantics inspected; no upstream code copied.
Next: actual BSP fingerprint binding, bounded whole-map inspection and immutable
same-map up/down link publication. Retain passage value evidence for motion.
Then P3-06 first-class up/down motion. Both P3-06 checkboxes remain open.
Read phase-3-nav-movement.md P3-06 and existing P2-07 traversal link contracts.
After P3-06: P3-07 finite recovery, P3-08 cross-primitive offline matrix.
No subagents. All binaries x86. No live server or project-wide Finish.
Real NAV compatibility partial; real NAV/BSP read-only. No push authorized.
Preserve untracked .focalspan.json and .serena/; do not stage them.
