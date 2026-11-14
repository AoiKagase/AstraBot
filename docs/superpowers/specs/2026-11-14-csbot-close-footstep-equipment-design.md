# Close enemy footsteps and sniper equipment

User authorizes next missing CSBot behavior. Base3a7fd83; read-only oracle ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, CCSBot::OnEvent(cs_bot_event.cpp326–373), EquipPistol/DoEquip(cs_bot_weapon.cpp412–480), IsGameEventAudible(bot_util.cpp746–754).

## Approach
Extend existing public sound intake and WeaponEquipmentPlanner; no parallel weapon command transport. A perception-noise-memory trigger is rejected because native equips before memory replacement/NAV localization. PM_Move callback interception is a separate intake project and is not required for this public-event equipment slice.

Known standard material step samples become Footstep with native Low priority and1100-unit strict audible radius. Only fresh public sound from a current alive hostile player can create a per-observer equipment receipt; self/friendly/unknown/dead/stale sources do not. Record raw3D distance before randomization and memory admission; no LOS/FOV requirement, no invented firing/target identity. No additional per-frame world scan: only bounded managed-handle iteration per footstep.

At next existing full decision, a current sniper(Scout3,SG55013,AWP18,G3SG124) and rawdistance<300 may select an owned usable allowed pistol, even with a loaded/unknown sniper clip, low skill and no combat target. Existing five-second equipment interval and observed CurWeapon acknowledgement stay shared; no reload/aim/fire invented. Preserve existing USE/C4/grenade/freeze/death/disabled-AI priority. Receipt lifetime is bounded to0.25s to the next Full Update, explicitly an adapter scheduling limit. Actor/map/round/clock/death/reset discard receipts; no stale replay after equip confirmation, absent ammo or unavailable transport.

## Verification
Pure tests cover sample whitelist, range/priority, strict300, sniper IDs, skill/clip independence, pistol ammo/allow and5s interval. Runtime tests use public EmitSound+WeaponList/CurWeapon/AmmoX and actual client weapon command, hidden enemy, all4 sniper IDs, same-frame stronger noise, exactly300, friendly/self/stale/dead/disabled/round lifetime. Both x86 full builds and sequential CTests; frozen characterization unchanged; independent PE/ELF7exports. Existing Linux combat_policy failure remains separately identified. Live HLDS/ReHLDS, actual PM_Move-only footsteps and full native event scheduling remain unverified/unimplemented; no deployment.
