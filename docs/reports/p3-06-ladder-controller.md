# P3-06 portable ladder controller

Current status: the [host integration report](p3-06-ladder-host.md) describes
the completed host connection and its offline evidence. The sections below
record the successive implementation slices; their pending-work statements
describe those earlier checkpoints, not the current implementation.

This slice implements the portable Approach/Align/Contact/ClimbUp/ClimbDown/
Exit/Support lifecycle, abort/fall/timeouts and at most one fresh-clearance
re-acquisition. P3-06 first-class host motion remains incomplete.

Every accepted observation binds actor/map/route/step/tick and the selected
source/generation/link, exact origin/velocity/hull and current target. Inspection
has a 21-query ceiling. Terminal output releases actions and reports once;
re-acquisition does not restart the overall climb deadline. Completion requires
target-area support and independently observed detachment from ladder mode/contact.

Motor supports explicit Forward/Back Press/Hold/Release, rejects opposing
requests and suppresses directional buttons at zero effective speed. Ordinary
Walk does not infer these buttons. Standard CS ladder movement consumes buttons;
its vertical projection is controlled by facing/pitch, not analog magnitude.
The reference is the pinned ReGameDLL revision
`b0889847fe6d03898be88acc9e366660efb40ab5`, `PM_LadderMove` in
`regamedll/pm_shared/pm_shared.cpp`. No upstream implementation was copied.

While attached, Exit requires a fresh clearance-approved host `exitIntent`.
There is deliberately no analog-walking substitute for actual ladder dismount
physics. No live producer of that intent or LadderInspection exists yet, and
production Walk still rejects Ladder traversal. The next slice must bind the
selected immutable passage and solve bounded host inspection/dismount control
before enabling guarded dispatch or advancing a route cursor.

Tests cover independent standard-CS vertical projection in both directions at
8/16/100 ms, command button semantics, stale ownership/inspection, missing
observations, blockage, fall, finite re-acquisition and terminal uniqueness.
Separate scripted observations cover landing/detachment acceptance, including
grounded-but-attached and wrong-area regressions. These are not a full physical
simulation of mount/dismount or live-server acceptance.

Verification: Windows x86 NMake Debug /W4 /WX, 43/43 CTest passed. Linux x86
GCC -m32 Debug portable tests passed 37/37. Windows x86 Release build and exact
six DLL exports also passed. No live
server, Finish decision or new real-NAV compatibility claim is included.

## Selected-link host binding

`bindLadderPlan` now maps an exact route link to its owned discovery passage and
portable plan. It checks the current map, publication fingerprint/generation,
all selected-link fields, pair direction and endpoint correspondence, and current
entity/model/bounds identity. Up/down swap start/end and mount/dismount while
retaining the measured outward normal. Malformed pair counts, duplicate IDs,
changed route-link data and stale entities return typed failure with no plan.
Work is bounded to 2048 link comparisons with no traces or allocations.

This is identity binding only. It neither refreshes passage clearance nor
dispatches movement; the host inspection, physical dismount planner and route
integration remain pending. Adapter tests cover both directions, ownership
mismatches, corrupt publication shape/duplicates/endpoints and entity replacement.
Binding verification: Windows x86 Debug 43/43 CTest, Release build and six exports
passed. These adapter-only additions do not change the Linux portable sources.

## Current-frame inspection

`inspectLadderFrame` produces a current LadderInspection, selected-model contact
and separately observed MOVETYPE_FLY. A mandatory host callback validates
actor/agent/route/step/tick ownership. Each trace also rechecks map, ladder
identity/model/bounds, actor edict/serial and exact origin/velocity/view/hull,
movement mode, ground/duck flags and maxspeed. Water/basevelocity and nonstandard
movement modes are unsupported. A failure returns no observation.

The bound is four traces: standing-hull model overlap, point-model face normal
when touching, standing-hull path sweep ignoring only the actor, and a measured
world-BSP floor bound to containing NAV when grounded. A sweep spans at most
96 horizontal units and 4100 vertical units. Lower budgets stop without retry.
Inspection does not invent an exit intent or renew the entire discovery batch.

Fixtures test grounded/airborne observations, all exhausted budgets, per-trace
actor/map/ladder mutation, bad traces, wrong face, blockage, unsupported floor,
stale tick and missing ownership evidence. The four-query contact-plus-support
case preserves absent NAV support in the shaft instead of assigning an area.
Frame-inspection verification: Windows x86 NMake Debug /W4 /WX, 43/43 CTest
passed; x86 Release build and exact six exports passed. Linux portable source
files are unchanged by this adapter slice.

The host still must wire its registry/route callback and dispatch guards. The
engine's MOVETYPE_FLY is an observation from the preceding movement update;
detachment can change model overlap before that field returns to WALK. Controller
integration must handle this transition explicitly, not equate overlap with mode.
Physical exit planning remains required, especially clearing the upper hull
boundary and crossing to measured support with real CS air acceleration.

## Standard-CS movement prediction and exit mode handoff

`ladderVelocity` predicts standing ladder button projection against a measured
vertical face, including the independent 200-unit outward kick when the floor
point is solid and input points away from the ladder. Analog movement alone is
not a climb command. Looking upward at -45 degrees can produce approximately
282.84 vertical units/s at the200 base speed; this is distinct from the controller's
current slower alignment profile. The caller must prove contact, face and floor
point solidity before using this prediction.

`ladderAirStep` predicts one collision-free airborne WALK command using actual
rounded command milliseconds (1..120), supplied gravity/airacceleration/friction/
maxspeed, and the30-unit wish-direction component cap. It preserves pre-existing
horizontal velocity and uses half-gravity displacement/full-gravity velocity.
Neither helper performs a sweep, authenticates host cvars, chooses an exit route
or models a collision/landing. Jump/duck/use, water and basevelocity are outside
this profile. The formulas were independently implemented from the pinned
ReGameDLL PM_LadderMove/PM_AirAccelerate/PM_PlayerMove behavior; no upstream code
was copied.

The controller now permits observed FLY-without-overlap only in Exit/Support or
within the tightly bounded shaft exit handoff. That transient observation emits
no completion; target support and mode/contact detachment are still mandatory.
The same mismatch in the middle of a climb still fails with WrongContact.
This fixes mode observation latency, not the still-pending physical exit planner.

Tests cover four face orientations, up/down/floor kick, angle projection,
8/16/100ms gravity/air acceleration, speed limits, preservation of existing
velocity, invalid parameters, and both-direction mode handoffs. Full Windows
x86 Debug43/43 and Linux x86 Debug37/37 passed before the final handoff regression;
that regression separately passed on both platforms. Release6 exports
remain unchanged. No live acceptance or P3-06 completion is claimed.

## Host physics binding

The current-frame observer now requires public `sv_gravity`, `sv_airaccelerate`,
`sv_maxspeed` and `sv_maxvelocity` values, effective entity gravity (zero means1),
entity friction, and the minimum server/player maxspeed. It returns those values
with the same stamped observation and rechecks them around every trace. Missing,
nonfinite, unsupported or changing values discard the whole observation; no
default server cvars are invented. Nonzero punch angles, frozen/on-train state
and observer mode are rejected because they alter the assumed command physics.

Air prediction now applies the public maximum velocity component clamp at the
half-gravity and final-gravity stages, matching the inspected PM_CheckVelocity
call sites. The ReHLDS SV_RunCmd movement input binding was also inspected at
revision `6266cd23faee4a6e9cf3974f9605b2cadd86f0a4`; no code was copied.

Tests cover cvar/actor-physics mutation after every trace, entity gravity scaling,
server maxspeed below player maxspeed, invalid cvars, punch/freeze rejection and
velocity clamping. Windows x86 Debug43/43 passed; the Linux x86 ladder target
passed after rebuilding the modified portable library. Release build/six exports
remain verified. Full exit trajectory planning and dispatch authorization remain
pending; the following command inspection supplies one-frame evidence.

## Actual-command frame inspection

An optional owned BotCommand now adds a bounded prediction to inspectLadderFrame.
The default observation remains four queries. A caller requesting command proof
must explicitly budget up to seven queries (within the existing21 host guard
ceiling); exhaustion never retries or increases that budget. Attached commands
measure actual world point contents one unit below the standing feet for the
ladder's floor kick. Detached airborne commands use the current air physics,
including the FLY-to-WALK handoff. Detached ground WALK is left to its own guard.

The exact command-duration displacement is hull-swept while ignoring only the
actor. A single flat world-floor collision may be followed by a second horizontal
sweep for the remaining frame time, with predicted vertical velocity clipped to
zero. Walls, ceilings, dynamic collision support, slopes and further collisions
are rejected by this profile. A0.05-unit lift disambiguates floor contact; the
reported floor-collision endpoint retains the measured floor height.

A touching actor's measured world floor may lie outside NAV. In that case the
packet has no NAV support; it never substitutes the selected target area. This
allows inspection in the shaft while the controller still requires real target
NAV support and detachment before completion. Prediction is not an observation
of arrival and does not create an exit intent or advance a cursor.

Independent box/point fixtures cover upward motion, detached airborne motion,
the lower outward kick and floor slide, every lower budget, mutation after all
seven queries, invalid contents, wrong support and blocked slide. Windows x86
Debug43/43 passed and Release/six exports passed. Portable sources are unchanged.
The newly added code-review-graph instruction was checked, but its MCP tools
are not callable in this session; review used FocalSpan and the focused Git diff.

## Upper exit candidate and world approval

`planUpperLadderExit` now forecasts release above the model's upper standing-hull
contact boundary and a controlled air approach to the selected target area.
It uses the actual first command duration and forecasts that fixed duration for
at most2 seconds/256 frames. Forward -45-degree ladder input supplies the lift;
air wishes alternate60 degrees while cancelling lateral momentum. This is a
bounded candidate policy, not a claim about arbitrary future frame-time changes.

The forecast is partitioned into boxes whose XY spans do not exceed31 units.
Up to four vertical standing-hull sweeps cover each entire box because the hull
width is32; vertical travel is covered continuously. At most18 columns are
produced, with no expansion/retry. Excess per-frame XY displacement, excessive
rise (96 units above target support), missing target-area landing or exhausted
bounds reject the candidate. This profile handles the tested same-face/across-top
fixtures, not arbitrary irregular ladder tops or elevated destination floors.

The frame observer's upper-exit request generates the candidate internally,
checks its actual first command, every column and the actual flat world landing
floor/NAV area under the existing21-query cap. Only then does it publish a current
`exitIntent`; a predicted landing never becomes current support. The controller
permits the bounded upper flight height and requires approved input while airborne.
Support begins after actual grounding at the endpoint, and Complete still waits
for observed mode/contact detachment and target support.

Both same-face and across-top candidates/world checks pass at8/16/100ms. Tests
exercise every smaller query budget, stale actor/map/link/cvars at every query,
blocked clearance and wrong landing support. Windows x86 Debug43/43 and Linux
x86 Debug37/37 passed; the final host contact-boundary adjustment also passed
the adapter target. Release build/six exports are verified.

Remaining: lower-exit policy, post-landing ground approach, Walk ownership/cursor
and host dispatch wiring, including preserving25Hz decision cadence while
revalidating held intents at each actual engine frame. A reforecast alone is not
proof that a held command equals a newly generated first command. No live
acceptance, P3-06 completion or Finish is claimed.

## Lower exit and jump dismount candidates

The user identified jump as another ladder dismount mechanism. The clean tracked
`regamedll/pm_shared/pm_shared.cpp` at the locally verified ReGameDLL revision
`679973265e1ac99a43193119e0da212ee568f5f9` confirms that `PM_LadderMove` responds
to IN_JUMP by switching to WALK and replacing velocity with270 along the ladder
face normal. `PM_PlayerMove` skips the ordinary PM_Jump launch when a ladder was
found this frame. This is not a vertical ground jump. Behavior was independently
implemented; no upstream code was copied.

`ladderJumpAirStep` models that airborne launch followed by same-frame air
acceleration and gravity. Grounded jump is explicitly unsupported here because
it uses the ground friction/movement branch. `planJumpLadderExit` forecasts a
bounded jump/air landing using the existing18-column,2-second/256-frame ceiling.
Outward progress must remain monotonic so the forecast cannot assume away ladder
re-entry. Host validation checks the actual first command, model detachment at
its predicted endpoint, all world columns, and the target NAV/world landing floor.
An insufficient frame duration to clear model contact is rejected. Actual command
prediction alone never publishes an exit intent or advances a cursor.

The explicit frame exit request now selects upper rise for an Up link, lower
floor kick for a grounded Down link, and jump/air for an airborne Down link.
The floor kick requires measured solid foot-point contents plus an actual flat
floor at the predicted slide endpoint. Both lower policies stay within21 queries
and preserve missing current NAV support inside the shaft. A predicted landing
is never substituted for observed support. Upper jump candidates can be modeled
portably where geometry allows; the current host upper profile still uses rise
because a normal-only jump does not supply height to reach an upper platform.

Tests cover four outward jump normals,8/16/100ms, lower jump and floor-kick
candidates, missing floor-point solidity, predicted contact retention, all smaller
query budgets, stale identity/physics at every query, blocked columns, and wrong
landing support. These are offline model/trace fixtures, not live acceptance.
Windows x86 Debug43/43 and Linux x86 Debug37/37 passed. The x86 Release adapter
build and exact six undecorated exports passed as well.

Remaining integration includes earlier lower exit entry (before bottom contact),
controller jump acceptance with one-shot dispatch feedback, post-landing ground
approach, Walk ownership/cursor advancement, and actual-frame guarded dispatch.
The controller still rejects jump until that dispatch contract is connected.
P3-06 remains open; project-wide Finish and live validation remain unclaimed.

## Walk ownership and one-shot jump dispatch

Lower exit now starts8 units above the selected endpoint (explicit bounded
profile, configurable up to32), allowing airborne jump departure before the
shaft floor is reached. A fresh approved Press is accepted once. The controller
records its command tick and requires the matching actor/map/route/step/link
dispatch result, including the actual later dispatch tick. Duplicate/wrong
results are rejected. While waiting it releases Jump; detached movement without
dispatch evidence fails, and a still-attached actor times out without another
jump. Rejected transport produces a typed terminal failure. Lower airborne exit
also requires a fresh air intent, never an analog-ground fallback.

Walk now optionally owns a Ladder and an exact immutable LadderPlan. Its input
is a value-only current LadderObservation from a trusted host observer. Every
selected external-link field and every bound geometry field must remain equal.
Observation costs plus reserved host queries cannot exceed21. Ladder state,
reason, plan and jump press tick are carried in WalkDecision. The explicit
reportLadderDispatch seam forwards only to the active owned controller.

Observed Complete updates the primitive and advances exactly the active cursor
once, using verified target support. It does not declare the entire route
Arrived; ordinary final-area movement remains separate. Abort releases Jump and
directional buttons. Invalid initial observations are rejected before primitive
entry. Scripted trusted observations test both directions, jump dispatch,
support-before-advance, stale ticks, changed link identity/cost, budgets, missing
observations and disabled profiles. They are ownership/lifecycle tests and do
not claim physical simulation or live acceptance.

The host still has to enable this profile and supply observations, preserve
decision cadence while guarding the actual queued command, and forward transport
feedback. Integration must also verify published ladder endpoints satisfy the
Corridor hull-inset constraint: fixture endpoints only13 units inside a NAV edge
cannot form a standing-hull corridor. Do not loosen that constraint to pass.
Measured post-landing approach and host E2E evidence remain outstanding.
Windows x86 Debug43/43 and Linux x86 Debug37/37 passed, followed by targeted
ladder tests on both platforms for the final primitive-entry ordering change.
Release x86 build and the exact six exports passed.
