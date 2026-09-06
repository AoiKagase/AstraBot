# State

Status: in_progress
Goal: entire Phase 3; never mark complete for a slice or merge.
Main: 8aa1610, fast-forward integrated in .worktrees/main-integration, not pushed.
Current branch: codex/p306-ladder-host-binding.
Current boundary: bounded current-frame Ladder inspection after selected-link binding.
Reports: docs/reports/p3-06-ladder-publication.md and p3-06-ladder-controller.md.
Discovery/publication through 8984b15 preserves immutable fingerprint-bound links.
Portable controller adds up/down states, directional buttons, finite reacquire,
fresh inspection binding, and target support plus actual detachment before Complete.
Verification: frame inspection Windows x86 Debug43/43 PASS and Release6 exports PASS.
Portable controller at main: WSL GCC -m32 Debug37/37 PASS including final Support
change. Subsequent binding changes are adapter-only; Linux portable sources unchanged.
Tests model standard-CS vertical projection only; scripted detachment is not a
physical dismount simulation. inspectLadderFrame now produces bounded observation
under a required current actor/route/tick callback; host wiring/exitIntent pending.
P3-06 first checkbox complete; second remains open. Production Walk rejects Ladder.
Host binding checks exact link fields, pair endpoints/direction, map/fingerprint/
generation and current entity/model/bounds. No traces or movement authorization.
Frame observer caps4 traces: model overlap/face, path, optional world support/NAV.
Each query guards identity and exact snapshot physics; unsupported water/basevelocity.
Next: wire current inspection callback and solve verified dismount control,
then guarded host dispatch and cursor advance. Do not fake attached analog exit.
MOVETYPE_FLY can outlast contact until the next PM step. Current controller rejects
that pair; integration must explicitly handle physical detachment before dispatch.
Top exit requires clearing hull-expanded model height; position tolerance alone
can stop inside the ladder. Account for real CS air acceleration/landing geometry.
After P3-06: P3-07 finite recovery and P3-08 cross-primitive offline matrix.
No subagents. All binaries x86. No live server or project-wide Finish.
Real NAV compatibility partial; real NAV/BSP read-only. No push authorized.
Preserve untracked .focalspan.json and .serena/; do not stage them.
