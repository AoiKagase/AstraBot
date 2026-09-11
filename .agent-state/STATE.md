# State

Status: P12 A/B implemented; live acceptance pending.
Integration target: local main (user requested merge and task worktree cleanup).

P12 implementation 7130889 integrates with main 967d75c. Preserve main's
Roam arrival/session cancellation, route style, runtime diagnostics and economy
observation fixes alongside ground/step dispatch, Jump posture/capabilities,
narrow NAV physical validation and transient edge cooldown.

Evidence: docs/reports/p12-ab-compatibility-fix.md.
Reference: ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5.
Scope: A/B only; C links/damage shortcuts and project Finish remain excluded.

Verification boundary:
- Regression sources exist; no test configure/build/run, CTest or canonical
  until explicit user-confirmed live PASS.
- Integration verification uses x86 Release, tests OFF, pinned SDK, six exports.
- The previously deployed DLL is historical evidence; the merged artifact is
  recorded separately and is not automatically redeployed.
- FocalSpan and diff checks accompany integration; local config is not committed.

Next: obtain server operation authorization, compare identical Zbot/AstraBot
NAV/start/goal/physics including the three de_dust edges, five passes per basic
case, then 2v2 and configured BOT counts for at least 10 minutes/three rounds.
After explicit live PASS run focused regressions and canonical All once.
No live acceptance or project Finish has been declared.
