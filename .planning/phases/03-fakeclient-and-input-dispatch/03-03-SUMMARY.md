---
phase: 03-fakeclient-and-input-dispatch
plan: 03
subsystem: multi-actor-integration
tags: [multi-actor, fakeclient, receipt, stale-input, native-guard, x86]
requires:
  - plan: 03-02
    provides: generation-safe queue and RunPlayerMove dispatcher
provides:
  - PluginRuntime two-actor fake-host integration evidence
  - external disconnect actor/queue cleanup
  - Phase 3 cross-platform offline verification
affects:
  - phase-4-csbot-compatibility-surface
actuals:
  tasks: 4
  commits: 0
  verification: passed
requirements-completed: [LIFE-02, LIFE-04, TEST-02]
---

# Plan 03-03 Summary

Added a fake public engine/GameDLL host that exercises the complete Phase 3
path through `PluginRuntime`: native controls are suppressed before managed
creation, two FakeClients are created and joined in isolated slots, commands
reach only their bound entities, and receipts reject stale input after a
round reset. Native control re-enable prevents another create before the
engine function is called.

Managed actor disconnect hooks now clear the dispatcher binding and queued
input, retire the ActorRegistry record, and invalidate the LifecycleSession
slot. This also remains safe when manager removal is already in progress.

## Files

- `tests/fake_client_isolation_tests.cpp`
- `src/adapter/metamod/plugin_runtime.cpp`
- `CMakeLists.txt`
- `docs/source-manifest.json`

## Verification

- RED observed before disconnect cleanup: external disconnect left the actor
  record available for a later manager remove.
- Windows x86 Debug: full Metamod CTest passed 10/10 and portable CTest 5/5.
- Debian WSL Linux x86 Debug: full Metamod CTest passed 10/10 and portable
  CTest 5/5.
- Manifest checker passed with 35 entries and 30 C/C++ files.
- PE and ELF artifact verifiers passed with x86 and six exact exports.
- FocalSpan and CRG were refreshed at the current implementation checkpoint.

## Decisions

- Integration tests compile adapter sources directly so internal C++ test
  accessors do not widen the DLL's six-export public ABI.
- External disconnect is authoritative for actor retirement; later manager
  remove returns `NotFound` instead of invoking cleanup twice.
- Offline fake-host evidence is not substituted for live HLDS/ReHLDS
  movement, stability, or CSBot gameplay acceptance.
