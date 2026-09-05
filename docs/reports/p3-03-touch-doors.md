# P3-03 touch-door contact boundary

Walk now handles ordinary untargeted touch doors through supported approach,
one bounded contact attempt and finite waiting. This adds offline touch-door
evidence to the existing Use-door and stair evidence. The combined P3-03 checklist
remains open for wall avoidance and narrow-passage control.

## Control and limits

The adapter marks an observed door touch-capable only for an ordinary door class,
BSP solid, empty targetname, no use-only flag and no water flag. Private master
state is not read or assumed unlocked. A typed Door observation includes the
physical hull collision endpoint/normal. No engine Touch/Use function is called
directly and no private data pointer is accessed.

The passive DoorWait clock starts on first touch-door observation. The host
selects a 3-second timeout covering approach, contact and opening; the existing
Use-only timeout stays at 1 second. Failure never resets this clock or retries a
contact. Repeated immediate obstruction by the same last door is still rejected.

Approach targets stop 0.0625 units before the measured collision endpoint. Every
approach segment passes the normal ground/clearance inspection. Remaining query
and sample budgets limit additional inspection work. Ground is cached only within
that synchronous decision; all actual engine queries have unique consecutive
ordinals even when several inspections reuse their local ordinal 1.

Once the measured collision is within 0.125 units, Walk requests one frame of
contact motion. The requested speed corresponds to 0.5 units in the observed
frame, capped by Motor/actor speed limits. This is a distinct contact command,
not a clear segment. Cached frames are neutral. Before dispatch, the host traces
again and requires the same door generation/capability, a nearby frontal collision,
matching horizontal direction and actual measured travel sufficient to touch but
no greater than 0.75 units. Unsupported slopes/glancing contact, changed targets,
expired commands and too-short pulses fail closed. The engine remains responsible
for collision and Touch delivery; Nav never changes position itself.

The dispatch guard consumes at most one trace. It reserves ordinal 1 and is
included in the existing 21-query frame/decision total; a same-frame Walk decision
starts actual queries at ordinal 2 and receives the reduced budget. The 4-sample
and stair limits remain unchanged. Contact guard counts and pulse context are
retained in MotionTrace/status. Clear observations remain neutral and require a
later full ground/segment inspection before normal movement or arrival.

## Verification

- Windows x86 NMake Debug, adapter + portable CTest: 31/31 PASS.
- WSL Debian GCC `-m32` Debug portable CTest: 26/26 PASS.
- Windows x86 Release DLL: PASS, exactly the six required exports.
- All builds use warnings-as-errors.

Fake-engine tests at 8/16/100 ms verify supported approach, one physical contact,
delayed opening and goal arrival; locked timeout; entity-generation replacement;
targetname changes between queue/dispatch; cancel; unsupported targeted doors;
an expired pulse; a frame too short to reach contact; and lost grounded state.
They compare actual hull calls with total query accounting, including the guard,
and verify there is no Use request or repeated contact pulse. The collision model
keeps the hull outside the brush with an explicit collision epsilon; stationary
commands do not invent new collision impacts.

Portable tests verify passive-wait timeout, supported approach/contact/arrival,
consecutive wire ordinals after a reserved guard, same-decision ground reuse and
exhausted budgets. Existing Walk, Use-door, stairs, route and loader tests pass.

## Source basis and acceptance

The independent implementation refers to ReGameDLL_CS commit
`679973265e1ac99a43193119e0da212ee568f5f9`, `regamedll/dlls/doors.cpp`:
ordinary non-use-only doors register DoorTouch, and a nonempty targetname prevents
touch activation. Master-dependent activation may fail. The referenced file was
unchanged locally; no upstream implementation was copied.

These are synthetic offline observations, not live GoldSrc physics acceptance.
No server was started; project-wide Finish and live door/stair/movement checks
remain pending. Real NAV compatibility remains partial. Next: wall avoidance and
narrow-passage speed/lateral correction, then the remaining Phase 3 plans.
