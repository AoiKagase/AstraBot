# P1-02 Metamod-P load/unload live acceptance

This scenario is intentionally separate from the portable and offline adapter
tests.  Run it only with the pinned ReHLDS/HLDS, Metamod-P, and ReGameDLL_CS
server installation used for the release candidate.  Record every component
version or commit before starting; the Metamod-P checkout used to build the
adapter must be `7ec9b014f8c0a947a724644aebe34eb33706e44b`.

## Build and export evidence

Configure with the external SDK checkout; do not copy SDK headers into this
repository:

```powershell
cmake -S . -B build-metamod `
  -DASTRABOT_BUILD_METAMOD=ON `
  -A Win32 `
  -DASTRABOT_METAMOD_SDK_ROOT=H:\sourcecode\003.Game\amxmodx\metamod-p
cmake --build build-metamod --config Release
```

For a single-configuration generator, start the VS 2026 developer prompt with
the x86 target selected (for example `VsDevCmd.bat -arch=x86`).  Linux builds
must use a 32-bit toolchain such as `-m32`; a 64-bit adapter configure is
rejected.

Windows export check:

```powershell
dumpbin /exports build-metamod\Release\astrabot_mm.dll
```

Linux export check:

```bash
nm -D --defined-only build-metamod/astrabot_mm.so
```

The only exported adapter entry symbols must be the following undecorated
names:

```text
Meta_Query
Meta_Attach
Meta_Detach
GetEntityAPI2
GetEngineFunctions
```

`Plugin_info`, Core symbols, raw addresses, and SDK implementation symbols are
not part of the public export contract.

## Server procedure

1. Install `astrabot_mm.dll` or `astrabot_mm.so` in the server's Metamod plugin
   location and add it to the pinned test server plugin list.
2. Start the pinned ReHLDS/HLDS + Metamod-P + ReGameDLL_CS server and capture
   the complete server log from startup.
3. Confirm that the plugin is loaded and attached without a Metamod error.
4. Confirm exactly one line matching the following text for the attach cycle:

   ```text
   astrabot version=0.1.0 adapter=metamod-p interface=5:13 outcome=attached
   ```

5. Confirm that no lifecycle, StartFrame, movement, ReAPI, AMXX, or GameDLL
   hook side effect occurs in P1-02.
6. Unload the plugin, or change map if that is the supported unload operation
   in the pinned server setup.  Confirm detach completes without a crash.
7. Repeat the unload request once.  Confirm it is harmless and does not add a
   second identity line.
8. Confirm the server log contains no error or crash report throughout the
   load, attach, map/unload, and repeated-unload sequence.

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
| Load command and timestamp |  |
| Attach identity line count |  |
| Hook side-effect check |  |
| Detach/map-change result |  |
| Repeated unload result |  |
| Server error/crash log check |  |
| Operator and date |  |
