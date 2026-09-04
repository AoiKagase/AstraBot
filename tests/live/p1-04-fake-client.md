// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

# P1-04 FakeClient allocation and mapping live acceptance

This scenario is run only after all project plans are complete and the
project-wide `Finish` state has been explicitly confirmed.  Linux `-m32`
builds and live-server validation are post-Finish checks; they are not part of
the P1-04 pre-Finish implementation gate.

P1-04 stops after GameDLL `ClientPutInServer` and external mapping publication.
CS team/class join, movement, ReAPI, AMXX, and Nav are not acceptance items.

## Build evidence

Use the pinned Metamod-P checkout and the GoldSrc x86 ABI:

```powershell
cmake -S . -B build-metamod-x86 `
  -DASTRABOT_BUILD_METAMOD=ON `
  -A Win32 `
  -DASTRABOT_METAMOD_SDK_ROOT=H:\sourcecode\003.Game\amxmodx\metamod-p
cmake --build build-metamod-x86 --config Release
```

Record the artifact hash and confirm the existing five undecorated exports:

```text
Meta_Query
Meta_Attach
Meta_Detach
GetEntityAPI2
GetEngineFunctions
```

## Server procedure

1. Record ReHLDS/HLDS, Metamod-P, ReGameDLL_CS, OS, and architecture.
2. Install `astrabot_mm.dll` in the test server's Metamod plugin directory.
3. Start the server and confirm one attach identity line.
4. On the first frame after map activation, confirm one FakeClient allocation,
   `player` factory call, metadata setup, connect, and put-in-server sequence.
5. Confirm exactly one terminal mapping trace containing map generation, slot,
   player generation, and `BotAgentId`; no raw pointer or private-data address
   may appear.
6. Confirm the FakeClient is visible as a normal GameDLL-owned client.  Do not
   treat team/class join as a P1-04 result.
7. Change map and confirm the player/agent mapping is cleared before the next
   map.  The next map must receive a newer generation.
8. Unload or repeat the map-change operation and confirm no crash, duplicate
   mapping, or stale-player state.
9. Confirm the complete server log has no error or crash.

## Evidence template

| Item | Evidence |
| --- | --- |
| ReHLDS/HLDS version or SHA |  |
| Metamod-P SHA | `7ec9b014f8c0a947a724644aebe34eb33706e44b` |
| ReGameDLL_CS version or SHA |  |
| OS/architecture |  |
| Compiler/CMake |  |
| Built artifact path and hash |  |
| Export command/output |  |
| Allocation call order |  |
| Map generation |  |
| Slot/player generation |  |
| BotAgentId |  |
| Mapping trace count |  |
| Raw pointer/private-data leak check |  |
| Map-change cleanup |  |
| Repeated unload/map-change result |  |
| Server error/crash log check |  |
| Operator and date |  |
