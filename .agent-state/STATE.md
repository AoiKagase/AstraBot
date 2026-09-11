# State

Status: P12 movement encounter blocker implemented; live acceptance pending.
Integration target: local main (user requested merge and task worktree cleanup).

P12 implementation 7130889 integrates with main 967d75c. Preserve main's
Roam arrival/session cancellation, route style, runtime diagnostics and economy
observation fixes alongside ground/step dispatch, Jump posture/capabilities,
narrow NAV physical validation and transient edge cooldown.

The current movement fix preserves a healthy autonomous Roam route across
periodic goal alternatives, keys Recovery progress to the active directed edge,
counts fresh Jump/Ladder guard rejection toward bounded no-progress recovery,
and emits granular Jump guard diagnostics at `astrabot_debug 2`.

Evidence: docs/reports/p12-ab-compatibility-fix.md.
Reference: ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5.
Scope: A/B only; C links/damage shortcuts and project Finish remain excluded.

Verification boundary:
- Regression sources exist; no test configure/build/run, CTest or canonical
  until explicit user-confirmed live PASS.
- Integration verification uses x86 Release, tests OFF, pinned SDK, six exports.
- The movement-fix x86 Release tests-OFF build and six-export check pass. Test
  targets, CTest and canonical remain intentionally unbuilt/unrun until live PASS.
- The previously deployed DLL is historical evidence; the merged artifact is
  recorded separately and is not automatically redeployed.
- FocalSpan and diff checks accompany integration; local config is not committed.

Next: deploy the movement-fix DLL, obtain server restart authorization, and
capture at least 30 seconds with two BOTs at `astrabot_debug 2`. Confirm that
11->2036 is traversed or enters bounded recovery/detour/Hold instead of remaining
Running indefinitely, then run 2v2 for at least 10 minutes/three rounds.
After explicit live PASS run focused regressions and canonical All once.
No live acceptance or project Finish has been declared.
