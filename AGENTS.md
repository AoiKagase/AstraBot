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
- Portable Core/host builds use the x64 environment and the adapter uses the
  GoldSrc-compatible x86 environment. The adapter must use the pinned SDK
  checkout at `H:\sourcecode\003.Game\amxmodx\metamod-p` with SHA
  `7ec9b014f8c0a947a724644aebe34eb33706e44b`.
- Run these commands from the repository root. Keep the portable and adapter
  build directories separate:

```powershell
# Portable x64 configure, build, and Debug tests
rtk powershell -NoProfile -Command '$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"; cmd /c ("call ""$vs"" -arch=x64 -host_arch=x64 && cmake -S . -B build-portable-test -G ""NMake Makefiles"" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=OFF -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON && cmake --build build-portable-test && ctest --test-dir build-portable-test --output-on-failure")'

# Metamod-P x86 configure, build, and Debug tests
rtk powershell -NoProfile -Command '$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"; cmd /c ("call ""$vs"" -arch=x86 -host_arch=x64 && cmake -S . -B build-metamod-x86-test -G ""NMake Makefiles"" -DCMAKE_BUILD_TYPE=Debug -DASTRABOT_BUILD_METAMOD=ON -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON -DASTRABOT_METAMOD_SDK_ROOT=H:\sourcecode\003.Game\amxmodx\metamod-p && cmake --build build-metamod-x86-test && ctest --test-dir build-metamod-x86-test --output-on-failure")'

# Metamod-P x86 Release adapter artifact (tests stay disabled)
rtk powershell -NoProfile -Command '$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"; cmd /c ("call ""$vs"" -arch=x86 -host_arch=x64 && cmake -S . -B build-metamod-x86-release -G ""NMake Makefiles"" -DCMAKE_BUILD_TYPE=Release -DASTRABOT_BUILD_METAMOD=ON -DASTRABOT_BUILD_TESTS=OFF -DASTRABOT_WARNINGS_AS_ERRORS=ON -DASTRABOT_METAMOD_SDK_ROOT=H:\sourcecode\003.Game\amxmodx\metamod-p && cmake --build build-metamod-x86-release")'

# x86 export verification (run in the same VS developer environment)
rtk powershell -NoProfile -Command '$vs = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"; cmd /c ("call ""$vs"" -arch=x86 -host_arch=x64 && dumpbin /exports build-metamod-x86-release\astrabot_mm.dll")'
```

The adapter export check must find exactly these undecorated names:
`Meta_Query`, `Meta_Attach`, `Meta_Detach`, `GetEntityAPI2`, and
`GetEngineFunctions`. Linux `-m32` and live-server validation remain
post-Finish checks and must not be started before the project-wide Finish
decision, which is only made after every project plan is complete.

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

- Linux builds and real-device/live-server validation must not be started
  before `Finish` has been explicitly confirmed for the entire project.
- Determine and record `Finish` only after every plan in every project phase has
  completed its implementation, applicable verification, and required
  documentation evidence. Completing an individual phase or plan never sets
  `Finish`.
- After `Finish` is confirmed, run the Linux build and real-device/live-server
  checks as post-Finish validation, and report their results separately.
- If post-Finish validation fails, record the follow-up or reopened work
  explicitly; do not silently present the phase as fully accepted.

If FocalSpan is unavailable, cannot update, or cannot provide the required
context, report the blocker and do not claim the implementation is complete
without explicit user approval for an exception. Preserve unrelated user
changes. Do not stage `.focalspan/` or `.focalspan.json` unless the user
explicitly requests committing the local FocalSpan index/configuration.
