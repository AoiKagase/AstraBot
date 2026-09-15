---
phase: 01-foundation-and-abi
plan: 01
subsystem: build
tags: [cmake, c++14, x86, windows, linux, ctest]
requires:
  - phase: none
    provides: []
provides:
  - Reproducible x86-checked CMake foundation
  - SDK-free `astrabot_core` static target
  - Deterministic build identity contract and test
affects: [01-02, 01-03, phase-2-metamod-lifecycle]
actuals:
  tokens: 1800
  tasks: 5
  commits: 1
tech-stack:
  added: [CMake, C++14, CTest]
  patterns: [x86 configure guard, target-scoped warnings, SDK-free Core target]
key-files:
  created:
    - include/astrabot/build_identity.hpp
    - src/core/build_identity.cpp
    - tests/build_identity_tests.cpp
  modified:
    - CMakeLists.txt
    - cmake/AstraBotBuild.cmake
    - .gitignore
key-decisions:
  - "Use C++14 to match the pinned ReGameDLL-CS build baseline."
  - "Reject non-32-bit configurations at CMake configure time."
  - "Keep portable Core compilation independent from the Metamod-P SDK."
patterns-established:
  - "All AstraBot targets receive project-scoped warning settings from AstraBotBuild.cmake."
  - "Build identity is exposed as SDK-free value functions and checked by CTest."
requirements-completed: [HOST-03, TEST-01]
coverage:
  - id: D1
    description: "CMake foundation and build identity Core target"
    requirement: HOST-03
    verification:
      - kind: unit
        ref: "astrabot_build_identity CTest on Windows x86 Debug"
        status: pass
      - kind: unit
        ref: "astrabot_build_identity CTest on Debian WSL Linux x86 Debug"
        status: pass
    human_judgment: false
  - id: D2
    description: "Non-x86 configuration is rejected"
    requirement: HOST-03
    verification:
      - kind: other
        ref: "CMake x64 configure returns the explicit AstraBot 32-bit diagnostic"
        status: pass
    human_judgment: false
  - id: D3
    description: "SDK-free build identity behavior is deterministic"
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "tests/build_identity_tests.cpp::astrabot_build_identity"
        status: pass
    human_judgment: false
## Accomplishments

- Added a CMake foundation with required C++14 settings, target-scoped warnings, and a configure-time 32-bit pointer-size guard.
- Added `astrabot_core` and an SDK-free build identity contract exposing pointer width, C++ standard, architecture, and version.
- Verified the same build identity CTest on Windows x86 Debug and Debian WSL Linux x86 Debug; verified x64 rejection.

## Task Commits

1. **Task 1-5: Foundation build, test, verification, and commit** - `eb8641e` (`build: establish x86 CMake foundation`)

## Files Created/Modified

- `CMakeLists.txt` - Project options, x86 guard, portable Core/test target wiring.
- `cmake/AstraBotBuild.cmake` - Target-scoped compiler warning helper.
- `include/astrabot/build_identity.hpp` - SDK-free build identity declarations and x86 assertion.
- `src/core/build_identity.cpp` - Build identity implementation.
- `tests/build_identity_tests.cpp` - Deterministic CTest coverage.
- `.gitignore` - Build output exclusions while preserving `.focalspan/`.

## Decisions Made

- Kept the implementation C++14 and free of Metamod/ReGameDLL includes.
- Used separate Windows and Debian WSL x86 build directories so generator/toolchain identities cannot mix.

## Deviations from Plan

### Auto-fixed Issues

**1. [Environment - WSL access] Enabled explicit Debian WSL invocation**

- **Found during:** Task 4 (cross-platform verification)
- **Issue:** Generic and explicit WSL invocation initially returned `Wsl/Service/E_ACCESSDENIED`.
- **Fix:** Re-ran the same verification with the installed Debian distribution under the required elevated WSL permission.
- **Files modified:** None
- **Verification:** Debian WSL reported GCC/G++ 14.2 and CMake 3.31.6; Linux x86 CTest passed.

**2. [Environment - command quoting] Escaped the Unix Makefiles generator for bash**

- **Found during:** Task 4 (Linux configure)
- **Issue:** PowerShell-to-bash quoting split `Unix Makefiles` into separate arguments.
- **Fix:** Passed `-GUnix\\ Makefiles` as one bash argument.
- **Files modified:** None
- **Verification:** Linux x86 configure/build/CTest completed successfully.

---
**Total deviations:** 2 environment-only corrections
**Impact on plan:** No scope change and no source changes beyond the planned foundation.

## Issues Encountered

- Linux verification required elevated WSL service access; it is now verified in Debian WSL.
- The initial RED build emitted localized MSVC diagnostics, but the failure was the intended missing `build_identity.hpp` contract.
- code-review-graph reported five heuristic test gaps because it did not associate the small `assert` test with the four value functions; the direct CTest result is the authoritative check for this slice.

## User Setup Required

None.

## Next Phase Readiness

- Plan 01-02 can consume the x86 CMake foundation and add the pinned Metamod-P SDK boundary.
- FocalSpan indexes the Core symbols and build constraints; graph source relationships are now available for the committed Core files.

---
*Phase: 01-foundation-and-abi*
*Completed: 2026-09-15*
