---
phase: 4
verified: 2026-09-15
status: passed
requirements:
  - COMP-01
  - COMP-02
  - COMP-03
  - TEST-02
---

# Phase 4 Verification

## Must-haves

| Must-have | Evidence | Status |
|---|---|---|
| `bot_*` commands/CVars have explicit contracts | SDK-free command registry, CVar state, and BotConfiguration tests | PASS |
| Profile input is bounded, read-only, and transactional | Fixed-buffer ProfileLoader tests for malformed, duplicate, size, count, and unchanged bytes | PASS |
| Team/difficulty selection is deterministic | ProfileCatalog exact-team priority, fallback, and selection-index tests | PASS |
| Actor commands preserve quota and ownership safety | Fake host actor-command tests for disable/stop/quota/native conflict/name/slot/stale handle paths | PASS |
| Native CSBot ownership is not mixed | Native guard and actor command tests | PASS |
| Release ABI remains stable | Windows PE and Linux ELF x86 checks with six exact exports | PASS |

## Automated evidence

- Windows x86 portable CTest: 10/10 passed.
- Windows x86 Metamod CTest: 17/17 passed.
- Debian WSL Linux x86 portable CTest: 10/10 passed.
- Debian WSL Linux x86 Metamod CTest: 17/17 passed.
- Python manifest/artifact tests: 6/6 passed.
- Source manifest: 54 entries and 49 C/C++ files.
- C/C++ source format: LF, UTF-8 without BOM, no trailing whitespace, and
  final newline for all 49 C/C++ files.
- FocalSpan status is fresh; CRG full graph is HEAD-matched and includes the
  current staged compatibility source.

## Acceptance boundary

Phase 4 is complete at the offline compatibility-contract layer. Real
HLDS/ReHLDS server acceptance remains TEST-04, and Nav loading, locomotion,
perception, combat, objectives, and complete CSBot parity remain later phases.
