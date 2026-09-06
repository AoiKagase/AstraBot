# State

Status: P3-06 implementation and offline verification complete.
Goal: P3-06 (user /goal). Completion requires a successful commit of this state.
Branch: codex/p306-ladder-host-binding. No push or merge authorized.
Reports: docs/reports/p3-06-ladder-host.md and p3-06-ladder-controller.md.

Immutable fingerprint/generation-bound discovery and publication, measured
passage binding, owned Walk ladder controller and NavConsole dispatch are
connected. Approach/align/contact/up/down/exit/support, finite abort/fall/retry,
one-shot lower airborne Jump and actual detached support-before-cursor-advance
are implemented. Jump replaces velocity with270 outward; it does not add a
regular vertical jump impulse. Grounded jump remains unsupported; floor kick
has separate proof. Standard-CS reference679973265e1ac99a43193119e0da212ee568f5f9.

Published endpoints satisfy standing-hull Corridor inset. Ground paths have
measured floor samples, allowing a bounded upper-lip model capture only for
the exact mount. Outside-NAV floor is never fabricated as NAV support.
Guard and observation budgets are shared (21/frame); guards validate actual
rounded command duration and exact current-frame motion. Deferred invalidation,
replaced ladder and cancellation prevent stale sends.

Windows x86 Debug43/43 and Linux x86 Debug37/37 passed. Release x86 and exact
six exports passed. Host independent synthetic fixtures reach the goal up/down
at8/16/100 ms, send exactly one lower Jump and measure actual query count<=21.
Queued Jump is blocked on replacement before/during tracing and cancellation.
Ground proof tests cover per-query staleness, missing floor and short budgets.

P3-06 offline implementation is complete. This is not project-wide Finish or
real-map/live acceptance. P3-07/P3-08 are outside this goal; no live HLDS/ReHLDS
checks before explicit Finish. No subagents used. All binaries are x86.

Preserve unrelated AGENTS.md/.gitignore edits and untracked tool configs;
exclude .focalspan/ and .focalspan.json from commits. FocalSpan is mandatory.
Graph is stale at0eee09c; focused diffs/source and direct tests provided review
evidence. No memory edits authorized.
