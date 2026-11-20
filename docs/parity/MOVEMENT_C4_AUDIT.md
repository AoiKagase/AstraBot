# Movement / C4 reference audit and execution ledger

Reference: ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5,
REGAMEDLL_FIXES enabled. Scope: Compatibility movement, friend avoidance,
navigation lifecycle, C4 fetch/plant/guard/defuse/escape. Enhanced remains intact.
Vanilla public Engine/messages/events only; no private data or fixed offsets.

Baseline: build-action-adapter-x86-1451/movement-c4-audit-evidence/baseline.json
and before/. Existing dirty/untracked files and deployed/root DLL are preserved.

## Contract ledger

| ID | Reference and ordering | Current Astra path / omission | Verification / status |
|---|---|---|---|
| MC01 | CBot::BotThink: Upkeep, ResetCommand, Update, ExecuteCommand; state owns path update | Tagged pre/post navigation boundary; Idle/Hunt lifecycle connected | Fixed/offline verified; native assessment below |
| MC02 | UpdatePathMovement: FindPathPoint before IsFriendInTheWay; 0.5s check, 5-3*aggression patience, movement on expiry then destroyed path | Current follower target preview; deferred destruction feedback | Fixed/offline verified; native assessment below |
| MC03 | StuckCheck bot lifetime: five velocity samples, 20 ground/10 ladder threshold; stuck clears after >75 displacement; explicit ResetStuckMonitor only | Persistent monitor across routes, pre-intent samples, explicit lifecycle reset | Fixed/offline verified; narrow-area/air safety differences retained |
| MC04 | MoveTowardsPosition +/-0.25; StrafeAway +/-0.5 FIXES; GetMoveSpeed=maxspeed if run/crouch else 0.4*maxspeed | Directional projection/public speed; actual objective crouch | Fixed/offline verified; native assessment below |
| MC05 | MoveTo non-PROGRESSING -> task-specific exit; defuse known bomb, feet distance<100, not reloading; else Idle | Same-update typed result and tagged known target; bounded reload inference | Fixed/offline verified; exact hidden reload timer constrained |
| MC06 | Defuse OnEnter crouch/self-defense; Use, >1s confirm defuser, Idle on no/other defuser; OnExit stand/reset/look cleanup | Time-bounded BarTime and explicit Unknown; one-second reevaluation | Fixed/offline verified; internal defuser unavailable via Vanilla |
| MC07 | Escape OnEnter stand/run/destroy/knife; UpdatePathMovement then random destination+FASTEST iff not progressing | Duplicate preflight and radius redraw removed; typed failure invalidation | Fixed/offline verified; native assessment below |
| MC08 | Fetch, Plant, Hide own entry/update/exit; plant timeout5s; actual command/result distinct | Transition corridor cleanup, objective holds, plant timeout and guard exit | Fixed/offline verified for listed regressions; complete state equivalence unproven |

Every completed row must record inputs, retained state, reset triggers, RNG
requests, current code path, failing/passing tests and observation limitations.
Source-derived expectations are not executed-reference traces. Native command
intent is not proof of physical progress.

## Execution

| ID | Public inputs / retained state | Reset boundary / RNG ownership | Executable regression coverage |
|---|---|---|---|
| MC01 | actor/frame, alive/freeze, active state/task, NAV readiness; state entry time and Attack overlay | actor/map/round; real state entry/exit once; no new selection draws in post-pass | compat_state_machine, compat_actor_command |
| MC02 | current path point, player origins/teams, priority/aggression; cached friend/patience/current intent | friend change, expiry, real corridor invalidation; no new RNG in target preview | nav_roam_controller, player_avoidance |
| MC03 | public origin, ground/ladder/crouch, update time; five velocities, stuck spot, Wiggle deadlines | explicit monitor reset, >75 displacement, actor/map/round/freeze; typed direction/duration/jump draws remain on controller RNG | locomotion, nav_roam_controller |
| MC04 | direction, view yaw, public maxspeed, posture, existing axis/buttons; final command | ResetCommand per update, last axis writer wins, freeze veto; no movement RNG | action_adapter, player_avoidance, compat_actor_command |
| MC05 | tagged path outcome, known bomb, feet position, reload observation/lease; pending destroyed path | goal/actor/map/round change or one consumption; Attack defers; no extra retry draw | compat_state_machine, scenario_behavior, compat_actor_command |
| MC06 | BarTime recipient/time/duration, visible human defuser; attempt entry and receipt expiry | cancel, expiration, new attempt, actor/map/round; no random start confirmation | public_observation, compat_state_machine, compat_actor_command |
| MC07 | known planted bomb, public NAV, path result; selected escape area/path | EndOfPath/PathFailure or lifecycle;32-candidate draw once per destination selection, actual path search afterward | bomb_navigation, scenario_behavior, nav_roam_controller |
| MC08 | C4 ownership, site trigger, active weapon, planted/drop notices, guard target; approach/plant/guard lifetimes | pickup/loss, confirmed plant, >5s plant attempt, guard end, lifecycle; shared approach deadline not redrawn per Bot | compat_actor_command, public_bomb_clock, scenario_behavior, action_adapter |

The suites above run in full CTest on both builds. Cases such as the exact
100-unit floating-point boundary, all slopes/ladders, and every original native
stall geometry are not individually proven by suite names; native exposure and
precision limitations below remain separate acceptance entries.

- Baseline saved and ledger created before implementation.
- Reference source inspection from planning found no pinned-source difference.
- Public-information gaps remain Unknown; bounded retry is labelled inferred.
- No commit/push, broad staging or root DLL replacement is authorized here.
- Source-derived regression tests and both complete x86 builds pass. Backed-up
  deployment is complete; final native replay and STATUS evidence follow below.

## Resolved reference contracts and implementation

The following expectations are derived from the pinned sources, not from an
executed CSBot trace. Source parity and native acceptance are separate gates.

### MC01 / MC05: state and path ownership

CBot::BotThink runs Upkeep at 1/30s, then (when due) ResetCommand and Update at
1/10s, then ExecuteCommand. CCSBot::Update calls StuckCheck before its normal
state/Attack update. MoveTo::OnEnter selects FASTEST for defuse/find-bomb and
SAFEST for ordinary tasks and computes a new path. Idle's old roam path is not
the new MoveTo path. Idle::OnEnter destroys its path; it does not recreate the
Bot's complete physical history.

MoveTo::OnUpdate performs task checks, calls UpdatePathMovement, and only when
the result is not PROGRESSING applies task-specific completion checks. Defuse
checks a known position (not necessarily currently visible), a non-reloading
weapon, and a strict distance below100 after substituting feet Z. The default
exit is Idle. A live defuser visible at the bomb can instead cause guarding.

Astra now carries a tagged NavigationStep outcome into finishNavigation in the
same full update. Ordinary link continuation and polite waiting are Progressing;
unavailable input is not PathFailure. The post-pass uses the pre-pass's actor,
map/round/frame-tagged StateBehaviorInput. Rebuilding that input from an
incomplete objective cache had caused immediate Defuse->Idle reversal in the
integration fixture; retaining the actual observed input fixes that boundary.
Entry transitions abandon old corridors, preserving physical history. A path
destroyed after a valid movement command is reported missing on the next step,
rather than silently rebuilt for MoveTo. Attack defers that pending result.

The intermediate native run also exposed ordinary roaming still labelled Idle.
The pinned Idle/Hunt lifecycle enters Hunt after task selection, and Hunt returns
to Idle after30s for reselection. A failing regression now covers this lifetime;
navigation-ready Compatibility callers enter Hunt and retain its timestamp.
Callers without a valid NAV and legacy geometry-only callers retain their gates.

Tests: compat_state_machine (geometric radius does not end Progressing;
non-progressing path can enter Defuse; unconfirmed attempt exits after1s),
compat_actor_command (public path failure, real NAV defuse handoff, no premature
occluded use). The tests initially failed against the corresponding old paths.

### MC02 / MC03: friend checks and physical monitoring

UpdatePathMovement computes FindPathPoint first, then IsFriendInTheWay uses
public player origins within100, projection in front, and distance<30 to the
finite origin-to-floor-goal segment. Its result is cached0.5s. Patience is
5-3*aggression; expiry destroys the path but still issues this update's saved
movement. That current update remains Progressing unless another failure occurs.

StuckCheck keeps five samples on the Bot, uses the fixed0.1s full-think interval,
20 units/s on ground and10 on ladders, and latches stuck until displacement>75.
ResetStuckMonitor is an explicit operation. Wiggle samples direction0..3 and
duration0.5..1.5; initial jump delay0..0.5 and subsequent1..2 intervals apply
only with the reference jump eligibility gates. It does not Wiggle crouched.

Astra previews the current follower target without advancing the real follower.
A reached/invalid preview cannot reuse an old friend ray; active stuck recovery
retains its still-valid path target. The adapter now samples public entity-origin
motion before state execution, even without a path. Same-time follower sampling
does not overwrite that origin with feet or sample twice. Route start/clear does
not erase Compatibility's velocity history; actor/map/round/freeze and explicit
interaction exits do. Enhanced retains its own monitor/reset behavior.

Tests: locomotion (repeated valid repaths without displacement; pre-path sampling;
stuck-to-airborne transition), nav_roam_controller (friend patience, view-relative
Wiggle, clock/lifecycle, adjacent ledge/drop safety). Repath and airborne cases
were observed failing before their corrections.

Ruling: preserve the prior grounded-next-narrow-area recovery exit and airborne
continuation safety. These are explicit safety adaptations, not exact CSBot
StuckCheck identity; otherwise the existing narrow-portal/air regressions return.

### MC04: actual command composition

MoveTowardsPosition applies +/-0.25 projections to directional movement methods;
StrafeAwayFromPosition uses the REGAMEDLL_FIXES +/-0.5 thresholds. GetMoveSpeed
returns public maxspeed when running/crouching, otherwise0.4*maxspeed. Direction
methods replace only their axis and make opposite buttons mutually exclusive.
ExecuteCommand adds IN_DUCK from posture and zeroes frozen commands.

Astra's Compatibility projection now produces directional inputs, while Enhanced
keeps continuous projection. Runtime uses public maxspeed, the final-area walk
factor, and the same current speed for make-way instead of fixed240. Objective
Plant/Defuse states put crouch into actual ActionProposal/buttons and suppress
pending movement/jump. The integration planting posture test failed before this
connection. Action_adapter tests cover continuous-vs-reference projection.

Ruling: retain the existing ground-safety veto and hard planting hold; do not
reintroduce planting cancellation merely to mimic an unconstrained make-way.

### MC06: defuse receipts and public-information limits

Defuse::OnEnter crouches and adopts self-defense; OnUpdate looks at the known
bomb and uses it. After more than1s, an absent/different internal defuser causes
Idle. OnExit stands, resets the stuck monitor, restores task/disposition/look.
Those internal defuser facts are not universally exposed by Vanilla.

Public BarTime retains recipient actor, map/round/frame, observation time and
duration. A positive, unexpired receipt from this attempt is Self; a valid cancel
is None; missing, expired or prior-attempt data is Unknown. A currently visible
bomb plus an observed external human defuser can establish Other. Unknown after
the1s start-confirmation deadline ends the unconfirmed trial and reevaluates;
this bounded policy is explicitly inferred, not a false claim of confirmed None.
The outer scenario cannot replace an active Defuse state before its own result
handling. CT guarding returns to Idle when its observed defuser stops.

Public reload-request tracking is also inferred. Its five-second command lease
expires even if a rejected request never produces a clip increase. It is not
the GameDLL reload timer; exact optional reload observations take precedence.
Unknown is not published as a known non-reloading weapon. Public observation
tests cover expiry, cancellation, old attempt, round mismatch and rejected
reload request lifetime. Defuse/Plant/Buy and reached guard holds perform no
navigation search or Wiggle while intentionally stationary.

### MC07 / MC08: bomb lifecycle and destination selection

Escape::OnEnter stands/runs, destroys its path and equips knife. OnUpdate keeps
knife/look-around, advances its path, and draws/computes a FASTEST replacement
only if path movement is not progressing. FindMinimumCostArea retains32 lowest
reciprocal-distance costs among areas at least150x150, preserves iteration-order
ties, and makes one random draw; if none qualify it draws from all areas.
Reachability is tested by subsequent path computation, not the candidate draw.

Astra retains that selection helper, removes the scenario's duplicate preflight
escape A*, and no longer redraws solely for proximity<64 or normal link replan.
Only typed EndOfPath/PathFailure invalidates the cached escape area. Actual
patience destruction is deferred; it is not an extra draw during a moving frame.
Travel estimation is skipped for T and for already escaping/defusing actors.
The ordering distance stored before the real path search is an explicit straight
line lower bound, not an actual corridor cost.

Fetch::OnEnter destroys its old path; a successful pickup or disappearance
returns to task selection. Its failed ComputePath has a Hunt call followed by
continued path-result handling: the source's comment alone is not an unconditional
Hunt outcome. Plant::OnEnter crouches/looks down, selects C4 until observed,
attacks while held, guards after confirmed plant, and times out after5s from state
entry. Hide must leave GUARD_BOMB_DEFUSER when that observed defuser is gone.
Existing public C4 knowledge, carrier transfer, cover/camp and no-jump tests are
retained; actual input delivery remains distinct from observed planting success.

The second native iteration exposed an additional MC08 boundary: the state
correctly held still, but the legacy objective action required an old route target
within96 units. A large bomb trigger could therefore hold the carrier crouched
without attacking. A regression moves an already planting actor within a wide
public bombsite away from its old destination and fails on that implementation.
Compatibility Plant now directly owns observed C4 selection and attack, looking
down while crouched; its5s state deadline remains the owner of retries. Public
site membership and C4 ownership are still required. The legacy geometry action
remains available to Enhanced. The aborted trial is preserved as iteration2,
including the prolonged Plant holds; these are not hidden by transit-only metrics.

The20-minute iteration3 then exposed a second independent observation issue:
CurWeapon(active C4) followed by CurWeapon(inactive grenade4) replaced the legacy
active-weapon cache. The public store already retained the active entry, but the
C4 action path read the legacy cache. That cache now ignores inactive *different*
valid inventory entries only when the retained active entry belongs to this
actor/map/round. Deactivation of the current weapon and reset sentinels still
invalidate it. The public message sequence fails the added integration test
before correction and passes afterward; an explicit current-C4 deactivation test
ensures this does not invent an equipped C4. Iteration3's later five-second
selection holds remain archived alongside the first18 successful plants.

## Evidence gates and remaining precision limits

### Open MC01/MC03 geometry counterexample (not an observation limitation)

The20-minute native iteration3 reproduces a38.372s noncarrier ledge stop, Bot2 at game
48.292–86.664. The sampled origin settles at(-443.9,-632.1,178.9), standing feet
142.9. NAV area1734 interpolates floor131.3484 there, a11.55-unit discrepancy.
The outer containing query accepts the area with64 tolerance; JumpDrop's first
launch update requires8 and rejects the1734→96 Drop before ordinary follower/
Wiggle. A* nevertheless returns Found toward1793. The resulting PathFailure/
Idle/neutral loop is real, not a corpse heartbeat or an intentional guard hold.

Pinned cs_bot_pathfind.cpp:1512–1520 emits movement/Wiggle before its progress
timeout; cs_bot_nav.cpp:166–174 uses traced ground and NAV floor without this
additional8-unit launch membership gate. This remaining integration discrepancy
is **unimplemented/unresolved**. Physical launch validation or safe recovery must
be made consistent, with actual-height drop damage, air continuation and landing
checks retained. Increasing a global tolerance without those checks is not an
accepted fix. The concrete input is preserved in
`movement-c4-audit-evidence/residual-ledge-case.json`; a dedicated sloped-area
repeated-repath regression is still pending. Offline monitor tests do not close
this native geometry counterexample.

The final cache-corrected run also contains a13.005s Hunt wait at game
610.217–623.222, Bot3 moving less than24 units around(-638.5,-881.2,164).
Of103 full-update commands,97 are neutral, while the tagged path result remains
Progressing and three goals are selected over the interval. This remains an
open MC01/MC02 traffic/reselection case; its cause is not proven to be the ledge
launch mismatch. A dedicated replay/regression is pending. State-local success,
goal changes and earlier smaller maxima do not close this aggregate stop.

The [controlled area 41 replay](STATUS.md)
used the recorded start and adjacent goals with known teammate positions.
It identified a direct-route steering difference. The goal sequence and
teammate geometry of the historical stop remain unknown.

Review found and prompted corrections for raw-Replan escape invalidation,
interaction-hold navigation/RNG, lifecycle velocity reset, airborne stuck latch,
unbounded reload-request inference and monitoring before path intent exists.
The November8 code-review graph lacks the new symbols; current source and
behavioral tests provide coverage, not graph presence. FocalSpan is refreshed.

No executed-reference trace has been captured for this revision. Final native
acceptance, source/artifact/deployment hashes, physical episodes and remaining
NoRoute/Idle stalls are recorded in the current STATUS section and comparison.json.
Original exact causal setups not reproduced remain unverified. Full CSBot parity,
precise hidden weapon/defuser timing and Linux/Vanilla native acceptance are not
implied by offline passing tests.


## MC09: Hunt destination selection follow-up (2026-11-11)

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



## MC10: B-site progress-timeout exclusion repair (2026-11-12)

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



## MC11: A-side ramp / explicit path endpoint repair (2026-11-12)

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
