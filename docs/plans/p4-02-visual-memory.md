# P4-02 — Visual memory and confidence decay

Approved scope: all observed players without team selection; last observed
position/time and linear confidence decay over configurable 5,000,000us (zero
rejected). No sound, NAV belief diffusion, combat, persistence or live checks.

Base: main `96a57d1`. Worktree: `.worktrees/p402-visual-memory`.
Branch: `codex/p402-visual-memory`. No subagents, push or cleanup.

## Contract

- SDK-free `core::world::VisualMemoryModel`, fixed 32 owners x 31 targets.
  Read-only snapshots carry owner/agent/map/frame identity; entries carry target
  generation, last known position, last-seen simulation time and confidence.
- Ingress is public `ObservationBatch` plus geometry-free `MemoryFrame` with
  eligible player/agent identities. Memory never reads engine positions.
- Advance each simulation frame, then ingest current-frame batches after
  adapter revalidation. Cached, duplicate, old or future batches cannot refresh
  knowledge. Empty and deferred scans still age existing memory.
- Confidence is `1 - age / retention`; delete at age >= retention. New valid
  sightings replace position/time and restore 1.0. No position extrapolation.
- Death, spectators, disconnect, observer retirement, generation reuse and map
  changes retire knowledge. Agent replacement clears owner memory. Per-map
  generation high-water marks reject reuse of older identities.
- Invalid time/frame clears knowledge but retains time/tick high-water marks.
  Recovery requires a later tick and time at least the previous valid time.
  Malformed batches are rejected atomically. Expose frame work and cumulative
  update/expiration/retirement/rejection diagnostics without rejected geometry.
- Lifecycle callbacks cancel in-flight memory publication. No extra traces or
  DLL exports.

## Verification and integration order

1. Portable contracts, fake-engine StartFrame/lifecycle tests, 1/8/16 observer x
   8/16/100ms matrices and existing movement regressions.
2. Windows x86 portable/Metamod Debug warnings-as-errors, Linux x86 portable
   Debug, Release DLL PE32 and exact six exports.
3. Review diff, refresh/query FocalSpan, stage explicit paths, check staged
   whitespace and commit. Preserve original root changes/tool configs.
4. Fast-forward `.worktrees/main-integration` main and rerun all gates on merged
   state. Record results and commit evidence. Fix failures on the dedicated
   branch, reintegrate and reverify.

Offline completion neither completes Phase 4 nor declares project-wide Finish.
