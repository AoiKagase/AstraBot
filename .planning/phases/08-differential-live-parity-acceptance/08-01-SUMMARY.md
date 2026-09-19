---
phase: 8
plan: 01
subsystem: artifact-acceptance
tags: [x86, windows, debian, metamod, exports, ctest, offline, phase8]
requires:
  - plan: 07-06
provides:
  - current-HEAD Windows/Linux x86 artifact matrix
  - PE/ELF exact-six-export evidence
  - cross-platform offline CTest and regression checkpoint
affects:
  - 08-02 differential replay
  - 08-03 live-server acceptance
actuals:
  tasks: 4
  verification: passed
  requirements-completed: []
  requirements-progress: [TEST-01, TEST-04]
production-commit: uncommitted-working-tree-checkpoint
---

# Plan 08-01 Summary

Recorded current-HEAD `5a62a98243ce98081ebd64d24c5495295f53ec58` artifact
evidence in `tests/fixtures/phase8/08-01-artifact-matrix.json`. Windows x86
portable and Metamod builds passed; Debian WSL Linux x86 portable and Metamod
builds passed. The corresponding CTest suites passed 30/30 and 37/37 on each
platform. PE and ELF checks passed x86 architecture and the exact six required
Metamod exports. The artifact SHA-256 values and build identities are recorded.

## Files

- `tests/fixtures/phase8/08-01-artifact-matrix.json`
- `docs/source-manifest.json`
- `.planning/ROADMAP.md`
- `.planning/STATE.md`

## Verification

- Windows portable build: passed.
- Windows Metamod build: passed; CTest 37/37.
- Debian WSL portable build: passed; CTest 30/30.
- Debian WSL Metamod build: passed; CTest 37/37.
- PE/ELF artifact verification: passed, six exact exports each.
- Python artifact/manifest tests: 6/6 passed.
- Phase 6 verification: 9/9 passed.
- Phase 7 scenarios: 7/7 passed.
- Source manifest: 114 entries / 107 C/C++ files.
- FocalSpan: fresh and ready after update.
- CRG: current HEAD matched `5a62a98`; graph output used as navigation evidence.

## Boundary

This closes only the offline artifact checkpoint. It does not prove live
HLDS/ReHLDS operation, differential reference parity, or multi-Bot stability.
PAR-06, TEST-03, and TEST-04 remain pending.
