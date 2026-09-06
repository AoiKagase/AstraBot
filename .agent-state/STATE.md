# State

Status: complete — P3-08 implementation and applicable offline verification.
Goal: approved P3-08 reproducible movement evidence with separate gate states.
Branch: codex/p308-offline-gate; dedicated .worktrees/p308-offline-gate.
Plan: docs/plans/phase-3-nav-movement.md, existing P3-08.
Report: docs/reports/p3-08-offline-gate.md.

Portable 90 + fake-host 186 declared rows, each run twice with ordered traces.
Frame intervals 8/16/100 ms; actor matrices 1/8/16, specialized host rows 1.
Measured starts/goals, typed terminals, commands/receipts, query/replan/time
budgets, history/trace limits and source/build/fixture hashes. Python 3.9+
standard-library checker rejects malformed, missing, stale and false success.
No production navigation, public API or DLL export changes.

Verification 2026-09-06:
- Windows x86 portable Debug/WX, inspector ON: 41/41.
- Windows x86 Metamod Debug/WX: 47/47.
- WSL Debian Linux x86 GCC -m32 Debug: 40/40.
- Final input/log/review refinements: affected tests Windows 2/2 and 5/5;
  Linux 2/2. Independent review findings resolved; final diff checked.
- MSVC 19.51.36256.0; GCC 14.2.0.
- SDK SHA 7ec9b014f8c0a947a724644aebe34eb33706e44b.

Observed hosted baseline: run 34023536620 on db146e4, all three jobs success.
This P3-08 branch's hosted run awaits separately authorized push/PR.
Real NAV remains Partially validated (provenance/detail/other-version gaps).
Live remains Not yet validated; tests/live/nav/manifest.json lists pending rows.
No project-wide Finish or live HLDS/ReHLDS execution. Assess all remaining
project plans before a separate explicit Finish decision.

Preserve branch/worktree and unrelated root AGENTS.md/.gitignore/tool edits.
No push, merge, cleanup or memory update performed or authorized.
FocalSpan and graph consulted; graph misses new unindexed test functions,
so scoped source review and executable verification supplied the evidence.
Implemented through bounded subagents with parent progress reporting.
