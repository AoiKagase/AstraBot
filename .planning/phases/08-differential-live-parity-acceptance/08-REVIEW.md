---
phase: 08-differential-live-parity-acceptance
reviewed: 2026-09-17T20:10:00Z
depth: deep
files_reviewed: 2
files_reviewed_list:
  - src/adapter/metamod/plugin_runtime.cpp
  - tests/compat_actor_command_tests.cpp
findings:
  critical: 1
  warning: 0
  info: 0
  total: 1
status: issues_found
---

# Phase 08: Code Review Report

**Reviewed:** 2026-09-17T20:10:00Z  
**Depth:** deep  
**Files Reviewed:** 2  
**Status:** issues_found

## Summary

The main regression path is covered: the test installs a distinct Metamod hook-table callback and verifies two hooked calls with zero calls through the explicitly unhooked attached table. The runtime correctly clears the retained hook-table pointers on detach. One correctness gap remains in partial hook-table handling.

## Critical Issues

### CR-01: Partial hook tables suppress the valid fallback dispatcher

**File:** `src/adapter/metamod/plugin_runtime.cpp:1980-1985`

**Issue:** The selector chooses `hookedGameDllFunctions_` solely because `dllapi_table` is non-null. If Metamod returns a non-null table whose `pfnClientCommand` is null (a partial/unsupported hook table), the subsequent guard returns `false` and never tries the valid `gameDllFunctions_` callback. A FakeClient then cannot complete its bounded menu join even though the attached public GameDLL table can dispatch the command. The test only covers both table pointers being populated or both command slots being null, so this case is untested.

**Fix:** Select the hook-table dispatcher only when its command callback is present; otherwise select the attached table, and retain the existing null checks:

```cpp
gamedll_funcs_t *dispatchFunctions =
	hookedGameDllFunctions_.dllapi_table != nullptr &&
	hookedGameDllFunctions_.dllapi_table->pfnClientCommand != nullptr
		? &hookedGameDllFunctions_
		: gameDllFunctions_;
```

Add a regression case with a non-null hook table and `pfnClientCommand == nullptr`, while the attached table points to the private/public-boundary fallback callback, and assert the fallback is used.

## Overall Assessment

The requested hook-table dispatch behavior is implemented and the principal regression test is meaningful, but the partial-table fallback defect should be fixed and covered before this change ships.

---

_Reviewed: 2026-09-17T20:10:00Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
