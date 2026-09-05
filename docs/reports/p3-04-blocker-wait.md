# P3-04 portable blocker wait foundation

P3-03 through `1df97cd` was fast-forward integrated into local main from
`d32cc21`. Integrated main passed Windows x86 Debug adapter/portable 32/32
and WSL Debian Linux x86 portable 27/27. No push or live validation was run.

This first P3-04 boundary adds a portable `BlockerWait` controller. It owns one
route-step-bound observation, with explicit fact lifetime, yield interval and
total attempt timeout. No allocation, SDK dependency, static mesh mutation,
movement intent, trajectory prediction or graph search is introduced.

Player observations carry a separately adapter-validated generation-safe
PlayerId. The lower PlayerId may inspect an avoidance segment; the other yields
until the finite yield interval ends. Neither recommendation permits movement.
Geometry and other classified obstacles request inspection immediately. A
future Walk integration must fully check the chosen segment and keep its shared
query budget. Unknown/malformed or unavailable observations request replan.

Fresh stamped clearance invalidates the fact and requests passage reinspection,
including when a door reopens. Absence of a blocker does not prove clearance.
Fact lifetime and attempt timeout use subtraction from the supplied monotonic
simulation clock. Refresh/replacement/expiry never restarts the attempt. At the
exact deadline timeout wins over clearance. Actor/map/route/portal replacement
aborts; repeated ticks are neutral and cannot refresh observations. Terminal
notifications are emitted once. Owners must call update/abort and use monotonic
time for fact reads; this value object never schedules its own timeout.

Synthetic tests cover complementary actor priority, finite yielding, replacement
without deadline extension, exact expiry, clear passage invalidation, unavailable
capability, stale stamps/ticks, slot generation/map/route/step invalidation,
invalid profiles, cancellation and deterministic replay near UINT64_MAX.

Verification: Windows x86 NMake Debug adapter/portable 33/33 and WSL Debian
GCC -m32 Debug portable 28/28 pass with warnings-as-errors. Release x86 adapter
build passes with exactly the six contracted exports. Hosted CI remains pending.

This is a controller foundation, not completion of P3-04's first checklist.
Walk/host integration, actual player observation, clearance-based sidestepping,
replan consumption and scenario tests remain. The separate per-player adapter
mapping/join/dispatch commit also remains. Live multi-Bot proof is post-Finish;
real NAV compatibility remains partial. No project-wide Finish is declared.
