# Vanilla CS parity repair ledger

Authority: [STATUS.md](STATUS.md). Branch `codex/vanilla-csbot-parity`.
Base AstraBot `819471d`; behavioral reference ReGameDLL-CS
`b0889847fe6d03898be88acc9e366660efb40ab5`.

## Scope

Vanilla public Engine/messages/sounds are mandatory. ReGameDLL is optional.
No pdata, offsets, private layouts, NAV generation/learning, deployment, server
configuration, staging, commit or push. Core stays SDK independent and exports
stay unchanged. Unrelated files and generated artifacts are preserved.

Later user-authorized build/redeployment work is recorded separately in
STATUS.md; the original implementation scope above does not override that
authorization. The 2026-11-07 C4/congestion follow-up extends D10 with polite
friend waiting and crowd-cost replanning, and D13 with planting-input isolation.
Both have offline regression evidence; native acceptance remains pending.

Nov8 follow-up(D08/D10/D13): patience-expiry movement before invalidation,
live friendly NAV occupancy, public Touch priority/make-way behavior, and
thin-destination portal containment. See STATUS.md for exact reference
functions, offline regressions, remaining observation limits, and native
recheck scenarios1735 and18->69. Same-route fallback remains legal when no
cheaper valid route exists. No Compatibility reservations were added.

Public protocol cross-checks used YaPB `4967a22`, RealBot `2f9d5c7`, PODBot
`75f61e1` and SyPB `4c364fb`; their AI/private-data implementations were not
adopted. AI decisions use the fixed CSBot behavioral reference above.

The implementation inventory below is not nineteen proven reference matches.
Trace replay and live acceptance remain open for every row. Unimplemented work
must not be classified as a public-information limitation. D01, D12 and D09 were
registered as the three highest priorities at implementation start.

## D01-D19 implementation inventory

Test names below omit the common `astrabot_` prefix. Runtime integration is in
`src/adapter/metamod/plugin/plugin_runtime.cpp`; helpers are SDK-free Core headers.

| ID | Difference; repair stage | Implemented treatment | Legacy contract; checks |
|---|---|---|---|
| D01 | Synthetic loaded rifle; 1/5 | PublicObservationStore/ObservationAdapter actual WeaponList/CurWeapon/AmmoX input. Unknown clip/reserve/reload stays unknown; pickups do not double-add ammo. | P04/P08; public_observation, observation_adapter, compat_actor_command |
| D02 | Perfect aim; 3/5 | Profile error/tracking, visible body target, reaction/AttackDelay, sniper stop/scope receipt and finite retry. | P06/P08; combat_intent, vanilla_behavior, weapon_actions |
| D03 | Friendly fire false; 5 | Public teammate trace before firing; grenade line/hull/floor prediction. Prediction is not landing evidence. | P06/P08; combat_intent, compat_actor_command, grenade_trajectory |
| D04 | Nearest enemy without reaction; 3 | Recognition queue and visible attacker attribution feed profile reaction and acquisition delay. | P06; vanilla_behavior, perception_adapter |
| D05 | Smoke; 3 | Smoke chord occlusion/lifetime and flash/FOV gates affect enemy/objective/scenario knowledge. | P06; vanilla_behavior, compat_actor_command |
| D06 | Missing sound/events; 1/3 | Dynamic public messages, sound hooks, recipient isolation, chronological ring and duplicate suppression. | P04/P06; public_observation, perception_adapter, metamod_hooks |
| D07 | States only trace; 3 | StateBehaviorDecision semantic actions drive Move/Use/Buy/Attack execution; buying runs on full update. State-by-state reference ordering still needs replay. | P05; compat_state_machine, scenario_behavior, compat_actor_command |
| D08 | Static NAV cost; 4 | HP/aggression/attack, transient team danger and visible friendly occupancy feed route costs. | P07; nav_query, nav_danger_memory, nav_roam_controller |
| D09 | Compatibility reservations; 4 | Goal/link reservations and Coordinator optimization remain Enhanced-only. | P07; nav_roam_controller |
| D10 | Frame stuck/deterministic Wiggle; 4 | Timed five-speed monitor, ladder threshold, shared RNG Wiggle/jump and 75-unit recovery. Recovery jumping no longer requires a never-populated clearance flag. Five-second ground corridor progress timeout preserves failed-link exclusion; grounded entry into the next narrow area ends Wiggle. Airborne/special traversal remains protected. | P07; locomotion, nav_roam_controller, compat_actor_command; 2026-11-07 native replay pending |
| D11 | Independent frame retry; 4 | Compatibility uses elapsed time; legacy Enhanced failure policy remains separate. | P07; nav_roam_controller |
| D12 | Global C4 knowledge; 1/6 | Per-actor sight/hearing/notification gate; recipient-specific T BombDrop/BombPickup radar knowledge permits hidden dropped-C4 recovery. T planted position requires public plant notice. Unknown-position CT searches known sites. | P04/P06/P09; public_observation, public_bomb_clock, bomb_navigation, compat_actor_command |
| D13 | Site-directed preplant; 2/6 | Shared round 10..30 second draw; Compatibility hunts before decision and chooses site afterwards. Carrier change does not redraw. | P03/P09; vanilla_behavior, compat_actor_command |
| D14 | Central objective owner; 6 | Compatibility local choice, Enhanced Coordinator optimization, human defuse lease yields interaction. | P09; team_coordinator, scenario_behavior |
| D15 | Hostage/VIP/escape/follow/radio; 6 | Public VIP, finite hostage retry/inferred escort, human radio/follow; public estimated bomb clock/NAV travel, uncleared site search and both-team escape with knife; postplant Hide chooses persistent unoccupied NAV cover and stops on arrival; Profile Teamwork drives rogue/follow/stop decisions. | P09; scenario_behavior, public_bomb_clock, bomb_navigation, teamwork_policy, compat_actor_command |
| D16 | Buying absent; 7 | Public money/equipment/zone/ArmorType, profile purchase order, observed receipts, bounded retries and timeout diagnostics. | P10; vanilla_management, public_observation |
| D17 | Deterministic/in-use profiles; 2 | Preserved ReactionTime/AttackDelay/Teamwork/preferences; unused eligible profile random selection. | P03/P10; profile_loader, vanilla_management |
| D18 | Quota only Add; 7 | Timed fixed/fill/match, join window, autovacate, manual quota accounting/team/removal rules. Standard radio uses host-provided metadata and delayed team scheduling. | P10; bot_configuration, compat_surface, outgoing_radio, fake_client_isolation |
| D19 | RNG ownership/order; 2/8 | Named call sites, shared round ownership and scripted type/range/actor/timing/order/exhaustion checks. Full reference-wide consumption equivalence remains unverified. | P03/P11; compatibility_rng, vanilla_behavior, bomb_navigation; reference traces still required |

## Capabilities and remaining limits

The optional official ReGameDLL API exposes metadata/hooks/GameRules/playerMove,
but no safe per-actor exact reload/timer/accuracy getter. Negotiation therefore
reports MetadataOnly and preserves Vanilla values. See
[REGAMEDLL_CAPABILITIES.md](REGAMEDLL_CAPABILITIES.md).

Plant notice plus `mp_c4timer` and NAV cost/public maxspeed are explicit
estimates, never exact internal countdown/travel values. Unknown kit stays
unknown. Hostage motion is inferred ownership, not an authoritative follower
receipt. Custom chatter voice playback is not implemented: only public standard
radio equivalents from the host's own BotChatter.db are sent. Missing mappings
are diagnosed rather than invented. Complete grenade bounce/landing outcomes
still need live acceptance. The human's private auto-follow permission timer is
not read: accepted public radio supplies an explicitly inferred permission proxy.

Secondary retry (0.5 seconds, at most three sends) is transport recovery policy,
not a recovered CSBot cooldown. Repeated flashbang cycles require a fresh public
reserve-decrease receipt; a command request alone cannot rearm the cycle.

## Stage mapping and verification

Repair 0 covers the inventory/authority; 1 maps to P04/P06; 2 to P03/P10;
3 to P05/P06; 4 to P07; 5 to P08; 6 to P09; 7 to P10; 8 to P11/P12.
These labels do not renumber the historical .planning phases.

Final build/test/export receipts belong in STATUS.md. Intermediate Windows
64/65 was followed by a passing corrected airborne integration test. The fixture
now integrates gravity and stays outside goal-arrival tolerance during flight.
Linux x86 diagnostic values proved correct float32 storage but unequal x87
excess-precision literal comparisons; assertions now compare stored bits exactly,
without epsilon or new SSE2 requirements. Commands compare string content rather
than cross-translation-unit literal addresses.

FocalSpan/graph are navigation aids, not live proof. Graph affected-flow coverage
is absent. Parent-side FocalSpan update works; fixed reference excerpts are read
without modifying that checkout. Generated indexes are not staged.

## Open acceptance

Recorded P11 reference replay, Vanilla Windows/Linux x86 live scenarios,
ReGameDLL enabled/absent, supported maps, 1v1/2v2/multi-Bot stability/FPS and
P12 acceptance remain open. Full match is not claimed. Every ID needs those
receipts or an actual separately justified public-information limit before
being marked fixed and verified. Passing offline tests does not close this gate.
The comparison CLI and its input contract are documented in
[TRACE_REPLAY.md](TRACE_REPLAY.md). Its synthetic tests verify the comparator;
production capture/normalizer and native reference replay are still open work.
