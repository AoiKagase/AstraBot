# State

Status: complete — P3-08 planning only; implementation not started.
Goal: plan the existing P3-08 offline evidence and separate acceptance gates.
Branch: codex/p307-progress-recovery. No push or merge authorized.
Report: docs/reports/p3-07-progress-recovery.md.
Plan: docs/plans/phase-3-nav-movement.md, P3-08 execution plan (2026-09-06).

Planning base: db146e4. Four ordered, unnumbered implementation slices:
evidence contract/first replay; full scenario and map-change matrix;
1/8/16-actor scheduling and finite budgets; CI/report/live prerequisites.
No source/tests/CI implementation or live execution in this planning change.
Planning verification: existing report/test/CI contracts and Markdown diff review.
P3-07 evidence below is historical, not a fresh P3-08 verification run.

Actor/goal-owned Recovery survives route replacement. Only actual dispatch
receipts accumulate Walk 500ms / crouch 1000ms windows. Corridor-forward and
displacement high-water observations avoid both oscillation evasion and false
stuck during legitimate lateral avoidance. Wait/side/reverse stages are 250ms;
movement cannot dispatch past its stage deadline. One replan, then finite abort.
No unsupported obstacle inference or fabricated edge exclusions.
Ground recovery shares the 21-query limit and existing transport guards.
Cancellation, explicit goal reset, typed causes and terminal uniqueness tested.

Verification on 2026-09-06:
- Windows x86 Metamod Debug /W4 /WX: 44/44.
- Windows x86 portable Debug /W4 /WX: 36/36 (inspection tools OFF).
- Linux x86 GCC -m32 Debug: 38/38 (Debian WSL).
- Windows x86 Release: built, PE32 x86 and exact six exports verified.
- SDK SHA: 7ec9b014f8c0a947a724644aebe34eb33706e44b.
Synthetic permanent stalls abort in 1.984/2.016/2.400 seconds at 8/16/100ms
frames; transient stalls arrive in 3.424/3.424/3.800 seconds.

P3-08 remains next. This is not project-wide Finish or real-map/live acceptance.
No live HLDS/ReHLDS checks before explicit project-wide Finish.
No subagents used. FocalSpan mandatory; graph was consulted but trails HEAD.

Preserve unrelated AGENTS.md/.gitignore edits and untracked tool configs.
Exclude .focalspan/ and .focalspan.json from commits. No memory edits authorized.
