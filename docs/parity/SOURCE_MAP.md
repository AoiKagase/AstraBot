# P00 Source Map

## P06 perception boundary

| Reference surface | Pinned source / call path | AstraBot current boundary | P06 status |
|---|---|---|---|
| FOV | basemonster.cpp::FInViewCone, cs_bot_vision.cpp::IsVisible | ObservationAdapter::collectVisibility; strict 2D dot threshold and no vertical FOV inference | IMPLEMENTED_OFFLINE_VERIFIED; live/private state unverified |
| Visibility trace | cs_bot_vision.cpp::IsVisible; eye -> chest/head/feet/edges | public enginefuncs_t::pfnTraceLine, self skip, fraction check, five ordered probes | IMPLEMENTED_UNVERIFIED; smoke/glass/engine differential open |
| Enemy selection | FindMostDangerousThreat and UpdateReactionQueue | buildManagedWorldSnapshot selects only visible hostile belief contacts | PARTIAL; no private current-threat queue |
| Last-known enemy | m_lastEnemyPosition, m_lastSawEnemyTimestamp, enemy death handling | MemorySample with actor generation, knowledge state, last-seen frame and bounded expiry | IMPLEMENTED_OFFLINE_VERIFIED |
| Listening | cs_bot_event.cpp audible event path, cs_bot_listen.cpp | AudibleEvent -> NoiseMemory; range, priority, nearer replacement and retention | IMPLEMENTED_UNVERIFIED; no live event hook |
| Event knowledge | CCSBot::OnEvent -> CSGameState/chatter/state | PerceptionEvent -> explicit Core memory/noise mutation | IMPLEMENTED_UNVERIFIED; live GameEvent delivery absent |
| Runtime cadence | cs_bot_update.cpp Upkeep/Update and reaction queue | perception scan occurs on existing Full Update sequence; command path reuses belief | IMPLEMENTED_UNVERIFIED; no live differential cadence trace |

The Core remains SDK-free. The reference is a behavioral comparator only; no
ReGameDLL private class, pdata offset, or implementation code was copied.

## P05 state machine boundary

| Reference surface | Pinned source | AstraBot current boundary | P05 status |
|---|---|---|---|
| State owner / `SetState` | `regamedll/dlls/bot/cs_bot.h`, `cs_bot_statemachine.cpp` | `include/astrabot/compat/state_machine.hpp`, `src/core/compat/state_machine.cpp` | IMPLEMENTED_OFFLINE_VERIFIED |
| Full Update dispatch | `regamedll/dlls/bot/cs_bot_update.cpp` | `PluginRuntime::updateManagedBotMovement`, existing `BotTimingScheduler` | IMPLEMENTED_UNVERIFIED; scheduler unchanged |
| Ordinary state lifecycle | `regamedll/dlls/bot/states/cs_bot_*.cpp` | `CompatibilityStateMachine::invokeEnter/Update/Exit` and owned instances | IMPLEMENTED_UNVERIFIED; internals deferred |
| Attack overlay | `CCSBot::m_attackState`, `Attack()`, `StopAttacking()` | `attackOverlayActive_`, `beginAttack`, `stopAttack`, existing combat intent handoff | IMPLEMENTED_UNVERIFIED; no live trace |
| Managed actor ownership | `CCSBot` instance lifecycle | `PluginRuntime::managedBotStateMachines_`, reset on actor/map/round lifecycle | IMPLEMENTED_UNVERIFIED |
| Transition trace | reference watch/debug state transitions | `StateTraceRecord` and `IStateTraceSink` in Core tests | IMPLEMENTED_OFFLINE_VERIFIED |

The reference source is used as a behavioral comparator only. No private
ReGameDLL class, pdata offset, or reference implementation is copied into the
AstraBot Core or public Metamod adapter.

## P04 observation boundary

| Reference surface | Pinned ReGameDLL-CS consumer examples | AstraBot current boundary | P04 status |
|---|---|---|---|
| Player public state | `CCSBot::IsAlive`, `cs_bot.cpp` `pev->origin`/`pev->v_angle`, vision FOV checks | `ObservationAdapter::collectActor`; public `edict_t` fields become `CompatibilityObservation` and then `WorldSnapshot` | `IMPLEMENTED_UNVERIFIED` |
| FOV | `CCSBot::IsVisible`, `m_iFOV` checks in `cs_bot.h`/vision | `OBS-PLAYER-FOV` from `entity->v.fov` | `EXACT_ENGINE_API`, not MATCH |
| Weapon private state | `CBasePlayerWeapon` active/clip/ammo/timers/accuracy consumers | explicit invalid `WeaponObservation` fields; existing synthetic `WeaponRecord` remains | `UNAVAILABLE` |
| Bomb state | `CSGameState::IsBombPlanted`, `GetBombPosition`, `IsBombLoose` | `collectPlantedBomb` public classname/model/dmgtime/entity origin; C4 bit proxy | `INFERRED` |
| Visibility trace | `CCSBot::IsVisible` / `UTIL_TraceLine` | observation IDs exist; Engine trace collection remains unimplemented | `NOT_YET_IMPLEMENTED` |
| Timing context | CSBot command/full-update lifecycle | existing command sequence attached; unavailable event counters remain zero | `IMPLEMENTED_UNVERIFIED` |
| Observation trace | reference state is in-process, no copied trace implementation | `ObservationTraceRecord` and bounded adapter sink with per-adapter sequence | `IMPLEMENTED_OFFLINE_VERIFIED` |

P04 implementation files are `include/astrabot/compat/observation.hpp`,
`src/core/compat/observation.cpp`,
`src/adapter/metamod/observation_adapter.{hpp,cpp}`, and the integration in
`src/adapter/metamod/plugin_runtime.{hpp,cpp}`. The Core remains SDK-free.

## Method and limits

The map was built from the pinned ReGameDLL source, the current AstraBot source
and CMake target lists, direct symbol searches, FocalSpan retrieval, and CRG
architecture metadata. The CRG index was built on an older SHA and had no
cross-community edges for the current tree, so it was used only for orientation;
direct source and build evidence are authoritative.

Status meanings are intentionally behavioral: a similarly named class or test is
not evidence of CSBot parity.

## ReGameDLL common bot layer

| Reference file | Primary symbols/role | AstraBot correspondence | Status / notes |
|---|---|---|---|
| `game_shared/bot/bot_constants.h` | shared bot constants | `include/astrabot/*`, engine button constants | DIFFERENT; no shared CSBot constant set |
| `game_shared/bot/bot.h` | `CBot`, 30Hz command and 10Hz full-think contract, `BotThink`, `ExecuteCommand` | `include/astrabot/runtime/bot_timing_scheduler.hpp`, `src/adapter/metamod/plugin_runtime.cpp`, `runtime/bot_command.cpp`, `metamod/input_dispatcher.cpp` | IMPLEMENTED_UNVERIFIED; deterministic timing and template tests pass, live/private state remain open |
| `game_shared/bot/bot.cpp` | base movement, weapon buttons, `BotThink`, `ExecuteCommand` | `src/core/runtime/bot_timing_scheduler.cpp`, `src/adapter/metamod/plugin_runtime.cpp`, `action_adapter.cpp`, `input_dispatcher.cpp` | IMPLEMENTED_UNVERIFIED public command cadence boundary; decision/private state is not CSBot parity |
| `game_shared/bot/bot_manager.h` | `CBotManager`, server-frame management | `plugin_runtime` StartFrame hooks, `FakeClientManager` | DIFFERENT; no equivalent manager scheduling |
| `game_shared/bot/bot_manager.cpp` | bot creation/maintenance and `StartFrame` | `plugin_runtime.cpp` | PARTIAL; lifecycle exists, quota/manager behavior does not |
| `game_shared/bot/bot_profile.h` | profile values, difficulty/team/profile manager contract | `compat/profile_catalog.hpp`, `metamod/profile_loader.cpp` | PARTIAL; parsed profile data is not the runtime CSBot profile |
| `game_shared/bot/bot_profile.cpp` | profile selection and random profile choice | `profile_catalog.cpp` | DIFFERENT; no CSBot profile selection/RNG parity |
| `game_shared/bot/bot_util.h` | shared bot utility APIs | scattered `world`, `nav`, and adapter helpers | DIFFERENT; no one-to-one utility boundary |
| `game_shared/bot/bot_util.cpp` | shared bot utility behavior | scattered helpers | MISSING/DIFFERENT; behavior has not been mapped as a whole |
| `game_shared/bot/improv.h` | common improv/tactical helper declarations | no direct runtime equivalent | MISSING |
| `game_shared/bot/nav.h` | NAV types, directions, places and route contracts | `include/astrabot/nav/nav_model.hpp` | DIFFERENT; independent data model |
| `game_shared/bot/nav_area.h` | area graph, hiding spots, random adjacent/spot selection | `nav_model`, `nav_query`, `nav_roam_controller` | PARTIAL; legacy NAV/A* contracts exist, CSBot area semantics do not |
| `game_shared/bot/nav_area.cpp` | area links, places, hiding/encounter selection | `src/core/nav/*` | DIFFERENT; deterministic actor-seeded route selection replaces reference RNG |
| `game_shared/bot/nav_file.h` | NAV file format/load contract | `legacy_nav_reader.hpp`, `metamod/nav_loader.cpp` | IMPLEMENTED_UNVERIFIED; read-only loader is not CSBot loader parity |
| `game_shared/bot/nav_file.cpp` | NAV file parsing | `legacy_nav_reader.cpp` | IMPLEMENTED_UNVERIFIED; format and limits differ until differential proof |
| `game_shared/bot/nav_node.h` | legacy node model | no direct node model; area model only | MISSING |
| `game_shared/bot/nav_node.cpp` | node graph behavior | no direct node graph | MISSING |
| `game_shared/bot/nav_path.h` | path object and route types | `nav_query.hpp`, `locomotion.hpp` | PARTIAL; independent corridor/follower |
| `game_shared/bot/nav_path.cpp` | CSBot path cost/selection | `nav_query.cpp`, `nav_roam_controller.cpp` | DIFFERENT; cost/tie/RNG parity unverified |
| `game_shared/bot/simple_state_machine.h` | simple state support | `behavior_state.hpp/cpp` | DIFFERENT; only high-level enum machine |

## ReGameDLL Counter-Strike bot layer

| Reference file | Primary role | AstraBot correspondence | Status / notes |
|---|---|---|---|
| `dlls/bot/cs_bot.h` | `CCSBot`, all state objects, attack overlay and CS-specific fields | `plugin_runtime.hpp`, core controllers | PARTIAL; no equivalent object/state ownership |
| `dlls/bot/cs_bot.cpp` | CSBot base behavior, scenario helpers, enemy/task access | plugin runtime and core contracts | PARTIAL; public edict-only approximation |
| `dlls/bot/cs_bot_init.cpp` | construction, initialization, spawn/reset | fake-client manager, lifecycle, join controller | PARTIAL; physical spawn is externally observed, not GameDLL-private initialization |
| `dlls/bot/cs_bot_init.h` | init declarations | no direct header | DIFFERENT |
| `dlls/bot/cs_bot_update.cpp` | `Upkeep`, `Update`, full decision loop | `onStartFramePost` / `updateManagedBotMovement` | DIFFERENT; one per-frame adapter loop, no 30/10 split |
| `dlls/bot/cs_bot_statemachine.cpp` | `SetState`, task changes, attack overlay | `behavior_state.cpp`, objective planner | DIFFERENT; no `OnEnter/OnUpdate/OnExit` state set |
| `dlls/bot/cs_bot_vision.cpp` | FOV/trace visibility, enemy recognition, reaction queue | `perception.cpp`, world snapshot | DIFFERENT; current live sensor enumerates hostile edicts without CSBot trace/FOV |
| `dlls/bot/cs_bot_listen.cpp` | hearing/noise investigation | perception sound schema only | MISSING from live integration |
| `dlls/bot/cs_bot_event.cpp` | GameEventType reactions | user-message/menu/team hooks | MISSING for CSBot event coverage |
| `dlls/bot/cs_bot_weapon.cpp` | aim offset, fire cadence, recoil, zoom, weapon classification | `combat_intent.cpp`, `weapon_state.cpp`, `action_adapter.cpp` | PARTIAL; live weapon state is synthetic/unavailable |
| `dlls/bot/cs_bot_nav.cpp` | stuck monitor, movement, jump, approach points | `nav_roam_controller.cpp`, `locomotion.cpp`, `movement_physics.cpp` | PARTIAL; live progress remains unaccepted |
| `dlls/bot/cs_bot_pathfind.cpp` | path positions, ladders, portals, jump/fall, costs | `nav_query.cpp`, `locomotion.cpp`, `jump_drop.cpp`, `special_traversal.cpp` | DIFFERENT; no CSBot cost/route parity |
| `dlls/bot/cs_bot_learn.cpp` | NAV learning/analyze/save processes | no compatibility equivalent | MISSING; future Astra learning must stay enhanced-only |
| `dlls/bot/cs_bot_manager.h` | CS manager, quota, scenario, zones and profile control | `plugin_runtime`, `native_bot_guard`, `compat_surface` | PARTIAL; lifecycle/guard only |
| `dlls/bot/cs_bot_manager.cpp` | manager frame, bot quota, zones, profile selection | fake-client commands and native guard | MISSING for manager semantics |
| `dlls/bot/cs_bot_radio.cpp` | radio commands, help response and voice feedback | `team/radio_intent.cpp` | PARTIAL; intent contract has no live radio dispatch |
| `dlls/bot/cs_bot_chatter.h` | chatter/meme contracts | no live chatter equivalent | MISSING |
| `dlls/bot/cs_bot_chatter.cpp` | chatter selection, interpretation and timing | no live chatter equivalent | MISSING |
| `dlls/bot/cs_gamestate.h` | bomb/hostage/scenario GameDLL state | `objectives/round_objectives.hpp`, runtime entity scan | PARTIAL; only narrow bomb observations |
| `dlls/bot/cs_gamestate.cpp` | scenario state and random bomb/hostage choices | objective planner and C4 scan | DIFFERENT; private state and RNG unavailable |

## Reference state files

Every state file in the pinned `dlls/bot/states/` directory was inspected and is
listed here. Astra's `BehaviorState` has only `Initial`, `Roam`, `Seek`,
`Engage`, `Retreat`, `Dead`, and `Recovering`; these names do not establish a
state-level match.

| Reference state file | Astra correspondence | Status |
|---|---|---|
| `cs_bot_attack.cpp` | `combat_intent`, action adapter | PARTIAL; no attack overlay/state lifecycle |
| `cs_bot_buy.cpp` | `profile_catalog`, compatibility commands | MISSING live buy state |
| `cs_bot_defuse_bomb.cpp` | objective planner, `IN_USE` adapter | PARTIAL; dispatch boundary only |
| `cs_bot_escape_from_bomb.cpp` | no state equivalent | MISSING |
| `cs_bot_fetch_bomb.cpp` | bomb-carried observation only | MISSING state behavior |
| `cs_bot_follow.cpp` | no follow state/controller | MISSING |
| `cs_bot_hide.cpp` | no hiding-spot state/controller | MISSING |
| `cs_bot_hunt.cpp` | no hunt state/controller | MISSING |
| `cs_bot_idle.cpp` | `NavRoamController` only | PARTIAL; no task/state semantics |
| `cs_bot_investigate_noise.cpp` | perception schema only | MISSING live integration |
| `cs_bot_move_to.cpp` | nav corridor/follower | PARTIAL; different route semantics |
| `cs_bot_plant_bomb.cpp` | objective planner, `IN_ATTACK` adapter | PARTIAL; C4 boundary only |
| `cs_bot_use_entity.cpp` | no state equivalent | MISSING |

## AstraBot current module inventory

| Area | Current implementation | Evidence / boundary |
|---|---|---|
| Metamod/engine adapter | ABI, exports, hooks, fake clients, join, messages, NAV loader, input dispatcher, action adapter | `src/adapter/metamod/*`; built in the current x86 target |
| Runtime/lifecycle | generation-stamped actors, command queue, per-frame movement, physics/readiness | `src/core/runtime/*`, `plugin_runtime.cpp` |
| NAV | legacy reader, immutable model/snapshot, query/corridor, locomotion, jump/drop, special traversal, roam | `src/core/nav/*`, `nav_roam_controller.cpp`; no CSBot parity proof |
| Perception/world | bounded world snapshot and memory assembler; live actor positions/velocities | `src/core/world/*`, `perception.cpp`; no live trace/FOV/noise feed |
| Behavior/objectives | bounded behavior state machine, proposals, bomb/hostage/buy proposal contracts | `src/core/behavior/*`, `src/core/objectives/*`; not a CSBot state graph |
| Combat/weapons | target belief, aim intent, weapon inventory and reload intent | `src/core/combat/*`; runtime builds a synthetic rifle record |
| Team/radio | team reports and radio intent/cooldown contracts | `src/core/team/*`; no live delivery/chatter |
| Compatibility/profile | command/CVAR surface, profile catalog/loader, native bot guard | `src/core/compat/*`, `src/adapter/metamod/*`; `RuntimeMode`, `RuntimeModePolicy`, and `PluginRuntime::Snapshot::mode` define the P01 boundary |
| Tests and evidence | P00 CTest 42/42; P01 CTest 43/43; phase8 PowerShell fixture checks passed; Python process unavailable | `CMakeLists.txt`, `tests/`, `docs/evidence/` |

## P01 compatibility boundary

`CvarState` is the single source for `RuntimeMode::Compatibility` (default) and
`RuntimeMode::Enhanced`, selected by the `astrabot_mode` server CVar and exposed
through `CompatibilitySurface`. `RuntimeModePolicy` provides the five enhanced
capability predicates; all are false in Compatibility Mode and true in Enhanced
Mode. `PluginRuntime::Snapshot::mode` and runtime diagnostics make the selected
mode observable. Existing Nav/Combat/Objective controllers remain unproven
baseline candidates; no controller is promoted to `MATCH` by P01.

## P03 Compatibility RNG boundary

| Reference boundary | AstraBot implementation | Status |
|---|---|---|
| `RANDOM_FLOAT` / `RANDOM_LONG` -> `g_engfuncs.pfnRandomFloat/Long` | `compat::EngineRandomSource` -> injected public engine callbacks | IMPLEMENTED_OFFLINE_VERIFIED |
| Global engine RNG ownership and multi-Bot ordering | One `PluginRuntime`-owned Compatibility source shared by all managed Bots | IMPLEMENTED_OFFLINE_VERIFIED |
| Strict call type/bounds/order test source | `compat::ScriptedRandomSource` and `RandomTapeEntry` | IMPLEMENTED_OFFLINE_VERIFIED |
| Optional RNG parity record | `RandomTraceRecord` and `IRandomTraceSink` | IMPLEMENTED_OFFLINE_VERIFIED |
| Reference CSBot production callsites | `docs/parity/RNG_MODEL.md` semantic inventory | PARTIAL; AstraBot production callsites remain UNVERIFIED |

The Core contract is value-only and does not include `edict_t`, `Vector`,
`enginefuncs_t`, or GameDLL-private types. The adapter does not copy the
ReGameDLL or engine generator algorithm and does not create per-Bot streams.

## Transitive reference surfaces requiring future mapping

CSBot reads GameDLL-owned player/weapon/rule state through its in-process class
objects. The next parity phases must map the relevant fields in `dlls/player.cpp`,
`dlls/weapons.cpp`, `dlls/wpn_shared/*`, `dlls/gamerules.cpp`, `dlls/soundent.cpp`,
`dlls/hostage/*`, and API/private-data paths. AstraBot currently uses public
Metamod/HLSDK `edict_t`/engine fields and does not have the same private object
access.
