# Native NAV extent centers implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans inline; one fresh most-capable final review, no implementer delegation.

**Goal:** Repair Compatibility A* center geometry required by native Help travel distance, keeping07 itself active.
**Architecture:** A default-off cost-context flag selects extent midpoint distance/heuristics and zero edge cost; production Compatibility context opts in. Floor/drop/portal geometry stays separate.
**Tech Stack:** SDK-free C++14 Core; public SDK adapter; Windows/Linux x86 CMake/CTest.
**Spec:** docs/superpowers/specs/2026-11-15-nav-reference-centers-design.md

## Global Constraints
SDK-free C++14/publicSDKonly; no native bodies/private data/ReAPI/assets/new exports/services; NAV read-only. Preserve shared checkout/caches/user files/evidence. Explicit9owned paths/localFF, no push/deploy. Full cachedVSHostx86/x86 NMake andWSL builds before serialized suites; independentPE/ELF7/manifest/identities; known22/live25separate. OriginalHelp07unfinished including topology investigation.

## Review Focus
1. Native centers use all three extent midpoint coordinates, while surface/drop/portal consumers retain interpolated floor heights.
2. Real Compatibility context reaches the shared A* consumer; default/Enhanced cannot accidentally opt in or retain a previous mode's geometry.
3. Native zero-distance edges/heuristics retain0; unchanged default minimum policy and corridor validity still work.
4. Safest danger/health/fall/crouch/jump/crowd logic and stable open-list order survive the geometry change.
5. Source/test/artifact identity covers this prerequisite only; no claim that Help delivery, ladder topology or live behavior is complete.

### Task 1: Native traversal center geometry
**Files:** include/astrabot/nav/nav_query.hpp; src/core/nav/nav_query.cpp; src/adapter/metamod/runtime/navigation/bot_navigation_runtime.cpp; tests/nav_query_tests.cpp and statement_queue_runtime_tests.cpp; this spec/plan; docs/parity/STATUS.md and IMPLEMENTATION_BACKLOG.md.
**Interfaces:** NavVector extentCenterOf(const NavArea&); NavPathCostContext.useExtentCenters bool defaultfalse appended; NavQuery signatures unchanged. ProductionmanagedPathCostContext selects flag by RuntimeMode::Compatibility.
- [x] Add realA* Core tests with literal expectedpaths/cost0 and lethal bilinear drop/danger controls. Add only compile-time flag declaration if necessary. Build astrabot_nav_query, run ctest -R '^astrabot_nav_query$': expected semanticRED from missing extent-center geometry before implementation.
- [x] Extend existing registered statement_queue_runtime: real public mode setters and actualPlugin service costcontext drive controlled nonplanar NavQuery routes, restore originalMode withRAII, oldcontrols remain. Build/run thattarget: expected semanticRED onCompatibility path, Enhanced controlpasses.
- [x] Implement extentCenterOf and context-selected A* distance/heuristic/zero policy, opt actualCompatibility producer in. Formatchangedhunks; buildfocusedNavQuery+runtime; ctest -R '^astrabot_(nav_query|nav_legacy_limit|nav_roam_controller|statement_queue_runtime|runtime_ports_contract|runtime_ownership_contract)$' expected allPASS. Onefreshmostcapable read-onlysource review; oneImportantCriticalTDDfixpass ifneeded, nore-review.
- [x] FullWindows/Linux x86 target builds andserializedCTest, separatePE/ELF7/manifest/343input+user4+art2 fingerprints. ExpectedWindowsallPASS/Linuxknown22only; no goldenregeneration/weakenedassertions. Update docs with07remaining/prerequisite identity; explicit9pathcommit, postcommitfocused task-done/localFFmaster/postmergeidentity. Resume original07 afterward, fullgoalactive.

Pre-flight: producer flag and shared A* consumer are one interface; no separate tasks. Tests use real path selection rather than a standalone getter check. Priorquestionoptout/inline/per-unitintegration authority remains; no repeated approval/worktree menus. Existing completed06 evidence is baseline, not07A verification.

Verification receipt: Core2semanticRED0.04s/runtime2semanticRED0.14s before behavior; focused6/6PASS2.11s; review0/0/0. FullWindows135/135PASS11.62s, Linux133/134PASS13.82s(known22only), independent7PE/ELF/manifestPASS.343source/4user/2artifact identities held. Commit/localFF receipt is session RESULT.json. Original07 remains active after this prerequisite.
