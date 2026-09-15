---
phase: 01-foundation-and-abi
plan: 02
subsystem: metamod-abi
tags: [metamod-p, hl_sdk, c++14, x86, windows, linux, dll, shared-library]
requires:
  - phase: 01-foundation-and-abi
    provides: x86-checked CMake foundation and SDK-free Core target
provides:
  - Pinned Metamod-P/HLSDK external include boundary
  - Minimal C ABI Metamod plugin artifact
  - Exact six-name x86 export verification evidence
affects: [01-03, phase-2-metamod-lifecycle]
actuals:
  tokens: 2600
  tasks: 4
  commits: 1
tech-stack:
  added: [Metamod-P SDK headers, MSVC module definition]
  patterns: [extdll bootstrap include order, C ABI export boundary, stdcall export alias]
key-files:
  created:
    - include/astrabot/metamod/abi_contract.hpp
    - src/adapter/metamod/plugin_exports.cpp
    - src/adapter/metamod/astrabot_mm.def
    - tests/metamod_abi_tests.cpp
  modified:
    - CMakeLists.txt
key-decisions:
  - "Consume Metamod-P through the pinned SDK path and never copy its headers or sources."
  - "Use extdll.h before meta_api.h to satisfy the HLSDK bootstrap include contract."
  - "Use an MSVC module-definition alias for the single undecorated GiveFnptrsToDll export."
patterns-established:
  - "C ABI declarations are isolated under include/astrabot/metamod and do not cross into Core."
  - "Metamod API tables are zero-initialized until later lifecycle plans install hooks."
requirements-completed: [HOST-01, HOST-02, TEST-01]
coverage:
  - id: D1
    description: "Metamod ABI declarations compile against the pinned SDK"
    requirement: HOST-02
    verification:
      - kind: unit
        ref: "astrabot_metamod_abi CTest on Windows x86 Debug"
        status: pass
      - kind: unit
        ref: "astrabot_metamod_abi CTest on Debian WSL Linux x86 Debug"
        status: pass
    human_judgment: false
  - id: D2
    description: "Minimal AstraBot plugin exports the exact six required names as x86"
    requirement: HOST-01
    verification:
      - kind: other
        ref: "dumpbin /exports build-metamod-x86-test/astrabot_mm.dll"
        status: pass
      - kind: other
        ref: "readelf -h and nm -D --defined-only build-linux-x86-metamod-test/libastrabot_mm.so"
        status: pass
    human_judgment: false
  - id: D3
    description: "Plugin metadata and interface-version checks are validated without a live server"
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "tests/metamod_abi_tests.cpp::main"
        status: pass
    human_judgment: false
## Accomplishments

- Added the pinned Metamod-P/HLSDK include boundary with the required `extdll.h` bootstrap order and no copied SDK files.
- Added a minimal `astrabot_mm` plugin with safe metadata, interface checks, empty API tables, and six required C exports.
- Verified Windows/Linux x86 ABI tests and real PE/ELF architecture/export surfaces.

## Task Commits

1. **Task 1-4: ABI boundary, RED test, minimal exports, and GREEN verification** - `025f908` (`feat: add Metamod ABI plugin boundary`)

## Files Created/Modified

- `CMakeLists.txt` - Metamod SDK root validation, include paths, plugin/test targets.
- `include/astrabot/metamod/abi_contract.hpp` - SDK-backed C ABI declarations and type aliases.
- `src/adapter/metamod/plugin_exports.cpp` - Minimal metadata and safe export implementations.
- `src/adapter/metamod/astrabot_mm.def` - Windows x86 undecorated export set.
- `tests/metamod_abi_tests.cpp` - ABI function-pointer and metadata contract checks.

## Decisions Made

- Kept `GiveFnptrsToDll` as `WINAPI` while using a Windows-only `.def` alias to make the public symbol name exactly `GiveFnptrsToDll`.
- Used compile-time type checking for the stdcall function and artifact-level symbol inspection for the undecorated export name.

## Deviations from Plan

### Auto-fixed Issues

**1. [SDK include boundary] Added direct HLSDK subdirectory paths**

- **Found during:** Task 1 (ABI RED build)
- **Issue:** The Metamod-P engine-callback wrapper includes HLSDK headers by unqualified name.
- **Fix:** Added `hlsdk/common`, `hlsdk/dlls`, `hlsdk/engine`, and `hlsdk/pm_shared` to the external include set.
- **Files modified:** `CMakeLists.txt`
- **Verification:** The next build reached the intended missing-export link failure.
- **Committed in:** `025f908`

**2. [SDK include order] Restored the HLSDK bootstrap order**

- **Found during:** Task 2 (ABI RED build)
- **Issue:** Including `dllapi.h`/`engine_api.h` before the SDK bootstrap caused undefined `edict_t`/`g_engfuncs` diagnostics.
- **Fix:** Included `extdll.h`, then `meta_api.h`, then `h_export.h`.
- **Files modified:** `include/astrabot/metamod/abi_contract.hpp`
- **Verification:** The next build reached the intended missing-export link failure.
- **Committed in:** `025f908`

**3. [Windows ABI] Removed stdcall decoration from the public export set**

- **Found during:** Task 3 (artifact export inspection)
- **Issue:** MSVC exported `_GiveFnptrsToDll@8`, violating the exact six-name public surface.
- **Fix:** Added `astrabot_mm.def`, kept the `WINAPI` function signature, and moved the test to compile-time type checking for that function.
- **Files modified:** `CMakeLists.txt`, `include/astrabot/metamod/abi_contract.hpp`, `src/adapter/metamod/plugin_exports.cpp`, `src/adapter/metamod/astrabot_mm.def`, `tests/metamod_abi_tests.cpp`
- **Verification:** `dumpbin /exports` reports exactly six names; ABI CTest remains passing.
- **Committed in:** `025f908`

**4. [Linux tooling] Used readelf/nm because Debian lacks file**

- **Found during:** Task 4 (Linux artifact inspection)
- **Issue:** The Debian image has no `file` command.
- **Fix:** Used `readelf -h` for ELF32/Intel 80386 and `nm -D --defined-only` for the exact symbol set.
- **Files modified:** None
- **Verification:** ELF header and six symbols passed; no source or build configuration change was needed.
- **Committed in:** `025f908`

---
**Total deviations:** 4 environment/ABI corrections
**Impact on plan:** No gameplay scope change; the public six-export contract is stricter and now verified on both platforms.

## Issues Encountered

- The initial SDK include path and include order were insufficient; both were isolated and corrected before export implementation.
- Directly linking the undecorated stdcall export from the host test was incompatible with MSVC import naming; compile-time signature checking plus artifact export checking resolves the distinction.
- This phase proves ABI/loadability at the artifact level only. Runtime hook behavior and real server loading remain Phase 2/Phase 8 acceptance.

## User Setup Required

None.

## Next Phase Readiness

- Plan 01-03 can add source provenance and reusable PE/ELF verification scripts using the now-stable artifact names.
- Phase 2 can install lifecycle hooks into the empty API tables without changing the public export surface.

---
*Phase: 01-foundation-and-abi*
*Completed: 2026-09-15*
