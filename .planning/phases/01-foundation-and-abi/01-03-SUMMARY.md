---
phase: 01-foundation-and-abi
plan: 03
subsystem: verification
tags: [provenance, source-manifest, python, pe, elf, x86]
requires:
  - phase: 01-foundation-and-abi
    provides: x86 CMake foundation and Metamod ABI artifact
provides:
  - Versioned source-origin manifest with pinned reference SHAs
  - Cross-platform source manifest checker
  - PE/ELF x86 exact-export verifier
affects: [phase-2-metamod-lifecycle, phase-8-live-parity]
actuals:
  tokens: 3100
  tasks: 5
  commits: 1
tech-stack:
  added: [Python standard library, dumpbin, readelf, nm]
  patterns: [source coverage validation, magic/header validation, exact export-set validation]
key-files:
  created:
    - docs/source-manifest.json
    - tests/source_manifest_checker_tests.py
    - tests/x86_artifact_verifier_tests.py
    - tools/check_source_manifest.py
    - tools/verify_x86_artifact.py
  modified:
    - .gitignore
key-decisions:
  - "Reference directories are approved parent-directory inputs and are validated by exact path plus pinned SHA."
  - "The artifact verifier checks the real PE/ELF file rather than trusting CMake architecture settings."
  - "Linux can fall back from file to readelf when the minimal Debian image lacks file."
patterns-established:
  - "Source provenance and license status are explicit per project source entry."
  - "PE and ELF export checks require an exact six-name set, including an undecorated GiveFnptrsToDll."
requirements-completed: [HOST-01, HOST-02, HOST-03, TEST-01]
coverage:
  - id: D1
    description: "Source manifest covers 11 Phase 1 entries and all 6 C/C++ files"
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "source_manifest_checker_tests.py valid/missing-entry/wrong-SHA cases on Windows x86 Python 3.11"
        status: pass
      - kind: unit
        ref: "source_manifest_checker_tests.py valid/missing-entry/wrong-SHA cases on Debian WSL Python 3.13"
        status: pass
    human_judgment: false
  - id: D2
    description: "Windows PE and Linux ELF artifacts are x86 with exact six exports"
    requirement: HOST-01
    verification:
      - kind: integration
        ref: "verify_x86_artifact.py on build-metamod-x86-test/astrabot_mm.dll"
        status: pass
      - kind: integration
        ref: "verify_x86_artifact.py on build-linux-x86-metamod-test/libastrabot_mm.so"
        status: pass
    human_judgment: false
  - id: D3
    description: "Missing artifacts and wrong-format inputs fail closed"
    requirement: HOST-02
    verification:
      - kind: unit
        ref: "x86_artifact_verifier_tests.py missing-artifact/wrong-format cases"
        status: pass
    human_judgment: false
## Accomplishments

- Added the source manifest with exact ReGameDLL-CS and Metamod-P reference commits and explicit independent-origin entries.
- Added deterministic standard-library tooling for source coverage, architecture, and exact PE/ELF export verification.
- Verified all six provenance/artifact tests plus direct manifest and PE/ELF CLI checks on Windows/Linux x86.

## Task Commits

1. **Task 1-5: Provenance manifest, RED tests, verifier implementation, GREEN verification, and commit** - `b1928b6` (`chore: add Phase 1 provenance checks`)

## Files Created/Modified

- `docs/source-manifest.json` - Reference pins and Phase 1 source-origin manifest.
- `tests/source_manifest_checker_tests.py` - Manifest acceptance and rejection cases.
- `tests/x86_artifact_verifier_tests.py` - PE/ELF artifact and negative cases.
- `tools/check_source_manifest.py` - Standard-library source coverage checker.
- `tools/verify_x86_artifact.py` - PE/ELF x86 and exact export checker.
- `.gitignore` - Python cache exclusion.

## Decisions Made

- Kept reference directories outside the project root but restricted them to exact approved paths and SHAs.
- Used `readelf -h` and `nm -D --defined-only` when Debian did not provide `file`; the verifier still validates actual ELF headers and symbols.
- Used the existing uv-managed Python 3.11 on Windows without installation or download.

## Deviations from Plan

### Auto-fixed Issues

**1. [Test fixture] Targeted a C++ entry for missing-source coverage**

- **Found during:** Task 2 (RED test review)
- **Issue:** The initial fixture removed a Python entry, which did not affect the checker's C/C++ exact-coverage set.
- **Fix:** Changed the fixture to remove `src/core/build_identity.cpp` explicitly.
- **Files modified:** `tests/source_manifest_checker_tests.py`
- **Verification:** The missing-source test fails before the checker and passes after implementation.
- **Committed in:** `b1928b6`

**2. [Tooling environment] Added a readelf fallback for missing file**

- **Found during:** Task 4 (Debian artifact verification)
- **Issue:** `file` is not installed in the Debian image.
- **Fix:** Verify ELF32/Intel 80386 with `readelf -h` and exports with `nm`, falling back to `readelf -Ws` when needed.
- **Files modified:** `tools/verify_x86_artifact.py`
- **Verification:** Linux direct CLI passed for `libastrabot_mm.so`.
- **Committed in:** `b1928b6`

---
**Total deviations:** 2 test/tooling corrections
**Impact on plan:** No runtime dependency or scope change; cross-platform verification is stronger on minimal Linux images.

## Issues Encountered

- Windows Store Python aliases could not start; the existing uv-managed Python 3.11 ran the Windows tests without installation.
- The code-review graph can index the source tree, but its current changed-file view prioritizes committed files and is not a substitute for the direct verifier results.
- Live server/plugin acceptance is not proven by this phase and remains Phase 2/Phase 8 work.

## User Setup Required

None.

## Next Phase Readiness

- Phase 1 offline foundation, ABI, provenance, and x86 artifact checks are complete.
- Phase 2 can add lifecycle hooks and native CSBot suppression without changing the six-export contract.
- AstraNav generation/learning/editing and advanced Wallbang remain outside the v1 CSBot parity scope.

---
*Phase: 01-foundation-and-abi*
*Completed: 2026-09-15*
