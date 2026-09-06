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
