## 2026-11-14 native gunshot ingestion and seconds-based sound memory

Source base 6fc332d; read-only behavioral oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5. Independently implemented
public-event intake and hearing-time slice; this supersedes the upstream
frame-clock/PlaybackEvent limitations recorded in the earlier auditory-look entry.

- A public PrecacheEvent post table maps 25 standard gun scripts to the final
  engine index (including override results). META_FUNCTIONS supplies this private
  provider without adding an eighth exported symbol. Public PlaybackEvent observes
  zero-delay server firing without changing playback or inventing attack commands.
  Unknown/non-gun scripts, client/host-only events, invalid/dead/non-player/reused
  invokers fail closed. Precache entries survive activation and rounds, then clear
  on map deactivation, detach and fresh attach.
- Native gun receipts are High priority: ordinary guns 2000 units, TMP1000,
  AWP99999; M4A1 bparam1 and USP bparam2 select silenced1000-unit range. Position
  comes from public invoker origin, rather than the native zero event origin.
  Per-observer strict-radius, self/team and source-generation filters feed the
  shared bounded ring. Dual EmitSound/PlaybackEvent receipts coalesce; incidental
  same-frame weapon audio cannot suppress a High shot or widen its known range.
- Perception receives global seconds and profile ReactionTime. Noise becomes ready
  at age >=reaction, remains recent for <3 seconds, and expires only after20 seconds.
  Recent replacement requires equal/higher priority AND strictly closer distance
  measured from the current observer. Receipt time is retained across delayed
  publication; stale/future receipts and clock rollback cannot renew memory.
  Tick IDs remain frame identities/diagnostics. Clock-less offline inputs retain
  their legacy tick policy. Explicit audibility range permits valid AWP endpoints
  more than32768 units apart; diagnostic tick age saturates independently.
- New tests cover20/100/500-FPS equivalent timing and3/20-second boundaries,
  moving observers, actual pre/post callbacks, override selection, silencer range,
  AWP beyond2000/32768, duplicates, teams, serial reuse, map resets and preactivation
  precache preservation. Independent review found host-only events audible to enemies
  and an explicit-AWP validation cap; parent also reproduced invalid serial receipts.
  All three regression assertions failed RED before their fixes and pass GREEN.
  Linux exposed a C++14 constexpr-reference linkage issue, fixed with a value argument.
  No remaining Critical/Important findings or deferred minors.

Offline evidence: complete MSVC HostX86/x86 Debug build PASS, CTest **98/98 PASS**
(9.69 seconds). Complete Debian Linux x86 Debug build PASS, CTest **96/97 PASS**
(11.47 seconds). The only failure is the pre-existing astrabot_combat_policy
weapon/skill/recoil chest-policy case recorded in
[the coding-conventions baseline](../verification/coding-conventions-20261113/VERIFICATION.md).
Combat-policy source/expectations are unchanged. Frozen runtime characterization
passes unchanged on both OSes; it was not regenerated from the candidate. Source
manifest, ownership, ports and whitespace gates pass. PE and ELF separately PASS
x86 and seven exact exports. All258 recorded source inputs and root BotProfile.db
remain unchanged across verification.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: 333e4b75a8d06db9d9991df794c93c61962c147f709126158049ba94e9d4299c
- Linux SO: bc5f47057a7c221957c0ec2056d88c65d0094f915e8c36abd605e48acbb5383e

Evidence: .agent-state/sessions/csbot-gunshot-hearing-time-bde84cb0-bce1-442d-8782-5bc804c44bdf/ (RESULT.json, tested-source-identity.json,
review.md, RED logs, compiler identity, complete builds and sequential OS CTest logs).
Limits/rulings: NAV-constrained noise randomization and exact investigate policy
remain existing differences; PM_Move footsteps and custom delayed playback intake
are separate work. Late attach without prior public precache receipts does not
infer unknown indices. Live HLDS/ReHLDS gunshot/camera/FPS acceptance is NOT RUN;
offline and artifact gates do not establish live acceptance. No push/deployment.

## 2026-11-14 auditory look reactions for the public sound feed

Source base f326cad; behavioral oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5, read-only. This is an independent
implementation of the auditory-look slice, not full native hearing parity.

- AudibleEvent and NoiseMemory retain explicit public receipt seconds separately
  from frame IDs. Fresh generic sounds require profile reaction delay and age
  <=0.5 seconds. A clear, nearby (<1000 units) High-priority sound without a
  visible enemy starts a 0.25-second look and random 2-4-second inhibition.
- Ordinary looks use 1-2-second holds; stationary Hide uses 2-5 seconds, or
  5-15 with one enemy remaining. Five-degree alignment starts the hold timer;
  ordinary targets within 100 units clear. Shared scripted RNG requests pin the
  bounds. The existing non-attacking spring runs at command cadence, with world
  movement reprojection and no synthesized firing or enemy identity.
- Direct auditory rays ignore monsters/glass and use neither FOV nor smoke as
  hearing occlusion. Blocked rays use the nearest read-only NAV approach and
  the existing guard bend routine, now shared without changing its geometry.
  Bend results are cached by received time/position, NAV revision and own pose
  within 10 units. Absent approaches conservatively skip the look.
- Visible enemy/attack, USE/C4, grenade, freeze/configuration, ladder and
  breakable ownership gate the modifier. Actor/map/round/death/clock changes
  invalidate held state. Ordinary guard scanning respects auditory inhibition.
- Known standard shot samples establish High priority in the EmitSound feed.
  SND_STOP creates no receipt. Independent review found same-frame Medium
  weapon audio could deduplicate away a later High shot; its actual callback
  regression failed RED, then passed after priority was included in the key.
  No Critical findings or deferred minors.

Offline gates: complete Windows HostX86/x86 Debug build PASS; CTest **96/96 PASS**
(9.28 seconds). Complete Debian Linux x86 Debug build PASS; CTest **94/95 PASS**
(11.52 seconds). Only pre-existing astrabot_combat_policy fails with the exact
weapon/skill/recoil chest-policy message in the coding-conventions-20261113
baseline; its source and expectations are unchanged. New pure and actual
EmitSound-to-RunPlayerMove regressions pass on both OSes, including reaction,
command smoothing, movement/USE ownership, no fire, stop notification, frozen/
stale/visible-enemy/beep rejection, positive NAV approach/cache reuse, death and
round reset. Pure tests also exercise actor-generation and clock reversal.
Shared guard regressions and unchanged frozen characterizations pass. Source,
ownership, ports and whitespace checks pass. PE/ELF independently verify x86
and seven exact public Metamod exports. Tested source/artifact hashes stayed fixed.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: e51346b5db1f1d9cbee617a6efd4d1a21ea67c748b12dc22c6c6196ca3da3172
- Linux SO: 8b2bd125d113901fc0080cb13876fc00046fa46cbf6a6e93bff819bccc1ce872

Receipts: .agent-state/sessions/csbot-noise-look-9bf0ec08-83d5-4573-beb8-7ea7b5e57783/.
Native camera comfort/FPS/real-server acceptance is NOT RUN. No deployment/push.
Limits: client PlaybackEvent-only gunshots are not supplied by the existing
EmitSound adapter. Upstream hearing range/position and frame-based replacement/
expiry remain the previous inferred pipeline; explicit seconds fix look freshness,
not the whole hearing system. Dynamic approach data is inferred from compiled NAV.

## 2026-11-14 CSBot chatter voice transport for existing concepts

Source base aa6abd5; pinned read-only behavioral oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5. Independent implementation; host
BotChatter.db and audio are read through public Engine APIs, never distributed.

- The eight existing outgoing concepts can play host voice samples in normal
  and important-only minimal modes. Radio mode uses the existing GameDLL menu
  path; off suppresses chatter. A voice-only concept no longer requires a
  standard-radio mapping. Missing voice/transport falls back only to the
  default bank's explicitly declared radio equivalent.
- Profile VoiceBank now inherits through defaults/templates with VoicePitch.
  The bounded parser handles BaseDir, Count (Many=4), Place, Radio, Important
  and End. Shared cyclic cursors and per-round shuffling avoid repeated variants.
  Current read-only NAV place and public visual enemy count filter selections.
  Missing samples are removed once on bank initialization when GetFileSize is
  available. Database loads, absent-bank results and duration lookups are cached.
- Public SendAudio sends speaker byte, sample string and profile pitch short to
  same-team clients, including dead teammates, plus public observer targets
  matching that team. Audio is never broadcast to enemies. Observed recipient-only
  TextMsg ignore/accept-radio receipts gate recipient audio by map/entity serial.
- Optional registered BotVoice feedback starts per recipient and broadcasts its
  stop after sample duration+1 second, on death, removal, round or map cleanup.
  A zero-active fast exit keeps ordinary frames free of voice polling callbacks;
  active feedback expires even while bot_stop is set. Team speech occupancy uses
  audio duration+0.1 second; GetApproxWavePlayLen supplies milliseconds, with the
  reference 1-second duration fallback. No per-frame file loads/world scans.
- Independent review found two Important issues: feedback with all audio ignored
  was not tracked, and VIP direct FollowMe could be replaced by queued chatter.
  Both were reproduced RED then fixed GREEN. Direct radio retains precedence;
  all-ignored feedback still expires. No Critical findings or deferred minors.

Offline gates: full MSVC HostX86/x86 Debug build PASS; Windows CTest **94/94 PASS**
(9.00 seconds). Full Debian WSL Linux x86 Debug build PASS; CTest **92/93 PASS**
(11.54 seconds). Only pre-existing astrabot_combat_policy fails with the same
weapon/skill/recoil chest-policy message as the coding-conventions-20261113
baseline. Combat-policy sources/expectations are unchanged. New catalog/profile/
scheduler/actual-message regressions pass on both OSes; frozen characterization
baselines remain unchanged and match. Source/ownership/ports manifests and
whitespace checks pass. PE and ELF independently verify x86 and seven exact
Metamod exports. The installed host DB parsed all eight supported concepts.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: b68dc2a620aa24a61365cb779acb5dad7d0aeb84157190f639fd3d471266624d
- Linux SO: 8139ec131aecce219ff9ffeb4e29dc66a89d73833044d298307c5d98d185b764

Receipts: .agent-state/sessions/csbot-chatter-voice-61b98bd2-eed8-4863-8e6b-0faf7101e201/.
Native client audibility/UI acceptance is NOT RUN; no deployment or remote push.
Other CSBot statement generation/composition remains open. Engine-RNG shuffle
is a portable approximation of reference CRT random_shuffle consumption, not
reference replay equivalence. Unobserved private ignore-radio state on late
attachment cannot be recovered; public observer fields may be incomplete.

## 2026-11-14 CSBot contact-driven primary weapon exchange

Obstacle work was committed as 30cd290 and fast-forward merged into master.
This follow-on starts from 30cd290; pinned behavioral reference remains
ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, read-only.

- Grounded weaponbox-first Touch exchanges an existing primary only when the
  dropped primary comes first in the profile's ordered preferences and the
  last visible enemy was at least 2.5 seconds ago. Pistol preferences do not
  affect the choice. No distant weapon scan or weapon-seeking route is added.
- Standard world models infer ordinary primary contents; public entvars.weapons
  identifies the carried primary. Unknown/custom/shield models and ambiguous
  inventories skip exchange. Empty primary slots retain ordinary GameDLL pickup.
- A synchronous public drop command names the current primary classname before
  the original GameDLL Touch continues, including while a pistol is active.
  Inventory and pickup confirmation remain GameDLL-owned. No private layouts.
- VIP public ScoreAttrib, actor ownership/join state, map lifecycle, death,
  spectator/freeze and configuration gate the request. Enemy visibility history
  resets with a new actor/round. Player-first contacts exit before engine calls.

Offline verification: Windows x86 Debug complete build PASS; CTest **92/92 PASS**
(7.79 seconds). Debian Linux x86 Debug complete build PASS; CTest **90/91 PASS**
(11.11 seconds), with only the pre-existing astrabot_combat_policy threshold
failure. Its exact message matches the coding-conventions-20261113 baseline;
combat-policy sources and expectations are unchanged. The new weapon pickup
fixture and frozen runtime characterizations pass on both OSes. An initial
characterization delta consisted entirely of 48 extra classname queries on
player-as-box contacts; the early FL_CLIENT rejection removed those calls.
Historical baselines were not regenerated. Independent review and follow-up
found no Critical/Important findings. Source manifest and whitespace checks pass.
PE/ELF independently verify x86 and seven exact public Metamod exports.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: 7986efa38846eadfb5f72eaf82c198e077e0fe006c4555f406b27959289df2df
- Linux SO: 438631d9bf4b51d4c0b99c0769699b45af6e78be35d37e4bf9da68319df0d0ff

Receipts: .agent-state/sessions/csbot-weapon-pickup-bbdb03c5-3039-48d9-a33a-b1e549a82baa/.
Native HLDS/ReHLDS pickup/physics acceptance is still open; no server deployment
or remote push was performed. Public model inference does not establish exact
private weaponbox contents on modified servers.

## 2026-11-14 CSBot contact-driven obstacle interaction

Source base: AstraBot bee8026; behavioral reference ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5. This entry records implementation
and offline evidence only; native door/breakable map acceptance is open.

- Ordinary func_door / func_door_rotating movement remains touch-open, as
  assumed by the reference game_shared/bot/nav.h. The pinned bot has no
  remote-button search routine; no invented USE or attack is sent to doors.
- Public Touch detects damageable func_breakable outside enemy attack. A
  local-path box-side rejection preserves the conservative BotTouch contract,
  using the local movement goal raised 36 units. A current controller path
  query prevents a destroyed corridor's old diagnostic goal rejecting contacts.
- Actor-local center look runs with the non-attacking spring at the command
  boundary, five-degree alignment and 0.2-second hold. Contact requests primary
  input without constructing an enemy or stopping movement; world direction
  is reprojected when the view turns. Pending contact survives a look expiry
  between Touch and the next scheduled command.
- Active grenades select an allowed carried primary, pistol, then knife, with
  a five-second equip-request throttle surviving look expiry. Publicly known
  fully empty weapons are skipped. Unknown ammunition remains unknown.
- Public KeyValue captures material and entity serial, excluding material 7
  unbreakable glass and trigger-only/nondamageable brushes. Missing material
  provenance is conservatively skipped (including late plugin attachment).
  No private CBreakable fields, pdata, offsets or per-frame entity scans.
- Death/removal, entity reuse, round/map changes and freeze invalidate or gate
  contact commands. Existing player avoidance, USE/C4 interaction ownership,
  compatibility mode isolation and public Metamod exports are preserved.

Verification receipts: .agent-state/sessions/csbot-obstacles-e16ecfdf-ed93-458e-93fc-1f72d0d61e43/.
Windows baseline 90/90 passed. The new adapter test first failed for absent
KeyValue support, then passed; expanded regressions exposed and verified
look-expiry/equip-throttle and known-empty-primary fixes. Tests cover reverse
Touch order, unbreakable/trigger-only/unknown/reused entities, door command
preservation, local-path geometry, world movement direction, grenade selection,
freeze, enemy-attack precedence and round invalidation. Source review found two P2 issues (ammo selection
and stale path ownership), both corrected; follow-up has no Important/Critical
findings. Source manifest check passes. PE and ELF independently pass x86 and
seven exact Metamod exports. Final Windows x86 Debug full build and CTest: **91/91 PASS** (7.41 seconds).
Final Debian Linux x86 Debug full build: PASS; CTest: **89/90 PASS**, only
pre-existing astrabot_combat_policy threshold test fails with
"weapon/skill/recoil chest policy matches reference thresholds". The same failure
is recorded in docs/verification/coding-conventions-20261113/VERIFICATION.md and
its baseline-linux-combat-policy.log; the combat policy files are unchanged.
The new obstacle regression passes on both OSes. Ownership inventory was
registered and both suites rerun. No tested source changed after fingerprinting.

Limits: no new native obstacle-map run or deployment was performed. Material
provenance and unknown ammo are public-information limits, not full CSBot parity.
General-purpose button solving and arbitrary func_pushable destruction are not
part of the pinned BotTouch behavior.

## 2026-11-12 A-side ramp / explicit path endpoint repair

MC11: source correction, RED/GREEN regression, both x86 builds and backed-up
deployment verified. Native: original1251->481 endpoint completion not encountered: native exact-case remains UNVERIFIED.
General endpoint-motion/END records: 8 saved examples. Other long
stops remain; do not declare full CSBot parity or general stall elimination.

### Source and BSP findings

- Saved bot1 game582.304..589.517 stopped7.213s near(1352.8,2348.1,29.5).
  NAV1251->481 is an uphill crouch strip:481 x1350..1375,y2350..2400,
  north floor11.53222, south24.03226, center(1362.5,2375,17.78225).
  Old last-area arrival accepted rectangle distance<=20 and floor distance<=64
  even before crossing y2350; retiring that corridor while retaining its Hunt
  goal caused repeated starts, endpoint stops and .4..0.6s retry gaps.
- Fixed reference CSBot `b0889847fe6d03898be88acc9e366660efb40ab5`, FIXES:
  UpdatePathMovement closeEpsilon20 compares full3D path-end distance <20.
  Explicit requested positions survive path generation and have NAV ground Z;
  Hunt's area destination uses its center projected to NAV GetZ. Path completion
  owns goal retirement. The historical two-corner center-height approximation
  differs from the four-corner floor by up to13.31203 units at actual area867;
  explicit Hunt endpoints now use surfaceZAt too.
- Actual `de_dust2.bsp` v30 SHA256 `15945389528d113562ede0a2c80647ebfa799079ed1c05a25379bcf84e4e9286`,
  9582planes/8321clipnodes/43models. Read SDK bspfile.h layouts, world hull1
  (standing32x32x72), hull3(duck32x32x36) and nearby brush model40.
  At(1359.5,2349.7), duck-origin floor29.8577 vs standing-origin47.3203;
  recorded duck origin29.9 is consistent with NAV feet11.9. A rising floor,
  not a justification for a zero-movement arrival at the boundary. Model40 is
  func_breakable tgt_a at x1280..1344/y2354..2432/z16..94. Its compiled hulls
  were sampled too. Live destroyed/enabled state, moving entities and players
  require existing public engine traces; offline BSP cannot assert those states.
  The tested straight duck-hull segment toward the endpoint clears world hull3
  but intersects the compiled box40 hull at fraction0.0022168; sliding/stepping
  and actual box state therefore matter. This is not a claim that the straight
  segment is free of obstacles or that offline clip-hulls simulate player motion.
  BSP/NAV files unchanged, no generation or map patch.
- YaPB4967a22, RealBot2f9d5c7, PODBot75f61e1, SyPB4c364fb were inspected for
  public origin/mins, trace and crouch/collision handling. YaPB directly sets
  desired jump velocity in some branches; that behavior was not transplanted.
  PODBot handles duck-height trace origins; SyPB traces hulls for obstacle
  avoidance; RealBot uses public directional TraceLine. Provenance/hashes in
  reference-sources.json; RealBot working bytes differ, so pinned excerpt was
  read separately. AI decisions remain based on CSBot.

### Contract and regression coverage

NavCorridor appends hasEndpoint/endpoint, default false for legacy/Enhanced.
Compatibility supplies true goal position or Hunt center, both with NAV surface Z.
Alternative corridors retain the endpoint. Follower keeps intermediate portal
steering, but the final segment aims at the endpoint and uses strict3D <20.
An endpoint completion retires the goal even if the actor is just outside the
NAV polygon. Finite endpoint validation and restart lifetime are explicit.
No public Metamod export changes, new private offsets, velocity writes or
GameDLL-specific hooks were required. Vanilla public-observation path retained.

RED: saved1251/481 slope/BSP duck-feet case stopped before its endpoint; second
RED reproduced failure to retire a narrow goal when already within endpoint
range. GREEN: moving/crouched intent, strict20 boundary, vertical25 vs area64,
inside-large-area continued movement, endpoint retirement, invalid endpoint,
legacy/Enhanced and existing movement/C4 suites pass. Source-derived expected
values are not captured CSBot runtime traces. A further nonplanar-area test
first confirms route start, then requires completion at interpolated floor21
rather than historical center42. It fails without the Hunt Z projection and
passes with it. Initial fixture debugging and RED/GREEN logs are retained.
No runtime speed increase masks
the stop.

Windows69/69 pass, PE x86/7 exact exports. Linux68 pass+1 external-NAV
performance skip, ELF x86/7 exact exports. DLL
`e37378893e9d35580dddaa88e43d8aca01c8e6d813fcec0a2ff28bbeee8c182d`;
backup `astrabot_mm.dll.bak-20261112-110014`. Source/build/deploy/launch identities
match. Vanilla GameDLL native and Linux native remain unverified (offline
Vanilla/provider-absence tests pass; optional API supplements zero fields).

### Native8 / acceptance limits

8BOT/0humans de_dust2, normal45 381.6s and restarted10
263.3s. Events {'Round_Start': 10, 'Planted_The_Bomb': 10, 'Bomb_Defused': 3, 'Target_Bombed': 6}.
HLDS stopped; server.cfg byte-identical restored; transient secret state removed.
Plant jump 0/333 samples.

| Observation | Saved run | Current run |
|---|---:|---:|
| Game seconds | 664.355 | 645.985 |
| Target-region >=3s stationary maximum | 7.213s | 0s |
| All transit >=3s stationary episodes | 11 | 31 |
| All transit >=7s stationary episodes | 1 | 3 |
| 1251->481 link samples | original case | 0 |
| True endpoint481 selections | original case | 0 |
| Original1251->481 completed cases | original case | 0 |
| General endpoint contract evidence | old area tolerance | 8 examples |

Zero target-region stops without target exposure is not an exact-case pass.
First new-path full-update records identify true goals; continuation diagnostic
goal_area may name an intermediate area. General endpoint evidence requires
>=3 moving full updates inside final area with horizontal endpoint distance>20,
then END within20 and >24 horizontal displacement in the same actor/map/round.
It does not replace the original481 native case.

- bot6 round2 path1 endpoint area40: moving inside area at game12.555 (line3726), END at12.92 (line3790), XY distance18.971, horizontal displacement92.481.
- bot6 round2 path8 endpoint area6: moving inside area at game54.198 (line13530), END at54.981 (line13693), XY distance16.193, horizontal displacement193.253.
- bot1 round3 path39 endpoint area54: moving inside area at game146.499 (line35846), END at147.125 (line36001), XY distance1.803, horizontal displacement151.183.

Broader native residual in exploratory iteration1 (separately archived):
bot7 game226.37..268.284 at(-236.9,1536.3,68),
41.914s Idle/neutral, UnsafeDrop on1773->139 then failed retries. This was
observed in that run; causality versus different RNG/profile/route exposure is
not established. Do not claim it was fixed or lower overall worst stop. It is
not a public-information limitation. Other friend/escape waits also remain.
Final-run longest transit records: [{"bot": "7", "seconds": 9.261, "times": [348.392, 357.653], "lines": [86252, 88374], "start": "(-660.5 -800.0 164.0)", "end": "(-680.7 -800.0 164.0)", "states": {"STATE-HUNT": 74}, "outcomes": {"1": 74}, "neutral_commands": 64, "all_commands": 74, "goals": ["(187.5 -187.5 8.0)", "(-250.0 -762.5 62.5)"]}, {"bot": "6", "seconds": 9.106, "times": [348.547, 357.653], "lines": [86311, 88372], "start": "(-621.4 -782.9 164.0)", "end": "(-642.2 -783.5 164.0)", "states": {"STATE-HUNT": 73}, "outcomes": {"1": 73}, "neutral_commands": 69, "all_commands": 73, "goals": ["(-950.0 -762.5 128.0)", "(-1975.0 1675.0 32.0)"]}].
Additional BSP check of the final-run9.261s Hunt stop at(-660.5,-800,164)
through(-680.7,-800,164): world standing-hull floor origin164 matches feet128,
inside NAV41 x-875..-500/y-900..-650 and its125..129 floor. Thus diagnostic
current_area=0 during neutral commands must not be treated as proof that the
actor physically left NAV. Route was41->42; recovery cause remains unresolved.
See residual-bsp.json and residual-nav-probe.log. Compiled floor geometry alone
does not establish current brush/player collisions or why movement was suppressed.

Limits: comparisons use different RNG/profiles and exposure. Stationary metric
is ~10Hz, anchor24, >=3s across transit states; excludes all Hide/Buy/Plant/
Defuse (including moving Hide), freeze/dead/no-nav/intentional holds. Command
intent alone is not progress. FPS measured in comparison.json; concurrent host
work and route exposure prevent a controlled performance conclusion. Graph
index datedNov8 lacks current relationships; current source/scoped diff and
executed gates used instead. FocalSpan refreshed independently.

Evidence: `build-action-adapter-x86-1451/aside-stall-evidence/` baseline617,
before/, bsp-geometry.json, nav-probe.log, reference-sources.json, RED/GREEN,
source manifest, verification/deployment/scoped patches/final-gate.
`aside-stall-live-evidence/` comparison.json, aside-comparison.json,
motion-analysis.json, qconsole.log, launch/shutdown/status receipts and
secret-free helper copies. Existing unrelated edits/rootDLL/evidence preserved;
no staging/commit/push.
The pre-height-projection DLL/SO, logs and ~21-minute exploratory run are kept
in aside-stall-iteration1-evidence/ and aside-stall-iteration1-live-evidence/.
Iteration1 captured population only at start; the final run requires both
start and end status. Neither run substitutes for a missing original-case
arrival exposure.

## 2026-11-12 B-site progress-timeout exclusion repair

MC10: source fix / regression / both x86 builds / backed-up Windows deployment
verified. Native assessment: B-approach timeout followed by >24-unit physical progress observed; see retry records.
No complete-stall-elimination or full-CSBot-parity claim.

### Cause and reference contract

- Pinned CSBot `b0889847fe6d03898be88acc9e366660efb40ab5`, FIXES enabled:
  `cs_bot_pathfind.cpp` UpdatePathMovement's giveUpDuration5 runs/stands,
  DestroyPath and PATH_FAILURE. MoveTo returns Idle after task checks;
  ComputePath throttles 0.4..0.6s and rebuilds without excluding that old edge.
  Those reference files were compared against the pinned commit (no diff).
- Saved bot7 game152.165..166.854 near(-1361.3,2447,37.9) stopped14.689s.
  Actual de_dust2 NAV query157->643 finds157->823->643 (3 expansions),
  but excluding157->823 returnsNoRoute. Old expireGroundRoute excluded that
  edge after5s; subsequent MoveTo/Idle retries could only search the graph
  with its sole approach excluded, entering the existing failure backoff.
- Fix in `NavRoamController::updateInternal` expireGroundRoute: timeout drops
  the corridor and clears that transient exclusion. Retain physical-failure
  exclusions, unsafe-drop/HP/fall validation, air/ladder continuation,
  freeze, persistent stuck monitor, retry RNG and Enhanced behavior.
  No public Engine/Metamod export or private-data/offset dependency added.
- RED `testBsiteProgressFailureDoesNotBlacklistTheOnlyApproach`: actual-NAV
  reduced sloped jump fixture157/823/643, progress timeout then sole-route
  retry failed on old code. GREEN emits157->823->643 movement on next update.
  Generic timeout's old permanent-exclusion expectation corrected. Existing
  unsafe drop/bypass, airborne/late-arrival and RNG suites pass. Expectations
  are source-derived, not a trace captured from an executing CSBot.
- `profile navigationStep` appends failure_reason/current_area/goal_area/
  path_link. Destroyed-friend next-update feedback can have zero path metadata
  and MovementNotProduced: analysis explicitly separates it from a timeout.
  New command intent alone is not accepted as physical progress.

### Build, deployment and native evidence

Windows69/69 passed, PE x86/7 exports; Linux68 passed+1 external-NAV performance
skip, ELF x86/7 exports. Source/build/deployment/launch identity verified.
DLL SHA256 `ea47c34c608d0e5f9af374f19f89f47dcd4bb5c8f7843fa06422c416ca8583a0`.
Backup `astrabot_mm.dll.bak-20261112-003114`. Previous sources, evidence and root DLL
preserved; scoped changes only, no stage/commit/push.

de_dust2, 8BOT/0humans: normal45 398.6s; restarted10
264.9s. Events: {'Round_Start': 10, 'Planted_The_Bomb': 10, 'Bomb_Defused': 1, 'Target_Bombed': 8}.
HLDS stopped, original server.cfg restored byte-for-byte, temporary secret state
removed. Plant jump: 0/331 samples.

| Observation | Saved pre-fix run | This run |
|---|---:|---:|
| Game duration | 645.346s | 664.355s |
| B-region maximum >=3s stationary episode | 14.689s | 5.227s |
| All transit stationary episodes>=3s | 5 | 11 |
| All transit stationary episodes>=10s | 1 | 0 |
| 157->823 samples in this run | unavailable at full-update granularity | 576 |
| Original goal643 approach samples | saved original case | 0 |
| B-approach timeout recoveries with >24 physical progress | original failure | 1 |

Exposure actors: 1, 2, 3, 4, 5, 6, 7, 8; rounds: 2, 3, 4, 5, 6, 10, 11.
- bot6 round6 game372.505: STATE-HIDE path65 -> path66, (157->823) -> (157->823), command after0.139s, horizontal>24 after0.882s, max horizontal5s=90.48. Original goal643 differs from this goal424.
Do not promote friend-repath recoveries to proof of this timeout fix. Command
and displacement records are in `bsite-comparison.json` (line/time scoped).

Limits: different RNG/profiles/combat exposure, observational run comparison.
Same-team Hunt agreement is also retained in comparison.json; not full-path
identity. Physical stops use ~10Hz live dispatched navigation, 24-unit anchor,
>=3s across transit states; freeze/dead/no-navigation and objective holds excluded.
The stationary summary excludes all Hide/Buy/Plant/Defuse states (including
moving Hide approaches), so it is not an all-behavior maximum. Explicit timeout
recovery records also inspect moving Hide states and require horizontal>24,
not just a vertical jump. Broader stationary totals are not an overall fix claim.
Windows ReHLDS native only; Linux/Vanilla native unverified. Exact goal643
condition is unverified unless its recorded exposure and recovery say otherwise.
Shorter B-side friend waits, A-side retreat waits and prior ledge1734->96 launch
validation mismatch remain separate residuals; no unimplemented case is labelled
an unavailable public observation. Graph review index datedNov8 lacks current
flow/test relationships; current source, scoped baseline diff and executed
build/test/native evidence were used instead. FocalSpan updated separately.

Evidence: `build-action-adapter-x86-1451/bsite-path-evidence/` baseline617,
before/, RED/GREEN, actual-NAV probe, source manifest, verification, deployment,
scoped patches and final-gate.json. `bsite-path-live-evidence/` comparison.json,
bsite-comparison.json, motion-analysis.json, qconsole.log, launch/shutdown/status
receipts and secret-free helper copies. Previous hunt-route evidence retained.

## 2026-11-11 Hunt destination selection follow-up

CSBot source mismatch fixed and regression/build/deployment verified. Native
route concentration decreased in this run; general stall improvement is not
established. Long stops remain open, not public-information limitations.

- Reference: `b0889847fe6d03898be88acc9e366660efb40ab5`, FIXES enabled;
  `HuntState::OnUpdate` unconditionally draws `RANDOM_LONG(0, areaCount-1)`
  and its final traversal overwrites the oldest-area candidate. Compatibility
  now does so even with eligible large areas. Enhanced reservations unchanged.
- RED: new eight-actor/shared-history/large-area test failed on old source.
  GREEN: actor-scoped typed/ranged/ordered tape consumed, SAFEST route retained,
  corridor continuation does not redraw; failure/reselection tapes explicit;
  equal draws can still share a route. Old oldest-area expected result corrected.
- Windows x86: 69/69 passed, PE/7 exports passed. Linux x86: 68 passed,
  1 external-NAV performance skip, ELF/7 exports passed. Windows DLL SHA256
  `b78eb76173a7d7a79361a52a69a3d32c3846da19001b460819bcb0f343c25e29`.
  Backup `astrabot_mm.dll.bak-20261111-223639`; build/deploy/launch hashes match.
- Native de_dust2: 8 BOT / 0 humans, normal45 382.4s,
  restarted short10 269.5s. Final HLDS stopped,
  original server.cfg restored byte-for-byte. Events: {'Round_Start': 10, 'Planted_The_Bomb': 10, 'Bomb_Defused': 2, 'Target_Bombed': 7}.

| Same-team Hunt pair agreement | Previous saved run | This run |
|---|---:|---:|
| Same destination | 13.70% (421/3074) | 3.20% (96/2998) |
| Same next link | 11.55% | 3.04% |
| Same current NAV area | 11.03% | 3.57% |
| Observed game duration | 665.538s | 645.346s |
| Transit stationary episodes >=3s | 17 | 5 |
| Episodes >=3s per10 game minutes | 15.33 | 4.65 |
| Transit stationary episodes >=10s | 3 | 1 |
| Maximum stationary episode | 13.005s | 14.689s |

Concentration: 1Hz samples grouped by same game-second/team/round, >=2 Hunt
bots; not necessarily all four teammates. Next-link agreement is not full-path
identity. Different RNG/profiles/combat exposure and duration: observational
comparison, not controlled CSBot execution or causal FPS/stall proof. Physical
stops use ~10Hz dispatched/live navigation, 24-unit anchor, >=3s across states;
exclude freeze, dead/no-navigation and objective holds. Counters alone do not
prove cause. Source-derived tests are not captured CSBot traces. Linux/Vanilla
native execution remains unverified.

Residuals: B-side bot7 at game152.165..166.854 stopped14.689s across
MoveTo/Idle near (-1361.3,2447.0,37.9); current area157, objective643,
PathSearchFailed/neutral commands repeatedly. Record as unresolved; no claim
that random destination selection repairs this objective-path failure. Earlier
ledge1734->96 launch mismatch and original A14->1214 cases remain open/unverified.
Plant jump commands: 0 of 330 sampled
Plant commands; nonattack waits>=1s: 0.

Evidence: `build-action-adapter-x86-1451/hunt-route-evidence/` (baseline,
RED/GREEN, source manifest, verification, deployment, scoped patches),
`hunt-route-live-evidence/comparison.json`, `concentration.json`,
`motion-analysis.json`, `qconsole.log`, launch/shutdown/status receipts and
secret-free helper copies. Previous evidence preserved. No commit/push/staging.

# CSBot Parity Status

## 2026-11-11 movement/C4 detailed audit: current revision

This section supersedes earlier November11 claims about the legacy64/96-unit
interaction gates and delayed navigation-failure delivery. Reference remains
b0889847fe6d03898be88acc9e366660efb40ab5 with REGAMEDLL_FIXES.
See [MOVEMENT_C4_AUDIT.md](MOVEMENT_C4_AUDIT.md) for MC01–MC08 contracts,
inputs/lifetimes/RNG, source-derived expectations, safety rulings and coverage.

Implemented: actor/map/round/goal/path/update-tagged NavigationStep and a
same-update state completion pass; current path-point friend checks; five-sample
physical monitoring across route changes; reference directional movement and
public maxspeed; Idle/Hunt30s lifecycle; known-position feet-distance<100 defuse
after path completion/failure; time-bounded BarTime receipts and1s reevaluation;
state-owned objective holds/plant5s timeout; escape search/draw deduplication.
Compatibility Plant directly owns C4 selection/attack inside a public bombsite,
independent of the old corridor destination. Enhanced retains its old paths.
The legacy CurWeapon cache also retains an active weapon when a different
inactive inventory entry follows it; own deactivation and reset still apply.

The wide-trigger plant regression failed before its fix and passes afterward.
Other RED/GREEN contracts are documented in the audit; no executed CSBot trace
was captured. Same-input source-derived tests are not a reference-runtime replay.
Windows x86 complete build:69/69 passed; PE x86/seven exact
exports passed. Linux x86 complete build:68 passed/one external-NAV performance
case skipped,69 registered; ELF x86/seven exact exports passed.

DLL SHA256:464a82e5dc44e8ffa280100ab4bf59af1bfa1bc8881c7101efceac6c715bb813
Linux SO SHA256:da7d8252d6969840c84fa11fb944d88738d7a49ae449ed506db0930aeb8b5c11
205-file source manifest SHA256:84787c999402bb6dbff8aee1c0c9066be5b77ef0bcdc289bb9edb0a7a8bf106d
Build/deployed/launch identity verified; backup:astrabot_mm.dll.bak-20261111-195030.
615-file pre-task baseline retained; unrelated changes are rejected by the
preservation check. Existing dirty/untracked files and root DLL are preserved.

Final8-bot/zero-human de_dust2 Compatibility run:
2026-11-11T19:50:36.501943+09:00 through 2026-11-11T20:01:45.495128+09:00.
Normal fuse45:389.6 wall seconds;
fuse10 after restart command:276.3 wall seconds
(one-second restart delay separately recorded). Teams:{'1': 4, '2': 4}.
Events:{'Round_End': 11, 'Round_Start': 10, 'Planted_The_Bomb': 10, 'Target_Bombed': 8, 'Bomb_Defused': 2}. Profile FPS:{'windows': 655, 'median': 68.49, 'minimum': 36.37}.
This includes diagnostic/indexing load and changing RNG/profiles, so it is not
a controlled FPS or timing A/B. HLDS stopped; original server.cfg restored and
hash-verified. No commit/push or unrelated server changes.

Full-update command samples:41192;
plant jump:0;
plant attack without crouch:0.
Plant nonattack waits of at least1s:[].
A-region transit exposure:{'STATE-MOVE-TO': 11, 'STATE-ESCAPE-BOMB': 6}.
Original A-region MoveTo14→1214 link samples:0.
Regional Escape/MoveTo exposure does not establish the original friend/goal setup.
Longest transit stationary episode (24-unit anchor, across state changes):
Bot3, 13.005s, game[610.217, 623.222], (-638.5 -881.2 164.0) to (-660.3 -874.9 164.0); states{'STATE-HUNT': 103}, neutral97/103 commands. See motion-analysis.json for goals and line ranges.
Defuse exit receipts:{'0': 26, '3': 1} (0=Unknown,1=Self,2=Other,3=None).
Unknown is not treated as observed absence. A receipt gap can cause bounded
retry; public BarTime capture is implemented, but hidden defuser/reload state
and rejected GameDLL action reasons are not inferred as exact observations.

| Judgment | Evidence / remaining work |
|---|---|
| 修正・検証済み | Listed offline contracts, both builds/export checks, native planting posture and state/command delivery; actual sampled displacement is archived |
| 未実装 / 未解決 | Final Hunt13.005s neutral wait; earlier area1734→96 Drop launch rejects feet11.55 above NAV floor (outer tolerance64 vs launch8), causing38.372s Idle/neutral loop. Safe recovery and dedicated regressions are pending; the whole equivalence goal is not closed |
| 実機未検証 | Exact original A-site7/13/26-second causal setup unless fully reproduced; Linux and Vanilla native play; executed CSBot trace comparison |
| 公開情報の制約 | Precise private reload timer and global internal defuser are unavailable; delivered-reload lease and explicit Unknown/1s reevaluation are documented substitutes |

Detailed machine evidence:
`build-action-adapter-x86-1451/movement-c4-audit-live-evidence/comparison.json`,
`motion-analysis.json`, `motion-rows.json`, `wait-metrics.json`, launch/shutdown
and RCON receipts; build/source/deployment manifests under
`movement-c4-audit-evidence/`. Aborted iterations1/2 and their deployed DLLs are
retained. Hide guard holds and dead/no-navigation heartbeats are not transit
stalls; freeze deadlines are excluded. Command intent alone is not success.
Remaining long stops and missed native cases prevent any full-parity claim.

The preceding20-minute iteration3 is also retained under
`movement-c4-audit-live-iteration3-evidence/`:18 plants,14 explosions,4 defuses,
no original A-region14→1214 samples, residual38.372s ledge stop and4.74s Escape
stop. Its later five-second C4 selection holds led to the CurWeapon cache fix.
These known geometry defects remain open even if a shorter final run does not
reproduce them. This is not a public-information limit.

## 2026-11-11 friend-wait and MoveTo failure repair: latest native evidence

Reference: b0889847fe6d03898be88acc9e366660efb40ab5. This supersedes the
earlier lateral-only make-way interpretation: REFERENCE.md freezes a
REGAMEDLL_FIXES build. Its StrafeAwayFromPosition replaces forward/back
as well as lateral movement at the 0.5 projection cutoffs. Astra now
dispatches both axes, retaining unaffected inputs and the existing
ground/freeze/action safety gates. Priority and the 0.33-second contact
lease are unchanged. No extra route reservation or new wait timer added.

MoveToState::OnUpdate exits to Idle when path movement stops progressing.
Astra formerly omitted actor-local navigation failures from the state
input and used Hunt as its generic exit. NoRoute, failed replan and
patience-driven path invalidation now deliver one-shot feedback scoped
to actor/map/round/goal generation. MoveTo abandons its path and cached
goal on the Idle exit; Attack can defer consumption. Dead/slot/map/round
reset discards feedback. No shared bomb-approach RNG is reset.

The intermediate 8-bot trial exposed a 21.969-second escape wait near A:
CTs still in MoveTo issued legacy defuse actions at a wider radius and
stopped before the scenario owned the interaction. The observation bridge
also labelled currently visible C4 as Remembered, preventing DefuseBomb
entry. It now carries current visibility independently of cached knowledge;
Compatibility dispatches defuse only from DefuseBomb. The existing
scenario interaction bound remains 64 units; CSBot's 100-unit feet-based
approach and its internal one-second defuser confirmation are not newly
claimed as fully reproduced by this scoped correction.

Regression cases fail on the corresponding old implementations and pass
after repair: front/back make-way on neutral input; unknown-ground veto;
actor-local navigation failure -> MoveTo/Idle; occluded remembered C4
does not issue premature IN_USE; visible C4 enters DefuseBomb. Existing
human-defuser handoff passes with a valid public bomb-map/entity fixture.
Complete Windows x86 Debug build:69/69 CTest passed, PE x86/seven exact
exports passed. Complete Linux x86 build:68 passed, external-NAV performance
case skipped (69 registered), ELF x86/seven exact exports passed. WSL is
available again; the previous section's environment failure is historical.
Linux and Vanilla native play were not exercised by this run.

Latest deployed DLL SHA256:d0873f1a6489d78cec5186d657a683f5d897dbe12754d26c683813ab3ba2f4ee.
Backup:astrabot_mm.dll.bak-20261111-173537. Source/build/deployed identity verified
against the 204-file source manifest; unrelated baseline files preserved.

Final hlds_start.bat run:2026-11-11T17:35:43.785389+09:00 to
2026-11-11T17:46:52.972473+09:00 (669.2 wall seconds), de_dust2,
Compatibility,8 BOTs/zero humans confirmed through RCON; team counts
{'2': 4, '1': 4}. Normal45-second rounds and any explicitly recorded
short-fuse stress phase are distinct conditions, with restart after fuse
changes. Native RNG/profiles differ from the prior run: this is not a
deterministic timing A/B. Events:{'Round_End': 10, 'Round_Start': 10, 'Planted_The_Bomb': 9, 'Target_Bombed': 6, 'Bomb_Defused': 3, 'Dropped_The_Bomb': 1, 'Got_The_Bomb': 1}.

About1Hz sampled positions,24-unit stationary anchor, excluding freeze,
action holds, plant/defuse and reached camp holds: longest MoveTo
stationary episode:7.081 seconds; longest EscapeBomb:
4.979 seconds. These are observed maxima, not
guaranteed upper bounds. 174 escape samples; navigation
results:{'0': 170, '3': 1, '2': 2, '10': 1}. 1823 explicit actor
MoveTo->Idle navigation failure exits. Touch make-way:163
applied samples,including93 backward and
66 forward;17 safety vetoes.
Plant input samples:56,IN_JUMP:0.
Diagnostic profile FPS:{'windows': 648, 'median': 59.92, 'minimum': 15.02} (instrumented Windows server only).

Original A-site MoveTo wait:26.074 seconds,link14->1214. Latest active
A-region MoveTo confinement maximum:3.026 seconds,including the same
link. The former long interval did not recur. Escape had only
1 sampled suppression episode(s),each represented by
[0.0] seconds between samples. The exact
original A-region escape box was not revisited in EscapeBomb during this
run; this limits reproduction coverage. The4.979-second EscapeBomb
physical confinement elsewhere had nonzero movement intents throughout:
it is a remaining geometry/traffic issue,not proof that movement succeeded.

Any residual stops, exact A-transit exposure and following movement are
listed in comparison.json,analysis.json and wait-metrics.json. Do not
equate state-local maxima with the absence of aggregate traffic delays.
Largest aggregate stationary episode (may span state transitions):
bot4,13.004 seconds at (1178.2 2337.1 151.6),game time[291.026, 304.03],NAV results{'2': 7, '0': 7}. This remains a recorded limitation, not a repaired case.
No complete CSBot parity or zero remaining movement defects declared.
HLDS stopped after evidence capture; original server.cfg restored byte
for byte. DLL remains deployed. No commit/push or root DLL replacement.

Evidence:build-action-adapter-x86-1451/friend-wait-nov11-evidence/
and friend-wait-nov11-live-evidence/; intermediate trials retained in
friend-wait-nov11-live-iteration1-evidence/ and iteration2-evidence/.
FocalSpan refreshed after edits. Code-review graph impact/review queries
were consulted, but its November8 snapshot lacks these new symbols/flows;
current source, compiler, behavioral tests and native logs provide the
actual verification. Graph test-link absence is not a runtime verdict.

## 2026-11-11 ledge and bomb-escape repair: deployed, 8-bot native improvement verified

Compared against CSBot reference b0889847fe6d03898be88acc9e366660efb40ab5.
Four connected causes were repaired using public Engine/NAV inputs:

- IsFriendInTheWay (cs_bot_pathfind.cpp:1097+) uses player entity origins
  against floor-level path points. Astra had used feet for both players,
  falsely blocking short portals with friends beyond the endpoint. The
  adapter now supplies current public origins; locomotion remains in feet.
  NavRoamObservation explicitly marks the available friend-ray origin and
  validates finite values. Geometry-only callers retain their prior input.
- CSBot retains its path/last-known area across NAV gaps. A safe descent
  near an adjacent polygon connected to the landing no longer reverses into
  geometry-only off-mesh recovery. Public safe-drop bounds remain required.
- Off-mesh recovery previously exhausted four geometry candidates and
  latched neutral until external displacement or lifecycle reset. Timed
  Compatibility now performs the existing randomized Wiggle/jump sequence
  on no progress and retries bounded candidates. Freeze/action suppression
  and Enhanced's bounded recovery behavior remain intact. This adapts the
  reference StuckCheck/Wiggle policy; it is not a claim of identical timers.
- EscapeFromBombState::OnUpdate (cs_bot_escape_from_bomb.cpp:40+) redraws a
  destination when path movement stops progressing. Cached escape areas
  are now invalidated after failure, arrival, or patience-driven path
  destruction, while ordinary polite waiting preserves the current draw.

Regression evidence: saved old-controller source fails the new off-mesh
Wiggle and adjacent-ledged-descent cases; current source passes. Public
Engine integration also reproduced the short-portal false wait before the
origin correction and tests escape retry after failure/patience expiry.
The Touch fixture now checks actual RunPlayerMove side inputs: its frame%16
diagnostic sampling did not reliably intersect actor update cadence.
Complete Windows x86 Debug build:69/69 CTest passed; PE x86/seven exact exports
passed. Linux rebuild/CTest/ELF checks remain ENVIRONMENT-UNVERIFIED:
two WSL launch attempts failed with Wsl/Service/0x80072747. Previous Linux
artifacts are not evidence for this source revision. No Linux redeployment.

Deployed DLL SHA256: e981c4398623b3f40dce6f79e43550963a35f742e0b554762e171ce8e01982a5.
Backup:astrabot_mm.dll.bak-20261111-162325. Source/DLL/deployed hashes match.
Final hlds_start.bat run:2026-11-11T16:23:32.272379+09:00 to 2026-11-11T16:33:52.906209+09:00,
620.6 wall seconds, de_dust2, Compatibility, eight BOTs/zero
humans (repeated RCON status),4T/4CT (TeamInfo). Intermediate trials were
preserved separately and were not mixed into this final run.

Test phases:mp_c4timer10 at16:23:38 JST, then45 at16:28:27 with sv_restart1
at16:28:34. The first planted bomb retained the GameDLL's cached45-second
fuse despite the changed public CVar; subsequent short-fuse rounds exercised
escape, followed by fresh ordinary45-second rounds. These deliberate stress
conditions and differing native RNG/profiles preclude an exact timing A/B.

Final log: 8 plants,
3 defuses, 5 explosions;
42 sampled plant inputs, zero IN_JUMP contamination.
Escape:499 sampled updates, result counts
{'0': 480, '10': 10, '3': 7, '2': 1, '1': 1}; 83 explicit escape retry
records. Failed escape samples resumed successful movement after observed
intervals [0.977] seconds. The previous off-mesh NoRoute latch did not
recur. However, the final run still contains a5.983-second escape wait
(bot8,542.064-548.047s, nav_result10/0) and a26.074-second MoveTo wait
(bot3,544.009-570.083s, nav_result10/0) near the A site. Both are classified
as movement suppression/polite waiting rather than persistent NoRoute;
the aggregate traffic delay remains open. Do not interpret this repair as
zero remaining stops or complete CSBot parity. Freeze, action holds,
plant/defuse and reached camp positions were excluded by the analyzer.
Sampling is about1Hz, not an every-command guarantee.

Non-carriers actually traversed the wall region. Maximum sampled confinement
was6.975 seconds while moving through it;
the earlier28.184/25.247-second looping confinement did not recur. This is
region confinement, not fully stationary duration, and does not establish
that all narrow passages on all maps are resolved. Profiling-on windows:
median59.23 FPS, minimum35.39;
uncontrolled diagnostic measurements, not a performance improvement claim.

HLDS stopped; server.cfg restored byte-for-byte (original SHA256
07997dbfc1f40913fabe021f097e548a0caa9bad722f17e052ce2108f0baaf23). No commit/push.
Evidence:build-action-adapter-x86-1451/ledge-escape-nov11-evidence/ contains
baseline/source manifests, scoped patches, red/green regression logs,
build/CTest/PE logs and deployment receipts. Sibling
ledge-escape-nov11-live-evidence/ contains qconsole.log, analysis.json,
comparison.json, RCON snapshots and shutdown.json. Earlier trial logs are
in ledge-escape-nov11-live-iteration1-evidence/ and -iteration2-evidence/.
Graph coverage of C++ flows remained incomplete; current-source inspection,
executable tests and native logs provide the acceptance evidence. FocalSpan
was refreshed; its index is not a substitute for native acceptance.

## 2026-11-11 area1735 C4 route repair: deployed, native recovery observed

The earlier 39.015-second C4-carrier stop had a reproducible false-NoRoute
cause in NavQuery::buildAlternativeCorridor. Independent prefix/suffix
searches preferred a suffix returning to the original start through the
failed edge, rejected that result, and never searched the longer valid bypass.
On the actual de_dust2 NAV, excluding1735->96 while routing1735->1469 returned
NoRoute after five subsearches/455 expansions before this repair. The repaired
query finds1735->1736->1737->1740->1741->1742->1093->1453->1452->96 and onward
to1469 with one search/260 expansions, at aggression0.0/0.5/1.0, health100.

CSBot reference b0889847fe6d03898be88acc9e366660efb40ab5,
CCSBot::ComputePath (cs_bot_pathfind.cpp:1608+, NavAreaBuildPath at1650),
searches start-to-goal as a whole. Astra now likewise performs a whole A*
search for this recovery, excluding the failed directed edge during expansion.
The exclusion remains Astra's recovery policy; this does not assert that
CSBot exposes the same exclusion contract. Existing costs, fall constraints,
search limits, normal-query behavior and public interfaces remain intact.
Only src/core/nav/nav_query.cpp and tests/nav_query_tests.cpp changed in code.
Regression coverage exercises the previously rejected longer bypass, single
search accounting, subsequent ordinary routing, and ResourceLimit propagation.

Complete Windows x86 Debug build:69/69 CTest passed; PE x86/seven exact exports
passed. Linux x86 Debug:68 passed, one external-NAV performance test skipped;
ELF x86/seven exact exports passed. Native evidence below is Windows ReHLDS,
not a Vanilla/Linux native acceptance claim. Graph queries lacked complete
C++ flow coverage; targeted current-source review and executable tests were
used. FocalSpan was refreshed after the source repair.

Deployed at15:25:12 JST with backup astrabot_mm.dll.bak-20261111-152512.
Built/deployed SHA256:
6a8f4f5ed902766f48cfc9252f0bf567ab55ba44e6d7887c15b31f4a285c9f16.
Executed hlds_start.bat, de_dust2, compatibility mode,15:25:19–15:32:31 JST
(about7m12s). RCON confirmed eight BOT participants/zero humans; TeamInfo
confirmed4T/4CT. Five rounds completed:five plants, three defuses, two explosions.
The following round was partial. All39 sampled plantInput records had
buttons=1, zero IN_JUMP. Profiling-on one-second windows:median94.32 FPS,
minimum49.52; diagnostic measurements, not a controlled performance comparison.

Crucially, C4 carrier bot3/Ulysses reached area1735 targeting1469 at85.012s
(log line11262), with NoRoute/path_sequence0. At86.012s (line11361) it had
recovered path_sequence12, nav_result0, speed240 and moved to area42.
It reached1469 at110.039s and planted successfully (line14235,15:27:13).
The long stationary failure did not recur in this run. The one-second sample
interval bounds this observed recovery; it is not an exact per-command delay
or a deterministic A/B timing ratio. Actual-NAV reproduction independently
verifies the repaired1735->96 exclusion case.

Remaining separate issues:non-carriers bot1/bot4 spent28.184s/25.247s moving
within the surrounding ledge region; bomb-escape NoRoute stops of6.919s,
5.984s and5.983s remain. Thus the earlier area1735 false-NoRoute repair is
verified, while complete narrow-ledge/escape navigation remains open.
HLDS was stopped and the original server.cfg restored byte-for-byte.
Evidence:build-action-adapter-x86-1451/c4-1735-nov11-evidence/ (reproduction,
source patches, build/export/deployment receipts) and sibling
c4-1735-nov11-live-evidence/ (qconsole.log, analysis.json,
carrier-1735-recovery.json, RCON snapshots, shutdown.json).

## 2026-11-11 autonomous 8-bot native verification: partial pass

Executed the user's hlds_start.bat on Windows, de_dust2, compatibility mode,
DLL SHA256 d71e198a845e79a2aa0746986f8adc5482c7256f91778447446afd1e682b73c0.
Run: 15:03:02–15:09:54 JST, about 6m51s. RCON status repeatedly confirmed
eight BOT participants and no humans; TeamInfo diagnostics confirmed 4T/4CT.
Metamod reported AstraBot RUN. Five rounds reached a natural result: five
plants, four CT defuses and one bomb explosion. The following round was
observed partially before controlled shutdown.

Planting acceptance: all 28 sampled plantInput records had buttons=1; zero
IN_JUMP contamination. This is native evidence for no recurrence in the
recorded planting attempts, not an every-command trace guarantee.

Make-way behavior is active in native execution: 91 playerAvoidance records,
87 applied and four ground-safety vetoes. This verifies execution of the
contact mechanism, not complete traffic-deadlock resolution. Profiling-on
one-second frame windows had median 63.03 FPS and minimum 44.15 FPS; these
are reference measurements under diagnostic logging, not a performance SLA.

Narrow-ledge acceptance FAILED. C4 carrier bot4 remained near
(-382.3,-637.8,163.5), current area1735, for at least 39.015 seconds
(game time364.074–403.089, archived log lines44304–48414), with repeated
nav_result=2(NoRoute), failure_reason=5(PathSearchFailed), path_sequence=0,
and no movement intent. The initial rejected segment was1735->96; the goal
was bombsite route area1469. It was still unresolved when the run was stopped.
This is distinct from the earlier wait-loop/portal-target reproduction and
must remain an open navigation repair. Shorter1736->1735 stops, a14.359s
guard-route stop and a17.067s bomb-escape route failure also remain recorded.
Intentional freeze, action holds/defusing, planting and reached camp positions
were excluded from the stationary-episode detector. Region confinement is
reported separately from being fully stationary.

Evidence: build-action-adapter-x86-1451/live8-nov11-evidence/ contains
qconsole.log(12,825,990 bytes; 48,455 lines), analysis.final.json, RCON snapshots,
launch/shutdown receipts and the analysis script. Log SHA256:
b4225f2885db59139c1af1d4ea7a8ecc6b6a43f24b0459432b4670ab45c8bc16.
The server was quit normally. Temporary quota8/join-after-player0 settings were
replaced with the original server.cfg bytes, verified by SHA256; the deployed
DLL was unchanged. No source implementation or NAV edits were made in this
verification. This run does not close all stuck/parity acceptance items.

## 2026-11-08 friend deadlock / same-link replanning / thin ledges

The Nov8 native log session (19:31:39–19:37:39 JST, lines45838–91585,
de_dust2, up to12 alive bots) confirmed planting-input improvement but retained
friend deadlock: bot8 stayed at(-99,-583,36) for48.956 seconds while path
sequences14..25 repeatedly chose1453->8; bots4/10/12 queued nearby. A C4
carrier on14->89 and a teammate on89->14 also waited in opposing directions.
Snapshot SHA256: c9cc896bf19251a6f460dd57e91995d018255b978c72afce4ebf0ca0ba431dd4.

Reference comparison found three missing/different mechanisms:

- cs_bot_pathfind.cpp:1493–1515 destroys a path on patience expiry but still
  moves toward the saved goal that update. Astra previously returned before
  producing movement. It now emits the normal safety-checked intent and
  invalidates the route afterwards. Occupancy remains a finite cost: a sole
  valid corridor is not banned merely because friends occupy it.
- IsFriendInTheWay and CNavArea::GetPlayerCount enumerate live players without
  a visibility gate. Navigation-only friendly blocking/density now use public
  entity positions and TeamInfo, excluding self/dead players/enemies. Enemy
  perception and objective knowledge remain gated. Feet-space NAV matching
  is used by this adapter in place of private area pointers.
- BotTouch/CCSBotManager::GetPlayerPriority and the0.33-second make-way action
  were absent. A public DLL_FUNCTIONS Touch hook now records generation-safe
  contacts. Human priority0, observed scenario priority1(C4/VIP), then1+the
  monotonic bot construction ID determine who yields. Current incumbent
  priority is recomputed after ownership changes. Classic CSBot lateral-only
  strafe projection is used; the REGAMEDLL_FIXES projection branch differs.
  Unknown hostage-escort priority is not invented. Action holds, freeze,
  airborne/ladder safety, and a public ground trace veto prevent unsafe
  sidestepping. No Compatibility route reservations or slot-based priority
  were introduced. Contact targets expire on map/round/actor reuse or0.33s.

For thin wall-top NAV polygons, a directed portal's16-unit inset could cross
the far edge of an8-unit destination. CSBot ComputePathPositions uses a step-in
explicitly smaller than its minimum area size. Astra now caps its inset at
half the destination depth and anchors it to the destination entry edge;
ordinary wide portals retain their previous inset. Four-direction regression
fixtures prove targets remain strictly inside the thin destination. This
geometric reproduction does not establish that every native ledge stall has
the same cause. The logged1735 path-selection failure and18->69 physical
stall remain native recheck scenarios; dangerous-drop rejection was retained.

Behavioral RED/GREEN evidence: patience expiry movement(including the only
occupied corridor), thin portal overshoot, touch-hook registration, and touch
priority/lifetime tests. Executed adapter scenarios verify a later-created C4
carrier causes the earlier ordinary bot to yield through RunPlayerMove, and
missing lateral floor support vetoes the avoidance. New diagnostics:
`profile playerAvoidance ... applied=... ground_safe=...`.

Final verification: complete Windows HostX86/x86 Debug build and69/69 CTests
passed(4.52s), PE x86/seven exact exports passed. Complete Linux x86 Debug
build and68 passed/one external-NAV performance test skipped(69 registered,
1.84s), ELF x86/seven exact exports passed. Review caught and fixed stale
creation-ID ranking for an unmanaged fakeclient reusing a managed slot.

Authorized deployment completed2026-11-08 20:58:21 JST after two HLDS-stopped
checks. Destination:
`D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll`.
Backup:`astrabot_mm.dll.bak-20261108-205821`, matching previous SHA256
`7e63f2935c857b7ec493e317f5a3433c9eae740feae15e1b2dfc9d06b9c530ff`.
Built/deployed SHA256 both:
`d71e198a845e79a2aa0746986f8adc5482c7256f91778447446afd1e682b73c0`.
Linux built(not deployed) SHA256:
`5cebffe059d36480017470a9bf739e8f455885bba053fcaa6e35ab2fc8512cd8`.
Receipts,69-test logs and204-source-file manifest are under
build-action-adapter-x86-1451/friend-ledge-nov08-evidence/. The613-file baseline
preservation check found no unrelated changes. No server start/settings change,
staging, commit or push was performed.

FocalSpan and the code-review graph were refreshed. Runtime flow/test edges
remain incomplete in the graph; direct source review, behavioral regressions
and executed adapter tests supply the evidence above. Native replay of this
new artifact is pending; no complete-CSBot-parity or live-FPS claim.

## 2026-11-07 C4 jump cancellation / multi-bot congestion repair

Reported after the 16:05 deployment: planting sometimes jumped and cancelled,
and larger bot populations jammed on shared routes. Action translation stopped
analog movement for C4 but retained navigation's IN_JUMP and directional bits.
SelectC4, Plant and PlantContinue now remove those inputs, retain crouch and
plant attack, and cancel pending jump-crouch sequencing during action holds.
While a current actor/map/round plant attempt is active, navigation and its
stuck monitor are paused; action evaluation still observes completion/timeout.

Compatibility now checks currently perceived friendly positions against the
intended ground path at 0.5-second intervals (100-unit personal space, 30-unit
path radius), waits for `5 - 3 * aggression` seconds, then invalidates the path.
Replanning uses the existing observed teammate-density costs. Waiting resets
the progress monitor instead of triggering Wiggle. If waiting interrupted an
existing Wiggle, release invalidates that old movement/jump decision. Attack,
airborne movement, ladders and active special traversals bypass polite waiting.
No reservations, future teammate-route knowledge, or arbitrary route jitter
were added. Same-route selection remains possible when it is still optimal.

RED/GREEN regressions reproduced both input contamination and missing polite
waiting, plus stale Wiggle resumption after waiting. Tests cover wait expiry,
clearing, crowd-driven alternate-route choice, attack bypass, and friends
behind, beside or on a different floor. These are offline regressions; native
plant continuity and high-population throughput still require a new HLDS log.
Final verification: complete Windows HostX86/x86 Debug build, 68/68 CTests
passed (8.73 s), PE x86 and seven exact exports passed. Complete Linux x86
Debug build, 67 passed and one external-NAV performance test skipped (68
registered, 2.43 s), ELF x86 and seven exact exports passed.

At 16:39:48 JST, HLDS was confirmed stopped twice and the Windows DLL was
redeployed to `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll`.
Backup: `astrabot_mm.dll.bak-20261107-163948`, verified to match the previous
`a84ca61a7707d840735e778c5d443efb2c24ab6a7c035e257d4156da6e50e9aa` DLL.
Built/deployed SHA256 both:
`7e63f2935c857b7ec493e317f5a3433c9eae740feae15e1b2dfc9d06b9c530ff`.
Linux built SHA256:
`76253b0b88323f074f33d3223535f59a0445b290373bc5cbbe05df5139209a1d`.
Receipt, verification hashes and the 202-source-file manifest are under
`build-action-adapter-x86-1451/c4-crowd-evidence/`. Comparison to the prior
source manifest found changes only in the eight intended source/test paths.
No server start, settings change, staging, commit or push was performed.

FocalSpan was refreshed and the code-review graph updated. The graph still
does not resolve this adapter execution chain or the controller's test edges;
targeted source review and executed tests provide the evidence above. Native
acceptance and full CSBot parity are not claimed.

## 2026-11-07 remaining stuck / postplant load repair

The latest `de_dust2` log session (15:12:03–15:20:03 JST, lines
34717–83075) repeatedly stalled on directed link `1736 -> 1735` for about
41 seconds. Compatibility now abandons a ground corridor after five elapsed
seconds without follower progress, preserving the failed-link exclusion for
replanning. This follows the pinned reference's path-progress timeout. Arrival
is processed before expiry; airborne movement, active jump/drop and ladders
are protected. Grounded entry into the next narrow area ends Wiggle even when
the displacement is less than 75 units. Regression tests cover expiry,
landing, late arrival, link exclusion and narrow-area recovery.

The postplant log window included a 250975 us FullUpdate maximum and an
82698 us PathSearch maximum. Guard selection now keeps an immutable snapshot
area index, avoids revisiting candidate areas, and reuses definitive NoRoute
results only with identical map/round/bomb/start/anchor and complete path-cost
inputs. Resource-limit failures are not cached. Every Hide.Spot random draw
is retained. A retained valid guard target skips rebuilding its path-cost
context. NavDocument area lookup and A* record lookup now use ID indexes;
ordered areas, neighbors, open-list traversal and strict cost tie handling
remain unchanged. No Compatibility reservations or artificial retry delay
were added.

Allocation-failure injection also reproduced an inconsistent default document
copy assignment after introducing the area index. Copy assignment now allocates
a complete candidate before committing; all eight injected cases keep lookups
consistent, and failed copies preserve the previous document.

Offline real-NAV comparison: 737-area `de_dust2`, 80 route cases plus 64
cost-context cases produced byte-identical routes, costs and search counters.
For nine bots' first guard selections, five-trial median decreased from
109172 us to 41382 us (62.1%); retained-target median remained 8 us. Accepted
selections (135) and random requests (90) matched. The harness uses optimized
header code with the Debug Core library; this is not a live FPS result.
ScenarioUpdate, BombGuardSelection and PathCostContext profiler boundaries
were added for the next native capture.

Final verification and authorized redeployment (2026-11-07 JST):

| Gate | Result |
|---|---|
| Windows Debug HostX86/x86 complete build / CTest | PASS; 68/68, 7.28 seconds |
| Debian WSL Linux Debug x86 complete build / CTest | PASS; 67 passed, one external-NAV performance test skipped, 2.84 seconds |
| PE / ELF artifact verification | PASS independently; x86 and seven exact Metamod exports each |
| Real-NAV output comparison | 144/144 byte-identical to the pre-index corpus |
| Dirty-tree preservation | 125 pre-task files checked; no unrelated content changed |
| Windows deployed DLL | `a84ca61a7707d840735e778c5d443efb2c24ab6a7c035e257d4156da6e50e9aa` |
| Linux built SO (not deployed) | `ba8e18d7ee115c91e45fba473316f9cadb775a86cb22522eea62fc6fafba6dbb` |

At 16:05:04 JST, with HLDS stopped, the Windows DLL was deployed to
`D:/SteamCMD/cstrike_rehlds/cstrike/addons/astrabot/dlls/astrabot_mm.dll`.
Source and destination SHA-256 match. The previous DLL is preserved as
`astrabot_mm.dll.bak-20261107-160504` in the same directory (SHA-256
`2e587673b70e0f5c422bc25978766e3d7ff9e7af8856169ad434555cea36f0b9`).
Receipt, source manifest, comparison corpora and harnesses are retained locally
under `build-action-adapter-x86-1451/repair-20261107-evidence/`.
No server start or configuration change was performed.

Native repetition of the stalled links and planted-bomb FPS measurement remains
pending; this entry does not claim complete CSBot equivalence or elimination
of every live stuck location. Graph flow/test-edge coverage remains incomplete;
current source, executable tests and artifact hashes provide the evidence above.

## 2026-10-20 movement / dropped C4 / postplant guard repair

The latest `de_dust2` server log contains repeated stuck/Wiggle movement.
The adapter never sets `clearanceAvailable`; the Wiggle jump gate nevertheless
required it, suppressing every recovery jump in this runtime path. Compatibility
now follows the pinned reference's grounded/not-crouched, ladder/NAV and jump
interval checks without inventing a measured head-clearance value. This repairs
a confirmed suppression path, not every possible stuck location.

T dropped-C4 knowledge now receives dynamically resolved `BombDrop` (three
coordinates plus planted flag) and `BombPickup` messages through `WriteCoord`.
Only the actual managed T recipient learns the coordinates; CT, wrong recipients,
incomplete/mixed packets and stale generations do not acquire them. Sight remains
the fallback. Private bomb state is not accessed.

Planted-bomb Guard/Hide now selects NAV cover within 750 units around the bomb
area, prefers sniper flags when the observed weapon is a sniper, and rejects
visible occupied spots. It keeps a choice for 30–60 seconds, reselects when
occupied, and stops on arrival. Missing cover holds the current position instead
of sending every bot onto the C4. There is no Compatibility route reservation.
Candidate selection uses connected NAV areas and a route check; this is not a
claim of identical reference search ordering or complete Hide-state/RNG parity.

Regression coverage includes the previously failing unknown-clearance Wiggle
jump, recipient/length isolation, T recovery behind an opaque wall, pickup/round
invalidation, CT planted-bomb convergence, persistent/unoccupied NAV cover and
the runtime Hide movement/arrival stop. Native repeats of the three reported
scenarios and their FPS remain pending. Build/deployment receipts identify the
tested artifacts separately.

## 2026-10-20 BOT population regression

The deployed server log shows managed bots joining and then being kicked by
Console. Population maintenance counted active humans from `entvars.team`,
which remained zero on this CS server even after public `TeamInfo` announced a
team. With `bot_join_after_player=1`, the desired population therefore stayed
zero. Population maintenance now uses observed `TeamInfo` for every client,
including an explicit spectator/unassigned value, with an engine fallback only
before a message is known. Disconnect/map cleanup invalidates the cached team.

The actor-command regression fails before this repair and passes afterward:
human CT with zero engine team permits bot creation, spectator notification
overrides a stale engine team, and disconnected slot reuse loses the old team.
Live acceptance with a connected human remains pending; offline regression
success must not be treated as that acceptance. Build/deployment identities
are recorded separately in the local redeployment receipt.

## 2026-10-18 Vanilla compatibility repair — active, not accepted

This is the authoritative current status. Historical completion statements below
describe their recorded offline scope and do not close the current 19-difference
repair. See [VANILLA_REPAIR_LEDGER.md](VANILLA_REPAIR_LEDGER.md) for D01–D19,
source integration, tests and remaining gates. The approved repair stages 0–8
are distinct from the legacy planning phase numbers and P03–P12 parity work.

Current branch: `codex/vanilla-csbot-parity`, base `819471d`. The behavioral
reference remains ReGameDLL-CS `b0889847fe6d03898be88acc9e366660efb40ab5`.
Vanilla public APIs are required; optional ReGameDLL capability discovery must
not enable private layout access or become a startup requirement.

Public observation, actual weapon state, profile/aim/reaction, shared round
plant timing, local C4 knowledge, Enhanced-only reservations/optimization,
timed movement recovery, buying/quota, scenario/radio and Teamwork behavior
have been integrated. Scoped reviews found and repaired sniper movement/scope
deadlocks, grenade cancellation and repeat-flash receipt, hostage competition,
automatic-follow state availability and delayed-radio slot-generation reuse.
This remains an uncommitted working-tree implementation, not a release.

Final offline verification of this tree (2026-10-19 JST):

| Gate | Result |
|---|---|
| Windows Debug HostX86/x86 complete build | PASS; cached compiler architecture preserved |
| Windows CTest | 68/68 passed, zero failures, 3.10 seconds |
| Debian WSL Linux Debug x86 complete build | PASS; `-m32`, strict warnings |
| Linux CTest | 67 passed, zero failures, 1 external-NAV performance test skipped; 68 registered, 1.44 seconds |
| JSONL comparison CLI Python tests | 11/11 passed on Windows and Linux; synthetic comparison tests only |
| PE and ELF artifact verifier | PASS independently: x86, seven exact Metamod exports each |
| Source whitespace / staged whitespace | `git diff --check` and `git diff --cached --check` passed; no staging performed |

Verified local build artifacts (not deployed):

| Artifact | SHA-256 |
|---|---|
| `build-action-adapter-x86-1451/astrabot_mm.dll` | `516118b0b7288e195d49bcb7aa6d6abe0acde18d9fd3be94a370065da8dba712` |
| `build-parity-linux-x86/libastrabot_mm.so` | `6f2e2ed07bec74ff41a7c9897bf5bf2136548f6b53c57fad8839a65230cace67` |

The graph was refreshed on the repair branch without errors; affected-flow
coverage remains zero. FocalSpan is updated separately. Neither index verifies
native behavior. A fixture's default BotProfile.db is preserved if it exists.
Movement fixtures distinguish actual C4-goal commands from subsequent stuck
recovery when their stub deliberately reports no displacement.

**Still open:** production capture/normalizer integration, pinned native
reference captures and RNG replay tapes, full D19 consumption equivalence,
Vanilla/ReGameDLL enabled/absent live scenarios on both OSes, supported maps,
1v1/2v2/multi-Bot stability and FPS. The comparison CLI is implemented and tested;
its producer integration is unimplemented, not an API limitation. See
[TRACE_REPLAY.md](TRACE_REPLAY.md). Optional ReGameDLL negotiation currently
supplements zero actor fields (MetadataOnly); custom chatter voice-bank playback
is not implemented. Those are not claims of completed functionality.

No server configuration, deployment, staging, commit or push was performed.
Every D01-D19 row retains reference/live acceptance gates. The approved plan's
all-nineteen completion condition has not been reached; no complete CSBot
equivalence is claimed.

## 2026-10-03 approved local release

The user approved committing and redeploying the three fixes below. Fresh release
gates passed: full MSVC HostX86/x86 Debug build, CTest **56/56** (3.76 s), PE x86
and seven exact Metamod exports. The 1,272,320-byte build retains SHA-256
`ced3e55e32882d6123e1e886e0e559fff2a9260d539331e4f6dd6150fcafb68e`.
Approved destination: `D:/SteamCMD/cstrike_rehlds/cstrike/addons/astrabot/dlls/astrabot_mm.dll`.
Previous DLL SHA-256: `0599732edd351866adffebec2f02972df88cd45a57fd29d1033e90c1269a629f`.
The exact local commit, collision-safe backup and post-copy hash are recorded in
the execution workspace's `three-fix-deployment-receipt.json` after execution.
The built DLL includes NavMovement, RunPlayerMove and SynchronousLog timings,
game/wall window duration, frame count and throttled movementGate diagnostics.
Enable with `astrabot_profile 1`; it remains off by default. No profile CVar,
server start or push is part of this release. Physical runtime/FPS acceptance
and same-condition ZBot differential validation remain open.

## 2026-10-03 airborne continuity, physical-failure retry and preplant variation

Implemented in the working tree based on `98c4d4e5e623291bf7eb373391e8f91770104981`.
This offline amendment preceded the approved local release recorded above.
Final HostX86/x86 MSVC 14.51 Debug full build: PASS; CTest **56/56 PASS**
(5.70 s); PE x86 and seven exact Metamod exports: PASS. Independent direct-source
review: no remaining Critical/Important findings. Built DLL SHA-256
`ced3e55e32882d6123e1e886e0e559fff2a9260d539331e4f6dd6150fcafb68e` (1272320 bytes).
Live movement, collision, plant outcomes, team behavior and FPS remain unverified.

The comparison is independently interpreted public ReGameDLL_CS CSBot at
`b0889847fe6d03898be88acc9e366660efb40ab5`, not an identified ZBot binary.
No proprietary implementation was copied and no exact parity is claimed.

- Read-only qconsole evidence SHA-256
  `396371ce63c8ea59bb61dc14dd098933c324cd91e6470401b50974d353d240f6`:
  all 103 observed armed jumps were followed by an airborne neutral command.
  The continuing-movement gate incorrectly reused grounded spawn readiness.
  Initial joined/spawn readiness still requires grounding; established live
  movement preserves flight direction, route and landing tracker. Cached
  IN_JUMP is stripped while airborne. FreezeTime, death and spectator gates stay.
  Actual grounded landing feedback exposes measured health loss separately.
- After the first human A plant, the log records a successful route repeatedly
  vetoed as UnsafeDrop around directed link 1504 -> 1505. Assignment generation
  changed 36 times in the sampled interval; A* success did not establish physical
  traversal safety. Unsafe failures now retain bomb entity identity, failed link
  and position anchor across objective/roam reassignment. One immediate directed
  bypass is allowed; failed retry uses 128, 256 then 512 active server frames.
  FreezeTime pauses this Core budget. Expiry permits one fresh safety evaluation
  of the original route. New bomb, lifecycle reset or >=64-unit grounded XY
  displacement clears the history; adjacent-area jitter does not.
  This is a bounded Astra policy, not a recovered ZBot constant.
- Existing coordinator failure handling hands the bomb objective to another
  eligible CT; repeated old-owner failure reports cannot invalidate that owner.
  Core locomotion vetoes now identify the actual rejected corridor link after
  a safe walking prefix, rather than reporting the preceding walking link.
- Opt-in profiler adds actual game and wall window seconds and StartFrame count,
  and separates Core NavMovement, the engine RunPlayerMove callback and synchronous
  runtime console logging. Parent stages remain inclusive and must not be summed.
  movementGate emits at most one record per bot per second. Disabled scopes read
  no additional wall clock. These diagnostics do not demonstrate an FPS gain.
- Public CSBot manager chooses an earliest plant decision at RestartRound time
  + random 10..30 seconds (`cs_bot_manager.cpp:114-129`); that includes FreezeTime.
  Idle/Hunt can move the carrier before later closest-zone selection
  (`cs_bot_idle.cpp:172-190,879-880`, `cs_bot_hunt.cpp:67-74,171-213`).
  Astra now samples an initial reachable site through the existing compatibility
  RNG boundary and keeps it for the carrier/round. At deadline, site arrival or
  urgency it transitions once to the nearest currently reachable route.
  Candidate travel plus five seconds of plant margin is reserved; missing timing,
  RNG or NAV authority falls back to the existing nearest route. Carrier loss,
  death, new round and observed planted bomb cancel the approach.
  This bounded site-directed movement differs from reference whole-map Hunt and
  SAFEST/danger-weighted route selection. It enables A/B alternatives without a
  claimed or enforced 50:50 final plant ratio. No learning was implemented.
- Offline regression coverage includes 10/60/90/100/128 Hz and variable command
  cadence, airborne single-jump/landing, delayed engine/log callback attribution,
  independently captured disabled diagnostics, dangerous-link bypass and bounded
  expiry at multiple frame gaps, entity generation and progress resets, FreezeTime,
  real controller failure -> CT handoff, multiple seed/round/start site choices,
  same-seed replay, held goals, timer/arrival transitions, unreachable and urgent
  fallback, and human-planted C4 -> CT movement/IN_USE convergence.
  Fixtures simulate public SDK observations, not BSP collision or engine physics.

## 2026-10-02 full-update ground cadence and bounded off-mesh commands

IMPLEMENTED_OFFLINE_VERIFIED for ground observation cadence and horizontal
recovery command construction and bounded recovery-candidate reselection.
Physical escape from a blocked off-mesh position remains LIVE_UNVERIFIED;
failed/exhausted attempts are not counted as recovery success.

Comparator: independently interpreted ReGameDLL_CS
`b0889847fe6d03898be88acc9e366660efb40ab5`, `cs_bot_nav.cpp:202-253`
(current-direction ground probes), `cs_bot.cpp:382` (StayOnNavMesh), and
`cs_bot_nav.cpp` (Wiggle). This is a fixed public CSBot proxy. No specific
ZBot binary/version or live differential run was available; no exact ZBot
parity or proprietary-source reuse is claimed.

- Read-only evidence: de_dust2 qconsole.log, SHA-256
  `ad86503d384270d2b6636db29ca03c31cdf05c2af2148dea790164b7a1c8aa5b`.
  Bert's 53 pre-ascent traversal observations had frame gaps 2 through 14,
  none 1. The old route required exactly the next server frame although the
  adapter probes only at full updates. The log contains an emitted jump and
  ascent before subsequent area=0 recovery, so it does not prove that the
  user's manual push alone caused ascent or identify its exact time.
- The last-round four-bot stalls are observed with freeze=0. Recovery XY
  direction shrank near 0.02 under 3D normalization, reducing 240-unit intent
  to roughly 4-unit horizontal command speed. This is separate from pre-wall
  missed lookahead and is not an objective waiting or freeze diagnosis.
- Ground routes now require the next full-update sequence and age <=0.25 s,
  a newer server frame, and unchanged actor/generation/map/round scope.
  Traces are sampled afresh at the current position; target/direction and
  existing grounding, NO_JUMP, drop/gap and height gates remain enforced.
- Recovery independently chooses a NAV geometry candidate with height in
  ranking (64-unit vertical tolerance), aims 5 units inside its XY bounds
  (half-width for thin areas), records the actual target and normalizes XY
  only. This is geometry-only, not proof of collision clearance or reachability.
- After 20 consecutive unsuppressed full updates without >1-unit cumulative
  improvement, the failed candidate is excluded, route state is cleared and
  NoRoute/RecoveryNoProgress reports one candidate-failure event. The next full
  update selects a different eligible candidate. Maximum four failed candidates
  are retained; no eligible candidate or an exhausted four-attempt budget is
  the final neutral-stop condition, not the first 20-update failure.
- A candidate failure joins the existing objectivePathFailed feed. The existing
  coordinator releases/reassigns the objective; PlantC4 keeps its existing
  carrier-specific semantics. Goal replacement retains recovery exclusions,
  preventing repeated selection of the same failed recovery area. Final blocked
  updates do not repeatedly report objective failure. Exact NAV re-entry clears
  recovery history before objective resolution and resumes normal route search.
  >=8-unit external 3D displacement from the first failed position, actor/map/
  round change or controller reset also permits a fresh attempt. FreezeTime
  spends no progress budget. Final exhaustion may remain stopped if no new
  position/lifecycle evidence arrives; there is no physical escape guarantee.
- Opt-in profiler diagnostics add one groundRecovery record per bot per
  second with update/route age, fresh far80/near30/gap10 trace values, selected
  recovery point, geometry-only flag, no-progress count, one-shot candidate
  failure flag and explicit reason.
- RED evidence: old cadence fails normal/variable FPS adapter fixtures; old
  recovery fails XY direction/target, stacked-height and bounded-progress
  assertions. GREEN covers far-missing/sloped cases at baseline 10, 60, 90,
  100, 128 and variable FPS (12 adapter scenarios), scope/age boundaries,
  sequence wrap, FreezeTime, small XY/large Z, stacked/thin NAV, Z tolerance,
  cutoff/reset transitions, cumulative progress and oscillation. These are
  synthetic SDK fixtures, not physical BSP replay or live FPS measurements.
- Additional RED: the first-stop implementation fails alternate-candidate
  and public RunPlayerMove direction tests. GREEN: failed candidate -> different
  candidate, all-candidate/four-attempt exhaustion, retained exclusions across
  goal replacement, alternate NAV entry -> objective path search, public
  RunPlayerMove alternate direction -> neutral only after both fixture candidates
  fail -> routed movement after NAV entry. Recovery controller -> existing team
  coordinator integration reassigns dropped C4 to another actor and lets the old
  owner attempt another recovery area. NAV entry with an unresolved objective
  also clears history. Fixtures are synthetic; they do not model engine physics.
- Final HostX86/x86 MSVC 14.51.36231 Debug full build: PASS; CTest 56/56 PASS
  (6.49 s); PE x86 and seven exact Metamod exports: PASS. Read-only final
  review: no Critical/Important findings.
- Working-tree build based on HEAD
  `1feec9a5f0cb39da88b2291935cd34c011951071`: 1,210,880 bytes,
  SHA-256 `0599732edd351866adffebec2f02972df88cd45a57fd29d1033e90c1269a629f`.
  This records the pre-commit offline artifact. The approved October 3 local
  commit/deployment receipt is reselection-deployment-receipt.json in the
  execution workspace. The previous server DLL has SHA-256
  `80c505ee1d75422845c1549507c1c2d20de88971516deb80f622b21d5b201980`.
- Live wall clearance/landing, pushed-off-NAV automatic escape, actual FPS,
  multi-bot performance and reference differential validation: NOT RUN.
  Offline evidence: reselection-red.log and reselection-full-build.log in the
  execution workspace; at offline verification, 10 tracked paths changed without staging or commit.
  Recovery candidate selection scans all NAV areas; live cost is unmeasured.
  P07/P07.6 remains PARTIAL. P08 is not started.

## 2026-10-02 combined round-freeze and ground-lookahead verification

IMPLEMENTED_OFFLINE_VERIFIED. Normal Round_End schedules FreezeTime after
mp_round_restart_delay; Round_Start ends it without starting another interval.
Initial Game_Commencing schedules it after the ordinary-game three-second
restart delay. Prepared-generation duplicate Game_Commencing/Round_End events
cannot restart or extend that interval. Reference: independently interpreted
ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5,
multiplay_gamerules.cpp (Game_Commencing/NeededPlayersCheck, RestartRound and
OnRoundFreezeEnd). Career/custom hook delays are unavailable at this boundary.

- Reproduction: restoring the baseline HEAD lifecycle fails the Round_Start
  end-freeze regression. Before the initial-round fix, its three-second start
  boundary regression also fails. The corrected public adapter passes both.
- Public execution regression covers three frozen frames, zero movement and
  horizontal velocity, retained command timing/vertical velocity, no ground
  lookahead probes, and movement resumption after Round_Start.
- Core suppression preserves the saved objective route without accumulating
  stuck time. The bounded near30 fallback and scope/safety tests remain green.
- Complete HostX86/x86 MSVC NMake Debug build: PASS. Complete CTest: 56/56 PASS
  (2.87 seconds). PE x86/seven exact Metamod exports: PASS.
- Final combined Debug DLL: build-action-adapter-x86-1451/astrabot_mm.dll,
  1210880 bytes, SHA-256 80c505ee1d75422845c1549507c1c2d20de88971516deb80f622b21d5b201980.
- Both approved changes and their pre-existing source/test prerequisites are
  included in the local commit scope; no push. Deployment receipt and final commit
  identity are recorded outside the repository after the stopped-server copy.
- No server was started for this verification. Live HLDS/ReHLDS timing,
  physical clearance/landing, FPS and multi-Bot acceptance remain NOT RUN.
  Historical September 27 logs describe the old deployed DLL only.

This record supersedes the earlier near-ground-only artifact below.
P07/P07.6 remains PARTIAL; P08 is not started.

## 2026-10-02 bounded near-ground jump fallback

`IMPLEMENTED_OFFLINE_VERIFIED` for the approved 30-unit upward-jump fallback.
Pinned CSBot `b0889847fe6d03898be88acc9e366660efb40ab5`,
`cs_bot_nav.cpp:202-253`, continues to the near probe when the running-only
80-unit probe misses or finds a slope. The current Astra adapter and Core no
longer require flat far ground for that near-ground decision. Known far-drop
and missing immediate-ground vetoes, configured step height, 41.8-unit jump
limit, current-area NAV_NO_JUMP, grounding/ladder and observation-scope checks
remain intact. This is a bounded fallback improvement, not full 80/30/10 parity.

- Baseline: `7f86bdb54c6bfd725de52a75a4fe6fcfdfc686fd` plus nine pre-existing
  modified source/test files. This earlier record predates the final combined FreezeTime build above.
- Core regression: 17 cases cover fallback and retained safety/height gates;
  six cases failed against the old Core gate before the fix.
- Public-adapter fixture: deterministic engine RNG + synthetic NAV/ground
  traces -> PluginRuntime -> RunPlayerMove emits IN_JUMP for missing/sloped
  far ground. Both cases fail when only the old adapter gate is restored.
- Actor slot/map/round/actor-generation/frame and target/direction isolation
  are covered by the locomotion and route-scope tests.
- Focused locomotion + actor-command CTest: 2/2 PASS.
- Full HostX86/x86 MSVC 14.51.36231 NMake Debug build: PASS;
  complete CTest: 56/56 PASS, 4.62 seconds.
- `verify_x86_artifact.py`: PASS, PE x86 and seven exact Metamod exports.
  Debug DLL: `build-action-adapter-x86-1451/astrabot_mm.dll`, 1,210,880 bytes,
  SHA-256 `4d7abb7f314674cf18212e271d14fd6ff6ea3db8269bb98b79c84dd3f6d254cb`.
- Live HLDS/ReHLDS, physical clearance/landing, FPS, multi-Bot, Linux x86
  and full pinned differential validation: NOT RUN in this change.

The DLL identity above supersedes historical build identities only for this
dirty working-tree Debug build. P07/P07.6 remains PARTIAL; P08 is not started.

## P07.6 production runtime integration audit

`PARTIAL` — the production adapter now has a public freeze execution gate,
typed Goal/NAV transaction diagnostics, persistent-route temporary-area
handling, and opt-in one-second profiler counters. Focused x86 tests are
offline evidence only. Fresh live HLDS/ReHLDS freeze, Goal, physical progress,
FPS, and multi-Bot intervals remain open. `P08 NOT STARTED`.

P07.6-PERF additionally measures exact visibility TraceLine/FOV/LOS/body-probe
counts and one-second stage aggregates, adds default-off vision/path/trace A/B
CVars, and avoids non-objective bomb-site NAV corridor enumeration. No live FPS
or alive-Bot scaling result is inferred from these offline checks.

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
