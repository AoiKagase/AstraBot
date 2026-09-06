# State

Status: in_progress
Goal: entire Phase 3; never mark complete for a slice or merge.
Main: 7d6a034, fast-forward merged in .worktrees/main-integration; clean, not pushed.
Current branch: codex/p306-ladder-discovery, based on main.
Current boundary: host-connected fingerprint-bound ladder discovery/publication.
Report: docs/reports/p3-06-ladder-publication.md.
P3-05 implementation slices have offline evidence; live acceptance is pending.
Host uses standard-CS public physics, bounded query guards, exact Press dispatch
feedback and measured landing/cooldown. Private jump overrides unsupported.
Verification: Windows x86 Debug42/42 PASS. WSL -m32 Debug36/36 + final fingerprint
target PASS. Release6 exports PASS.
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
NavConsole load now reads current engine map's BSP and streams SHA-256 (512MiB
hard cap, fixed working memory); compares optional NAV BSP size before enrichment.
publishLadders retires old routes/links, uses bounded discovery and Graph::compose,
handles deferred invalidation, publishes all or native-only fallback with diagnostics.
LadderDiscovery retains immutable links and matching passage pairs. Source ID is
ladderSourceId; generation is monotonic; linkId=slot*16+face*4+exit*2+1 (up), +1 down.
Caps8192 slots,128 candidates,12288 traces,1024 passages,2048 links; no cap retries.
First P3-06 checkbox now has offline evidence. No real/live map acceptance claim.
Next: P3-06 first-class up/down motion and guarded host dispatch. Movement must
use selected link identity and immutable passage data; revalidate temporal proof.
Add contact/velocity observation, approach/align/contact/climb/exit/support,
fall/timeout detection and at most one fresh clearance-checked re-acquisition.
Currently Walk explicitly rejects selected Ladder traversal; no fallback motion.
Read phase-3-nav-movement.md P3-06 and existing P2-07 traversal link contracts.
After P3-06: P3-07 finite recovery, P3-08 cross-primitive offline matrix.
No subagents. All binaries x86. No live server or project-wide Finish.
Real NAV compatibility partial; real NAV/BSP read-only. No push authorized.
Preserve untracked .focalspan.json and .serena/; do not stage them.
