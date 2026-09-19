# CSSDK Participation Rebuild Design

## Goal

Restore reliable CS team and class participation for AstraBot FakeClients. A
bot added through `bot_add`, `bot_add_t`, or `bot_add_ct` must leave the
intro/spectator camera, enter its requested team and class, and remain in a
normal player state after `sv_restart`.

## Reference implementation

Use the complete CSSDK snapshot from:

`H:/sourcecode/003.Game/amxmodx/AstraBot_bk/third_party/cssdk`

Preserve its license, README, and provenance material. The old participation
behavior is represented by the `AstraBot_bk` `src/adapter/cstrike/JoinState`
state machine and its message decoding boundary.

## Current failure boundary

The current checkout creates FakeClients correctly but uses an adapter-local
`pendingTeamJoins_` path that calls `jointeam` and `joinclass` directly. It does
not retain the old menu-driven state machine, bounded retries, menu fallback,
or explicit team/class confirmation. This leaves a FakeClient in the
intro/spectator camera on the target ReGameDLL setup.

## Rebuild design

1. Copy `third_party/cssdk` from the old checkout into the current checkout,
   including its provenance and license files.
2. Port the old `JoinState` and message decoding behavior behind a narrow
   AstraBot adapter interface. The state machine owns menu phases, bounded
   retries, timeout, cancellation, and team/class confirmation.
3. Replace the current direct `pendingTeamJoins_` dispatch path with the
   state-machine adapter. Keep FakeClient creation, actor identity, lifecycle,
   and input dispatch ownership in the current runtime.
4. Match the old FakeClient userinfo setup required by the join path, including
   `_vgui_menus=0`, `_ah=0`, and `*bot=1`, while retaining current lifecycle
   ownership and cleanup.
5. Register CSSDK include paths and adapter sources in CMake for the Metamod
   target. Do not copy ReGameDLL private implementation or link against it.

## Verification

- Build the existing Windows x86 `astrabot_mm` target with the pinned SDK and
  compiler environment.
- Verify PE architecture and exact exports.
- Deploy the resulting DLL to the configured ReHLDS AstraBot DLL directory,
  keeping a timestamped backup.
- Live acceptance must confirm `bot_enable 1`, `bot_quota 20`, all three add
  commands, team/class entry, leaving the camera state, and `sv_restart 1`
  recovery. Offline checks remain separate from HLDS acceptance.

## Scope boundary

This change replaces only the participation path and its required CSSDK
integration. Existing navigation, movement, actor lifecycle, and compatibility
command ownership remain in the current checkout unless the join-state adapter
requires a narrow interface change.
