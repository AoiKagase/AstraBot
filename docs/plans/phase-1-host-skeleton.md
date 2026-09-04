# Phase 1 plan — Host skeleton

## Goal

On a pinned ReHLDS/HLDS + Metamod-P + ReGameDLL_CS server, load AstraBot,
create a normal FakeClient, complete CS team/class join, associate an external
`BotAgent`, submit simple movement, disconnect cleanly, and repeat after a map
change.  No Nav, combat, planner, Experience DB, ReAPI requirement, or AMXX
bridge is in scope.

Design prerequisite: [GoldSrc host validation](../research/goldsrc-host.md) and
[architecture contract](../architecture.md#host-boundary-contract).

## Planned modules

```text
src/core/identity.*          value IDs, TickId, generation
src/core/command.*           BotCommand value
src/host/game_host.*         host port and lifecycle results
src/adapter/metamod/*        plugin entry, hooks, engine/GameDLL dispatch
src/adapter/cstrike/*        join message/state adapter
src/debug/host_trace.*       structured lifecycle/command trace
tests/core/*                 value/command tests
tests/adapter/*              fake table/message/lifecycle tests
tests/live/*                 reproducible server scenario instructions
```

Names are provisional file boundaries; public concrete types remain subject to
implementation review.  Metamod-P/HLSDK headers are adapter-private.

## Commit-sized tasks

### P1-01 — Bootstrap build and portable host contracts

- **Goal:** establish C++ toolchain targets and the value-only Core/host port.
- **Files/modules:** root build configuration, `src/core/identity.*`,
  `src/core/command.*`, `src/host/game_host.*`, unit test target.
- **Implementation outline:** define generation-safe IDs, explicit `TickId`,
  bounded movement/view/buttons command values and typed lifecycle result/error.
  Add MPL SPDX headers.  Do not include any SDK header in portable targets.
- **Dependencies:** [pinned compiler/CMake and combined-binary policy](../toolchain-policy.md)
  and [license inventory](../research/license-matrix.md).
- **Tests:** value equality/order, generation mismatch, command bounds/defaults;
  compile a translation unit that includes every portable public header.
- **Acceptance:** Windows and Linux CI configure/build/test; include graph proves
  no GoldSrc/ReAPI/Metamod header in Core/host public headers.
- **Risk:** prematurely freezing the API.  Keep surface limited to Phase 1 data.

### P1-02 — Metamod-P load/unload skeleton

- **Goal:** load as `astrabot_mm` and report one structured adapter identity line.
- **Files/modules:** `src/adapter/metamod/plugin_entry.*`, SDK import/build target,
  `src/debug/host_trace.*`.
- **Implementation outline:** implement `Meta_Query`, `Meta_Attach`,
  `Meta_Detach`, exact `META_INTERFACE_VERSION` check, hook-table registration
  and rollback-safe attach state.  No Bot creation yet.
- **Dependencies:** P1-01; pinned Metamod-P `7ec9b014…`; completed combined-binary
  license/notices review for development artifacts.
- **Tests:** fake Metamod tables for null/mismatched/success attach and idempotent
  detach; exported-symbol inspection on Windows/Linux.
- **Acceptance:** live server loads/unloads without crash and log includes project
  version, adapter, interface version and outcome exactly once.
- **Risk:** calling convention/export differences.  Test both target platforms.

### P1-03 — Map/player lifecycle registry

- **Goal:** make map and player lifetime explicit before creating a Bot.
- **Files/modules:** `adapter/metamod/lifecycle.*`, `host/player_registry.*`, tests.
- **Implementation outline:** hook `ServerActivate`, `ServerDeactivate`,
  `ClientDisconnect`, `StartFrame`; create map/slot generations, ordered events,
  command-acceptance state and one cleanup path.
- **Dependencies:** P1-02.
- **Tests:** activate/deactivate cycles, duplicate disconnect, disconnect during
  join, slot reuse, stale command, partial attach rollback.
- **Acceptance:** no mapping survives deactivate; old IDs cannot target reused
  slots; hooks return the correct Metamod result without suppressing GameDLL.
- **Risk:** hook ordering and reentrancy.  State transitions must be explicit and
  no engine call may occur in a destructor.

### P1-04 — FakeClient allocation and external BotAgent mapping

- **Goal:** create a GameDLL-owned player and one external agent mapping.
- **Files/modules:** `adapter/metamod/fake_client.*`, `host/bot_agents.*`.
- **Implementation outline:** main-thread `pfnCreateFakeClient`; call GameDLL
  `player` factory; set info keys; `MDLL_ClientConnect` with reject handling;
  `MDLL_ClientPutInServer`; validate entity/slot; then atomically publish mapping.
  Do not replace `pvPrivateData` with an AstraBot object.
- **Dependencies:** P1-03 and fixed lifecycle sequence in host research.
- **Tests:** full table-driven failure at every step, max players, rejected name,
  null/private-data conditions, mapping publication only after success.
- **Acceptance:** live FakeClient appears as a normal CS client and trace relates
  one `BotAgentId` to one generation-safe `PlayerId` without exposing a pointer.
- **Risk:** engine/GameDLL version-specific initialization.  Any YaPB-derived
  reset step requires its own live proof, not cargo-cult copying.

### P1-05 — Message-driven CS join state machine

- **Goal:** select team and class based on actual menu messages.
- **Files/modules:** `adapter/cstrike/messages.*`, `join_state.*`, message fixtures.
- **Implementation outline:** adapt `VGUIMenu`, `ShowMenu`, `TeamInfo` to value
  events; tokenize bounded `menuselect` commands; guard the main-thread fake
  command context; make `pfnCmd_Args`/`pfnCmd_Argv`/`pfnCmd_Argc` return its
  values while `MDLL_ClientCommand(fakeEntity)` runs; retry/timeout/kick with
  reason codes.  Never call `pfnClientCommand` on a FakeClient.
- **Dependencies:** P1-04; captured ReGameDLL join message fixtures.
- **Tests:** tokenizer quotes/semicolons/empty args, nested/reentrant context,
  old text menu, VGUI menu, team full/stacked, already assigned,
  out-of-order/duplicate messages, timeout and disconnect.
- **Acceptance:** live T and CT cases reach confirmed joined state; no fixed delay
  is used as the success signal; failure leaves no agent/entity mapping.
- **Risk:** command-context trampoline and localized/menu variants.  Capture bytes
  and message IDs from the pinned server.

### P1-06 — Deterministic command submission and simple movement

- **Goal:** translate one Core `BotCommand` per tick to `pfnRunPlayerMove`.
- **Files/modules:** `adapter/metamod/movement.*`, host clock/tick pump, tests.
- **Implementation outline:** capture monotonic frame delta, clamp/quantize msec,
  validate ID/tick, translate view/forward/side/up/buttons/impulse, submit on main
  thread, record outcome.  StartFrame only schedules/dispatches; no planner.
- **Dependencies:** P1-05.
- **Tests:** msec boundaries, duplicate/out-of-order tick, dead/not-joined client,
  button transitions, map generation change, deterministic fake-engine calls.
- **Acceptance:** a joined Bot moves forward under a constant command; trace shows
  exactly one accepted engine call per command tick and rejects stale commands.
- **Risk:** incorrect msec creates collision/ghost behavior.  Compare elapsed
  server time and observed displacement under low/high server FPS.

### P1-07 — Removal, map-change replay, and live evidence

- **Goal:** close every lifecycle path and make the gate reproducible.
- **Files/modules:** remove/kick queue, live scenario script/docs, trace assertions.
- **Implementation outline:** quote/identify kick target safely; let
  `ClientDisconnect` own cleanup; stop commands before deactivate; replay after
  map activation; expose status counters and failure reason.
- **Dependencies:** P1-06.
- **Tests:** remove while joining/moving, external kick, server shutdown, map
  change with occupied slot, repeated create/remove; leak/sanitizer run where
  supported.
- **Acceptance:** the complete live gate below passes twice across a map change,
  with zero stale mappings and no crash/error log.
- **Risk:** cleanup split across kick and map hooks.  One registry transition must
  be authoritative and idempotent.

## Live gate

1. Record server OS/arch, HLDS/ReHLDS, GameDLL, Metamod-P and AstraBot versions.
2. Load `astrabot_mm`; assert one successful attach trace.
3. Create one T and one CT case; verify FakeClient, confirmed team/class and
   external mapping.
4. Submit a constant safe forward command and measure non-zero displacement.
5. Remove each Bot and assert `ClientDisconnect`, agent destruction and slot
   generation invalidation.
6. Change map, repeat steps 3–5, and assert map generation changed.
7. Save structured trace and exact commands as acceptance evidence.

Phase 1 does not claim Vanilla support until the same gate is run against a
pinned Vanilla CS GameDLL.
