# Execution ledger — docs/plans/p12-locomotion-redesign.md

Baseline `38ca8de`, worktree `.worktrees/p12-locomotion-redesign`, branch
`codex/p12-locomotion-redesign`. Implementation started; not live accepted.

## Preflight and ownership

| Work | Producer/consumer | Decision |
| --- | --- | --- |
| Terrain | TerrainSampler -> controller/adapter | Compatible ProbeResult; physical evidence remains mandatory |
| Path | PathFollower -> controller | Only measured support advances; special transitions are barriers |
| Execution | ExecutionPolicy -> RouteSession | Forward base heuristic/context; no source aggregation |
| Controller | sampler/follower -> Motor | Root owns adapter integration and CMake |
| Validation | Release -> live -> offline | User exception overrides baseline/TDD test-run and commit requirements |

- Terrain worker owns sampler, ground compatibility, physical world queries.
- Path worker owns follower and corridor changes.
- Execution worker owns execution/replan policies.
- Root owns controller/envelope/feedback, adapter integration, build and docs.
- FocalSpan worktree update and initial query succeeded before source edits.

## Status

- [x] Confirm committed baseline, preserve unrelated main changes, create worktree.
- [x] Initialize and query FocalSpan in worktree.
- [ ] Record runtime assets and reproduction scenarios.
- [ ] Terrain implementation and static review.
- [ ] Path implementation and static review.
- [ ] Controller/jump/envelope/feedback and adapter integration.
- [ ] Execution policy and static review.
- [ ] Release build and export inspection.
- [ ] Live acceptance (six scenarios, 1/2 BOT, ten repeats, explicit PASS).
- [ ] Remove comparison path and reaccept final Release artifact.
- [ ] Focused tests and canonical All (prohibited before PASS).
- [ ] FocalSpan update, documentation, scoped commit (prohibited before PASS).

## 2026-09-12 Release candidate handoff

- Baseline HEAD remains 38ca8de18e3269dfd6e04cf564eafb97c45b4e8e; no commit/stage performed.
- Native TerrainSampler, PathFollower, LocomotionController, MotionEnvelope/MovementFeedback and LocalDoor integrated with the adapter; legacy comparison remains OFF by default and ladder-specific control is retained.
- Directed-edge cooldown replaces source aggregation; policy heuristic/context forwarded.
- Static review corrections: native speed-limit check, dispatch duration alignment, Drop airborne tracking, native Hold non-destructive rejection, transient airborne crouch handling, fixed-axis monotonic progress, Door approach lifetime, no duplicate takeoff impulse, runtime sv_stepsize.
- Release x86 NMake target astrabot_mm succeeded; tests OFF. Exactly six required exports verified.
- Compile triage included controller namespace closure, MotionReason enum name, and C4456 shadowing at motion.cpp:1354; corrected and rebuilt Release only.
- Candidate DLL SHA256: ba702d6b2202e8703e18dda58c52302e888ef4c4f88fb4e6829b72c43a7e3e7e.
- New test sources and registrations prepared; no test configure/build/run, CTest or canonical executed.
- FocalSpan update and relevant integration query succeeded; git diff --check succeeded.
- See docs/evidence/p12-locomotion-live for artifact/deployment identity, startup smoke and 120 NOT_RUN acceptance records. Fixed scenario coordinates await confirmation.
- Still pending: real-device six-symptom acceptance and explicit PASS; remove comparison/duplicate legacy implementation; final Release reacceptance; focused offline checks, canonical once, final evidence/FocalSpan/stage/commit. This is not project Finish.
- Startup smoke: PID 42436, actual deployed DLL module confirmed via x86 PowerShell, NAV Ready and bot joined de_dust2. Server remains running. This does not count toward the 120 acceptance trials.
- Live acceptance failed: user reported crash. Runtime dump 20260912_185955_1.mdmp is zero bytes. Same Release is running under x86 CDB to capture second-chance AV/stack overflow; diagnosis pending. No tests/canonical/commit.
- Stack crash fixed: ActorState reset now constructs in place, frame inputs moved to coordinator scratch. Release build/6 exports successful. Generated fixed allocation: startFrame 477764 -> 20940 bytes, selectActor 519324 -> 16 bytes, NavConsole reset 519384 -> 2132 bytes (excluding prologue). New DLL 4b40a04a6dfafa097f4b070f424414b3e7244d322d359c14938d6cc0668d41aa deployed; live recheck in progress.
- Crash regression live result: 2BOT add/round-2 transition and 10 sv_restart 1 commands survived through round 12 under CDB, no AV/SOV. Server 16180/debugger38192 left running. Full 120-trial acceptance and explicit PASS remain pending.
