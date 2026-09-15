---
phase: 02-metamod-lifecycle-and-native-guard
verified: 2026-09-15T10:07:08.975Z
status: passed
score: 6/6
covered_files:
  - .planning/phases/02-metamod-lifecycle-and-native-guard/02-CONTEXT.md
  - .planning/phases/02-metamod-lifecycle-and-native-guard/02-01-PLAN.md
  - .planning/phases/02-metamod-lifecycle-and-native-guard/02-01-SUMMARY.md
  - .planning/phases/02-metamod-lifecycle-and-native-guard/02-02-PLAN.md
  - .planning/phases/02-metamod-lifecycle-and-native-guard/02-02-SUMMARY.md
  - .planning/phases/02-metamod-lifecycle-and-native-guard/02-03-PLAN.md
  - .planning/phases/02-metamod-lifecycle-and-native-guard/02-03-SUMMARY.md
  - .planning/REQUIREMENTS.md
  - CMakeLists.txt
  - docs/source-manifest.json
  - include/astrabot/metamod/abi_contract.hpp
  - include/astrabot/metamod/native_bot_guard.hpp
  - include/astrabot/runtime/lifecycle.hpp
  - src/adapter/metamod/native_bot_guard.cpp
  - src/adapter/metamod/plugin_exports.cpp
  - src/adapter/metamod/plugin_runtime.cpp
  - src/adapter/metamod/plugin_runtime.hpp
  - src/core/runtime/lifecycle.cpp
  - tests/lifecycle_generation_tests.cpp
  - tests/metamod_hook_table_tests.cpp
  - tests/native_bot_guard_tests.cpp
covered_digest: "v1:sha256:954850d5e55d615cb45820584910833338bc9c7dde44d7e5841353c49e862227"
behavior_unverified: 0
behavior_unverified_items: []
coincidental_reliance_items: []
---

# Phase 2 Verification Report

## Phase goal

The plugin exposes a safe Metamod-P lifecycle boundary, owns deterministic
map/round/slot state, and prevents silent native CSBot ownership mixing at
the public engine boundary.

## Must-haves

| Must-have | Status | Evidence |
|---|---|---|
| Valid Meta attach publishes lifecycle providers and invalid inputs fail closed | PASS | `astrabot_metamod_hooks`, Windows/Linux x86 |
| Five public DLL lifecycle hooks are populated and safe after detach | PASS | `metamod_hook_table_tests.cpp` |
| Core generations invalidate map/round/slot tokens | PASS | `astrabot_lifecycle_generation`, Windows/Linux x86 |
| Native controls and unmanaged FakeClients fail closed | PASS | `astrabot_native_bot_guard`, adapter fixture |
| Native command registration is selectively superceded | PASS | `metamod_hook_table_tests.cpp` |
| Actual artifacts preserve x86 and exact six-export ABI | PASS | PE/ELF verifier |

## Automated evidence

- Windows x86 Debug Metamod build: succeeded.
- Windows x86 Debug portable build: succeeded.
- Windows x86 CTest: 5/5 passed.
- Debian WSL Linux x86 Debug Metamod build: succeeded.
- Debian WSL Linux x86 Debug portable build: succeeded.
- Debian WSL Linux x86 CTest: 5/5 passed.
- `python tools/check_source_manifest.py --root . --manifest docs/source-manifest.json`:
  passed (`20 entries, 15 C/C++ files`).
- PE artifact: passed (`x86, 6 exact exports`).
- ELF artifact: passed (`x86, 6 exact exports`).
- FocalSpan AstraBot status: `ready=true`, fresh after the final source
  update; dependency indexes for ReGameDLL-CS and Metamod-P are also fresh.
- CRG current HEAD: `b39fa9e` graph match was confirmed before final
  staging; direct source/build evidence is authoritative for behavior.

## Source and scope checks

- No ReGameDLL-CS or Metamod-P source was copied or modified.
- No private GameDLL symbol, ReAPI dependency, or DLL patch was added.
- No Nav read/write path or AstraNav authoring behavior was added.
- C/C++ changes use the required x86 ABI, C++14, real-tab indentation,
  explicit bounds, and platform-specific export handling.

## Acceptance boundary

Phase 2 is verified for offline Core/adapter/artifact behavior. Real-server
HLDS/ReHLDS loading, native spawn race checks, and gameplay parity remain
later live acceptance gates and are not reported as passed here.
