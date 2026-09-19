---
phase: 6
status: passed
verified_at: 2026-09-17
requirements:
  - PAR-01
  - TEST-01
  - TEST-02
---

# Phase 6 Verification: Baseline locomotion

## Verdict

Phase 6 offline locomotion contracts and the ZBot-derived runtime movement
integration slice are verified. This does not prove real-server movement,
physics, stability, or full CSBot parity; those remain Phase 8 acceptance.

## Evidence

| Criterion | Status | Evidence |
|---|---|---|
| Spatial queries, directed links, and bounded path follower | PASS | Phase 6 CTest coverage and 06-01 summary |
| Walk/Crouch/Step/Jump/Drop/Ladder/Door contracts | PASS | Phase 6 CTest coverage and 06-02 through 06-04 summaries |
| Off-mesh recovery and typed movement stages | PASS offline | `astrabot_nav_roam_controller`, movement integration contract |
| Cross-platform/provenance verification harness | PASS offline | `phase6_verification.py`, source manifest, x86 artifact checks |
| Real-server position progress and physics | PENDING | Phase 8 live evidence required |

## Automated evidence

- Windows x86 CTest: `40/40 passed`.
- Phase 6 verification: `9 checks passed`.
- Source manifest: `121 entries, 114 C/C++ files`.
- PE artifact: x86 with 7 exact exports.

## Acceptance boundary

Core intent and adapter dispatch are not movement success. Phase 8 must verify
that a SHA-matching deployed DLL moves a joined Bot through at least two Nav
areas and 128 units within the agreed 10-second window, then repeat the
required lifecycle cases on Windows and Linux x86.
