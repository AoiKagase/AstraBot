# State

Status: in progress — P12 Idle heartbeat/Roam implementation integrated on `main`; live acceptance remains pending

Milestone: P12

Task: Integrate the committed P12 Idle/Roam, runtime identity, diagnostics, and MEDIUM/LOW contract changes while preserving the pre-live verification boundary.

Current checkpoint (2026-09-10, Asia/Tokyo):

- Working branch: `main`; feature source: `codex/p12-console-debug` through `7a045d4`; integration commit: `55311ab`.
- HIGH-01/HIGH-03, additional A/B/C, Idle heartbeat, autonomous Roam, actor-scoped diagnostics, and MEDIUM/LOW contract changes are integrated on `main`.
- Roam recent/rejected goals are cleared when the NAV/map session is invalidated; same-actor route replanning retains the active map-session history.
- The unrelated `AGENTS.md` wait rule and two root-document moves were reverted. The CSSDK dependency remains with `third_party/CSSDK-PROVENANCE.md`.
- The feature-side audit document version with implementation follow-up was selected for `docs/p12-live-source-audit.md`.
- Post-merge static review retained explicit-route arrival cancellation, same-goal route-state synchronization, and map/session Roam-history invalidation; the feature-side simplification that removed those guards was not adopted.

Verification boundary:

- Static review, graph review, FocalSpan status, and diff checks only. No tests-ON configure/build, CTest, canonical All, HLDS deployment, or live run was performed after this integration review.
- Existing Release adapter evidence from the prior implementation remains historical and is not treated as new live acceptance.
- Existing untracked user materials remain un-staged.

Open acceptance: explicit real-device/Finish acceptance with recorded date, SHA, DLL, environment, followed by the prescribed Windows x86 Debug focused tests and one canonical All run.
- git diff --check has passed after the latest source repairs.
- FocalSpan update completed after the latest edits; post-edit status is ready=true, stale=false, index_fresh=true, 414 files, 5193 symbols. Static checks only; no configure/build/CTest was run.
- Implementation commit 1fc65fb is present. On 2026-09-09, the Windows x86 Release Metamod adapter was rebuilt with tests OFF, the six required exports were verified, and the resulting DLL was deployed to D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll with matching SHA-256 e0c5bf61e7a283eece12cb7a66e34c7ea14aae577b63af52be2232490f60ce9b. Debug/CTest, canonical All, merge, and live HLDS/ReHLDS acceptance remain unperformed for this worktree state.
- Movement rejection diagnostics are bounded by actor/map/agent/source/error state; repeated identical refusals are suppressed while new generations remain visible.
- An offline/static result must not be reported as実機PASS or Finish.

Next:
1. Reinspect final diff and static symbol/switch coverage; run git diff --check again.
2. Wait for explicit user-confirmed real-device PASS with date, SHA, DLL, and environment before tests ON build, CTest, or canonical All.
3. After PASS, follow the documented Windows x86 Debug focused tests, one canonical All, STATE evidence, and narrow commit workflow.

Open acceptance:
- Live runtime behavior, death animation, real-map NAV compatibility, runtime weapon callbacks, model/render result, and Finish status remain unverified.

P12 runtime-input checkpoint (2026-09-10, Asia/Tokyo):
- Added actor-level RuntimeInputValidationReason classification and propagated it to RuntimeDecision and RuntimeDiagnostic.
- Preserved prior-frame actor/tick correlation through Movement dispatch and added runtime_validation to the console trace.
- Added a focused ActionStampMismatch propagation test; existing multi-actor isolation coverage remains in place.
- FocalSpan was updated successfully; git diff --check passed. Build, CTest, canonical verification, Finish, and live acceptance remain intentionally unperformed until the user confirms the real-device gate.
