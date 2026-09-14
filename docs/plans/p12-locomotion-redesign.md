# P12 local locomotion redesign

Approved in the implementation task on 2026-09-13 JST. Baseline: `38ca8de`.
This supplements P12-03; it does not create another phase or declare Finish.

## Binding constraints

- Preserve NAV IO/graph, player lifecycle, Motor and the six DLL exports.
- Independently implement terrain sampling, continuous path following and local
  locomotion. ReGameDLL_CS is a behavioral reference, not a source to copy.
- All work stays in `codex/p12-locomotion-redesign` and its dedicated worktree.
- User explicitly authorizes P12-only live-before-offline validation. Until an
  explicit live PASS: no test target configuration/build/execution, CTest,
  canonical verification or commits. Tests may be written, not run.
- Release x86 adapter builds use VS18 NMake, tests OFF, pinned Metamod SDK.
- Do not disturb another session's changes or running server without checking
  its ownership. Final acceptance requires the actual final Release artifact.

## Design

`MovementSnapshot -> PathFollower -> TerrainSampler -> LocomotionController ->
MovementIntent -> Motor -> MovementFeedback`.

- PathFollower advances from actual supported position and follows consecutive
  ordinary portals without a neutral frame. Special traversals stop lookahead.
- TerrainSampler separates floor candidates from hull clearance/support;
  checks direct and step-up travel and permits physically supported small
  descending transitions. Unknown/budget exhaustion is not blocked geometry.
- LocomotionController exclusively owns ground, jump/drop and recovery state;
  existing ladder transport remains specialized. Airborne state never falls
  through into ground walking. Jump Press requires dispatch acknowledgment.
- MotionEnvelope replaces the 0.5-unit centerline rejection with bounded
  corridor/physical evidence, generation/posture identity and finite lifetime.
- Generation, actor lifecycle and stale observations still fail closed.
- New/legacy selection is temporary for comparison, with no automatic fallback.
  Remove legacy only after new-controller live PASS and reaccept final artifact.
- Execution exclusions apply to evidenced directed edges, not whole source
  areas. Forward admissible heuristics through policy wrappers; preserve h=0
  for unknown custom cost contracts.

## Work sequence

1. Freeze baseline and record assets, physics and six fixed de_dust2 scenarios.
2. Implement/test-source terrain sampler and unified physical observations.
3. Implement/test-source path follower and bounded continuous progress.
4. Implement controller, motion envelope, jump/drop and adapter feedback.
5. Fix execution policy and bounded recovery/replanning.
6. Release-build, review, then live comparison; do not substitute offline tests.
7. After explicit live PASS, remove old controller and repeat final live checks.
8. Run focused tests, canonical All once, update FocalSpan and documentation,
   stage only intended paths, diff-check and commit after acceptance.

## Acceptance

Use de_dust2 fixed origin, goal, yaw and physical settings. Run each of six
scenarios ten times with one BOT and ten times with two BOTs, recording all
failures as well as successes. No teleport/manual push counts as success.

| Scenario | Requirement |
| --- | --- |
| Wall/corner | Reaches goal with autonomous correction |
| Descending slope | Continues over small support transitions and regrounds |
| Stairs up/down | Traverses without query exhaustion becoming a blocked edge |
| Obstacle jump | Clears reachable obstacle; rejects ceiling/unreachable cases |
| Oscillation | Advances or replans within five seconds; repeated excursions are not progress |
| Next route | Starts requested search within 250ms; sibling exits remain eligible |

Record BSP/NAV/DLL/SDK identities, command and dispatch tick, actual movement,
support classification, query counts, route search time and intentional wait
separately. Two-BOT cases also require successful yielding and recovery.
Explicit user PASS is required before offline tests and commits.
