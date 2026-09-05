# P3-05 crouch route following

This boundary integrates the posture foundation from `2bf3ba3`. The first
P3-05 implementation checklist has offline evidence; Simple Jump remains open.

Walk interprets selected source/target constraints before translation. Crouch
uses a route-owned posture gate with explicit standing/crouched hulls and a
one-second transition timeout. It confirms actual duck and hull observations,
then retains existing ground, sampled support, swept hull, corridor restriction
and measured portal advancement. NoJump allows ordinary walking while conflicts,
unknown/Precise hints and still-unimplemented Jump/external transitions fail
with typed diagnostics. The immutable graph and serialized hints are unchanged.

The posture query shares the existing 21-query/4-sample decision limit and
ordinal sequence. Active source/target crouch requirements remain held until
the actor fully crosses the portal. A clear subsequent area must pass standing
clearance and observed standing before movement resumes. A crouch-required
goal finishes with verified goal support while retaining duck.

The host supplies 32x32x72 standing and 32x32x36 crouched hull profiles. Queued
commands still require the same observed hull at dispatch. Release has a fresh
dispatch-time headroom query, included in the next decision's reserved queries;
closing headroom produces PostureChanged rejection. Cached release cannot
repeatedly force standing. Neutral/cancel commands preserve duck at zero movement
while the actor is crouched; lifecycle removal still suppresses invalid actors.
New goals can perform the safe release sequence. Diagnostics include posture,
posture failure, constraint failure and duck intent.

Portable tests drive a crouch transition and release, same-area crouched arrival,
and ordinary NoJump walking. They count real query calls and ordinals against
the limit. Adapter tests at 8/16/100 ms integrate actual Motor commands, change
the synthetic entity hull/flag/origin, pass a low-ceiling area, and verify standing
arrival. Further cases cancel under the ceiling, close headroom between queue
and dispatch, reopen it, or retain it until the finite timeout. Cancellation
never translates or forces standing. Existing regressions remain active.

Windows x86 NMake Debug adapter/portable: 36/36 PASS. WSL Debian GCC -m32 Debug
portable: 31/31 PASS. Windows Release x86 DLL: six contracted exports PASS.
Warnings are errors. Final diagnostic formatting is rebuilt and fake-client
tests rerun. FocalSpan updated and diff reviewed before commit. No push or live
server was performed; hosted CI and post-Finish live crouch remain unverified.

P3-05 Simple Jump, P3-06 ladders, P3-07 finite recovery and P3-08 evidence matrix
remain. This is not Phase 3 completion or project-wide Finish.
