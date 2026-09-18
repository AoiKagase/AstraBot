# Agent Instructions

## code-review-graph

Use the code-review-graph MCP before making architecture, dependency, impact, or review decisions.

- For codebase exploration, start with `get_architecture_overview` and `list_communities` when available.
- For symbol and relationship discovery, prefer `semantic_search_nodes` and `query_graph` over manual repository-wide scanning.
- For impact analysis, use `get_impact_radius` and `get_affected_flows` before changing shared contracts.
- For source review, use `detect_changes` and `get_review_context`, then use `query_graph` with `tests_for` to check coverage.
- Fall back to `rg`, targeted file reads, and compiler diagnostics only when the graph does not cover the question. State the fallback and its limitation.
- Do not treat graph indexes as current-status proof. Confirm important conclusions against the current source, build/test identity, and runtime evidence.

## FocalSpan

Use FocalSpan for every implementation, test, build-system, or architecture-impacting documentation change.

Before editing:

1. Confirm the working directory is the AstraBot repository root.
2. Run `focalspan status --json`.
3. If the index is stale or unavailable, run `focalspan update --root .` and resolve the reported issue before continuing when possible.
4. Query the relevant architecture, policy, and existing implementation with `focalspan -- "<question>"` before changing a contract.

After editing:

1. Run the smallest relevant build, test, or documentation check.
2. Run `focalspan update --root .` so the index represents the current tree.
3. Run a follow-up FocalSpan query when the change affects a contract or integration point.
4. Inspect the final diff and run `git diff --cached --check` after staging.

If FocalSpan is unavailable or cannot provide the required context, report that limitation and do not claim graph-backed or FocalSpan-backed verification. Do not stage `.focalspan/` or `.focalspan.json` unless the user explicitly requests those local index/configuration files to be committed.

## Change hygiene

- Preserve unrelated edits, untracked files, worktrees, configuration, and generated evidence.
- Stage explicit paths only; never use broad staging for a focused task.
- Keep implementation and offline verification distinct from live HLDS/ReHLDS or real-device acceptance.

## Windows x86 / NMake toolchain recovery

The Windows x86 CMake caches use the NMake generator and an MSVC HostX86/x86 compiler. Build and test commands must run inside the same Visual Studio developer environment that initialized the cache. On this machine, initialize it with:

```cmd
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x86
```

Then, in that same `cmd.exe` process, verify `where cl` resolves to the cached `Hostx86\x86\cl.exe` and build the complete target set before running CTest:

```cmd
cmake --build build-action-adapter-x86-1451 --config Debug
ctest --test-dir build-action-adapter-x86-1451 -C Debug --output-on-failure
```

If MSVC reports `fatal error C1083: 'windows.h': No such file or directory`, treat it as an uninitialized or mismatched developer environment first. Do not add ad-hoc Windows SDK include paths or change the CMake cache. Reopen/reinitialize the developer command environment, confirm the compiler architecture against `CMakeCache.txt`, and rebuild the affected target. A focused target build can leave other CTest executables absent; a full CTest run will then report those cases as `Not Run`, not as source regressions. Build the complete directory before interpreting the full CTest result.

The PE verifier is a separate gate. If `py -3` cannot create its Python process, record the verifier as environment-unverified and do not replace that result with an offline CTest claim.
