# P3-06 portable ladder controller

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
