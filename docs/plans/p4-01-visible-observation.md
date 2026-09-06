# P4-01 — Visible observation

Approved scope: geometric perception Core and CS adapter connected through
StartFrame. Base: main `7158917`; branch `codex/p401-visible-observation`,
dedicated `.worktrees/p401-visible-observation`. P3-08 offline implementation
is complete; project-wide Finish and live acceptance remain separate.

## Contract

- SDK-free settings, generation-safe player/observer/map/tick identity,
  simulation timestamp, observations, diagnostic reasons and visibility port.
- Private adapter roster includes connected, alive, non-spectator humans and
  bots. Only joined managed bots observe. No team/enemy selection or commands.
- Eye and body-center samples, in that order. Defaults: 4096 units, a circular
  full-angle 90-degree cone, 100000 us interval. Either sample passing distance,
  cone and a TraceLine with players/glass enabled produces one observation.
- Trace success requires intended-target contact or a clear endpoint. Solid,
  missing, malformed, non-finite and stale results fail closed. Engine pointers,
  serials and undisclosed candidates never reach the public observation batch.
- At most 4 observers/frame, 31 candidates/observer, 2 traces/candidate (248
  maximum/frame). Initial phases use agent ID modulo 16. Round-robin due work
  is fair; missed periods are not replayed. Report lateness, measured update
  interval, deferred frames and query counts without hidden positions.
- Batch timestamps change only on scans; roster invalidation runs every frame.
  Death, spectators, disconnect, serial/slot reuse and map changes retire data.
  Reentrant engine retirement cannot resurrect a staged observation.

## Implementation and acceptance

1. Core filter/scheduler and bounded portable tests.
2. Adapter-private roster/serial ownership and query conversion, existing
   bootstrap globalvars binding, StartFrame integration and lifecycle cleanup.
3. Fake-engine hook-to-publication tests for occlusion, failures, invalidation
   and 1/8/16 actor matrices at 8/16/100 ms; existing movement regressions.
4. Windows x86 portable/Metamod Debug warnings-as-errors gates, Release DLL and
   six exports, Linux x86 portable gate. Report retained logs and limitations.
5. Refresh FocalSpan, inspect/stage only intended changes, whitespace check,
   commit and verify. Preserve original worktree/unrelated changes. No push/merge.

Smoke, flash, sound, memory/belief confidence, combat and live-server validation
are deferred. This task does not complete Phase 4 or declare Finish.
