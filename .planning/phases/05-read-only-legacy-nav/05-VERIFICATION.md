---
phase: 5
verified: 2026-09-15
status: passed
requirements:
  - NAV-01
  - NAV-02
  - NAV-03
  - NAV-04
  - TEST-02
---

# Phase 5 Verification

## Must-haves

| Must-have | Status | Evidence |
|---|---|---|
| Supported legacy `.nav` v1-v5 files load into the format-neutral model | PASS | `astrabot_legacy_nav_reader` and `astrabot_nav_loader` fixtures |
| Bounds, IDs, geometry, references, and allocations fail closed | PASS | Core corruption fixtures plus bounded adapter reads |
| Missing, corrupt, oversized, map/BSP/hash-mismatched input has explicit diagnostics | PASS | `nav_loader_tests` and `nav_diagnostics_tests` |
| Failed publication cannot replace the previous immutable snapshot | PASS | transactional publisher and adapter failure-preservation assertions |
| Map lifecycle invalidates stale Nav state | PASS | PluginRuntime activation/deactivation integration and lifecycle regression tests |
| No Nav authoring or legacy `.nav` write path was added | PASS | read-only file boundary, source inspection, and unchanged-byte assertions |
| Six-export Metamod ABI remains exact | PASS | Windows PE and Debian Linux ELF x86 verifiers |

## Automated evidence

- Windows x86 portable CTest: 16/16 passed.
- Windows x86 Metamod CTest: 23/23 passed.
- Debian WSL Linux x86 portable CTest: 16/16 passed.
- Debian WSL Linux x86 Metamod CTest: 23/23 passed.
- PE x86 artifact: six exact exports.
- ELF x86 artifact: six exact exports, enforced by the Linux version script.
- Source manifest: 71 entries, 65 C/C++ files.
- Source manifest and artifact Python tests: 6/6 passed.
- C++ diff whitespace, CR, BOM, and SDK-boundary checks passed.
- FocalSpan: fresh after the final Phase 5 tree update.
- CRG: current HEAD `ed16012`, 15 affected lifecycle/loader flows reviewed.

## Acceptance boundary

Phase 5 delivers the offline read-only Nav loading and publication boundary.
It does not claim live HLDS/ReHLDS map loading, locomotion, combat,
multi-Bot stability, or full CSBot/ZBot gameplay parity. Those remain in later
phases and require runtime evidence beyond offline CTest.
