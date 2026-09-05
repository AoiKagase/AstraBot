# P2-08 Implementation Plan

Execution: inline executing-plans, no subagents.
Spec: ../specs/2026-09-05-p208-nav-evidence-design.md
Goal: reproducible bounded offline validation and evidence without API changes.
Architecture: test-only independent encoder and oracles consume public Nav APIs.
Tech stack: C++17, CMake/NMake, MSVC Debug/analyze/ASan, PowerShell SHA-256.

- [x] Verify main/worktree, Debug13 baseline and FocalSpan context.
- [x] T1: Write corruption tests against missing fixture helper; observe RED.
  Implement independent wire encoder/spans, full value and exact error checks;
  generate manifest with Windows SHA-256; verify GREEN.
- [x] T2: Test mutation/replay and byte decoding against missing fuzz helper;
  implement bounded xorshift32 mutations, invariants, failure artifacts; GREEN.
- [x] T3: Add independent linear spatial and Dijkstra differential tests.
  Add benchmark with warmup/median/p95, no timing assertions.
- [x] T4: Add separate MSVC ASan and Windows Debug CI; exercise configurations.
- [x] T5: Debug16, analyze16, ASan dedicated tests+100000 fuzz, benchmark.
  Map offline gate evidence, limitations, commands and fixture hashes.
- [x] Self-review; FocalSpan update/query. Empty-manifest RED -> fail-closed
  schema/full-set validation GREEN; four persisted script regression cases.
- [x] Explicit stage/check; delivery is recorded by the containing commit.
  Verify its hash/status before reporting completion.

All global constraints and numerical caps are fixed in the approved spec.
No main merge, remotes, Linux/live or project-wide Finish.
