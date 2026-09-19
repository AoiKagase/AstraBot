# AstraBot v1 Phase 8 acceptance summary

## Current result

The latest offset-scoped observation recorded four `Game_idle_kick` removals;
the acceptance status remains incomplete.

The subsequent grounded-readiness/cumulative-progress build passed the fresh
Movement gate from qconsole line `79240` (`readySamples=256`,
`movingSamples=3`, `idleKickLines=0`). Combat and C4 remain independent
failed gates.

Status: `incomplete — Windows live evidence partial; Nav/combat/C4 log gates failed; Linux/live parity pending` (2026-09-18). Windows x86 has real HLDS/ReHLDS team-entry, human damage/death, and restart-recovery records. The TeamInfo/readiness fix now reaches `ready=1` without idle-kick, but the fresh strict log interval still has no sustained post-spawn movement, Bot-to-Bot attack, or C4 plant/defuse event. Debian Linux x86, a pinned reference trace, and the remaining lifecycle/gameplay acceptance cases are not complete.

| Layer | Result | Meaning |
|-------|--------|---------|
| Offline build/artifact | Passed | Windows/Linux x86 builds and exact exports verified |
| Offline CTest | Passed | 37/37 on each x86 Metamod build after the current changes |
| Differential contract | Passed | Schema/replay/rejection behavior verified on synthetic data |
| Windows x86 real server | Partial | Plugin load, profile load, mixed CT/T team creation, readiness fix, human USP damage/death, and `sv_restart` recovery recorded; fresh Nav/combat/C4 log gates failed |
| Debian Linux x86 real server | Pending | No live Linux run recorded |
| Differential acceptance | Pending | A pinned captured reference trace is absent |
| PAR-06 / TEST-03 / TEST-04 | Pending | No requirement promotion without the remaining evidence |

## Deferred AstraNav boundary

The initial CSBot clone continues to load existing compatible `.nav` files read-only. Nav creation, learning, editing, analysis, optimized `astranav` storage, Wallbang information, and adaptive AI remain later extensions. The format-neutral Nav and SDK-free Core boundaries are preserved.

## Next action

Diagnose/fix the `roam_no_intent`/`Stuck` Nav gap and action integration, then rerun the log-only movement, Bot combat, and C4 checks before the remaining Debian Linux x86 scenarios. Keep `PAR-06`, `TEST-03`, and `TEST-04` pending.
