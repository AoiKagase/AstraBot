// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

# P1-03 map/player lifecycle live acceptance

This scenario is intentionally separate from portable and offline adapter
tests.  Run it only after the project-wide `Finish` gate has been confirmed,
using the pinned ReHLDS/HLDS, Metamod-P, and ReGameDLL_CS server installation.
P1-03 does not include FakeClient creation, join, movement, ReAPI, or AMXX.

## Build evidence

The adapter must be configured as a 32-bit target against the external pinned
Metamod-P checkout:

```powershell
cmake -S . -B build-metamod `
  -DASTRABOT_BUILD_METAMOD=ON `
  -A Win32 `
  -DASTRABOT_METAMOD_SDK_ROOT=H:\sourcecode\003.Game\amxmodx\metamod-p
cmake --build build-metamod --config Release
```

Record the adapter artifact hash and verify its five P1-02 exports with
`dumpbin /exports` on Windows or `nm -D --defined-only` on Linux.

## Server procedure

1. Record the ReHLDS/HLDS, Metamod-P, and ReGameDLL_CS versions or SHAs.
2. Install `astrabot_mm.dll` or `astrabot_mm.so` in the test server's
   Metamod plugin location and capture the complete server log.
3. Start the server and confirm one attach identity line, with no load error:

   ```text
   astrabot version=0.1.0 adapter=metamod-p interface=5:13 outcome=attached
   ```

4. Confirm `ServerActivate` starts one map generation and that no raw edict
   address or SDK object address is logged.
5. Change map or otherwise trigger `ServerDeactivate`, then start the next
   map and confirm the map generation advances without stale slot state.
6. Connect and disconnect a normal client. Confirm the disconnect path uses
   only the engine `pfnIndexOfEdict` result, does not suppress GameDLL
   behavior, and does not crash.
7. Confirm `StartFrame` advances ticks while producing no per-frame production
   console spam. No AI scheduling or engine command is expected in P1-03.
8. Repeat the map-change/unload operation once and confirm it is harmless.
9. Confirm the complete log has no error, crash, stale-player, or duplicate
   attach identity line.

## Evidence template

| Item | Evidence |
| --- | --- |
| ReHLDS/HLDS version or SHA |  |
| Metamod-P SHA | `7ec9b014f8c0a947a724644aebe34eb33706e44b` |
| ReGameDLL_CS version or SHA |  |
| OS/architecture |  |
| Compiler/CMake |  |
| Built artifact path and hash |  |
| Export command/output captured |  |
| Map activate/deactivate sequence |  |
| Slot disconnect observation |  |
| StartFrame/tick observation |  |
| GameDLL suppression check |  |
| Attach identity line count |  |
| Repeated unload/map-change result |  |
| Server error/crash log check |  |
| Operator and date |  |
