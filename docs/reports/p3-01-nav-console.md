# P3-01 console integration

Date: 2026-09-05. Base `03ee7d9` was fast-forward integrated into main before
this work. Scope is the existing P3-01 console slice. Movement and Finish are
not implemented or declared here. Real NAV compatibility remains partially
validated under the existing real-file report.

## Operator commands

These are server-console commands; no client command hook grants this capability.

| Command | Result |
|---|---|
| `astrabot_nav_load <nav-path>` | Read-only load into the active map generation |
| `astrabot_goto <area-id>` | Request a bounded route for the unique joined managed Bot |
| `astrabot_nav_status` | Print session identity, state, selected edges and cost |
| `astrabot_nav_cancel` | Cancel the current request generation |

Use an explicit path (quote paths containing spaces); relative paths resolve
against the server process working directory. Load after map activation. This
initial operator binding does not infer or certify the NAV's map provenance;
the operator must select the corresponding NAV. Map deactivation, active Bot
disconnect/removal and detach invalidate loaded state. Reload after those events.
Replacing a NAV, including a failed load, drops the old session and graph.
Invalid syntax does not replace an active goal.

Input is capped at 64 MiB before allocation, with 100,000 areas, 199,999 index
nodes, 1,000,000 edges, 256 MiB per logical mesh/index/graph/search allowance and
100,000 expansions. Remaining record limits match the compatibility-v1 profile.
No repair, file write, resave, hidden retry or unbounded search is performed.
Load rejection prints existing diagnostic kind/record/field numeric values and
byte offset; I/O rejection has a named input reason.

Actor selection validates the active FakeClient entity, its slot/generation,
registry connection, one BotAgent mapping, FL_FAKECLIENT, joined state and pending
removal. Human/ambiguous/unjoined/missing actors are rejected. Death cancels a
Ready session on frame observation. Goal replacement uses RouteSession generations.
Only Complete is Ready; same-area Complete is not arrival. No RunPlayerMove or
other movement command is emitted by the console path.

The initial current-area query is deliberately narrow: FL_ONGROUND and finite
hull/position, one downward trace from feet+2 to feet-4, non-solid hit with normal
Z >= 0.7, hit height within four units of feet, then NAV containing at that
height with two-unit tolerance. No nearest fallback supplies ground proof.
Missing/airborne/invalid support fails closed. This does not implement the full
P3-03 swept-hull/clearance/stair/stacked-floor acceptance matrix. Query requests
retain actor/map/tick/route/ordinal stamps. Reentrant invalidation during the
query is deferred until the synchronous request returns, and cannot publish a
Ready result. Valid simulation frametime in [0,60] seconds is recorded in elapsedUs;
other timing observations leave zero and do not drive movement.

Trace printing is portable in `src/debug/nav_command.*`. Each trace uses fixed
512-byte lines and prints at most 64 selected edges (66 lines total), explicitly
counting omitted edges. Cost formatting is locale-independent. Identity, current/
goal, state/reason, terminal flag, route status/cost/expansions, directed edge and
external source/generation/link identity are visible. Arrival remains unverified.
Events are emitted on requests and terminal transitions; normal frame observation
does not continually print the same status.

## Engine bootstrap and ABI decision

The user explicitly approved six exports instead of five in this task.
`GiveFnptrsToDll` receives/copies the plugin engine table and retains the engine
globals pointer inside the adapter. Missing bootstrap/command functions reject
attach before publishing hooks. The existing hook table remains used by the
existing host/movement path; console registration uses the plugin table.

Pinned Metamod-P `7ec9b014f8c0a947a724644aebe34eb33706e44b` source explains why:
`metamod/mutil.cpp` GetHookTables returns `meta_engfuncs`, whose
`engine_api.cpp` AddServerCommand uses the ordinary engine hook dispatcher.
`metamod.cpp` instead installs `meta_AddServerCommand` in `Engine.pl_funcs`;
`reg_support.cpp` records the originating plugin and `mplugin.cpp` disables its
registered commands on unload. The bootstrap table preserves that registration
path. Upstream code was inspected as an ABI reference, not copied.

The Windows module definition exports the x86 stdcall bootstrap without decoration.
The exact exports are Meta_Query, Meta_Attach, Meta_Detach, GetEntityAPI2,
GetEngineFunctions and GiveFnptrsToDll. Adapter/test linkage explicitly includes
Nav. Core and Nav use `_ITERATOR_DEBUG_LEVEL=0` consistently for the combined
MSVC ABI; warning levels remain unchanged. SDK macro collisions with standard
read/snprintf/max names are isolated at the adapter/header boundary.

## Offline verification and remaining acceptance

Windows x86 Visual Studio 2026, NMake, Debug, warnings-as-errors, inspector ON:
portable CTest 20/20 PASS (11.83 seconds) and fixture/manifest checks PASS.
Adapter-enabled CTest 24/24 PASS (11.91 seconds). Release DLL builds and exports
exactly six undecorated symbols. These checks ran after the final source changes.

Coverage includes successful/same-area/unreachable goto, invalid numeric/extra
arguments, status/cancel, death and disconnect, rejected human identity, missing
support, NAV file load and unchanged bytes, failed reload, reentrant map invalidation,
trace bounds/replay, missing bootstrap and registration through the correct table.
Existing host/join/movement and Nav tests remain in the verification set.

No remote CI, Linux, server start or real movement test was performed. Actual
Metamod load/unload and real support geometry remain post-Finish acceptance.
The next implementation slice is P3-02 corridor/portals, independent of unresolved
real-file provenance/detail rows. P3-01 is not claimed fully accepted.
