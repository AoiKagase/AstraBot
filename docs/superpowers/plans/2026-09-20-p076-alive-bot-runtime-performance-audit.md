# P07.6 Alive-Bot Runtime Performance Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Measure alive-Bot runtime scaling at the production adapter boundary and remove only the confirmed redundant NAV/objective work without changing P02 cadence or P06 visibility semantics.

**Architecture:** Extend the existing opt-in fixed-counter profiler with the requested per-stage and vision/NAV counters. Instrument the already-authoritative Full Update path and expose diagnostic toggles only through default-off CVars. Apply the smallest behavior-preserving guard to avoid bomb-site NAV searches when the actor cannot use an objective target; leave the O(Bot x candidate x body-probe) vision scan measurable and unchanged until live evidence justifies a separate parity-safe decision.

**Tech Stack:** C++14, CMake, Metamod-P/HLSDK public adapter boundary, portable CTest, PowerShell checks, FocalSpan, code-review-graph.

**Spec:** User-provided P07.6-PERF request at `C:\Users\SandS\.codex\attachments\3cfcb21c-72ce-4163-bde3-67da2a56977b\貼り付けたテキスト.txt`.

## Global Constraints

- Do not start P08 Combat/Aim/Weapon work.
- Do not modify `BotTimingScheduler`; retain 30 Hz command/upkeep, nested 10 Hz Full Update, absolute deadlines, and `msec` semantics.
- Do not change P06 FOV/LOS/body-probe visibility semantics while measuring them.
- Profiling is default-off, fixed-size, one-second aggregate output, and must not emit per-trace or per-frame long logs.
- Preserve existing dirty/untracked files; stage no files and create no commits unless explicitly requested.
- Treat offline tests and static/reference analysis separately from live HLDS/ReHLDS FPS acceptance.

## Review Focus

- Full Update must be the only production caller of heavy visibility scans; test the cached command-tick path.
- Alive/dead separation must not suppress engine command simulation or freeze handling; test dead exclusion only for alive-only counters.
- Profiler disabled must not call the high-resolution clock or alter decisions; test the disabled path.
- Path persistence must not turn a temporary current-area miss into an unbounded A* retry; retain existing NAV tests and add counter assertions where possible.
- Diagnostic toggles must be default-off and must not become P08 design dependencies; test registration/defaults.

### Task 1: Expand the opt-in profiler contract

**Files:**
- Modify: `include/astrabot/metamod/runtime_profiler.hpp`
- Modify: `src/adapter/metamod/runtime_profiler.cpp`
- Modify: `tests/runtime_profiler_tests.cpp`
- Modify: `CMakeLists.txt` only if a focused test target needs registration

**Interfaces:**
- Consumes: existing stage timings and trace/path counters.
- Produces: fixed-size counters for alive bots, vision candidates/FOV/LOS/body probes, world publish, runtime input/full update, NAV area lookup/path search/recompute/movement, dispatch, and trace serialization; one-second `RuntimeProfilerReport`.

- [ ] Write failing tests for disabled no-op accounting, separate FOV/LOS/body counters, report reset, and alive-bot snapshot.
- [ ] Run `astrabot_runtime_profiler` and observe the expected compile/assertion failure before implementation.
- [ ] Implement the smallest overflow-safe fixed-counter additions and report fields; keep disabled methods branch-only.
- [ ] Run the focused profiler test and verify the red-green transition.

### Task 2: Instrument production Full Update scaling and diagnostic toggles

**Files:**
- Modify: `include/astrabot/metamod/runtime_profiler.hpp`
- Modify: `src/adapter/metamod/runtime_profiler.cpp`
- Modify: `src/adapter/metamod/observation_adapter.hpp`
- Modify: `src/adapter/metamod/observation_adapter.cpp`
- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Modify: `tests/perception_adapter_tests.cpp`
- Modify: `tests/compat_actor_command_tests.cpp` only for CVar/default contract coverage

**Interfaces:**
- Consumes: public `edict_t`, existing scheduler branches, `ObservationAdapter::collectVisibility`, `NavRoamDecision`, and existing `astrabot_profile` CVar.
- Produces: accurate vision call/FOV/LOS/trace/body counts, alive-bot count, world publish/full-update/runtime input/NAV/movement/dispatch timings, and default-off `astrabot_perf_disable_vision`, `astrabot_perf_disable_pathsearch`, and `astrabot_perf_disable_trace` diagnostics.

- [ ] Write failing tests for five ordered body probes and default-off diagnostic controls without changing normal visibility behavior.
- [ ] Run the focused adapter tests and observe the expected missing-counter/toggle failure.
- [ ] Add counters at the actual probe/TraceLine/publish/NAV boundaries and gate only diagnostic measurement paths; keep the scheduler and normal path unchanged.
- [ ] Run focused perception/profiler/compatibility tests and verify counters remain zero when disabled and increase when enabled.

### Task 3: Remove confirmed redundant objective NAV scans

**Files:**
- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Modify: `tests/runtime_movement_integration_contract.py`
- Modify: `docs/parity/PRODUCTION_RUNTIME_MODEL.md`
- Modify: `docs/parity/TRACE_SCHEMA.md` if the aggregate counter schema changes

**Interfaces:**
- Consumes: public team/carrying-C4/planted-bomb observations and immutable NAV snapshot.
- Produces: unchanged objective target semantics, with no bomb-site area corridor search when the actor is not a Terrorist carrying C4; profiler evidence for objective/NAV scaling.

- [ ] Add a source/contract regression assertion proving non-objective actors do not enter bomb-site corridor enumeration.
- [ ] Run it and observe failure against the current unconditional entity/area loop.
- [ ] Move the cheap team/objective eligibility decision before the entity scan; retain the existing corridor selection only for eligible objective actors.
- [ ] Run focused NAV/runtime tests and verify normal roam, dead handling, freeze handling, and path persistence remain covered.

### Task 4: Verification and live handoff record

**Files:**
- Modify: `docs/parity/PRODUCTION_RUNTIME_MODEL.md`
- Modify: `docs/parity/STATUS.md`
- Modify: `.planning/ROADMAP.md` and `.planning/STATE.md` only if the current P07.6 status requires a factual update
- Add or modify: `tests/p076_production_runtime_docs.py`

- [ ] Add documentation checks for the execution inventory, reference cadence, scaling formulas, profiler schema, and explicit `P08 NOT STARTED` boundary.
- [ ] Run the documentation check and observe failure before updating the docs.
- [ ] Run matching Windows x86 Debug configure/build and full CTest; run focused perception/NAV/runtime tests and Phase 8 fixtures.
- [ ] Run `focalspan update --root .`, `focalspan status --json`, a follow-up query, `git diff --check`, and inspect the final diff without staging unrelated files.
- [ ] Record live HLDS/ReHLDS A/B procedure and leave FPS/scaling result as `PARTIAL` until the user supplies fresh intervals.
