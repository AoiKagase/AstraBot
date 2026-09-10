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
