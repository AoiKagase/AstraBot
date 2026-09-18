---
status: partial
phase: 08-differential-live-parity-acceptance
source: [08-01-SUMMARY.md, 08-02-SUMMARY.md, 08-03-SUMMARY.md, 08-04-SUMMARY.md]
started: 2026-09-18T13:35:00+09:00
updated: 2026-09-18T18:24:00+09:00
---

## Current Test

[testing paused — 3 items outstanding]

## Tests

### 1. Windows plugin and team/class entry
expected: AstraBot loads beside unmodified ReGameDLL-CS and managed Bots enter their requested CT/TERRORIST teams and classes.
result: pass
source: live-log

### 2. Human damage and Bot death
expected: A human player can damage a managed Bot and the server records the corresponding damage and death event.
result: pass
source: live-log

### 3. Autonomous post-join action
expected: After team/class entry and spawn readiness, managed Bots independently produce movement/action and remain active beyond the server idle-kick interval.
result: issue
reported: "Idle状態でKickされた事からもわかるように、現状チーム参加後に自立行動はしておりません。"
severity: major

### 4. Windows restart recovery
expected: After sv_restart, managed Bots become active spawned entities and a subsequent round starts.
result: pass
source: live-log

### 5. Nav locomotion progress
expected: RunPlayerMove and Nav intent generation result in sustained origin progress on the loaded de_dust2 Nav.
result: issue
reported: "Movement diagnostics show dispatched input but no sustained route progress and later Game_idle_kick removal."
severity: major

### 6. Linux x86 live parity
expected: The same lifecycle and autonomous-action checks complete on Debian Linux x86.
result: blocked
blocked_by: server
reason: No Debian Linux x86 live run is available.

### 7. Pinned differential reference trace
expected: A captured, pinned CSBot/AstraBot reference trace is compared for the live scenarios.
result: blocked
blocked_by: third-party
reason: Only the synthetic Phase 8 replay contract exists.

### 8. Bot-to-Bot combat log
expected: The fresh line-offset-scoped server log contains a damage-producing attack from one Bot to another Bot.
result: issue
reported: "Fresh post-fix interval contained zero Bot-to-Bot attack lines."
severity: major

### 9. C4 plant and defuse log
expected: The fresh line-offset-scoped server log contains Bot C4 plant and Bot C4 defuse events.
result: issue
reported: "Fresh post-fix interval contained zero Bot Planted_The_Bomb and zero Bot Defused_The_Bomb events."
severity: major

## 2026-09-18 action-adapter checkpoint

The Core-to-public-input bridge is implemented and offline-verified. The
focused adapter/runtime tests prove attack, defuse/use, reload-command, and
TeamInfo raw-team-zero fallback contracts. This does not change the live UAT
verdicts: the latest offset-scoped live interval did not provide a flushed
Bot action, Bot-to-Bot damage, plant, or defuse event, so those remain issues
until a clean server run produces fresh GameDLL log evidence.

## 2026-09-19 implementation checkpoint

The movement/action boundary and directed Nav corridor changes are offline
implemented and verified by the Windows x86 Debug and Release CTest suites
(42/42 each). The UAT verdict is unchanged for live acceptance: no fresh
server interval has yet demonstrated sustained movement, Bot C4 plant, Bot C4
defuse, or crash-free attack/death/restart behavior with the current DLL.

The subsequent current-DLL loopback interval did demonstrate sustained Nav
origin progress and advancing corridor indices without a crash. The live C4
plant/defuse gates remain open because no fresh plant or defuse event was
observed.

## Summary

total: 9
passed: 3
issues: 4
pending: 0
skipped: 0
blocked: 2

## Gaps

- truth: "Managed Bots autonomously act after team/class entry and remain active"
  status: failed
  reason: "The Windows run produced no sustained autonomous action and all four managed Bots were later removed by Game_idle_kick."
  severity: major
  test: 3
  root_cause: "Current-source regression confirmed that raw entity team 0 was rejected by movement readiness even after TeamInfo confirmation; the fix is offline-verified, but live deployment/retest is pending."
  artifacts: []
  missing:
    - "Live deployment of the pinned rebuilt DLL and movement proof beyond the idle-kick interval"
  debug_session: ".planning/debug/autonomous-post-join.md"

- truth: "Managed Bots make sustained Nav locomotion progress"
  status: failed
  reason: "The deployed run reported roam_no_intent/Nav diagnostic failures and no sustained origin progress."
  severity: major
  test: 5
  root_cause: "The pre-fix deployed run reported roam_no_intent/Nav diagnostics, but the deployed DLL did not match the rebuilt current-source artifact; a post-fix live run is required to isolate any remaining Nav/physics gap."
  artifacts: []
  missing:
    - "Post-fix deployed DLL/source identity match"
    - "A live regression run that reaches and verifies Nav intent dispatch"
  debug_session: ".planning/debug/autonomous-post-join.md"

- truth: "Bot-to-Bot combat produces damage logs"
  status: failed
  reason: "Fresh post-fix log interval contained zero Bot-to-Bot attack lines."
  severity: major
  test: 8
  root_cause: "NavRoam reports roam_no_intent/Stuck and no live combat action integration reaches the server log."
  artifacts: []
  missing:
    - "A successful Nav/action path and a fresh Bot-to-Bot attack log"
  debug_session: ".planning/debug/autonomous-post-join.md"

- truth: "Bot C4 plant and defuse events are logged"
  status: failed
  reason: "Fresh post-fix log interval contained zero Bot plant and defuse events."
  severity: major
  test: 9
  root_cause: "Nav/objective action path did not reach the C4 interaction state in the fresh run."
  artifacts: []
  missing:
    - "A successful objective action path and fresh plant/defuse logs"
  debug_session: ".planning/debug/autonomous-post-join.md"
