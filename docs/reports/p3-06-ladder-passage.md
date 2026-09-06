# P3-06 measured ladder passage inspection

`inspectLadderPassage` consumes one discovered candidate, an explicit cardinal
face/exit variant and the same-map spatial index. It returns a value packet with
the selected entity identity, face, model contacts, supported bottom/top NAV
areas and cleared mount/climb/dismount origins. No graph or movement is published
yet; this SDK-backed seam is exercised by the ladder scanner test target.

## Evidence and limits

Each call makes at most 12 actual traces, without retries or allocation:
three point-model face samples, two floor/standing-clearance pairs, two standing
model intersection tests, and three mount/shaft/exit world-hull sweeps.
Tighter budgets terminate before the next engine trace and return no passage.
All traces include current map/entity checks before and after their callback.
Model-index/name, classname, skin, serial, bounds and stationary geometry are
revalidated. A failed check discards all earlier proof.

This initial profile supports stationary unrotated ladder brush geometry at
least 72 units tall, standard standing hull, world-BSP floor support, and a
cardinal plane confirmed at low/middle/high heights. Plane samples must agree;
normal and endpoint consistency, floor normal and NAV height are checked.
Both same-face and across-top exits are explicit variants. Sampling does not
establish arbitrary curved/oblique brush support; unknown geometry fails.
Dynamic floor entities, custom player hulls and moving ladders need additional
contracts and are rejected. Later movement must revalidate this temporal evidence
and observe actual ladder contact; discovery does not prove a future climb.

Independent reference inspection (no implementation copied):

- ReHLDS `6266cd23faee4a6e9cf3974f9605b2cadd86f0a4`,
  `engine/pr_cmds.cpp`: gHullMins/Maxs and TraceModel, world-space result fields,
  point hull0/standing hull1 and temporary engine-managed brush trace behavior.
  `engine/world.cpp`: hit-entity assignment and endpoint calculation.
  `common/qlimits.h`: 512 model slots, checked before invoking TraceModel.
- ReGameDLL `b0889847fe6d03898be88acc9e366660efb40ab5`,
  `pm_shared/pm_shared.cpp`: PM_Ladder requires brush/CONTENTS_LADDER contact;
  PM_LadderMove derives its normal from a model trace.

## Verification

Windows x86 NMake Debug, warnings as errors: full 41/41 tests passed; final
model-index guard addition rebuilt and reran the ladder target successfully.
An independent slab-intersection fixture expands boxes for the real standing
hull. It checks four faces and both exit variants; all budgets below 12;
map/serial mutation at every query boundary; changed bounds/model, malformed
traces, blocked paths, absent support/contact/NAV area and missing APIs.
No production DLL or portable source changed in this slice, so their preceding
Linux portable and DLL-export evidence is unchanged, not newly executed here.

Remaining P3-06 work: BSP fingerprint acquisition/binding, bounded whole-map
inspection and immutable generation-bound link publication, then first-class
up/down motion, bounded reacquisition and host dispatch integration. Both plan
checkboxes remain open. Live validation stays post-Finish; Phase 3 is active.
