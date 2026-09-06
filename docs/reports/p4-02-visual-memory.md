# P4-02 — Visual memory offline evidence

## Implemented behavior

All-player visual memory consumes only validated public `ObservationBatch`
values and geometry-free lifecycle identities. Per-observer snapshots retain
target generations, the last observed point and simulation timestamp. Confidence
falls linearly from 1.0 to deletion at five seconds; retention is configurable
and zero is rejected. Hidden current positions never update remembered points.

Current-frame-only, atomic ingestion rejects duplicate, reordered, future,
malformed and stale-identity publications. Empty scans and deferred observers
do not renew timestamps. Death/spectators, slot/serial reuse, disconnect,
observer/agent retirement and map changes remove corresponding knowledge.
Invalid clocks clear knowledge and retain temporal high-water marks, so cached
old observations cannot repopulate memory on recovery. Missing roster APIs also
clear knowledge; missing/failed visibility traces provide no new evidence.

The adapter adds no traces and publishes only after existing vision revalidation.
Reentrant lifecycle callbacks cancel in-flight memory work. A map retirement
clears all memory immediately. An interrupted frame does not publish a new
snapshot; surviving observers resume decay on the next valid frame.

## Tests and resource bounds

- `astrabot.visual_memory`: initial observation, exact 2.5s confidence 0.5,
  4,999,999us boundary and 5s expiration, custom retention, reacquisition, empty
  batches, duplicates, observer isolation, atomic invalid batch rejection,
  clock rollback/recovery, map/agent/generation changes and capacity.
- `astrabot.adapter.visual_memory`: actual StartFrame to memory publication,
  hidden movement with frozen last-known position, per-frame decay without a
  new scan, deaths/spectators, serial reuse and reentrant disconnect, observer
  death and map retirement; invalid engine time and recovery.
- Host matrices cover 1/8/16 observers at 8/16/100ms, delayed scans, occlusion,
  reacquisition and exact equality between engine trace count and vision-only
  diagnostics. Existing goto arrival also asserts memory while movement runs.
- Fixed capacity is 32 observers x 31 targets. Windows x86 model size is 49,888
  bytes, no dynamic containers or per-frame heap allocation. Decay visits at
  most 992 existing entries per valid frame. Host ingestion accepts at most
  4x31 observations; each lookup examines at most 31 remembered entries.
  Lifecycle forget scans are also bounded by the same fixed arrays.
- A Debug synthetic 100-frame full-capacity run took 50,620us on this machine
  while other builds ran. This is a diagnostic sample, not a real-time SLA or
  live server performance claim. The test prints timing for reproduction.

## Pre-merge verification

Dedicated branch `codex/p402-visual-memory`, base main `96a57d1`.
All targets are x86. Windows uses VS 2026 Community, NMake, Debug and /W4 /WX
for tests; Release disables tests. Linux uses Debian WSL GCC -m32 Debug with
warnings as errors. Inspector is ON for portable and Metamod Debug.

| Gate | Result |
|---|---|
| Windows portable Debug | 43/43 passed |
| Windows Metamod Debug | 51/51 passed |
| Linux portable Debug | 42/42 passed |
| Windows Metamod Release | Build passed, machine 14C/x86, magic 10B/PE32 |
| DLL exports | Exactly GetEngineFunctions, GetEntityAPI2, GiveFnptrsToDll, Meta_Attach, Meta_Detach, Meta_Query |

SDK SHA verified: `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
Retained logs: `build-portable-x86-test/p402-ctest.log`,
`build-metamod-x86-test/Testing/Temporary/LastTest.log` and
`build-linux-x86-test/p402-ctest.log` inside the dedicated worktree.

WSL service access required execution outside the sandbox. Its P3-08 checker
uses process-local `GIT_DIR` and `GIT_WORK_TREE` pointing at the Windows worktree
metadata and source directory. No Git configuration/metadata rewrite was made.

Graph tools were consulted first but had no symbol coverage for this worktree;
their zero-risk result is not review evidence. Fresh FocalSpan context and
scoped source/diff inspection supplement executable verification.

## Post-merge verification

Implementation commit `3b9923e` was fast-forwarded from the dedicated branch
into `.worktrees/main-integration` main. The complete matrix was then freshly
configured, built and tested in that main worktree:

| Gate on merged main | Result |
|---|---|
| Windows portable Debug | 43/43 passed, 35.82s |
| Windows Metamod Debug | 51/51 passed, 59.91s |
| Linux portable Debug | 42/42 passed, 20.11s |
| Windows Metamod Release | Build passed, PE32/x86, exactly six required exports |

Retained main-worktree logs are `build-portable-x86-test/p402-ctest.log`,
`build-metamod-x86-test/p402-ctest.log`, `build-linux-x86-test/p402-ctest.log`,
`build-metamod-x86-release/p402-headers.log` and
`build-metamod-x86-release/p402-exports.log`.
Merged Release DLL SHA-256:
`b7713aa2e32faf9f351d21da48b5c3882fced9bff919966bb167a31d81026b6e`.
The main Debug full-capacity timing sample was 41,692us per 100 frames, with
the same 49,888-byte fixed model. These timings remain diagnostic samples.

The final evidence commit changes only this report and task state; tested
source, tests and build configuration remain exactly those in `3b9923e`.
Original root modifications and local tool files were preserved. No push,
branch deletion or worktree cleanup was performed.

## Remaining acceptance

No project-wide Finish or live HLDS/ReHLDS check. Enemy/team selection, sound,
NAV distribution diffusion, combat and persistence remain out of P4-02 scope.
Offline success does not establish real-server behavior or complete Phase 4.
