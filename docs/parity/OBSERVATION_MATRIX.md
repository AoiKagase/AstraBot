# P00 GameDLL Observation Matrix

## P06 perception observations

| Observation | Reference consumer | AstraBot source / quality | Result |
|---|---|---|---|
| PERCEPT-VISION-FOV | FInViewCone / IsVisible(..., CHECK_FOV) | public edict_t angles plus SDK-free cone evaluator | IMPLEMENTED_OFFLINE_VERIFIED |
| PERCEPT-VISION-LOS | UTIL_TraceLine, fraction and self ignore | public pfnTraceLine; trace result is held at the adapter boundary | IMPLEMENTED_UNVERIFIED |
| PERCEPT-VISION-BODY | chest/head/feet/left/right masks | ActorObservation.visibleParts and MemorySample.visibleParts | IMPLEMENTED_OFFLINE_VERIFIED |
| PERCEPT-ENEMY-ACQUIRE | visible hostile scan plus reaction queue | visible hostile contacts only; Full Update-scoped | PARTIAL |
| PERCEPT-ENEMY-LAST-SEEN | m_lastEnemyPosition / timestamp | actor-generation-scoped MemorySample | IMPLEMENTED_OFFLINE_VERIFIED |
| PERCEPT-NOISE-HEAR | audible hostile GameEvent path | AudibleEvent with bounded distance/priority | IMPLEMENTED_OFFLINE_VERIFIED |
| PERCEPT-NOISE-PRIORITY | recent higher-priority or nearer replacement | 3-second replacement window, priority then distance | IMPLEMENTED_OFFLINE_VERIFIED |
| PERCEPT-NOISE-EXPIRE | ForgetNoise after 20 seconds | configurable bounded retention and reaction delay | IMPLEMENTED_OFFLINE_VERIFIED |
| PERCEPT-EVENT-DEATH | OnEvent(EVENT_PLAYER_DIED) | event mutation downgrades remembered contact | IMPLEMENTED_UNVERIFIED |
| PERCEPT-EVENT-ROUND | round lifecycle / CSGameState reset | round/map generation and explicit round events clear bounded belief | IMPLEMENTED_UNVERIFIED |
| PERCEPT-GROUND-TRUTH-LEAK | CSBot only uses recognized information | hidden entity position is scrubbed before Core publication | PASS boundary |

EXACT_ENGINE_API still describes acquisition only. Trace, smoke, private
reaction queues, and live GameEvent timing are not MATCH claims.

## P04 semantic observation inventory

This table is the P04 source of truth. Reference names are stable semantic
identities and function/member names; source line numbers are supporting
evidence only. `EXACT_ENGINE_API` means public collection parity, not CSBot
behavioral MATCH.

| Semantic ID | Category | Reference member/API and consumer | Astra source | Status | Delay | Risk / notes |
|---|---|---|---|---|---|---|
| `OBS-PLAYER-HEALTH` | player | `CCSBot`/`CBasePlayer` health checks in `cs_bot.cpp` | `ObservationAdapter::collectActor` `entity->v.health` | `EXACT_ENGINE_API` | same tick | high; life/combat gate |
| `OBS-PLAYER-ARMOR` | player | player armor used by weapon/damage decisions | `entity->v.armorvalue` | `EXACT_ENGINE_API` | same tick | high; private consumer semantics differ |
| `OBS-PLAYER-TEAM` | player | `m_iTeam`/team checks across `cs_bot.cpp` and GameState | public team plus TeamInfo/requested-team fallback | `DELAYED` | message/cache | high; raw team zero is not active team |
| `OBS-PLAYER-LIFE` | player | `IsAlive`, `deadflag`, spawn checks | `deadflag`, health, spectator flags | `EXACT_ENGINE_API` | same tick | high; lifecycle semantics still differ |
| `OBS-PLAYER-ORIGIN` | player | `pev->origin` in movement, danger, target, NAV | `entity->v.origin` | `EXACT_ENGINE_API` | same tick | high; physics feedback differs |
| `OBS-PLAYER-VELOCITY` | player | `pev->velocity` in movement/stuck/vision | `entity->v.velocity` | `EXACT_ENGINE_API` | same tick | high; no CSBot feedback equivalence |
| `OBS-PLAYER-VIEW-ANGLES` | player | `pev->v_angle` in aim/visibility | `entity->v.v_angle` | `EXACT_ENGINE_API` | same tick | high; aim state differs |
| `OBS-PLAYER-PUNCH-RECOIL` | player | punch/recoil state in weapon/vision paths | no P04 public mapping | `NOT_YET_IMPLEMENTED` | unavailable | high; do not default zero |
| `OBS-PLAYER-FOV` | player | `CCSBot::IsVisible(... CHECK_FOV)`, `m_iFOV` | `entity->v.fov` | `EXACT_ENGINE_API` | same tick | high; scope/internal FOV not matched |
| `OBS-PLAYER-DUCK` | player | posture/hull checks in movement/vision | no adapter field yet | `NOT_YET_IMPLEMENTED` | unavailable | medium |
| `OBS-PLAYER-GROUND` | player | ground/stuck/jump movement checks | `FL_ONGROUND` from public flags | `EXACT_ENGINE_API` | same tick | high; physics timing differs |
| `OBS-PLAYER-WATER` | player | water movement state | `entity->v.waterlevel` | `EXACT_ENGINE_API` | same tick | medium |
| `OBS-PLAYER-LADDER` | player | ladder movement state | no adapter ladder mapping | `NOT_YET_IMPLEMENTED` | unavailable | medium |
| `OBS-PLAYER-MAX-SPEED` | player | movement speed/weapon movement constraints | `entity->v.maxspeed` | `EXACT_ENGINE_API` | same tick | medium |
| `OBS-PLAYER-BUTTONS` | player | current input/button decisions | `entity->v.button` and command boundary | `EXACT_ENGINE_API` | same tick | high; command cadence differs |
| `OBS-PLAYER-OLD-BUTTONS` | player | latched input transitions | `entity->v.oldbuttons` | `EXACT_ENGINE_API` | same tick | medium |
| `OBS-PLAYER-SOLID` | player | collision/readiness | `entity->v.solid` | `EXACT_ENGINE_API` | same tick | high; GameDLL spawn lifecycle differs |
| `OBS-PLAYER-MOVETYPE` | player | movement mode/ladder/spectator | `entity->v.movetype` | `EXACT_ENGINE_API` | same tick | high |
| `OBS-PLAYER-MINS` | player | player hull/bounds | `entity->v.mins` | `EXACT_ENGINE_API` | same tick | medium |
| `OBS-PLAYER-MAXS` | player | player hull/bounds | `entity->v.maxs` | `EXACT_ENGINE_API` | same tick | medium |
| `OBS-PLAYER-SHIELD` | player | no required CSBot private decision consumer identified | no source required by P04 | `NOT_REQUIRED` | n/a | revisit only with consumer evidence |
| `OBS-PLAYER-ACTIVE-WEAPON` | weapon | `m_pActiveItem` / weapon pointer consumers | no public private pointer | `UNAVAILABLE` | n/a | critical combat parity gap |
| `OBS-WEAPON-CLIP` | weapon | `m_iClip`, `CBasePlayerWeapon` fire/reload | no public private state | `UNAVAILABLE` | n/a | critical |
| `OBS-WEAPON-RESERVE-AMMO` | weapon | `m_rgAmmo`/reserve ammo | no public private state | `UNAVAILABLE` | n/a | critical |
| `OBS-WEAPON-RELOAD` | weapon | reload state/timers in `cs_bot_weapon.cpp` | no portable public timer | `UNAVAILABLE` | n/a | critical |
| `OBS-WEAPON-NEXT-PRIMARY` | weapon | `m_flNextPrimaryAttack` | no portable public timer | `UNAVAILABLE` | n/a | critical time-domain gap |
| `OBS-WEAPON-NEXT-SECONDARY` | weapon | `m_flNextSecondaryAttack` | no portable public timer | `UNAVAILABLE` | n/a | critical time-domain gap |
| `OBS-WEAPON-ACCURACY` | weapon | weapon accuracy/spread state | no public private state | `UNAVAILABLE` | n/a | critical; never use zero default |
| `OBS-WEAPON-FLAGS` | weapon | weapon flags/silencer/burst state | no public private state | `UNAVAILABLE` | n/a | high |
| `OBS-WEAPON-SILENCER` | weapon | weapon silencer state | no public private state | `UNAVAILABLE` | n/a | high |
| `OBS-WEAPON-BURST` | weapon | burst mode state | no public private state | `UNAVAILABLE` | n/a | medium |
| `OBS-WEAPON-ZOOM` | weapon | scoped/zoom state and `m_iFOV` coupling | no public private state | `UNAVAILABLE` | n/a | critical FOV/weapon gap |
| `OBS-OBJECTIVE-C4-POSSESSION` | objective | `GetBomber`, GameState bomb carrier | `entity->v.weapons` C4 bit | `INFERRED` | same tick | public proxy, not carrier identity |
| `OBS-OBJECTIVE-BOMB-DROPPED` | objective | `CSGameState::IsBombLoose` / loose bomb entity | public entity scan | `INFERRED` | same tick | heuristic |
| `OBS-OBJECTIVE-BOMB-PLANTED` | objective | `CSGameState::IsBombPlanted` | classname/model/dmgtime adapter | `INFERRED` | same tick | not private GameState |
| `OBS-OBJECTIVE-BOMB-POSITION` | objective | `CSGameState::GetBombPosition` | planted entity `origin` | `INFERRED` | same tick | visibility/identity differ |
| `OBS-OBJECTIVE-BOMB-TIMER` | objective | GameState bomb timer | planted entity `dmgtime` | `INFERRED` | same tick | timer semantics unproven |
| `OBS-OBJECTIVE-DEFUSING` | objective | `CCSBot::IsDefusingBomb` | no public state | `UNAVAILABLE` | n/a | critical |
| `OBS-OBJECTIVE-DEFUSE-KIT` | objective | player defuse-kit state | no public state | `UNAVAILABLE` | n/a | high |
| `OBS-OBJECTIVE-BOMB-ZONE` | objective | bomb target/zone rules | public `func_bomb_target` bounds/NAV overlap | `INFERRED` | same tick | site identity/cost differ |
| `OBS-OBJECTIVE-HOSTAGE` | objective | `CHostage`, escort/rescue GameState | no live observation path | `UNAVAILABLE` | n/a | scenario gap |
| `OBS-OBJECTIVE-RESCUE-ZONE` | objective | hostage rescue zone/rules | no live observation path | `UNAVAILABLE` | n/a | scenario gap |
| `OBS-OBJECTIVE-VIP` | objective | VIP rules and escape state | no live observation path | `UNAVAILABLE` | n/a | scenario gap |
| `OBS-OBJECTIVE-ROUND-STATE` | objective | `CSGameState` round/scenario state | no public private state | `UNKNOWN` | n/a | source consumer mapping incomplete |
| `OBS-TRACE-LINE` | trace | `CCSBot::IsVisible` / `UTIL_TraceLine` | no adapter trace collection yet | `NOT_YET_IMPLEMENTED` | unavailable | critical visibility gap |
| `OBS-TRACE-HIT-ENTITY` | trace | trace result entity | no adapter trace collection yet | `NOT_YET_IMPLEMENTED` | unavailable | high |
| `OBS-TRACE-HIT-POSITION` | trace | trace result end/hit position | no adapter trace collection yet | `NOT_YET_IMPLEMENTED` | unavailable | high |
| `OBS-TRACE-FRACTION` | trace | trace result fraction/occlusion | no adapter trace collection yet | `NOT_YET_IMPLEMENTED` | unavailable | high |
| `OBS-VISIBILITY` | trace | FOV/body-part visibility checks | hostile alive enumeration + nearest distance | `INFERRED` | same tick | hidden enemies can be visible |
| `OBS-ENTITY-CLASSNAME` | entity | entity/class scenario scans | public `pfnSzFromIndex` | `EXACT_ENGINE_API` | same tick | semantic consumer differs |
| `OBS-ENTITY-MODEL` | entity | bomb/projectile/model checks | public `pfnSzFromIndex` | `EXACT_ENGINE_API` | same tick | heuristic use |
| `OBS-ENTITY-BOUNDS` | entity | `absmin/absmax` and objective geometry | public bounds/size fields | `EXACT_ENGINE_API` | same tick | site semantics differ |
| `OBS-ENTITY-DOOR-USE` | entity | doors/use entities and interaction | no observation path | `UNAVAILABLE` | n/a | objective/navigation gap |
| `OBS-ENTITY-PROJECTILE` | entity | grenade/projectile state | classname/model entity heuristic | `INFERRED` | same tick | no private projectile state |
| `OBS-PROFILE-SELECTED` | profile | `BotProfileManager` selection | `ProfileLoader`/`ProfileCatalog` | `DELAYED` | load/update | selection and RNG differ |
| `OBS-PROFILE-SKILL` | profile | aggression/skill consumers in state/weapon | parsed profile not applied to decisions | `NOT_YET_IMPLEMENTED` | unavailable | high |
| `OBS-TEAM-POPULATION` | scenario | CSBot manager population/team state | no equivalent manager observation | `UNKNOWN` | n/a | manager gap |
| `OBS-ROUND-FREEZE` | scenario | freeze-period GameRules state | no public private state | `UNKNOWN` | n/a | timing/objective gap |
| `OBS-ROUND-WIN-CONDITION` | scenario | GameRules scenario win state | no public private state | `UNKNOWN` | n/a | scenario gap |

## Classification counts

| Status | Count |
|---|---:|
| `EXACT` | 0 |
| `EXACT_ENGINE_API` | 19 |
| `EXACT_GAME_API` | 0 |
| `DELAYED` | 2 |
| `INFERRED` | 8 |
| `APPROXIMATED` | 0 |
| `UNAVAILABLE` | 17 |
| `NOT_YET_IMPLEMENTED` | 8 |
| `NOT_REQUIRED` | 1 |
| `UNKNOWN` | 4 |
| **Total rows** | **59** |

These counts are an inventory result, not a behavioral parity score. Critical
rows remain unavailable, inferred, or unimplemented, so P04 is `PARTIAL`.

This matrix separates what CSBot reads in-process from what AstraBot can observe
through the public Metamod/HLSDK boundary. `Exact` means the field is read at the
hook boundary; it does not mean the higher-level meaning is equivalent.

| CSBot input class | Reference access | AstraBot current source | Classification | Parity risk |
|---|---|---|---|---|
| Time/frame | `gpGlobals->time`, bot timers | `globals_->time`, `adapterFrameCount_` in `onStartFrame` | exact | cadence/order differs |
| Self position/velocity | `pev` and player internals | `entity->v.origin`, `v.velocity`, mins, flags | exact | physics feedback is sampled after a different command loop |
| Life/spawn | `CBasePlayer`/GameDLL spawn state | `deadflag`, health, `FL_SPECTATOR`, solid/movetype/effects | exact at boundary | GameDLL spawn readiness is delayed and inferred from public fields |
| Team | GameDLL player/team state | `v.team`, TeamInfo/requested-team fallback | delayed / inferred | raw `v.team` can remain zero; fallback is not private state |
| View/FOV/posture | `m_iFOV`, look state and posture members | `v_angle`, `angles`, flags, duck/ground observations | partial / unavailable | scope/FOV and CSBot look state are not available |
| Active weapon | `CBasePlayerWeapon` pointer and private fields | no private pointer; synthetic `WeaponRecord` in `decideManagedBotAction` | unavailable / synthetic | weapon selection, ammo, recoil, scope and silencer cannot match |
| Ammo/reload | `m_rgAmmo`, weapon clip/reload timers | `WeaponInventory` contract only; public reload command | unavailable | reload decisions are not grounded in GameDLL state |
| Enemy visibility | trace/FOV/body-part checks | hostile alive edict enumeration and nearest-distance selection | inferred | hidden enemies can become visible to Astra; visible body parts are absent |
| Enemy memory/reaction | CSBot recognized-enemy queue and timestamps | bounded `PerceptionAssembler` memory schema | partial | no live event/trace feed or CSBot reaction timing |
| Noise/hearing | `soundent`/CSBot listen code | audible-event schema only | unavailable in live path | InvestigateNoise cannot be accepted |
| Bomb carry | GameDLL player weapon/private state | `v.weapons & C4 bit` | inferred | bit is a public proxy, not full scenario state |
| Bomb planted | `CSGameState` and bomb entity | class/model/dmgtime/entity scan | inferred | entity heuristic is not full `CSGameState` |
| Bomb sites | GameDLL zone/state plus NAV places | `func_bomb_target` entity extents overlapped with loaded NAV | inferred | site identity and route choice differ |
| Hostage/VIP/scenario | `CSGameState`, hostage classes and rules | no corresponding live observation path | unavailable | hostage, VIP, rescue and escape parity are open |
| Profile/skill | in-process `BotProfile` | parsed `BotProfile.db`/loader record | delayed / partial | current runtime combat does not apply profile skill/aggression |
| Radio/chatter | CSBot radio/chatter objects and GameEventType | report/radio intent data structures | unavailable in live path | no delivery, interpretation or timing evidence |
| NAV internal state | `CNavArea`, places, hiding/encounter spots and path internals | read-only legacy NAV document/query/follower | inferred / partial | format, costs, ties, hiding and learning differ |
| Damage/death confirmation | GameDLL callbacks and player state | public health/deadflag plus existing log evidence | delayed | no complete combat feedback loop |

The highest-risk rows are active weapon/private player state, visibility/FOV,
GameState/objectives, NAV internals, and RNG. They must be resolved or explicitly
bounded before any `MATCH` claim.
