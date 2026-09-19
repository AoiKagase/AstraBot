# P00 GameDLL Observation Matrix

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
