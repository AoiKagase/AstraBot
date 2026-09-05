# P3-03 grounded stair traversal

This boundary extends the Walk host integration from `73bc116`. It implements
ordinary grounded stair probing and offline arrival; the existing combined
door/stairs/wall/narrow checklist remains open for its other behavior.

## Contract

GroundedArea and Floor queries now use the observed standing/duck hull rather
than a center-point line. A hull can be supported by a tread while its center
is still over the lower area. Floor request heights are feet coordinates and
are converted to hull origins for TraceHull; the result is converted back to
floor height. Unsupported hulls remain unavailable. Each value query issues
one engine trace, excluding player bodies for floor support and including
actors for clearance. Query stamps, floor-normal checks and the four-unit
measured-foot support tolerance remain required.

QueryRequest carries an explicit NAV height tolerance, defaulting to two units.
RouteOptions forwards its validated tolerance. The console's ordinary stair
profile selects 18 units for NAV/floor matching so a supported footprint can
straddle one riser even while its XY center belongs to the lower NAV area.
This is containing geometry, never nearest-area fallback. Loader validation,
file format and real-input compatibility limits are unchanged. The allowance
is a bounded offline policy, not live-calibrated proof for every NAV staircase.

After a well-formed blocked direct hull sweep across a measured floor-height
change, GroundProbe can check a stair path: up by the explicit step limit,
across, then down to the measured support (or across/down for descent).
Every segment must be clear and end at the requested point. Ceiling, blocked
across/down, stale replies, unknown/start-solid results and exhausted budgets
stop the probe. Flat obstructions are not treated as stairs. No jump/use action
or velocity-Z command is introduced; GoldSrc still owns step motion.

The console profile allows at most 21 queries and four samples per decision:
one ground query plus at most five queries per sample (floor, direct hull,
and three stair hulls). An insufficient remaining budget fails before issuing
the alternative path; no automatic limit increase or retry occurs. Step-up and
drop remain capped at 18 units, horizon at 48, sampling at 16. Successfully
checked stair alternatives are counted in ProbeResult/WalkDecision and
`step_probes` in motion output.

The existing cached-segment/freshness guards remain strict. A discontinuous
observed stair height may neutralize a cached command until the next scheduled
25 Hz decision revalidates the current support. This does not add per-frame
world queries, relax the 120 ms age bound or enable a catch-up burst.

## Evidence and limits

Portable tests verify ordered up/across/down and across/down coordinates,
16/18-unit ascent, 19-unit rejection, ceiling/across/down collision, stale
alternative results, exact remaining-query budgets and flat-obstacle rejection.
Route-session tests cover default/explicit NAV tolerance propagation and
negative/nonfinite rejection before querying.

Fake-engine tests run the full goto/Walk/Motor/RunPlayerMove path across a
16-unit tread in both directions at 8/16/100 ms frames. Independent scripted
footprint support lifts/lowers the actor as its hull crosses the tread. Arrival
requires measured support; a ceiling or 19-unit riser stops before entering it.
Existing flat movement, cancellation, generation and dispatch tests remain.

Windows x86 NMake Debug, Linux GCC `-m32` Debug and Windows x86 Release are the
verification configurations; all retain warnings-as-errors. Windows passed
30/30 tests and Linux portable passed 25/25; the Release DLL retained the six
approved undecorated exports. No server, real-device check or Finish
decision was run. Continuous support between discrete samples, detailed engine
step physics and real-map staircase coverage still require post-Finish evidence.
Door use/wait, wall avoidance, narrow-passage correction and later P3 work remain.
