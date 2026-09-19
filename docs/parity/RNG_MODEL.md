# P03 RNG Reference Model

## Scope and result

This document records the ReGameDLL-CS RNG call inventory and the AstraBot
Compatibility boundary established in P03. It is an observation and test model,
not copied ReGameDLL implementation code and not a live-server parity claim.

- Reference repository: ReGameDLL-CS
- Reference commit: b0889847fe6d03898be88acc9e366660efb40ab5
- AstraBot start: 71cf00aaa9858855f0ebcff1df6544eaab3c7d4b
- Local ReHLDS source observation: 0124d56c3d888d922eb045775f71c6682ad1226f
- Direct CSBot-related source files: 24
- Direct executable RANDOM_* expressions: 148
- Direct FLOAT calls: 111
- Direct LONG calls: 37
- Direct bot-root calls to UTIL_SharedRandomFloat/UTIL_SharedRandomLong:
  none found
- Broader ReGameDLL random-bearing files audited for transitive context: 62

P03 result for the reference inventory is PARTIAL. The boundary, production
callback delegation, tape consumption, and trace metadata are verified
offline. No reference callsite is promoted to MATCH because the AstraBot
production behavior has not yet supplied branch, type, bounds, and order
evidence for that callsite.

## Reference RNG source

The ReGameDLL-CS call chain is:

~~~text
RANDOM_FLOAT(low, high)
    -> g_engfuncs.pfnRandomFloat(low, high)

RANDOM_LONG(low, high)
    -> g_engfuncs.pfnRandomLong(low, high)
~~~

The macro definitions are in regamedll/dlls/enginecallback.h. The callback
signatures are in regamedll/engine/eiface.h:

~~~text
int32 pfnRandomLong(int32 lLow, int32 lHigh)
float pfnRandomFloat(float flLow, float flHigh)
~~~

ReGameDLL-CS does not define a per-bot generator for these macros. The
reference GameDLL uses the engine callback table, so AstraBot's production
adapter forwards the same two callback types and does not introduce a seed,
per-Bot stream, or replacement algorithm.

The local ReHLDS source observation shows:

- one engine generator state containing idum plus static ran1 shuffle state;
- SeedRandomNumberGenerator initializes state from engine time;
- RandomFloat produces [low, high);
- RandomLong treats the requested integer interval as inclusive and uses
  rejection sampling over the engine range;
- the ReHLDS source invokes its engine destination callback before producing
  the returned local value.

This describes the inspected ReHLDS source tree only. The current P03
production contract delegates to the live engine function table and does not
copy this algorithm into AstraBot. A deployed HLDS/ReHLDS identity and live
trace are still required for runtime acceptance.

The shared utility implementation in regamedll/dlls/util.cpp
(UTIL_SharedRandomLong, UTIL_SharedRandomFloat, U_Random, U_Srand) is a
different seed-parameterized utility. No direct call to it was found under the
CSBot bot roots. Its uses elsewhere in weapon and GameDLL code remain
transitive audit candidates only when a CSBot execution path proves reachability.

## Semantic inventory

The line lists below are auxiliary evidence. The stable identifiers are the
semantic IDs in the first column. A row can contain several lexical
expressions implementing one semantic operation; the count column preserves
the number of expressions. Every row has an explicit classification. P03 uses
UNKNOWN conservatively whenever an AstraBot equivalent or exact downstream
influence is not proven.

| Semantic ID | Reference file and function/context | Lines | Type / bounds | Count | Condition and purpose | Scope | Classification | AstraBot equivalent / status |
|---|---|---:|---|---:|---|---|---|---|
| RNG-CSBOT-CORE-DAMAGE-PANIC | dlls/bot/cs_bot.cpp, CCSBot::TakeDamage / Panic | 151, 462, 488, 494, 497 | FLOAT: 0..100; -75..75; -offset..offset; -50..50; 0.1..0.2 | 5 | damage panic chance, panic displacement, surprise/spot timing | behavior | UNKNOWN | no production callsite; boundary only, UNVERIFIED |
| RNG-CSBOT-CORE-ROGUE | dlls/bot/cs_bot.cpp, rogue/timer decision helpers | 778, 783 | FLOAT: 10..30; 0..100 | 2 | rogue timer and rogue chance at their reference branches | behavior/manager | UNKNOWN | no equivalent, UNVERIFIED |
| RNG-CSBOT-CORE-COLLECTOR | dlls/bot/cs_bot.cpp, collector selection helper | 889 | LONG: 0..count-1 | 1 | select one collected candidate | behavior | UNKNOWN | no equivalent, UNVERIFIED |
| RNG-CSBOT-CHATTER-SELECTION | dlls/bot/cs_bot_chatter.cpp, phrase selection | 50 | LONG: 0..count-1 | 1 | choose a phrase from a non-empty collection | chatter | CHATTER_ONLY | no live chatter callsite, UNVERIFIED |
| RNG-CSBOT-CHATTER-TIMERS | dlls/bot/cs_bot_chatter.cpp, BotPhrase/BotChatter timing helpers | 953, 958, 1088, 1093 | FLOAT: 0.5..1; 3..4; 0.1..0.5; 1..2 | 4 | speech start and next-phrase timing | chatter | CHATTER_ONLY | no live chatter callsite, UNVERIFIED |
| RNG-CSBOT-CHATTER-PITCH | dlls/bot/cs_bot_chatter.cpp, voice pitch selection | 1230, 1233, 1236 | LONG: 105..110; 95..105; 85..95 | 3 | pitch by voice/situation branch | chatter | CHATTER_ONLY | no live chatter callsite, UNVERIFIED |
| RNG-CSBOT-CHATTER-CHOICE | dlls/bot/cs_bot_chatter.cpp, meme and radio phrase branches | 1860, 1892, 2113, 2122, 2129, 2132, 2151, 2260, 2270 | FLOAT: 0..100; 2..4; 2..5; branch-specific 0..100; 2..4; 0.5..1; 0.3..1 | 9 | chatter probability, phrase choice, and delayed speech | chatter | CHATTER_ONLY | no live chatter callsite, UNVERIFIED |
| RNG-CSBOT-EVENT-REACTION | dlls/bot/cs_bot_event.cpp, CCSBot::OnEvent helpers | 91, 95, 175, 178 | FLOAT: 10..; 0..100; 2..; 0..100 | 4 | event reaction delay/chance and temporary response | behavior/event | UNKNOWN | no event equivalent, UNVERIFIED |
| RNG-CSBOT-EVENT-NOISE | dlls/bot/cs_bot_event.cpp, noise error placement | 419, 420 | FLOAT: -errorRadius..errorRadius | 2 | perturb heard-noise position on event path | perception | UNKNOWN | no live noise callsite, UNVERIFIED |
| RNG-CSBOT-INIT-COMBAT-RANGE | dlls/bot/cs_bot_init.cpp, initialization | 161 | FLOAT: 325..425 | 1 | initialize combat range | behavior | UNKNOWN | no profile/runtime equivalent, UNVERIFIED |
| RNG-CSBOT-LISTEN-NOTICE | dlls/bot/cs_bot_listen.cpp, hearing decision | 91, 224, 228, 233 | FLOAT: branch-specific 0..; 5..; 2..; 1.. | 4 | notice and look-at duration after sound | perception | UNKNOWN | no live listen callsite, UNVERIFIED |
| RNG-CSBOT-MANAGER-ROUND | dlls/bot/cs_bot_manager.cpp, CCSBotManager::RestartRound | 121, 133 | FLOAT: 10..; 0..100 | 2 | earliest bomb plant time and defense-rush decision | manager | UNKNOWN | no manager equivalent, UNVERIFIED |
| RNG-CSBOT-MANAGER-TEAM-ZONE | dlls/bot/cs_bot_manager.cpp, CCSBotManager zone helpers | 1062, 1728; manager.h:167 | LONG: 0..1; 0..1; 0..zoneCount-1 | 3 | team/zone selection and random zone position | manager/nav | UNKNOWN | no manager/zone equivalent, UNVERIFIED |
| RNG-CSBOT-NAV-STUCK | dlls/bot/cs_bot_nav.cpp, CCSBot::StuckCheck/Wiggle | 108, 362, 363, 387 | FLOAT: 0..0.5; 0.5..1.5; 1..2; LONG: 0..3 | 4 | stuck jump, wiggle direction, and recovery timestamps | nav | UNKNOWN | NavRoamController differs; UNVERIFIED |
| RNG-CSBOT-NAV-REPATH | dlls/bot/cs_bot_pathfind.cpp, repath helper | 1615 | FLOAT: 0.4..0.6 | 1 | repath timer after path decision | nav | UNKNOWN | no equivalent, UNVERIFIED |
| RNG-CSBOT-STATE-DECISION | dlls/bot/cs_bot_statemachine.cpp, state transition helpers | 268, 321, 366 | FLOAT: 3..15; 0..100; 0.25+turn..1.5 | 3 | hold/crouch and aim-offset timing | state/combat | UNKNOWN | BehaviorStateMachine differs, UNVERIFIED |
| RNG-CSBOT-UPDATE-MORALE | dlls/bot/cs_bot_update.cpp, CCSBot update helpers | 690, 760, 765 | FLOAT: 0..1; 0..100; 3..15 | 3 | teamwork/morale branches and hide duration | behavior | UNKNOWN | no equivalent, UNVERIFIED |
| RNG-CSBOT-VISION-LOOK | dlls/bot/cs_bot_vision.cpp, look-around helpers | 422, 439, 478, 480 | FLOAT: 2..4; 2..3; 5..10; 1.. | 4 | inhibit/look-at/look-around timing | perception | UNKNOWN | no FOV/trace equivalent, UNVERIFIED |
| RNG-CSBOT-VISION-APPROACH | dlls/bot/cs_bot_vision.cpp, approach/danger selection | 488, 515, 572 | LONG: 0..approachCount-1; 0..dangerCount-1; FLOAT: 10..30 | 3 | choose approach/danger spots and spot-check delay | nav/perception | UNKNOWN | no equivalent, UNVERIFIED |
| RNG-CSBOT-VISION-BLIND | dlls/bot/cs_bot_vision.cpp, CCSBot::Blind/IsNoticable | 1142, 1148, 1151, 1289 | FLOAT: 0..100; LONG: 1..directions-1; FLOAT: 0..100; 0..100 | 4 | blind movement/fire and notice chance | perception/behavior | UNKNOWN | no equivalent, UNVERIFIED |
| RNG-CSBOT-WEAPON-TACTICAL | dlls/bot/cs_bot_weapon.cpp, weapon timing helpers | 115, 142, 160, 165, 737 | FLOAT: 0..100; 0.15..0.4; 0.3..0.7; 0.15..0.5; 0.. | 5 | knife chance, fire delay, and non-hiding timing | combat/weapon | UNKNOWN | production combat has no RNG callsite, UNVERIFIED |
| RNG-CSBOT-WEAPON-AIM | dlls/bot/cs_bot_weapon.cpp, aim offset helper | 211, 212, 213, 216 | FLOAT: -error..error x3; 0.25..upper timestamp | 4 | aim X/Y/Z and next aim timestamp | combat/aim | UNKNOWN | reference-shaped test fixture only, UNVERIFIED |
| RNG-CSBOT-OBJECTIVE-STATE | dlls/bot/cs_gamestate.cpp, scenario position helpers | 85, 448 | LONG: i..1; 0..1 | 2 | scenario/bomb or free-position choice | objective | UNKNOWN | narrow objective scan differs, UNVERIFIED |
| RNG-CSBOT-ATTACK-STATE | dlls/bot/states/cs_bot_attack.cpp, AttackState | 56, 57, 88, 136, 139, 204, 303, 396, 398, 524, 525, 527, 532 | FLOAT: 7..10; 2..10; 0..100; 3..15; 0.5..2; 3..15; 0.3..1; LONG: 0..attackStates-1/-2 | 13 | attack dodge, retreat, hide, ambush and next-state branches | combat/state | UNKNOWN | no attack overlay equivalent, UNVERIFIED |
| RNG-CSBOT-BUY-STATE | dlls/bot/states/cs_bot_buy.cpp, BuyState | 71, 84, 86, 93, 120, 125, 129, 396, 428, 438, 443, 478, 499, 519 | FLOAT: 0..100; 0..100; 0..100; 0..100; LONG: 0..1; 0..MAX_BUY_WEAPON_SECONDARY-1; branch-specific | 14 | buy chance, shield/grenade choice, random weapon selection | manager/economy | UNKNOWN | profile/catalog only, UNVERIFIED |
| RNG-CSBOT-FOLLOW-STATE | dlls/bot/states/cs_bot_follow.cpp, FollowState | 57, 97, 129, 184, 247 | FLOAT: 2..5; 1..3; 2..5; 1..3; LONG: 0..targetAreaCount-1 | 5 | idle/wait timing and target area selection | state/nav | UNKNOWN | no follow state, UNVERIFIED |
| RNG-CSBOT-HIDE-STATE | dlls/bot/states/cs_bot_hide.cpp, HideState | 40, 44, 52 | FLOAT: 30..60; 0..100; 3..10 | 3 | hide duration, side choice, hold position | nav/state | UNKNOWN | no hide state, UNVERIFIED |
| RNG-CSBOT-HIDE-CHATTER | dlls/bot/states/cs_bot_hide.cpp, HideState chatter calls | 319, 321, 323, 334 | FLOAT: 10..15; 5..8; 3..4; 10..15 | 4 | chatter encouragement delays | chatter | CHATTER_ONLY | no live chatter callsite, UNVERIFIED |
| RNG-CSBOT-HUNT-STATE | dlls/bot/states/cs_bot_hunt.cpp, HuntState | 197 | LONG: 0..areaCount-1 | 1 | hunt area selection | nav/state | UNKNOWN | no hunt state, UNVERIFIED |
| RNG-CSBOT-IDLE-STATE | dlls/bot/states/cs_bot_idle.cpp, IdleState | 206, 209, 362, 408, 439, 466, 529, 532, 560, 566, 617, 639, 663, 666, 677, 814, 841, 867, 870 | FLOAT: 0..100; 10..30; branch-specific 0..100; 50; 10..30; 0..100; 0..100; 0..100; 0..100; 10..30 | 19 | camp/guard/hunt/hide decisions and durations | state/nav/behavior | UNKNOWN | NavRoamController differs, UNVERIFIED |
| RNG-CSBOT-MOVE-STATE | dlls/bot/states/cs_bot_move_to.cpp, MoveToState | 230 | LONG: 0..3 | 1 | relative movement direction choice | nav/state | UNKNOWN | no MoveTo state, UNVERIFIED |
| RNG-CSBOT-PROFILE-SELECT | game_shared/bot/bot_profile.cpp, BotProfileManager::Init/GetRandomProfile | 275, 606 | LONG: 0..2; 0..1 | 2 | profile preference and random valid profile selection | manager/profile | UNKNOWN | ProfileCatalog is not runtime CSBot selection, UNVERIFIED |
| RNG-CSBOT-NAV-AREA | game_shared/bot/nav_area.cpp, CNavArea selection helpers | 831, 2135, 3249, 3260, 3294, 3358, 3369, 3385, 3440, 3555 | LONG: 0..100; 0..1; 0..count-1 x7; 0..directions-1 | 10 | hiding/area/connection/direction selection | nav | UNKNOWN | NavQuery differs, UNVERIFIED |
| RNG-CSBOT-NAV-AREA-HEADER | game_shared/bot/nav_area.h, inline area selection | 1135, 1141 | LONG: 0..cheapAreaSetCount-1; 0..numAreas-1 | 2 | inline cheap-area/random-area choice | nav | UNKNOWN | NavQuery differs, UNVERIFIED |

The table counts sum to 148. The category total is deliberately conservative:

| Classification | Count | Reason |
|---|---:|---|
| COMPAT_BEHAVIOR_CRITICAL | 0 promoted | no AstraBot production branch/order proof yet |
| COMPAT_MANAGER_CRITICAL | 0 promoted | manager/profile influence is mapped but not reproduced |
| COMPAT_NAV_CRITICAL | 0 promoted | Nav source differs and no call-order proof exists |
| COSMETIC_ONLY | 0 promoted | global-stream effect prevents ignoring an unproven call |
| CHATTER_ONLY | 21 | direct chatter selection/timing/pitch/encouragement purpose is explicit |
| DEBUG_ONLY | 0 | no direct debug-only callsite identified |
| ENHANCED_ONLY | 0 | no reference callsite classified as Astra-only enhancement |
| UNKNOWN | 127 | direct reference use is known, but equivalent branch/order influence is unproven |

## Consumption and order constraints

1. Within a reference function, calls occur only when the surrounding branch is
   entered. Moving a request outside that branch is a parity defect.
2. Bounds retain their original type and expression. No float normalization or
   LONG-to-float probability conversion is allowed.
3. A single function with multiple calls retains lexical call order.
4. Manager-level calls share the same global engine stream as Bot calls.
5. Multi-Bot ordering follows the server-frame/update order; the Compatibility
   source is shared, not per-Bot.
6. Chatter calls remain inventory items even when their visible result is
   cosmetic, because a global stream call can shift later behavior calls.
7. Enhanced calls use a separate source instance and cannot advance the
   Compatibility sequence.

## AstraBot P03 boundary

The following offline evidence is MATCH for the boundary only:

- ScriptedRandomSource forwards exact request type and bounds.
- Tape detects exhaustion, mismatch, too many calls, and too few calls.
- Branch-dependent callers consume only when the branch executes.
- One shared scripted source preserves A/B/A sequence order.
- EngineRandomSource calls the public engine callback exactly once and fails
  closed when unavailable.
- PluginRuntime owns one production source configured from the engine table.
- Optional trace records carry global sequence, semantic ID, type, bounds,
  result, actor, and timing context.

The following remain UNVERIFIED or DIFFERENT:

- no production AstraBot decision currently consumes the Compatibility source;
- current NavRoamController uses actor/generation-derived deterministic choice;
- private CSBot profile, weapon, GameState, vision, and state objects are not
  available through the public adapter;
- no pinned reference RNG tape or live differential trace exists;
- live HLDS/ReHLDS acceptance is not performed by this phase.
