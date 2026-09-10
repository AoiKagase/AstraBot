# AstraBot — Functional Readiness Review

**Baseline:** `fc34d9beef64720e5fa752f0bc0c8d3f442af33b`  
**Review date:** 2026-09-09  
**Purpose:** Assess feature fulfillment, not phase labels, under four tiers: minimum playable, essential for normal CS play, recommended, and advanced.

## 1. Evidence and limits

This review combines the supplied `p12-live-source-audit.md` (D01) with pinned source inspection of the production input, scheduler, command, navigation, and feature-module boundaries listed below. It is not a new exhaustive line-by-line audit of every tracked file. No build, CTest execution, binary inspection, or HLDS session was run for this review. An attempted container archive download failed with DNS resolution failure; individual pinned files were available through web retrieval.

D01 describes an earlier session with Idle input only and no demonstrated attack/damage/kill, while new-DLL/new-process acceptance remains incomplete (D01, lines 69–89). Those are reported observations, not newly reproduced results. Any subsequent code or deployed DLL requires reassessment.

The four tiers below are proposed product acceptance categories, not an assertion that the existing repository already uses this taxonomy. “Essential” means essential for the declared supported gameplay scope; it does not require every custom map or optional extension before a limited playable build.

### Status vocabulary

- **CONNECTED / NOT LIVE-ACCEPTED:** A production invocation path exists; correct gameplay is not yet proven.
- **PARTIAL:** Some implementation exists, but a specific limitation or missing stage prevents full acceptance.
- **CORE ONLY / UNWIRED:** Value-level logic, API, or testable component exists without the required production producer/consumer chain.
- **NOT VERIFIED:** The available evidence does not establish an implementation or runtime guarantee.

No row below means “live validated” merely because a phase gate or unit-test target exists. Test registration is not test execution, and component execution is not gameplay success.

## 2. Executive verdict

| Tier | Verdict | Principal reason |
|---|---|---|
| Minimum playable bot | NOT YET ACCEPTED | Participation and physics infrastructure exist; sustained autonomous movement/combat and death-to-respawn behavior are not established. |
| Essential normal-CS features | INCOMPLETE | Production objective/economy input is missing; autonomous actor processing is primary-only. |
| Recommended behavior | MIXED | Several useful behaviors have Core implementations, but candidate generation, action execution, or automatic triggering is absent/unverified. |
| Advanced behavior | MOSTLY COMPONENT-LEVEL | Learning, contextual danger, wallbang, and traversal data structures must not be confused with live learning or motor execution. |

Do not interpret this as proof that every Core algorithm is wrong. The immediate product risk is incomplete end-to-end fulfillment. Conversely, do not describe missing integration as only a final live test: several production paths demonstrably lack required data or reject the requested capability. [D01 §3, §5; S01–S04]

## 3. Tier A — Minimum playable bot

Scope: at least one supported bot in a controlled, declared map/server configuration, with equipment available, behaving without continual manual movement commands. Full team economy and advanced learning are not prerequisites for this limited milestone.

| ID | Capability | Current evidence/status | Acceptance still required |
|---|---|---|---|
| MIN-01 | Plugin load, join, team, spawn, scoreboard presence | PARTIAL. Lifecycle and join paths exist; earlier reported participation is not acceptance of the newest DLL. [S04, S22; D01 §1.3] | New-process load and stable playable spawn, with DLL identity recorded. |
| MIN-02 | Continuous physics while idle | PARTIAL. Idle movement exists, but death and rejected-pending-command paths need separate treatment. [S05] | Neutral simulation during legitimate idle/death periods without replaying stale action or sending twice. |
| MIN-03 | Existing NAV loading and route calculation | CONNECTED / NOT LIVE-ACCEPTED. File loading, graph/index publication and console route requests exist. [S09] | Real supported NAV load and a route traversed by the spawned bot. |
| MIN-04 | Autonomous destination selection | PARTIAL. Production navigation context primarily reuses an existing executable route; no production objective feed establishes ordinary round destinations. [D01 §3.2, §5.2; S26] | Without repeated `goto`, choose useful reachable goals, arrive, then continue. |
| MIN-05 | Vision and opponent identification | CONNECTED / NOT LIVE-ACCEPTED. Vision processing is called in the frame pipeline; team and identity inputs can block eligibility. [S24; D01 §3.5] | See opponents, avoid selecting allies, and stop direct fire on lost visibility. |
| MIN-06 | Aim, reaction, direct shooting | CONNECTED / NOT LIVE-ACCEPTED. Core authorization is called; historical attack/damage/kill evidence is absent. [S07; D01 §1.3, §5.5] | Demonstrate actual firing, ammunition consumption, damage and death using server evidence. |
| MIN-07 | Reload and owned-weapon fallback | PARTIAL. Decision logic exists. Weapon-switch transport requires a handler whose production registration was not found in the inspected entry/lifecycle/console paths. [S06, S07] | Empty clip with reserve reloads; exhausted primary switches to a usable owned weapon without stopping physics. |
| MIN-08 | Death, next-round respawn and continued activity | PARTIAL. Retirement code exists; dead-player neutral updating is incomplete. [S05; D01 §4] | Death animation completes, stale attacks cease, respawn resumes behavior over multiple rounds. |
| MIN-09 | Stable connection without unwanted idle kick | NOT VERIFIED. Previous idle-kick reports require a current-session cause and remedy. [D01 §1.3; supplied conversation] | Maintain the bot through idle, active and round-transition periods. |

A stationary target on the scoreboard is not this milestone. A command-driven navigation demo is useful progress but is not the same as an autonomous playable bot.

## 4. Tier B — Essential for normal supported CS play

| ID | Capability | Assessment | Required integration or evidence |
|---|---|---|---|
| ESS-01 | Multiple autonomous bots | INCOMPLETE. Slot management is multi-client, but runtime input and execution explicitly select one primary. [D01 §5.3] | All supported managed bots receive independently validated inputs and commands; verify 2, then 8/16 as applicable. |
| ESS-02 | Round phase, objective state and time | CORE ONLY / UNWIRED. Production objective state is neutral. [D01 §5.2] | Real objective events/state, eligibility and timing, including supported map-family transitions. |
| ESS-03 | Bomb pickup, planting, guarding and defusing | CORE ONLY / UNWIRED. Actions/assignments exist; their names do not establish actual C4/use interaction. [S11, S13] | Observe → choose → navigate → execute → verify the actual game objective state. |
| ESS-04 | VIP, hostage and escape maps | CORE ONLY / UNWIRED. Objective-family and assignment models exist. [S12, S13] | Per-family production observations, eligible actors, interaction and completion checks. Essential only when claiming support for that family. |
| ESS-05 | Basic money, buy-zone/time and purchase execution | CORE ONLY / UNWIRED. Buy policy and abstract execution exist; production economy inputs remain absent. [S14, S15; D01 §5.2] | Money/equipment observation, real command implementation, inventory confirmation and bounded failure handling. |
| ESS-06 | Declared weapon coverage | INCOMPLETE. The audit identifies unclassified firearm IDs and an absent knife attack path. [D01 §3.7] | Explicit support matrix; implement firearm/melee semantics rather than routing every item through firearm authorization. |
| ESS-07 | Doors, crouch, stairs, ordinary jump, ladder and recovery | CONNECTED / NOT LIVE-ACCEPTED for existing supported movement. [S09, S10, S27] | Real map transitions, blockers and recovery; avoid invalidating a legitimate traversal merely because current area is temporarily unknown. |
| ESS-08 | Safety against stale identities and unintended fire | PARTIAL. Identity and target-relation defenses exist; end-to-end acceptance remains necessary. [S04, S07] | Slot/map reuse, invalid input and cancellation; when friendly fire is enabled, independently verify allies crossing the actual shot path. |
| ESS-09 | Round/map continuity and bot-count policy | PARTIAL. Map lifecycle exists; replay code automatically queues the primary, not proof of preserving a requested roster. [S04] | Declared count policy, reconnect/reset behavior, map rotation, no permanent inert secondary bots. |
| ESS-10 | Install/configuration and diagnostics | PARTIAL. Console commands and build targets exist. [S21, S23] | Reproducible package/config; actionable per-actor failure reasons, bounded logs and compatible target declaration. |
| ESS-11 | Runtime performance and stability | NOT VERIFIED at gameplay level. Offline registrations/replay evidence are not real engine frame-time evidence. [S21; D01 §1.3] | CPU, memory, traces, command scheduling and long-session/map-rotation behavior on declared server hardware. |

Bomb, hostage, VIP and escape logic must also distinguish authoritative public rule state from inaccessible opponent information. Do not repair missing input by fabricating certainty.

## 5. Tier C — Recommended behavior

| ID | Capability | Assessment | Remaining concern |
|---|---|---|---|
| REC-01 | Tap/Burst/FullAuto and reaction tuning | CONNECTED / NOT LIVE-ACCEPTED. Cadence is part of authorization. [S07] | Confirm actual shots, pauses, weapon timing and visibility interruptions, not authorization counts alone. |
| REC-02 | Sound, visual memory, smoke and flash constraints | Implementation and runtime processing present. [S24, S25; D01 §5.1, §7.1] | Real event compatibility and bounded perception update costs. |
| REC-03 | Explicit teammate reports | API/console-request path exists. [S09, S04] | Automatic report policy and evidence-preserving sharing must be separately established. |
| REC-04 | Cover, retreat and peek | CORE ONLY / PARTIAL. Evaluators need valid candidate locations. [S11] | Generate safe production candidates and execute/complete the action; empty candidate input is not a working cover feature. |
| REC-05 | Context-aware dropped-weapon acquisition | CORE ONLY / UNWIRED. Acquisition evaluation/state exists. [S11] | Dropped-item observation, approach, deliberate replacement and inventory-confirmed success. |
| REC-06 | Rotate, retake, save, flank and support | CORE logic present; production context incomplete. [S26] | Usable objectives/routes and explicit Intent → Action → execution integration. |
| REC-07 | Team coordination and economy specialization | CORE logic present. [S12, S14] | Full roster and objective/economy feeds, assignments consumed by executable actions. |
| REC-08 | Grenade throwing and secondary weapon operations | NOT VERIFIED as gameplay implementations. Buying/hearing grenades is not throwing them. [S07, S11, S14] | Throw selection/trajectory/safety; scope/special-fire support where declared. |
| REC-09 | Model variety, quota and difficulty configuration | PARTIAL / NOT VERIFIED. Existing console creation is not a full operational policy. [S23] | Confirm requested versus actual models, quota persistence and per-bot difficulty wiring. |
| REC-10 | Vanilla-CS portability | NOT VERIFIED. Inspect actual callback/prediction assumptions, not names alone. [S01, S22] | Separate compatibility matrix and live tests. `NEW_DLL_FUNCTIONS` must not be mislabeled a ReGameDLL-exclusive API. |

## 6. Tier D — Advanced capabilities

| ID | Capability | Assessment | Missing end-to-end proof |
|---|---|---|---|
| ADV-01 | Persistent map experience | CORE ONLY / UNWIRED in the inspected default runtime. Store APIs exist, but the runtime pipeline is default-constructed without persistence. [S16, S17] | Event collection, storage binding, restore/flush lifecycle and restart survival. |
| ADV-02 | Experience-driven routes | CORE policy exists; live selection unverified. [S18] | Real observations change data; the actual route request binds the policy and changes a valid route. |
| ADV-03 | Contextual danger | Model/lifecycle exists. [S19] | Production observations and an actual decision consumer. |
| ADV-04 | Map-session opponent adaptation | Model retention exists, but runtime death handling forgets a profile. [S02, S19] | Generate observations, preserve intended lifetime and use the aggregate in decisions. |
| ADV-05 | Human traversal learning | Observation-driven candidate aggregation exists. [S20] | Human movement observation, candidate normalization, validation and publication into executable navigation. |
| ADV-06 | GapJump / EdgeTraverse / LongJump and precision ledges | NOT IMPLEMENTED as demonstrated production motor behavior in the inspected path. [S10] | Movement controllers, physical feasibility and execution/recovery evidence, not just link metadata. |
| ADV-07 | Deliberate Drop traversal | Explicitly unsupported by the current primitive dispatcher. [S10] | Controller and safe completion criteria before any route exposes it as executable. |
| ADV-08 | Belief-based wallbang | CORE planning calculation only. Production command validation allows DirectFire only. [S19, S07] | Geometry/weapon penetration inputs, distinct authorization, dispatch and actual gameplay evidence. |
| ADV-09 | Suppressive fire | CORE planning calculation only; same command barrier. [S19, S07] | Region-based aim/fire execution and ammunition/safety policy. |
| ADV-10 | New NAV generation | NOT VERIFIED. Existing-file loading is implemented; learning links is not base-mesh generation. [S09, S21] | A separately specified generator/export and compatibility gate; not required where a supported NAV already exists. |
| ADV-11 | Optional AMXX API | No production module target established by inspected CMake configuration. [S21] | Public bridge/API and acceptance; not a prerequisite for standalone bot play. |

## 7. Concrete capability-chain findings

### FR-01 — Primary-only is a product limitation, not just a test omission

`buildRuntimeInputs` selects the primary, and the orchestrator rejects other actors. The production team input contains one member. Treat multi-bot autonomy as work to implement, not as functionality awaiting one more test. Preserve independently generation-checked per-actor state when generalizing. [D01 §5.3; S03]

### FR-02 — Objectives need interaction, not just an enum and destination

The current input builder explicitly leaves objectives unavailable. Even with a future provider, the inspected orchestrator mainly forwards action/tactical target areas and a separate combat result. Plant/defuse/hostage actions also require task execution and game-state completion evidence. Add a per-action producer/consumer checklist. [D01 §5.2; S11, S13]

### FR-03 — Basic switching has an execution dependency

Movement rejects weapon-selection commands without `WeaponSelectionHandler`. Its setter exists, but no registration was found in the inspected plugin entry, lifecycle configuration or console initialization. This is a source-audit finding to verify across the complete checkout, not a reproduced server failure. Test the production installation path, not only a manually injected test handler. [S05, S06, S22, S23]

### FR-04 — Learned data currently needs real producers and storage ownership

The production input builder does not populate the optional experience/opponent/contextual observation fields. The orchestrator owns a default `ExperiencePipeline`; that default has no storage provider. Document the event source, identity, storage binding, flush/restore trigger and consumer for each learned statistic. [S01, S03, S16, S17]

### FR-05 — Wallbang is blocked downstream

A wallbang utility result is not a combat command. The existing production composition calls the validation that rejects non-DirectFire modes. Do not weaken normal-fire checks globally: implement a distinct, evidence-constrained authorization and input provider before enabling either advanced mode. [S07, S19]

### FR-06 — Learned links do not supply learned movement

`TraversalLearningModel` aggregates already supplied observations and publishes eligible links. It does not, by itself, record human commands or implement air/ledge control. The current primitive supports ordinary Walk/Crouch/Jump/Ladder and rejects Drop/unknown kinds. Route capability filtering and actual controllers must agree. [S10, S20]

### FR-07 — Known integration defects still block the minimum tier

Retain the original audit's neutral-team map-stamp issue and managed-bot classification mismatch as targeted production regressions. Also test physics fallback after command rejection and legitimate movement during temporary current-area loss. These are separate from HLDS startup failures reproduced without AstraBot. [D01 §3.3–3.4, §3.8, §4; S05, S09]

## 8. Proposed acceptance order — no new phase numbering

1. **Single-bot playable slice:** new DLL/process; stable spawn; supported NAV; useful goal; movement; visible opponent; real shot; reload/fallback; death and next-round behavior. Start with a declared, deliberately small equipment/map scope.
2. **Normal CS slice:** multiple autonomous bots; real bomb state; plant/defuse; elementary buying and inventory confirmation. Confirm an actual round reaches a valid result.
3. **Declared map-family support:** hostage, VIP and escape production state/actions. Verify restrictions and completion individually.
4. **Recommended play quality:** cover/retreat/peek, acquisition, team coordination, grenades, configuration and performance.
5. **Advanced loop closure:** observations → storage/model → changed decision → execution → measured outcome. Enable wallbang/traversal only after their distinct execution contracts exist.

These are feature acceptance slices to map onto existing plans. They are not permission to rename completed task IDs or restart already implemented modules.

## 9. Codex follow-through

For each row, record a concrete producer, transformation, consumer, observable effect, focused test and live evidence status. Read the current checkout first and mark findings already fixed after this baseline accordingly.

Do not:

- equate a class, enum, compiled object, or passing synthetic test with a fulfilled gameplay feature;
- create fake objective/weapon/observation data to make the runtime pass;
- turn every missing callback into plugin-attach failure without considering independent capabilities;
- erase unrelated worktree changes;
- repeat the identical full suite for unchanged code/build inputs;
- mix naming/formatting cleanup into integration fixes.

Use focused tests during repair. Run each required full configuration once for the final changed build inputs. Live work must be explicitly authorized under a non-circular readiness policy; do not require final live acceptance before permitting the live tests that establish it.

## 10. Corrected interpretation of earlier phase reviews

Earlier statements that Phase 11 meant wallbang or advanced learned traversal was “implemented” must be narrowed to the actual delivered level: planning/model components exist, while production execution is absent or blocked. Similarly, offline completion of Economy/Experience does not establish live buying or persistent learning.

The release question is not “How many phases are marked complete?” It is “Which claimed gameplay chains complete with real input and an observed outcome?”

## 11. Evidence index

**D01:** User-supplied `p12-live-source-audit.md`, baseline `fc34d9b`. Principal sections: 1.3 (live limits), 3 (runtime blockers), 4 (death processing), 5.2 (objective/economy gap), 5.3 (primary-only), 5.5 (damage/kill evidence), and 7 (test coverage/limits). This is an audited report, not independent live proof.

The source references below use the same full baseline SHA. Symbol references are authoritative for navigation; line positions may differ between raw web indexing and local editor numbering.

- **S01** — `src/adapter/metamod/runtime_input.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/metamod/runtime_input.cpp`
- **S02** — `src/adapter/metamod/runtime_orchestrator.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/metamod/runtime_orchestrator.cpp`
- **S03** — `src/adapter/metamod/runtime_orchestrator.hpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/metamod/runtime_orchestrator.hpp`
- **S04** — `src/adapter/metamod/lifecycle.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/metamod/lifecycle.cpp`
- **S05** — `src/adapter/metamod/movement.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/metamod/movement.cpp`
- **S06** — `src/adapter/metamod/movement.hpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/metamod/movement.hpp`
- **S07** — `src/core/combat.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/core/combat.cpp`
- **S08** — `src/adapter/cstrike/combat.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/cstrike/combat.cpp`
- **S09** — `src/adapter/cstrike/nav/console.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/cstrike/nav/console.cpp`
- **S10** — `src/nav/local/primitive.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/nav/local/primitive.cpp`
- **S11** — `src/core/action_planner.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/core/action_planner.cpp`
- **S12** — `src/core/team_director.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/core/team_director.cpp`
- **S13** — `src/core/team_director.hpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/core/team_director.hpp`
- **S14** — `src/core/economy.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/core/economy.cpp`
- **S15** — `src/adapter/cstrike/buy_executor.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/cstrike/buy_executor.cpp`
- **S16** — `src/core/experience.hpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/core/experience.hpp`
- **S17** — `src/core/experience.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/core/experience.cpp`
- **S18** — `src/nav/query/adaptive_route.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/nav/query/adaptive_route.cpp`
- **S19** — `src/core/p11_learning.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/core/p11_learning.cpp`
- **S20** — `src/nav/enrichment/traversal_learning.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/nav/enrichment/traversal_learning.cpp`
- **S21** — `CMakeLists.txt`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/CMakeLists.txt`
- **S22** — `src/adapter/metamod/plugin_entry.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/metamod/plugin_entry.cpp`
- **S23** — `src/adapter/metamod/console_debug.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/metamod/console_debug.cpp`
- **S24** — `src/adapter/cstrike/vision.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/cstrike/vision.cpp`
- **S25** — `src/adapter/cstrike/sound.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/cstrike/sound.cpp`
- **S26** — `src/core/tactical_planner.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/core/tactical_planner.cpp`
- **S27** — `src/adapter/cstrike/nav/motion.cpp`  
  `https://github.com/AoiKagase/AstraBot/blob/fc34d9beef64720e5fa752f0bc0c8d3f442af33b/src/adapter/cstrike/nav/motion.cpp`
