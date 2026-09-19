# CSBot RNG Consumption Parity Implementation Plan

> For agentic workers: REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

Goal: Add a global-engine-backed Compatibility RNG boundary, strict scripted tape, bounded trace metadata, reference RNG inventory, and offline call-consumption evidence for P03.

Architecture: Keep the Core SDK-free. The Core owns the value-only RNG contract and scripted source; the Metamod adapter owns the engine callback adapter and one shared production source in PluginRuntime. Trace metadata is optional and supplied at the exact callsite without changing BotTimingScheduler.

Tech Stack: C++14, CMake/NMake x86, existing standalone CTest executables, Metamod/HLSDK engine function table only in the adapter, Markdown parity artifacts, PowerShell/RGK targeted source checks.

Spec: docs/superpowers/specs/2026-09-19-csbot-rng-consumption-parity-design.md

## Global Constraints

- Work from baseline 71cf00aaa9858855f0ebcff1df6544eaab3c7d4b; preserve all pre-existing dirty files.
- Use ReGameDLL-CS reference commit b0889847fe6d03898be88acc9e366660efb40ab5 as observation-only source.
- Do not copy ReGameDLL implementation code into AstraBot.
- Do not add std::mt19937, rand(), xorshift, PCG, a custom seed policy, or per-Bot Compatibility RNG state.
- Production calls must delegate to pfnRandomFloat and pfnRandomLong with unchanged argument type and bounds.
- Keep the P02 timing scheduler, 30Hz/10Hz cadence, deadline rebasing, no-catch-up behavior, command persistence, and msec conversion unchanged.
- Keep Core free of edict_t, Vector, enginefuncs_t, and GameDLL-private types.
- Use semantic callsite IDs; line numbers are auxiliary inventory data only.
- Stage explicit P03 paths only; never stage .focalspan/, .focalspan.json, .serena/, binaries, databases, archives, or unrelated files.
- Report offline CTest, artifact/Python verification, and live HLDS/ReHLDS acceptance as separate gates.

## Review Focus

1. A false branch must not consume a tape entry; pinned by compatibility_rng_tests.cpp::testConditionalConsumption.
2. A true branch and multi-call fixture must consume exactly the reference order and bounds; pinned by testAimOffsetFixture.
3. Bot A/B calls must share one monotonically ordered source, not per-Bot tapes; pinned by testSharedOrdering.
4. Enhanced calls must not advance Compatibility position; pinned by testEnhancedIsolation.
5. Missing engine callbacks and tape exhaustion/mismatch must fail closed and be observable; pinned by testTapeExhaustion, testTapeMismatch, and engine_random_source_tests.cpp::testUnavailableCallbacks.

---

### Task 1: Define the SDK-free contract and red tests

Files:

- Create: include/astrabot/compat/random_trace.hpp
- Create: include/astrabot/compat/random_source.hpp
- Create: tests/compatibility_rng_tests.cpp
- Modify: CMakeLists.txt in the astrabot_core source list and test section

Interfaces:

- Produce astrabot::compat::RandomType, RandomStatus, RandomActor, RandomTimingContext, RandomRequest, RandomTraceRecord, and IRandomTraceSink.
- Produce astrabot::compat::ICompatibilityRandomSource with separate nextFloat(const RandomRequest&) and nextLong(const RandomRequest&) operations.
- Produce astrabot::compat::RandomFloatResult and RandomLongResult; both carry a RandomStatus and a value.
- Produce astrabot::compat::RandomTapeEntry and ScriptedRandomSource declarations with verifyComplete(), failed(), position(), and optional trace sink registration.
- RandomRequest::floatRequest(site, actor, timing, lower, upper) and RandomRequest::longRequest(site, actor, timing, lower, upper) must populate exact type-specific bounds without calling a generator.

- [ ] Step 1: Write the failing core test first.

Add named test functions with real assertions and a main that returns nonzero on failure:

~~~cpp
bool testFloatForwardingAndTrace();
bool testLongForwardingAndTrace();
bool testScriptedSequence();
bool testTapeExhaustion();
bool testTapeMismatch();
bool testConditionalConsumption();
bool testAimOffsetFixture();
bool testSharedOrdering();
bool testEnhancedIsolation();
bool testModeBaselineEquivalence();
bool testNoDirectCompatibilityBypass();
~~~

The first test must construct a tape entry with FLOAT(-10.0f, 10.0f) -> 3.5f, call nextFloat, and assert status == RandomStatus::Ok, result 3.5f, exact bounds, semantic site, actor, and timing metadata in the emitted trace. The mismatch test must call a tape entry with a different upper bound and assert a non-OK status and failed() == true. The aim fixture must consume three offset floats followed by a timestamp float and assert the resulting four values and tape position.

- [ ] Step 2: Register only the red core target.

Add an astrabot_compatibility_rng executable linked to astrabot_core, include the project include directory, apply existing warnings, and register it with CTest. Add src/core/compat/random_source.cpp to astrabot_core only after the test has been written; initially leave that implementation absent so the target fails for the intended missing implementation.

- [ ] Step 3: Run the focused target and verify RED.

Run:

~~~text
cmake --build build-action-adapter-x86-1451 --target astrabot_compatibility_rng --config Debug
~~~

Expected result: compilation or link failure because the declared scripted source implementation is not present. If the failure is a CMake typo or malformed test rather than the missing implementation, correct the test/target and repeat until the failure is meaningful.

- [ ] Step 4: Inspect the red diff.

Run git diff --check on the explicit new/test paths and confirm git status --short still shows all pre-existing dirty files unchanged.

---

### Task 2: Implement the scripted Compatibility source and trace

Files:

- Create: src/core/compat/random_source.cpp
- Modify: CMakeLists.txt to compile the new Core source
- Modify: tests/compatibility_rng_tests.cpp only when a failing assertion identifies a test defect

Interfaces:

- ScriptedRandomSource::nextFloat accepts only a Float request and nextLong accepts only a Long request.
- Exact type, bounds, and optional semantic site must match the current tape entry.
- A request beyond the tape records RandomStatus::TapeExhausted; a type/bounds/site mismatch records RandomStatus::RequestMismatch.
- verifyComplete() returns true only when the source has no failure and every tape entry was consumed.
- Accepted calls increment one source-global sequence and notify the optional sink exactly once. Rejected calls do not fabricate a successful result or advance the accepted-call sequence.

- [ ] Step 1: Add the smallest implementation for tape cursor and status.

Implement the constructor, entry factories, cursor accessors, and request comparison in src/core/compat/random_source.cpp. Keep the source global to its instance and do not add any seed or generator member.

- [ ] Step 2: Run the core target and verify GREEN for the implemented slice.

Run:

~~~text
cmake --build build-action-adapter-x86-1451 --target astrabot_compatibility_rng --config Debug
ctest --test-dir build-action-adapter-x86-1451 -C Debug -R astrabot_compatibility_rng --output-on-failure
~~~

Expected result: the complete scripted/tape, trace, branch, aim-fixture, shared-order, enhanced-isolation, and bypass tests pass. If a test fails, fix the implementation rather than weakening the assertion.

- [ ] Step 3: Refactor only after green.

Remove duplicated request comparison and trace construction through small private helpers. Keep all tests green and confirm that no helper evaluates a random request before its caller reaches the branch.

- [ ] Step 4: Inspect Core boundaries.

Run:

~~~text
rtk rg -n 'edict_t|enginefuncs_t|Vector|pfnRandom|RANDOM_|rand\\(|std::random' include/astrabot/compat src/core/compat
~~~

Expected result: no SDK type, engine callback, direct reference macro, or independent generator in the Core Compatibility implementation.

---

### Task 3: Add the engine adapter and one shared production owner

Files:

- Create: src/adapter/metamod/engine_random_source.hpp
- Create: src/adapter/metamod/engine_random_source.cpp
- Create: tests/engine_random_source_tests.cpp
- Modify: CMakeLists.txt to compile the adapter into astrabot_mm and register the focused adapter test
- Modify: src/adapter/metamod/plugin_runtime.hpp
- Modify: src/adapter/metamod/plugin_runtime.cpp

Interfaces:

- EngineRandomCallbacks contains only typed function pointers compatible with the public pfnRandomLong and pfnRandomFloat signatures.
- EngineRandomSource::configure(const EngineRandomCallbacks&), available() const, nextFloat(const RandomRequest&), and nextLong(const RandomRequest&) implement ICompatibilityRandomSource.
- PluginRuntime owns one EngineRandomSource member and configures it from the engine table in giveEnginePointers; no per-slot source is added.
- The adapter emits an accepted trace record only after the engine callback returns. A missing callback returns RandomStatus::Unavailable and does not call through a null pointer.

- [ ] Step 1: Write the failing adapter tests.

Add fake C-compatible functions that record call count and exact bounds, then test:

~~~cpp
bool testEngineFloatForwardsExactBounds();
bool testEngineLongForwardsExactBounds();
bool testUnavailableCallbacks();
~~~

Assert one callback invocation per accepted request, unchanged -12.5f/12.5f and 0/1 bounds, returned values, and unavailable status when either function pointer is null.

- [ ] Step 2: Register and run the adapter target to verify RED.

Register astrabot_engine_random_source with the adapter test source and run:

~~~text
cmake --build build-action-adapter-x86-1451 --target astrabot_engine_random_source --config Debug
~~~

Expected result: failure because EngineRandomSource has not yet been implemented.

- [ ] Step 3: Implement the minimal forwarding adapter.

Implement the typed callback storage and exact one-call forwarding. Do not reproduce the ReHLDS generator, seed, or distribution algorithm in AstraBot.

- [ ] Step 4: Integrate the shared owner without adding a gameplay callsite.

Add the source member to PluginRuntime. In giveEnginePointers, copy only the two callback addresses into EngineRandomCallbacks and call configure. Leave all managed Bot behavior and P02 scheduler code unchanged; P03 establishes the production boundary but does not invent a combat/navigation callsite.

- [ ] Step 5: Run the adapter tests and existing adapter regression tests.

Run:

~~~text
cmake --build build-action-adapter-x86-1451 --target astrabot_engine_random_source --config Debug
ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_(engine_random_source|compatibility_rng|runtime_timing|compat_actor_command)" --output-on-failure
~~~

Expected result: all selected tests pass. Confirm the plugin source still contains no direct RANDOM_FLOAT, RANDOM_LONG, or rand() call.

---

### Task 4: Record the reference inventory and parity trace contract

Files:

- Create: docs/parity/RNG_MODEL.md
- Modify: docs/parity/TRACE_SCHEMA.md
- Modify: docs/parity/SOURCE_MAP.md
- Modify: docs/parity/PARITY_MATRIX.md
- Modify: docs/parity/KNOWN_DEVIATIONS.md
- Modify: docs/parity/STATUS.md
- Modify: docs/parity/TIMING_MODEL.md only for a cross-reference if required

Interfaces/evidence:

- Inventory every executable direct CSBot-related RNG expression from the pinned reference with semantic ID, function, type, exact bounds, branch condition, purpose, scope, influence, classification, order constraints, AstraBot equivalent, and status.
- Keep the 148-call total split as 111 FLOAT and 37 LONG after excluding the two preprocessor-guard matches.
- Record the 24 direct reference files and the broader transitive utility audit separately so unrelated engine random users are not falsely promoted to CSBot callsites.
- Record the callback chain RANDOM_* -> g_engfuncs.pfnRandom* and the local ReHLDS implementation observation with its source SHA, while distinguishing it from live runtime proof.
- Add the rng trace record fields to TRACE_SCHEMA.md without changing P02 timing semantics.
- Mark the Compatibility RNG boundary and engine delegation as implemented/offline-verified only; keep individual callsites UNVERIFIED unless a fixture proves branch, type, bounds, and order.

- [ ] Step 1: Re-run the pinned reference audit before editing the inventory.

Run targeted git -C H:\sourcecode\003.Game\amxmodx\ReGameDLL_CS queries for RANDOM_FLOAT, RANDOM_LONG, UTIL_SharedRandom*, profile, manager, nav, weapon, combat, objective, chatter, hiding, and state files. Confirm the reference worktree remains read-only for this task and use the pinned commit for all claims.

- [ ] Step 2: Add the machine-readable-oriented RNG table.

Create semantic IDs such as RNG-CSBOT-WEAPON-AIM-X and RNG-CSBOT-MANAGER-DEFENSE-RUSH, not line-number IDs. Use UNKNOWN when the call's compatibility influence cannot be established. Do not mark all rows MATCH merely because the source interface exists.

- [ ] Step 3: Update trace and parity documents.

Add the exact fields, offline evidence limits, current AstraBot boundary status, remaining unmapped callsites, global ownership, and P02 timing cross-reference. Keep live acceptance and Python/PE environment gaps explicit.

- [ ] Step 4: Review documentation consistency.

Run:

~~~text
rtk rg -n -i 'RNG|random|timing|MATCH|MISSING|UNVERIFIED|PARTIAL' docs/parity/RNG_MODEL.md docs/parity/TRACE_SCHEMA.md docs/parity/SOURCE_MAP.md docs/parity/PARITY_MATRIX.md docs/parity/KNOWN_DEVIATIONS.md docs/parity/STATUS.md docs/parity/TIMING_MODEL.md
git diff --check -- docs/parity
~~~

Expected result: all status claims agree, no P02 cadence wording changes, and no whitespace errors.

---

### Task 5: Full validation and scoped P03 handoff

Files:

- Modify only the P03 source, test, CMake, and parity-document paths listed in Tasks 1–4.
- Do not stage existing dirty files, generated indexes, binaries, databases, archives, or unrelated source changes.

- [ ] Step 1: Reconfigure in the matching x86 developer environment.

Run in one cmd.exe process:

~~~cmd
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x86
where cl
cmake -S . -B build-action-adapter-x86-1451 -G "NMake Makefiles" -DASTRABOT_BUILD_METAMOD=ON -DASTRABOT_BUILD_TESTS=ON -DASTRABOT_WARNINGS_AS_ERRORS=ON
~~~

Confirm where cl resolves to the cached HostX86/x86 compiler and do not add ad-hoc SDK include paths.

- [ ] Step 2: Build all targets and run focused tests.

Run:

~~~cmd
cmake --build build-action-adapter-x86-1451 --config Debug
ctest --test-dir build-action-adapter-x86-1451 -C Debug -R "astrabot_(compatibility_rng|engine_random_source)" --output-on-failure
~~~

Record exact test counts and failures.

- [ ] Step 3: Run the complete regression suite.

Run:

~~~cmd
ctest --test-dir build-action-adapter-x86-1451 -C Debug --output-on-failure
~~~

The P02 baseline was 44/44; added RNG tests may increase the total. No existing test may be removed, disabled, or skipped.

- [ ] Step 4: Run Phase 8 PowerShell checks without claiming live parity.

Run the existing Phase 8 scripts from the repository and record each exit code/result. Report live HLDS/ReHLDS acceptance as NOT RUN unless a fresh, identity-pinned server observation actually occurs.

- [ ] Step 5: Refresh and query FocalSpan.

Run:

~~~text
rtk focalspan update --root .
rtk focalspan status --json
rtk focalspan -- "Compatibility RNG source, EngineRandomSource, ScriptedRandomSource, RNG trace, PluginRuntime ownership, and P02 timing"
~~~

Require index_fresh=true. Do not stage .focalspan/ or .focalspan.json.

- [ ] Step 6: Inspect the final explicit diff and staged gate.

Before staging, run:

~~~text
git status --short
git diff --stat
git diff --check
~~~

Stage only the listed P03 paths, then run:

~~~text
git diff --cached --name-status
git diff --cached --check
git diff --cached --stat
~~~

Confirm every pre-existing dirty path remains unstaged.

- [ ] Step 7: Commit only after all gates are evidenced.

Use a concise message such as:

~~~text
feat: establish compatibility RNG parity boundary
~~~

Verify the resulting full commit SHA, final dirty paths, focused tests, full CTest count, Phase 8 result, FocalSpan status, Python/PE status, and live acceptance status. Stop after P03; do not begin P04.

## Expected final report

Use the requested P03 format and include:

- Result PASS, PARTIAL, or BLOCKED;
- AstraBot start and end SHA;
- reference RNG implementation, ownership, seed/state, production API;
- total inventory and category counts;
- Compatibility, production, tape, Enhanced-isolation, and trace architecture;
- verified, unverified, and differing callsites;
- every added test and behavioral fixture result;
- configure/build/focused/full CTest/Phase 8/Python/FocalSpan/live gates;
- explicit statement that P02 timing is unchanged;
- only evidenced parity-matrix status changes;
- known deviations and remaining RNG gaps;
- changed files, commit SHA/message, remaining dirty files, and P04 readiness.
