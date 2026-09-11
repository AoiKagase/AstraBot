# State Status: live acceptance pending — P12 A/B compatibility correction

Branch: codex/p12-ab-compat-fix
Workspace: H:/sourcecode/003.Game/amxmodx/AstraBot/.worktrees/p12-ab-compat-fix
Base: 8807783 plus inherited movement/cooldown changes. Original worktree preserved.
Scope: approved A/B; no C additional links or damage shortcuts.
Reference: ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5.
Plan/evidence: docs/reports/p12-ab-compatibility-fix.md

Implemented and tests-OFF Release built:
- Ground step-up always lifts; StepEvidence retained.
- ground_frame.hpp revalidates physical per-frame ground/steps at dispatch.
- Narrow NAV center membership for goals/crossings/recovery.
- PRECISE final segment lateral suppression; regression test sources.

Additional implementation:
- Shared Jump capabilities and observed source/flight/landing hull transitions.
- Measured forward obstruction Jump fallback reuses the running primitive.
- Regression sources updated; static review findings resolved.

Verification:
- Dedicated x86 NMake Release configured, tests OFF, W4/WX, pinned SDK.
- Final x86 Release build and six exports passed. DLL deployed and backup
  hash verified; full artifact identities are in the report above.
- FocalSpan updated/queried before edits; final refresh precedes commit.
- Independent ground/narrow review's PRECISE finding fixed.
- No test configure/build/run, CTest or canonical performed.

Gates:
- User approved pre-Finish live A/B comparison for this task only.
- Next: obtain server operation permission, then identical Zbot/AstraBot live
  cases (5 passes each), multi-BOT and lifecycle/combat acceptance.
- Explicit live PASS required before tests/canonical.
- Live comparative/multi-BOT/lifecycle/combat acceptance pending.
- Commit only intended source/tests/report/state; preserve local config/temp files.
