# NAV/BSP audit work log

Owner: current Codex session; only `docs/nav-bsp-feasibility/` and a unique temporary build directory.

Approved scope: Japanese feasibility report, input identities, location measurements, reproducible diagnostic harness. No NAV generation, product changes, deployment, or live-server operations.

- Task 1: complete — 16 NAV v5/BSP v30 pairs; SHA-256 identities saved and rechecked.
- Task 2: complete — 12,320 areas, 46,418 directional records; production semantic digests matched all areas; hull, ceiling, width, graph, entity and coverage evidence saved.
- Task 3: complete — 1,698 density samples; 1,350 additional hull-support samples; 1,325 production point queries, all original areas matched at tolerance 64; geometry versus body support separated.
- Task 4: complete — Japanese report, independent review, corrected full-cohort remeasurement and fresh artifact/identity/production verification complete.

Rulings:

- Work in the requested checkout, owning a new documentation directory only. Its dirty NAV sources are the actual audit subject; a clean worktree would measure a different implementation. Do not stage or commit unrelated work or reuse shared build output.
- Persist a diagnostic harness with the report for reproducibility; it has no NAV writer or regeneration functionality.
- The plan is supplied in the conversation, with no separate reachable spec file. The approved user message is the authority.
- Offline measurements classify candidates; no engine physics, runtime solidity, or navigation success is inferred from them.

Pre-flight: manifest identities feed all measurements; Python and production C++ summaries must agree before the report can use the measurements.

Verification and review:

- Isolated MSVC build uses the required Hostx86/x86 compiler (first `where cl` result), `/W4 /WX`; no shared CMake cache or DLL is changed.
- Python child-process Git cannot enter the external reference repository; a separate `rtk git` command verified the reference HEAD, and the supplied identity is labeled. Input and source hashes remain independent evidence.
- The first eight geometric/parser oracles passed. Final reviewer identified 66 unresolved-only candidate rows storing a resolved witness. Added `test_unresolved_only_candidate_saves_reproducible_witness`, observed RED (missing helper), then GREEN 9/9 after selecting an actual unresolved point. Full measurement rerun preserves all aggregate quantities.
- Final review: one P2, corrected; no additional important findings or deferred minors. Reviewer read only, no product changes.
- Final check: 16 input identities rechecked; 12,320 production area semantic digests matched; 1,325 physical point queries retained their original Area at tolerance64; 1,350 support rows consistent; 9/9 diagnostic tests passed. The corrected as_oilrig Area1544 witness is (597.5,-520), start_solid, classified 実機確認待ち. Every resolved CSV candidate exceeds the documented 2-unit threshold.
- Whitespace check of owned added files passed using `git diff --no-index --check`; cached diff check also passed. No files staged or committed, no unrelated edits changed. FocalSpan update and the post-edit `surfaceZAtInternal` query completed successfully; graphs/FocalSpan do not constitute engine validation.

Follow-up: additional stored information and computation cost

- User requested a report-only extension on precomputed NAV-side information. Ownership remains this documentation directory; no runtime cache, new format or product implementation is added.
- Rechecked current source: positional queries scan areas; link how/reciprocity and center distances are derived at query time. ID lookup and A* record lookup already have hash indexes and are not counted as missing optimizations. Existing artifact verification still passes despite a changed repository HEAD (source hashes match).
- Added report section 5: five candidate data groups, steady-state cache reuse versus persistence/load-time benefit, per-map payload estimates, optional sidecar and invalidation constraints, directed/dynamic routing and visibility limits, and three-condition performance validation. No speedup is claimed without measurement.
- Follow-up verification: current input/reference/product hashes and saved production comparisons pass; diagnostic tests 9/9 and report links pass. Payload calculations independently checked for de_dust2 and cs_siege. FocalSpan index refreshed; code graph HEAD is stale, so current-source reads are the implementation evidence for this addition. Documentation and artifact hashes refreshed only; no measurement or product implementation changed.

Commit/merge request:

- User explicitly requested commit and merge. The checkout is already on master at 7062a29, so the owned documentation is isolated on `codex/nav-bsp-feasibility` and will be fast-forwarded back to master. No push, unrelated staging, worktree deletion or branch cleanup is required.
- All 11 recorded source hashes match committed 91beea9. The later responsibility refactor changes only plugin_runtime.cpp among these 11 files. Restore the recorded source snapshot into a unique temporary directory for the complete saved-artifact verification; do not relabel historical evidence as validation of the later refactor.
- Commit verification passed: restored the 11 files from 91beea9 plus fingerprinted reference files into `C:/Temp/astrabot-nav-commit-fhwmtgqf/snapshot`, then ran the existing verify_artifacts.py. All 16 input pairs, 12,320 production area digests, 1,325 point queries and 1,350 support rows passed the saved-artifact checks; diagnostic tests 9/9. Only documentation/provenance text changed for integration.

- Integration hygiene: normalized owned text artifacts to LF before hashing so committed Git blobs and the artifact manifest have identical byte identities. No numeric measurements changed.
