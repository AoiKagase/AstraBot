# Noise NAV and investigation implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Per-bot NAV-constrained sound positions and CSBot investigation decisions.
**Architecture:** Optional perception resolver+consume port; SDK-free NAV resolver and investigation planner; runtime task/route/command wiring.
**Tech Stack:** C++14, public HLSDK/Metamod, CMake, Windows/Linux x86.
**Spec:** docs/superpowers/specs/2026-11-14-csbot-noise-navigation-design.md

## Global Constraints
Pinned source behavior-only; preserve seven exports, privacy, unrelated edits and frozen baseline. Offline verification is separate from live acceptance.

## Review Focus
- Rejected sounds never draw localization RNG; no-NAV and stale NAV cannot create a destination.
- Stacked/sloped floors and physical travel distance distinct from danger-weighted path cost.
- Target persists after consuming memory; close retargeting, failure, cancellation and new noises.
- Safe-time/cadence/RNG/teammate boundaries and public task precedence.
- Reset on death/round/map/actor/NAV/clock; no extra engine scans without relevant noise.

### Task1: Noise NAV localization
Files: perception.hpp/.cpp/world_snapshot.hpp; compat/noise_navigation.hpp/.cpp; adapter observation and service port; geometry/perception tests and hearing runtime fixtures.
Interfaces: INoisePositionResolver::resolve(sound,position,area,revision); PerceptionInput resolver pointer; PerceptionAssembler::consumeNoise(expected).
- [x] Write/run failing NAV, RNG order and consumption tests.
- [x] Resolve accepted noise per observer; retain area/revision; actual observation owner supplies NAV+RNG.
- [x] Adapt runtime sound fixtures to valid NAV and run focused regressions.

### Task2: Investigation policy and target ownership
Files: compat/noise_investigation.hpp; behavior priorities/objectives/lifecycle; navigation route preference; tests.
Interfaces: NoiseInvestigationPlanner::evaluate(input,rng), target(),active(),cancel(),reset(),shouldWalk(); safe/cadence/travel decision; observation consume port.
- [x] Write/run failing policy/boundary/retarget/reset tests.
- [x] Connect safe/profile/friends/task/visibility, height travel, consume and retained target, safest route, walk and completion.
- [x] Verify actual sound-to-navigation behavior and task/lifecycle guards.

### Task3: Review and delivery
- [x] Fresh read-only independent review; reproduce and fix Important findings.
- [x] Complete x86 builds/CTest, exact exports, contracts, stable source hashes and original profile preservation.
- [x] Parity/evidence update; explicit path commit and authorized local FF merge; no push/deployment.

## Completion ledger
Task1: complete — missing resolver/header RED to NAV/perception GREEN; actual sound fixtures supply valid NAV.
Task2: complete — missing policy/header RED to policy/actual two-area runtime GREEN. Death posture, actual speed, SAFEST query propagation and current-time readiness failures reproduced and corrected.
Task3: complete — independent review stale NAV/clock destination export under attack RED→GREEN; Windows101/101 and Linux99/100 with only the recorded unchanged combat-policy baseline failure. Frozen characterization recovered without baseline changes after excluding objective C4 from friend scans. PE/ELF x86 seven exports and contracts PASS.
Final review: /root/noise_navigation_review, gpt-6-astra high; no unresolved findings or deferred minors.
Final: Ruling: preserve native ComputePath throttle and consumed-no-path exit; fixtures wait for native retry opportunity. Cost if wrong: a valid sound may be consumed without travel; pinned source explicitly specifies that behavior.
Final: Ruling: live acceptance remains pending; offline tests and exact export gates do not establish real server behavior.
