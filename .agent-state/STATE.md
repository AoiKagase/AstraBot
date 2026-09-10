# State

Status: verifying — source implementation complete, live acceptance pending
Milestone: P12
Task: All-BOT autonomous movement, Jump/Drop, combat and round recovery

Goal:
Every managed BOT independently roams, perceives and fights enemy BOTs, recovers from route failures, and resumes after the next round spawn. Actual movement, damage and death remain live acceptance requirements.

Relevant:
- docs/plans/p12-autonomous-combat.md
- src/nav/runtime/execution.hpp
- src/adapter/cstrike/nav/console.cpp
- src/adapter/cstrike/nav/motion.cpp
- src/nav/local/walk.hpp
- src/adapter/metamod/runtime_orchestrator.cpp

Done:
- User approved the full autonomous-combat plan and implementation delegation on 2026-09-10.
- Starting branch codex/p12-console-debug, HEAD c16d431; unrelated .gitignore and local/untracked inputs preserved.
- Pre-edit FocalSpan status ready/fresh and architecture query succeeded. Graph metadata was stale; source inspection followed graph discovery.
- Integrated separate search/motion execution state, directed-edge exclusions, 250ms search backoff and 2s failed-goal cooldown.
- Integrated boundary-compatible Walk, attributed micro Jump, guarded Drop (128-unit fall/32-unit gap), actor retirement/combat diagnostics, and current-map NAV autoload once per ServerActivate generation.

Next:
- With applicable deployment/server authority, validate 1v1, 2v2 and operational BOT count for at least ten minutes/three rounds, including changelevel and next-round recovery.
- After explicit user-confirmed live PASS, compile/run focused regression tests and the canonical gate according to the existing verification policy.

Blocked:
- CTest configurations/builds, CTest and canonical verification remain prohibited until explicit user-confirmed live PASS.
- No HLDS/ReHLDS operations or DLL deployment are performed as part of source editing.

Verified:
- Tests-OFF x86 Release DLL target built successfully; six required exports verified. Artifact SHA-256 FB2FDAF210DD2634231FFE51DAABD839C4FAC631B8323936D741E801AD0EDB13. Not deployed.
- Initial FocalSpan and git diff --check passed; tests remain unbuilt/unexecuted. P12-wide acceptance and Finish are not declared.
