# Live Action Adapter Implementation Plan

> **For agentic workers:** implement task-by-task with TDD and preserve the
> repository's offline/live/Finish acceptance separation.

**Goal:** Connect Core combat and objective proposals to observable GoldSrc
attack, reload, C4 plant, and C4 defuse input dispatch.

**Architecture:** Keep Core SDK-free. An adapter-owned translator consumes
validated Core proposals and writes only `BotCommand`/public engine-bound
inputs. A runtime sensor builds bounded observations from current edicts; the
existing `RunPlayerMove` path remains the sole per-frame movement/action
dispatch boundary.

**Tech Stack:** C++17, CMake, Metamod-P/HLSDK public headers, focused native
tests, PowerShell live-log verifier, FocalSpan, and code-review-graph.

**Spec:** `.planning/phases/08-differential-live-parity-acceptance/08-07-PLAN.md`

## Global constraints

- Core remains SDK-free.
- ReGameDLL-CS/ZBot, YaPB, and RealBot are behavioral comparators only; do not
  copy private code or private symbols.
- Fresh offset-scoped server logs are required for live combat/C4 acceptance.
- Preserve unrelated dirty worktree changes and do not stage or commit.

## Task 1: Add the failing adapter contract

**Files:**

- Create: `src/adapter/metamod/action_adapter.hpp`
- Create: `src/adapter/metamod/action_adapter.cpp`
- Create: `tests/action_adapter_tests.cpp`
- Modify: `CMakeLists.txt`

**Steps:**

- [ ] Define a minimal adapter input/output contract for validated fire,
  reload, plant, and defuse intents.
- [ ] Add tests asserting attack/use buttons, reload command selection, view
  angle preservation, and invalid proposal rejection.
- [ ] Register the focused test target without changing production behavior.
- [ ] Run the focused test and confirm it fails because the translator is
  not implemented.

## Task 2: Implement proposal translation

**Files:**

- Modify: `src/adapter/metamod/action_adapter.hpp`
- Modify: `src/adapter/metamod/action_adapter.cpp`
- Modify: `tests/action_adapter_tests.cpp`

**Steps:**

- [ ] Implement the smallest translation that maps fire/plant to
  `IN_ATTACK`, defuse to `IN_USE`, reload to the public `reload` command, and
  carries the Core aim yaw/pitch.
- [ ] Reject invalid actor/frame/proposal identities without emitting input.
- [ ] Run the focused test and confirm GREEN.

## Task 3: Connect runtime observations and dispatch

**Files:**

- Modify: `src/adapter/metamod/plugin_runtime.hpp`
- Modify: `src/adapter/metamod/plugin_runtime.cpp`
- Modify: `tests/compat_actor_command_tests.cpp`
- Modify: `tests/phase8_action_adapter_boundary_test.ps1`

**Steps:**

- [ ] Add bounded alive-player, target, bombsite, and planted-bomb
  observations using public engine callbacks.
- [ ] Invoke existing `CombatController` and `RoundObjectivePlanner` with
  those observations and pass only validated translator output to the
  existing command dispatch.
- [ ] Combine action buttons with movement buttons and issue reload/weapon
  commands through the existing GameDLL command boundary.
- [ ] Add slot/generation/frame/action dispatch diagnostics and reset state
  on lifecycle cleanup.
- [ ] Extend the integration test to assert attack/use reach
  `pfnRunPlayerMove` while navigation remains intact.
- [ ] Run the boundary verifier in both diagnostic and required modes.

## Task 4: Verify current source and live gates

**Files:**

- Modify: `docs/evidence/phase8-live/windows-x86.md`
- Modify: `.planning/phases/08-differential-live-parity-acceptance/08-UAT.md`
- Modify: `.planning/phases/08-differential-live-parity-acceptance/08-VERIFICATION.md`

**Steps:**

- [ ] Run focused and regression tests on the fresh current-source x86
  build.
- [ ] Refresh FocalSpan and CRG after edits, then inspect the final diff.
- [ ] Capture deployed DLL identity and run the offset-scoped live verifier.
- [ ] Record movement, combat, plant, and defuse as independent gates and
  leave missing OS/differential gates pending.
