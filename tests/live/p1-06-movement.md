# P1-06 movement live validation

This document records post-Finish validation for deterministic command
submission and simple movement. It is not a Finish gate for P1-06.

## Scope

- GoldSrc x86 only; no Linux or live-server check before the project-wide
  Finish.
- P1-06 covers one accepted `BotCommand` per player/tick and its conversion
  to the original Metamod-P engine `pfnRunPlayerMove` table entry.
- Planner, Nav, combat, ReAPI, AMXX, and multi-bot scheduling are out of scope.

## Finish-before checks

| Check | Evidence | Result |
| --- | --- | --- |
| Portable x64 Debug configure/build/CTest | command/output | [ ] |
| Metamod-P pinned x86 Debug configure/build/CTest | command/output | [ ] |
| Metamod-P pinned x86 Release adapter DLL | artifact path | [ ] |
| `dumpbin /exports` five-symbol contract | output | [ ] |
| SDK include scan and `git diff --check` | command/output | [ ] |

## Post-Finish Linux check

Run the documented `-m32` adapter build and CTest only after the project-wide
Finish is explicitly confirmed.

| Check | Evidence | Result |
| --- | --- | --- |
| Linux `-m32` adapter build/CTest | command/output | [ ] |

## Post-Finish live scenario

Environment: pinned ReHLDS/HLDS, Metamod-P, and ReGameDLL_CS; x86 build of
`astrabot_mm`.

1. Load `astrabot_mm` and confirm the existing attach identity line appears
   exactly once.
2. Confirm the existing FakeClient reaches `JoinState::Joined` before any
   movement command is submitted.
3. Submit one valid command for the current `PlayerId` and current tick.
4. Confirm the next `StartFrame` calls the original engine table's
   `pfnRunPlayerMove` once with frame-derived msec and the expected ABI values.
5. Confirm a duplicate or stale tick produces no additional engine call.
6. Confirm map change, disconnect, dead state, unload, and double unload leave
   no pending movement state and do not crash.
7. Confirm normal GameDLL behavior is not suppressed and the server log has no
   error or crash.

| Scenario | Evidence (server log / harness trace) | Result |
| --- | --- | --- |
| attach identity line once |  | [ ] |
| joined bot receives one movement call |  | [ ] |
| duplicate/stale command no-call |  | [ ] |
| disconnect/map change clears pending |  | [ ] |
| unload/double unload safe |  | [ ] |
| no GameDLL suppression or server error |  | [ ] |
