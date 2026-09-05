# P3-05 Jump transition geometry and grounded inspection

Built from `9d39854` on `codex/p305-jump-transition`. The Simple Jump checklist
remains open until Walk/host dispatch integration is implemented and verified.

`JumpGeometry::derive` selects exactly `binding.step` from an immutable validated
corridor. A native directed portal with supported Jump hints is required; other
steps, external links, NoJump/duck-jump/unknown hints and down-jump geometry cannot
silently become a candidate. The source and target area IDs and attributes are
retained. All four native directions use the same constrained construction.

Takeoff and landing centers are biased around the selected portal by an explicit
preferred distance and the current actor's tangent position. Both regions shrink
their area's XY extent by the actual hull, the corresponding allowed radius and
an explicit clearance margin. Tangent coordinates must fit both regions. The
rounded candidates are checked again against these bounds, then NAV floor height
is interpolated independently on each side and converted to standing-hull origin.
The result is only geometry: it neither proves support nor selects a jump button.
No world search, graph rewrite or allocation is introduced by this derivation.

`JumpProbe::prepare` uses GroundProbe for current support and a bounded supported
approach/acceleration segment. Every sampled floor must remain in the source area.
Approach is capped by the ground profile; acceleration must cover the full command
lifetime of 120ms while staying inside the takeoff circle. A clipped acceleration
probe cannot authorize a longer command. The result supplies ground clearance
only, so SimpleJump still requires a separate velocity-bound launch inspection
before Press. The combined batch remains within 21 actual queries, without retries.

GroundProbe's helper ordinals are translated into unique enclosing ordinals.
Response stamps are checked before reversing this translation; an old response
whose ordinal happens to equal the helper ordinal remains stale. `JumpProbe::land`
produces current target-area support for landing and cooldown, with explicit
landing radius/height checks. Missing support never becomes arrival.

## Offline verification

- Windows x86 NMake Debug, warnings-as-errors: 39/39 tests passed.
- WSL Debian GCC `-m32` Debug, warnings-as-errors: 34/34 portable tests passed.
- Windows x86 Release DLL build and exact six exports passed.
- Four directions, selected nonzero step, external-link rejection, 32-unit rise
  boundary, narrow regions, actor/step mismatch and tangent clamping are covered.
- Preparation tests cover support loss, blocked movement, exhausted budget,
  insufficient acceleration coverage, source-area escape and stale ordinals.
- Geometry -> ground preparation -> SimpleJump -> launch inspection -> observed
  landing/cooldown completes in a synthetic Motor/ballistic fixture in all four
  directions at 8/16/100ms. Every decision has at most21 actual queries and the
  lifecycle emits exactly one Press. These tests supply the synthetic flight
  model explicitly; they do not establish live GoldSrc physics.
- Native `queryNavWorld` tests confirm preparation uses four TraceHull calls and
  landing one, with unknown support rejected. The expanded fake-client test was
  rebuilt and rerun after the full Windows suite.

Next work is the Walk/host connection: establish the host's physics model, own the
Jump lifecycle per selected step, reserve query budgets at dispatch, revalidate
queued commands and feed actual dispatch/airborne/landing observations back into
the controller. Route advancement must wait for supported landing and cooldown.
The production host still rejects Jump hints. No live acceptance, project-wide
Finish, or full real-NAV compatibility is claimed.
