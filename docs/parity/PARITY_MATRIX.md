# P00 Parity Matrix

## P04 observation parity gate

| Area | Reference evidence | AstraBot evidence | Result |
|---|---|---|---|
| Observation contract | CSBot reads in-process player/weapon/GameState/trace state | typed quality/source/freshness/lifecycle contract, Core tests | `IMPLEMENTED_OFFLINE_VERIFIED` |
| Public player fields | `pev` reads and `m_iFOV`/life consumers | x86 adapter fixture and runtime integration | `IMPLEMENTED_UNVERIFIED` |
| Team lifecycle | GameDLL team plus TeamInfo-driven runtime state | raw team plus TeamInfo/requested-team fallback | `DELAYED` |
| FOV | CSBot `CHECK_FOV` and internal FOV | public `pev->fov` same-tick observation; scope private state absent | `PARTIAL` |
| Weapon state | active weapon, clip, ammo, reload, timers, accuracy | explicit unavailable private fields; synthetic combat fixture | `PARTIAL` |
| Bomb/objective state | `CSGameState` planted/loose/defuse/hostage/VIP state | public entity/C4 proxies, remaining fields unavailable | `PARTIAL` |
| Visibility/trace | CSBot FOV/body-part `UTIL_TraceLine` | nearest hostile enumeration; trace collection not implemented | `PARTIAL` |
| Trace/lifecycle metadata | decision-time context and state identity | actor/frame/source/freshness/timing plus bounded sequence tests | `IMPLEMENTED_OFFLINE_VERIFIED` |
| P02 timing | scheduler event order/deadline semantics | unchanged; regression tests pass | `PASS REGRESSION` |
| P03 RNG | shared source/type/bounds/order | unchanged; regression tests pass | `PASS REGRESSION` |

P04 overall result remains `PARTIAL`; no row is promoted to `MATCH` merely
because the adapter exists or an offline fixture passes.

## Legend

- `MATCH`: behaviorally matched and currently evidenced. P00 has no row strong enough for this status.
- `IMPLEMENTED_UNVERIFIED`: an independent boundary or contract exists, but no differential/live proof establishes CSBot behavior.
- `PARTIAL`: a meaningful subset exists, with material behavior or evidence missing.
- `MISSING`: no current equivalent in the live compatibility path.
- `DIFFERENT`: current behavior is known to use a different contract or information boundary.
- `ASTRA_EXTENSION`: Astra-specific infrastructure or future behavior; it must not affect compatibility mode.
- `NOT_APPLICABLE`: reference behavior is not in scope for the current adapter boundary.

## Matrix

| Domain | Reference behavior | AstraBot location | Status | Evidence / gap |
|---|---|---|---|---|
| Runtime | `CBot::BotThink` 30Hz command and 10Hz full-think scheduling | `runtime::BotTimingScheduler` / `PluginRuntime::onStartFramePost` / `updateManagedBotMovement` | IMPLEMENTED_UNVERIFIED | Absolute `now + interval`, nested due gate, reference ordering, no catch-up; deterministic CTest evidence, live parity remains open |
| Runtime | `CBot::ExecuteCommand` usercmd generation and execution | `BotCommand` template, `CommandQueue`, `InputDispatcher::dispatchNext` | IMPLEMENTED_UNVERIFIED | Persisted Full Update command state, per-execution sequence/msec materialization, public `pfnRunPlayerMove` boundary; private state/live acceptance remain open |
| Lifecycle | create, initialize, spawn, disconnect and reset | `FakeClientManager`, `LifecycleSession`, `ActorRegistry`, `JoinController` | PARTIAL | current Windows evidence confirms load/join/spawn layers separately; full CSBot initialization not equivalent |
| State | `SetState` with `OnExit`, `OnEnter`, timestamp and task side effects | `BehaviorStateMachine` | DIFFERENT | enum machine has seven broad states, not CSBot's state objects or side effects |
| State | attack overlay supersedes normal state | `CombatController` call inside action decision | MISSING | no persistent attack overlay/state lifecycle |
| State | Idle | `NavRoamController` | PARTIAL | roaming intent exists; no CSBot Idle task selection |
| State | Attack | `combat_intent.cpp` | PARTIAL | aim/fire intent contract exists; no CSBot attack state, dodge, retreat or enemy lifecycle |
| State | Buy | `profile_loader`, compatibility commands | MISSING | no live purchase sequence/state |
| State | Defuse / Escape / Fetch / Plant | objective planner and action adapter | PARTIAL | bomb proposal and public buttons exist; only narrow C4 sensor path is wired |
| State | Follow / Hide / Hunt / InvestigateNoise / MoveTo / UseEntity | no corresponding state objects | MISSING | no live state implementations |
| Perception | trace/FOV visibility, visible body parts, recognition and reaction queue | `buildManagedWorldSnapshot`, `PerceptionAssembler` | DIFFERENT | live path enumerates hostile live edicts and chooses nearest target; no trace/FOV/noise/event feed |
| Events | `GameEventType` reactions and sound/listen behavior | user-message/menu/TeamInfo hooks | MISSING | message handling supports join/readiness, not CSBot event semantics |
| RNG | `RANDOM_*` source, call site order and branch consumption | `compat::EngineRandomSource`, `compat::ScriptedRandomSource`, `RNG_MODEL.md` | IMPLEMENTED_UNVERIFIED | boundary/callback/tape/trace are offline verified; 148 reference expressions remain unverified against AstraBot production decisions |
| NAV load | CSBot NAV file/area/place/hiding/encounter loading | `NavLoader`, `LegacyNavReader`, `NavSnapshot` | IMPLEMENTED_UNVERIFIED | independent read-only loader and snapshot pass offline tests; format/semantics not differential-verified |
| NAV route | path cost, route type, tie breaking and random choices | `NavQuery`, `NavRoamController` | DIFFERENT | actor/generation-derived deterministic starting index; no CSBot cost/RNG parity |
| NAV movement | path following, jump, ladder, crouch, stuck recovery | `locomotion`, `jump_drop`, `special_traversal`, movement physics | PARTIAL | contracts and offline tests exist; existing live evidence reports `roam_no_intent`/`Stuck` and no sustained movement |
| Combat | enemy choice, visibility, reaction, aim spot/error, fire cadence | `CombatController`, `ActionAdapter` | PARTIAL | controller can produce aim/fire intent; target visibility, profile skill and reaction are not sourced |
| Weapon | active weapon, ammo, recoil, reload, scope, silencer, grenades | `WeaponInventory`, action adapter | PARTIAL | runtime constructs a synthetic rifle record and maps reload; private weapon state unavailable |
| Objective | bomb pickup/carry, plant, guard, defuse, escape | `buildManagedObjectiveTarget`, `RoundObjectivePlanner` | PARTIAL | C4 entity/weapon-bit scan and `IN_ATTACK`/`IN_USE` boundary exist; live plant/defuse not observed |
| Objective | hostage, VIP/assassination, rescue and follow | objective schemas only | MISSING | no live sensor/action path for these scenarios |
| Economy/profile | BotProfile skill/aggression/teamwork/reaction and buy sequence | `ProfileCatalog`, `ProfileLoader` | PARTIAL | profile parsing/selection contracts exist; no CSBot runtime profile or buy behavior |
| Manager | quota, bot add/remove policy, team/scenario manager | `FakeClientManager`, compatibility command surface | MISSING | lifecycle commands exist; CSBot manager/quota semantics do not |
| Team/radio | radio command handling, help, chatter and behavior effects | `TeamReportBoard`, `RadioController` | PARTIAL | intent and cooldown contracts exist; no live radio/chatter dispatch |
| Private state | exact CBasePlayer/CBasePlayerWeapon/GameState access | public `edict_t` and engine APIs | DIFFERENT | many CSBot inputs are unavailable or inferred; see `OBSERVATION_MATRIX.md` |
| Differential oracle | replayable reference/Astra traces with exact state/RNG/command comparison | `tests/phase8_differential.py`, `docs/TRACE_SCHEMA.md` | IMPLEMENTED_UNVERIFIED | schema/rejection checks exist; pinned reference trace and RNG records are absent |
| Mode boundary | compatibility baseline isolated from Astra intelligence | `CvarState`, `RuntimeModePolicy`, `CompatibilitySurface`, `PluginRuntime::Snapshot` | IMPLEMENTED_UNVERIFIED | default `compatibility` mode, `astrabot_mode` selection, observable snapshot, and capability isolation tests; behavioral parity remains unproven |
| Safety boundary | actor generations, stale command rejection, native bot guard | lifecycle/queue/native guard | ASTRA_EXTENSION | useful Astra infrastructure; not a CSBot behavior match and must remain behavior-neutral |

## Current count

`MATCH 0`, `IMPLEMENTED_UNVERIFIED 6`, `PARTIAL 10`, `MISSING 7`,
`DIFFERENT 5`, `ASTRA_EXTENSION 1`, `NOT_APPLICABLE 0`.

These counts describe the rows above, not feature completeness or live acceptance.
