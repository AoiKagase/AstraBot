# Agent Instructions

## Confirmed Windows build method

- The confirmed compiler environment is Visual Studio 2026 Community:
  `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat`.
- CMake 3.31.4 in this environment does not provide a Visual Studio 2026
  generator. Do not use the default Visual Studio 2019 generator and do not
  reuse a build directory configured with a different generator.
- Use `NMake Makefiles` inside one `VsDevCmd.bat` environment. NMake is a
  single-configuration generator, so set `-DCMAKE_BUILD_TYPE` at configure
  time; `--config Release` does not select the configuration. The repository's
  assert-based CTest binaries must use `Debug`; Release defines `NDEBUG` and
  turns their `/W4 /WX` unused-variable diagnostics into build failures.
- All targets, including portable Core/host, tools and tests, use the
  GoldSrc-compatible x86 environment. The adapter must use the pinned SDK
  checkout at `H:\sourcecode\003.Game\amxmodx\metamod-p` with SHA
  `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
- Run these commands from the repository root. Keep the portable and adapter
  build directories separate:

All produced binaries are x86. `-host_arch=x64` selects the compiler host only;
`-arch=x86` selects the output target. Never reuse the historical x64 portable
build directories. CMake rejects 64-bit targets, including portable tests/tools.

```powershell
# Portable x86 configure, build, and Debug tests
rtk powershell -NoProfile -Command '$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"; cmd /c ("call ""$vs"" -arch=x86 -host_arch=x64 && cmake -S . -B build-portable-x86-test -G ""NMake Makefiles"" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON && cmake --build build-portable-x86-test && ctest --test-dir build-portable-x86-test --output-on-failure")'

# Metamod-P x86 configure, build, and Debug tests
rtk powershell -NoProfile -Command '$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"; cmd /c ("call ""$vs"" -arch=x86 -host_arch=x64 && cmake -S . -B build-metamod-x86-test -G ""NMake Makefiles"" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=ON -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON -DASTRABOT_METAMOD_SDK_ROOT=H:\sourcecode\003.Game\amxmodx\metamod-p && cmake --build build-metamod-x86-test && ctest --test-dir build-metamod-x86-test --output-on-failure")'

# Metamod-P x86 Release adapter artifact (tests stay disabled)
rtk powershell -NoProfile -Command '$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"; cmd /c ("call ""$vs"" -arch=x86 -host_arch=x64 && cmake -S . -B build-metamod-x86-release -G ""NMake Makefiles"" -DCMAKE_BUILD_TYPE=Release -DASTRABOT_BUILD_METAMOD=ON -DASTRABOT_BUILD_TESTS=OFF -DASTRABOT_WARNINGS_AS_ERRORS=ON -DASTRABOT_METAMOD_SDK_ROOT=H:\sourcecode\003.Game\amxmodx\metamod-p && cmake --build build-metamod-x86-release")'

# x86 export verification (run in the same VS developer environment)
rtk powershell -NoProfile -Command '$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"; cmd /c ("call ""$vs"" -arch=x86 -host_arch=x64 && dumpbin /exports build-metamod-x86-release\astrabot_mm.dll")'
```

The adapter export check must find exactly these undecorated names:
`Meta_Query`, `Meta_Attach`, `Meta_Detach`, `GetEntityAPI2`,
`GetEngineFunctions`, and `GiveFnptrsToDll`. The sixth export is the user-approved
engine bootstrap for Metamod-managed command registration (2026-09-05).

## Linux x86 CI and live validation

- Windows and Linux targets are 32-bit/x86, including portable tools and tests.
- Linux x86 builds and SDK-free portable tests run on every GitHub Actions push
  and pull request, independently of project-wide Finish. Use GCC with `-m32`
  and `gcc-multilib` / `g++-multilib`, Debug, tests ON, Metamod OFF and
  warnings-as-errors ON. CMake's 32-bit target check remains enabled.
- Linux real-device/live HLDS or ReHLDS validation remains post-Finish. Offline
  CI success never establishes live acceptance or declares Finish.

## FocalSpan and completion workflow

FocalSpan is part of the standard workflow for every implementation or source
change in this repository. Do not silently skip it.

Before editing:

1. Confirm that the working directory is the AstraBot repository root.
2. Run `focalspan status --json`.
3. If the index is stale or not ready, run `focalspan update --root .` and
   resolve any error before proceeding.
4. Query the relevant architecture, policy, and existing implementation with
   `focalspan -- "<question>"` before making design or code changes.

After implementation and verification:

1. Run the tests and builds appropriate to the change, and inspect the final
   diff.
2. Run `focalspan update --root .` so the index reflects the implemented
   state. Use a follow-up FocalSpan query when needed to verify the changed
   contract or integration points.
3. Stage only the intended repository changes and run
   `git diff --cached --check`.
4. Commit the implementation and its tests/documentation. A change is not
   complete until the commit succeeds.
5. Verify the commit with `git log -1 --oneline` and `git status --short`, then
   report the commit hash, verification results, and any remaining acceptance
   work.

## Finish gate and post-Finish validation

Treat `Finish` as an explicit project-wide state, not as a synonym for a phase
completion or every possible platform or live-environment check.

- Linux x86 build/portable-test CI runs continuously before and after Finish.
  Real-device/live-server validation, including Linux HLDS/ReHLDS, must not be
  started before `Finish` has been explicitly confirmed for the entire project.
- Determine and record `Finish` only after every plan in every project phase has
  completed its implementation, applicable verification, and required
  documentation evidence. Completing an individual phase or plan never sets
  `Finish`.
- After `Finish` is confirmed, run real-device/live-server checks as post-Finish
  validation and report their results separately from offline build/test CI.
- If post-Finish validation fails, record the follow-up or reopened work
  explicitly; do not silently present the phase as fully accepted.

If FocalSpan is unavailable, cannot update, or cannot provide the required
context, report the blocker and do not claim the implementation is complete
without explicit user approval for an exception. Preserve unrelated user
changes. Do not stage `.focalspan/` or `.focalspan.json` unless the user
explicitly requests committing the local FocalSpan index/configuration.

## Verification reuse policy

- Use focused tests while implementing. Before task completion, run the
  canonical full verification once using `tools/verify-canonical.ps1 -Profile All`.
- Reuse a passed result only when the content fingerprint and the platform,
  toolchain, dependency, and build identities match. The result must record
  the verified `HEAD` and commit tree; the commit/tree names themselves are
  not the cache key.
- A commit, merge, branch deletion, or branch switch alone never requires a
  full rerun. Rerun after conflict resolution, rebase/cherry-pick content
  changes, CMake/build configuration changes, dependency or platform/toolchain
  changes, or test/test-runner changes.
- Docs-only edits receive the smallest relevant documentation check. Do not
  repeat a full build/test when build inputs are unchanged.
- For CI, reuse only an exact successful same-SHA marker; never use a broad
  cache fallback. See `docs/testing-workflow.md` for the record format and
  workflow details.

## Efficient focused verification and failure triage

Long verification is a phase gate, not an implementation loop. Select the
smallest check that can falsify the change, and do not start `All` after every
edit or after every failed command.

### Select the check from the change

1. Inspect the scope first with `rtk git status --short` and
   `rtk git diff --name-only`. Classify the change before choosing a build.
2. For documentation, planning, `.gitignore`, or local-index-only changes,
   do not run CMake or CTest. Use the relevant link/reference or documentation
   check, `rtk git diff --check`, and the required FocalSpan check.
3. For one Core source/header or one Core test, reuse the matching portable
   x86 Debug directory and build only the affected target. Run only the exact
   registered CTest case with `-R` and `--output-on-failure`.
4. For adapter, Metamod host, or adapter-test changes, use the matching
   Metamod x86 Debug directory and exact adapter test. Add the Release/export
   check only when the adapter artifact, export surface, or Release-only
   configuration is affected.
5. For CMake files, test registration, compiler flags, dependency inputs,
   architecture, generator, or tool-runner changes, reconfigure the affected
   profile before testing. If the build identity no longer matches, use a new
   explicitly named directory; never reuse a directory configured with a
   different generator, architecture, build type, SDK, or dependency.
6. Run `tools/verify-canonical.ps1 -Profile All` only after the phase's
   implementation and focused checks are ready for the phase gate. The script
   may skip profiles through the exact verification cache; a commit, merge,
   branch switch, or branch deletion alone is not a reason to rerun a passed
   profile.

During implementation, discover test names without running them:

```powershell
ctest --test-dir build-portable-x86-test -N
ctest --test-dir build-metamod-x86-test -N
```

Then rebuild and run only the affected target/case. Replace the placeholders
with the actual target and registered CTest name from `ctest -N`:

```powershell
cmake --build build-portable-x86-test --target <affected-target>
ctest --test-dir build-portable-x86-test -R '^<exact-test-name>$' --output-on-failure
```

Use the corresponding `build-metamod-x86-test` directory for adapter work.
Do not use an unfiltered `ctest --output-on-failure` during implementation;
the unfiltered suite belongs to the canonical phase gate.

### Triage the first failure before rerunning

Treat the first failing command as evidence. Preserve its command, profile,
exit code, and first diagnostic, then classify it before rerunning anything:

- **Configure failure:** check the active `VsDevCmd.bat`, x86 target
  selection, generator, `CMAKE_BUILD_TYPE`, SDK path/SHA, and build options.
  Correct the mismatch and reconfigure only that profile. Use a fresh build
  directory when its identity changed; do not hide the cause by retrying the
  full matrix.
- **Compile or link failure:** fix the reported source/target and rerun only
  `cmake --build <matching-build-dir> --target <affected-target>` (add
  `--verbose` when the command line is needed). Do not rebuild unrelated
  profiles first.
- **One CTest failure:** run only that exact test with `-R '^name$'` and
  `--output-on-failure`, inspect the failure and its fixture, then change the
  code or test before repeating it. A second identical failure is not useful
  evidence by itself.
- **Export failure:** run the Release `dumpbin /exports` check and compare the
  six required undecorated names. Do not run the Debug CTest suites to diagnose
  an export-only failure.
- **Environment, SDK, or runner failure:** verify the toolchain, x86
  environment, pinned SDK SHA/cleanliness, and the selected build directory.
  This is an infrastructure correction, not a reason to repeat a long test
  suite unchanged.

If a canonical run fails after a profile has already passed, fix the cause and
rerun only the failed profile when the other profiles' build inputs and
environment identities still match. `tools/verification-cache.ps1` records
the fingerprint, platform/toolchain/dependency identities, verified `HEAD`,
and commit tree; never substitute a result from another branch, tree, or
broader cache fallback. After source, test, CMake, dependency, toolchain, or
runner changes, let the cache decide which profile is invalidated.

<!-- code-review-graph MCP tools -->
## MCP Tools: code-review-graph

**IMPORTANT: This project has a knowledge graph. ALWAYS use the
code-review-graph MCP tools BEFORE using Grep/Glob/Read to explore
the codebase.** The graph is faster, cheaper (fewer tokens), and gives
you structural context (callers, dependents, test coverage) that file
scanning cannot.

### When to use graph tools FIRST

- **Exploring code**: `semantic_search_nodes_tool` or `query_graph_tool` instead of Grep
- **Understanding impact**: `get_impact_radius_tool` instead of manually tracing imports
- **Code review**: `detect_changes_tool` + `get_review_context_tool` instead of reading entire files
- **Finding relationships**: `query_graph_tool` with callers_of/callees_of/imports_of/tests_for
- **Architecture questions**: `get_architecture_overview_tool` + `list_communities_tool`

Fall back to Grep/Glob/Read **only** when the graph doesn't cover what you need.

### Key Tools

| Tool | Use when |
| ------ | ---------- |
| `detect_changes_tool` | Reviewing code changes — gives risk-scored analysis |
| `get_review_context_tool` | Need source snippets for review — token-efficient |
| `get_impact_radius_tool` | Understanding blast radius of a change |
| `get_affected_flows_tool` | Finding which execution paths are impacted |
| `query_graph_tool` | Tracing callers, callees, imports, tests, dependencies |
| `semantic_search_nodes_tool` | Finding functions/classes by name or keyword |
| `get_architecture_overview_tool` | Understanding high-level codebase structure |
| `refactor_tool` | Planning renames, finding dead code |

### Workflow

1. The graph auto-updates on file changes (via hooks).
2. Use `detect_changes_tool` for code review.
3. Use `get_affected_flows_tool` to understand impact.
4. Use `query_graph_tool` pattern="tests_for" to check coverage.

