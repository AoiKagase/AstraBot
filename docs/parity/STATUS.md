# CSBot Parity Status

## P06 result: PARTIAL

| Gate | Result | Evidence / limitation |
|---|---|---|
| Vision boundary | IMPLEMENTED_OFFLINE_VERIFIED | Core stores FOV/LOS/body-region observations; public adapter uses the CSBot probe order and strict 2D dot > 0.5 fixture semantics |
| Ground truth / belief split | PASS boundary | hidden actors become ObservedAbsent; exact hidden coordinates are not copied into Compatibility belief |
| Last-known knowledge | PASS offline | actor-generation-scoped memory preserves last-seen position and expires by deterministic configured age |
| Hearing/noise | PASS offline boundary | range, reaction delay, 3-second replacement rule, priority/nearer selection, and bounded retention are modeled; source identity is not exposed |
| Event knowledge | PASS offline boundary | death/spawn/respawn/round mutations are explicit; live GameEvent delivery remains unavailable |
| Runtime integration | IMPLEMENTED_UNVERIFIED | Full Update-scoped perception scan is wired into managed bots; objective path remains public/inferred |
| Production fixture | PASS | Metamod observation -> public trace/FOV/body result -> PerceptionAssembler -> Compatibility belief |
| Deterministic tests | PASS | focused perception, adapter, and production-boundary tests pass |
| Full x86 CTest | PASS | complete current suite 51/51 PASS |
| Phase 8 PowerShell | PASS fixture-only | action boundary, live-log, objective, and slow-movement fixture checks pass; live combat/C4 fields remain false/unverified |
| FocalSpan | PENDING | refresh after final P06 edits |
| Live HLDS/ReHLDS | NOT RUN | engine trace parity, smoke, private CSBot queues, and live event delivery remain unverified |

P06 is intentionally PARTIAL. The compatibility information boundary and
offline semantics are implemented, but a public Metamod adapter cannot prove
ReGameDLL private m_enemyQueue, smoke handling, profile reaction timing,
GameEvent delivery, or exact random noise-position placement without live
differential evidence.

## P05 result: PARTIAL

| Gate | Result | Evidence / limitation |
|---|---|---|
| Reference state inventory | PASS | 12 ordinary states plus the separate Attack overlay inventoried from the pinned ReGameDLL-CS source |
| Compatibility state owner | PASS boundary | one `compat::CompatibilityStateMachine` per managed actor; existing planner state machine retained |
| SetState lifecycle | PASS offline | OnExit -> OnEnter -> state publication -> timestamp, including same-state requests |
| Attack overlay | PASS offline boundary | underlying state retained; overlay owns update while active; state changes stop overlay first |
| Full Update timing | PASS regression | state updates are gated by existing P02 Full Update events; scheduler unchanged |
| Runtime integration | IMPLEMENTED_UNVERIFIED | managed bots own/reset state machines; public C4 carrying request is wired in Compatibility Mode |
| Deterministic tests | PASS | focused state-machine target covers ordering, multi-step, blockers, overlay, isolation, lifecycle |
| Full CTest | PASS | complete x86 Debug suite `50/50 PASS` |
| Phase 8 | PASS fixture-only | live-log, objective, slow-movement, and action-boundary fixture checks passed; live combat/C4 remains unverified |
| FocalSpan | PASS freshness | refreshed after P05 source edits; `stale: false` |
| Live HLDS/ReHLDS | NOT RUN | no live state/differential trace claim |

P05 is intentionally `PARTIAL`. State orchestration and the Attack overlay
boundary are implemented and offline verified, but most state-internal
behavior, private observations, exact objective/vision/NAV decisions, and a
pinned differential runtime trace remain open. P06 is recorded above.

## P04 result: PARTIAL

| Gate | Result | Evidence / limitation |
|---|---|---|
| Observation Core model | PASS boundary | SDK-free typed values, quality/source/freshness/lifecycle metadata, explicit invalid values |
| Public ObservationAdapter | PASS boundary | x86 fixture covers player fields, FOV, actor/frame, C4 proxy, planted-bomb inference, missing engine context |
| PluginRuntime integration | PASS regression | world/objective/action fixture and FakeClient isolation remain green; decisions were preserved |
| Observation trace | PASS boundary | adapter-local sequence, actor/timing metadata, enhanced/RNG isolation fixture |
| FOV | PARTIAL | public `pev->fov` is collected; internal scope/visibility/trace parity is not available |
| Weapon state | PARTIAL | private active weapon/ammo/reload/accuracy/timers/zoom remain unavailable; synthetic weapon remains marked |
| Objective state | PARTIAL | public C4/entity proxies are inferred; defuse/kit/hostage/VIP/round state remains unavailable or unknown |
| Configure/build | PASS | current x86 HostX86/x86 NMake Debug worktree build |
| Focused tests | PASS | observation model 1/1, adapter 1/1, trace 1/1, integration/action/isolation and P02/P03 focused suites passed |
| Full CTest | PASS | current x86 complete build and `49/49 PASS` |
| Phase 8 PowerShell | PASS fixture-only | live-log umbrella, objective, slow-movement, and action-boundary checks passed; combat/C4 live fields remain separate |
| FocalSpan | PASS | final worktree index fresh after source/documentation update |
| PE/Python verifier | NOT RUN | `py -3` could not create its Windows Python process |
| Live HLDS/ReHLDS | NOT RUN | no new live acceptance claim in P04 |

P04 does not promote any observation to `MATCH` solely from an adapter or
offline fixture. P05 state orchestration is now implemented partially; internal
behavior and live differential acceptance remain open.

## P00 result

`PASS` for the P00 audit artifact set. This does not mean the CSBot-compatible
baseline passes. The Compatibility Mode boundary is `implemented_unverified`;
behavioral parity remains `not established`.

P00 was analysis/documentation only. No gameplay implementation, Astra extension,
or reference source was changed.

## Frozen baseline

- AstraBot audit commit: `9788c245f5017567c60c35fa3afe474d56d1a349`
- ReGameDLL-CS reference: `master` at `b0889847fe6d03898be88acc9e366660efb40ab5`
- Current Astra worktree: dirty before P00; pre-existing changes preserved
- Reference worktree: dirty, but all comparison claims use the pinned commit
- Current fresh DLL: `build-action-adapter-x86-1451/astrabot_mm.dll`, SHA-256
  `3d2bda1bd90065ca5553d0b4de13e9d6e32f03c59f099981c705d5e81dcc4d0f`

## Baseline gates

| Gate | Result | Evidence |
|---|---|---|
| Configure | PASS | CMake reconfigured `build-action-adapter-x86-1451` |
| Full x86 build | PASS | VS 2026 HostX86/x86, NMake, Debug, all targets |
| CTest | PASS | 42/42, 0 failed, 3.23 seconds |
| Phase8 PowerShell fixture/self-tests | PASS | live-log, objective, action-boundary checks |
| Python manifest/artifact tests | NOT RUN | `py -3` could not create the installed Python process |
| New live HLDS/ReHLDS run | NOT RUN | P00 did not start a new live server run |
| Existing live acceptance | PARTIAL | Windows join/human damage/death; autonomous action/C4/Linux remain open |

## Phase ledger

| Plan | Status | Result / blocker |
|---|---|---|
| P00 | complete | reference frozen; source map, matrices, observations and blockers recorded |
| P01 | complete | explicit runtime mode, policy isolation, snapshot diagnostics, and regression tests |
| P02 | complete | Compatibility timing/command cadence implemented and deterministically verified; live/private-state parity remains open |
| P03 | partial | Compatibility RNG boundary, engine delegation, scripted consumption, and trace schema are offline verified; specific callsites and live parity remain open |
| P04 | partial | observation boundary merged; private-state and live gates remain open |
| P05 | partial | compatibility state orchestration and attack overlay offline verified; internal behavior and live differential trace remain open |
| P06 | partial | perception boundary, bounded belief, public visibility fixture, and offline gates complete; live/private differential evidence remains |
| P07-P12 | pending | downstream behavior through live parity remain separate evidence gates |
| P13 | deferred | enhanced intelligence remains downstream of the baseline |

The repository's existing `.planning/STATE.md` is not rewritten by this audit;
its Phase 8 live-gap state remains authoritative for that separate workstream.

## P02 readiness

`NO` for claiming CSBot behavioral parity. P02 planning may begin, but timing,
RNG, private-state, visibility, state-machine, NAV, combat, and live blockers
remain open.

## P02 verification

- ReGameDLL-CS reference: `b0889847fe6d03898be88acc9e366660efb40ab5`
- Timing source: `runtime::BotTimingScheduler`, absolute `now + interval`
- Event order: `Upkeep -> CommandReset -> FullUpdate -> CommandExecute` when both gates are due
- Delayed frames: one execution/full update maximum; no catch-up loop; deadlines rebase from current time
- Deterministic CTest: `astrabot_runtime_timing` PASS and `astrabot_compat_actor_command` PASS
- Configure/build: PASS in VS 2026 HostX86/x86 NMake Debug environment
- Full CTest: 44/44 PASS, 0 failures
- Phase 8 PowerShell checks: PASS; slow-movement fixture reports partial live combat/C4/objective observations as expected
- PE/Python verifier: environment-unverified; `py -3` could not create the installed Python process
- New live HLDS/ReHLDS run: NOT RUN; live autonomous movement/combat/C4 and private-state parity remain open

## P01 verification

- Windows x86 configure/build: PASS
- CTest: 43/43 PASS, including `astrabot_runtime_mode_policy`
- Phase 8 PowerShell fixture and action-boundary checks: PASS
- New live HLDS/ReHLDS run: NOT RUN (out of P01 scope)

## Required next evidence

1. Define and prove compatibility-mode isolation before enabling any Astra-only
   planner, route memory, profile adaptation, team director, or learning behavior.
2. Differentially compare the new deterministic RNG boundary and reference call
   order; the P03 offline boundary exists, but the pinned production reference
   tape and live comparison remain open.
3. Map public/private observations and choose explicit `exact`, `delayed`,
   `inferred`, or `unavailable` policies.
4. Establish timing/command traces for the reference and Astra paths.
5. Keep real-server movement, combat, C4, Linux x86, and lifecycle acceptance
   separate from offline tests.

## P03 result: PARTIAL

| Area | Result | Evidence / limitation |
|---|---|---|
| Reference RNG model | PARTIAL | 148 executable direct expressions, callback chain, local ReHLDS implementation observation, and 62-file transitive audit recorded in `RNG_MODEL.md` |
| Compatibility RNG contract | PASS for boundary | SDK-free request/result types, exact type/bounds tape checks, too-many/too-few detection, and branch fixtures pass offline |
| Production engine adapter | PASS for boundary | Fake callback tests verify exact forwarding and unavailable-callback failure; no production decision callsite is claimed |
| Shared ownership | PASS for boundary | One PluginRuntime-owned source; multi-Bot A/B/A scripted ordering passes |
| Enhanced isolation | PASS for boundary | Separate source instances do not advance Compatibility tape |
| RNG trace | PASS for schema/source | Optional bounded record includes sequence, semantic site, type, bounds, result, actor, and timing context |
| Specific CSBot callsite parity | UNVERIFIED | AstraBot production behavior does not yet consume the new source at equivalent branches |
| Live HLDS/ReHLDS RNG parity | NOT RUN | No fresh pinned reference/AstraBot live RNG trace was collected |

P02 timing regression remains PASS and unchanged. P03 does not promote overall
CSBot parity to MATCH and does not authorize P04.

## P07 result: PARTIAL

| Gate | Result | Evidence / limitation |
|---|---|---|
| NAV load | IMPLEMENTED_OFFLINE_VERIFIED | Existing read-only legacy versions 1-5 loader/model tests; ladder object linkage remains unavailable |
| Area lookup | IMPLEMENTED_OFFLINE_VERIFIED | Containment, floor tolerance, bounded nearest recovery fixtures |
| Path cost | PARTIAL | Static distance, crouch, jump, and route-type costs; dynamic danger/fall/ladder/private state unavailable |
| Tie-break | IMPLEMENTED_OFFLINE_VERIFIED | Stable CSBot discovery-order equal-cost fixture |
| Path persistence/recompute | IMPLEMENTED_OFFLINE_VERIFIED | `pathSequence`, `fullUpdateSequence`, and explicit recompute reason fixture |
| Movement command intent | IMPLEMENTED_OFFLINE_VERIFIED | 20-unit arrival, posture/traversal intent, P02 cadence unchanged |
| Jump | PARTIAL | NAV attribute and bounded jump/drop intent/state tests; live jump physics unverified |
| Ladder | PARTIAL | Separate enter/maintain/exit controller tests; real ladder object/mount/climb physics unverified |
| Stuck | PARTIAL | Bounded offline recovery; exact CSBot averaged velocity/RNG/wiggle/live physics unverified |
| Enhanced isolation | IMPLEMENTED_OFFLINE_VERIFIED | Compatibility route ignores actor rotation; explicit Enhanced policy retains adaptive path selection |
| Deterministic fixtures | PASS | NAV query, route persistence, posture/traversal, jump/drop, ladder-state, and movement tests |
| Python/PE verifier | NOT RUN | Environment verifier remains separate and unexecuted |
| Live HLDS/ReHLDS | NOT RUN | No new live differential acceptance claim |

P07 remains `PARTIAL`: offline Compatibility route/traversal boundaries are implemented and tested, while live physics/private reference evidence is open. P08 is not started.
