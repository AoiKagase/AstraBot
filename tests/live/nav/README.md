# P3-08 post-Finish NAV acceptance

**Not yet validated. Do not start a server until project-wide Finish is
explicitly confirmed.** `manifest.json` defines the required cross-product
of environment, scenario, FPS band and Bot count. Empty results are pending,
not passes. Offline CTest and local WSL runs never populate these live results.

After Finish, record the decision reference before enabling any row. Pin OS,
server/GameDLL/Metamod-P versions and hashes, adapter commit/artifact hash,
plugins, physics/cvars, map/BSP and NAV SHA-256, lawful asset provenance,
start/goal positions/areas, actor identities, intended FPS band and measured
frame-time distribution. The SDK pin remains the one required in AGENTS.md.
Choose FPS targets for the pinned server and record them before execution;
the synthetic 8/16/100 ms schedules do not establish achievable live FPS.

For each row, retain ordered route edges, portal/target, primitive transitions,
issued and actually dispatched commands, reasons, query/replan/time counts,
observed supported arrival or the expected finite failure. Check upward and
downward stairs/ladders, jump takeoff/landing, headroom-safe crouch release,
door use/touch timeout, blocker expiry and recovery. Run from a clean map and
repeat with mid-movement map change; confirm old commands do not dispatch and
fresh-map navigation works. Partial/unreachable routes must never execute.
Multi-Bot rows must show actual per-actor dispatch and progress, not an actor
count printed by a synthetic runner.

Append a result with its full dimension key, UTC time, environment/asset
references, expected/observed outcome, status (`pass`, `fail`, `unavailable`),
measurements and retained log/replay paths with hashes. For unavailable rows,
record the missing prerequisite and next action. Retain every failed attempt;
a retry is a separate result, never an overwrite of contrary evidence.

A failure reopens the affected work explicitly in the project state/report.
Live acceptance requires an observed pass for every required row. Missing
asset/provenance/version coverage remains separate real-NAV follow-up work.
No automatic deployment or server-start script is provided by this offline task.
