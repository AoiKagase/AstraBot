# State

Status: in_progress
Active goal: P3-06 (user /goal); do not mark complete for a slice.
Branch: codex/p306-ladder-host-binding. No push authorized.
Main was8aa1610 in .worktrees/main-integration; reverify before merging.
Reports: docs/reports/p3-06-ladder-publication.md and p3-06-ladder-controller.md.
Discovery and immutable fingerprint/generation-bound publication implemented.
Ladder lifecycle/physics, binding, fresh observer and upper rise candidate exist.
Production Walk still rejects Ladder. No live acceptance or Finish claim.

Current slice: lower grounded floor kick, airborne jump physics/candidate and
host validation. User explicitly noted ladder dismount can use jump. Verified
clean tracked ReGameDLL_CS pm_shared.cpp at679973265e1ac99a43193119e0da212ee568f5f9:
PM_LadderMove changes to WALK and replaces velocity with270 outward; airborne
WALK then applies gravity/air movement, skipping regular PM_Jump with pLadder.
Do not imply floor kick is the only dismount mechanism. No upstream code copied.
ladderJumpAirStep is airborne-only; grounded jump needs ground friction/movement.
planJumpLadderExit uses18 columns/2 seconds/256 frames, monotonic outward flight.
inspectLadderFrame exitMsec selects upper rise, lower floor kick if grounded,
lower jump/air if airborne. Checks world geometry/landing and first-frame model
release before exitIntent. Predicted landing never becomes observed NAV support.
Windows x86 Debug43/43 PASS, Linux -m32 Debug37/37 PASS, Release/6 exports PASS.
WSL requires escalation; existing build-linux-x86-wsl is at same /mnt/h repo path.

Next: earlier lower exit entry before grounding; controller one-shot jump
acceptance/dispatch feedback; post-landing grounded approach; Walk ownership,
cursor advance; host wiring and actual-frame revalidation. Controller still
rejects jump until the one-shot dispatch contract is integrated. Current down
exit enters only at end height and limits rise to4; jump fixture starts8 above.
Grounded exit outside NAV needs measured support/approach; current observer only
permits absent NAV support while touching. Do not fabricate area identity.
25Hz decisions preserve held intents, but guards check actual engine-frame input.
Reforecast first command need not equal pending held command. Verify exact input.
Support plus real detachment is required before completion. Upper normal jump
does not supply height to reach a platform; host still uses rise.
Unsupported geometry explicit; no Walk fallback or serialized ladders.
P3-07/P3-08 belong to broader Phase3, outside current P3-06 goal.

No subagents. All binaries x86. No live server or project-wide Finish.
Preserve AGENTS.md/.gitignore edits and untracked tool configs; exclude from commit.
FocalSpan mandatory before/after changes. code-review-graph now callable but
graph was stale at5b21fd9; focused source/diff and actual tests supply evidence.
