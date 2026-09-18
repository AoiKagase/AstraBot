---
phase: 8
status: partial
verified_at: 2026-09-18
requirements:
  - PAR-06
  - TEST-03
  - TEST-04
---

# Phase 8 Verification: Differential live parity acceptance

## Verdict

The later offset-scoped observation recorded four `Game_idle_kick` removals;
the earlier no-idle-kick wording above is historical and is not the final
status for this run.

The subsequent grounded-readiness/cumulative-progress build passed the fresh
Movement gate from qconsole line `79240` (`readySamples=256`,
`movingSamples=3`, `idleKickLines=0`). Bot combat and C4 remain independent
failed gates.

Phase 8 is not complete. Offline artifact and replay-contract checkpoints are green. Windows x86 real-server evidence proves team entry, forced-death/`sv_restart` recovery, and a human USP attack killing a managed Bot. The TeamInfo/readiness gap is fixed and a fresh deployed run reaches `ready=1` without idle-kick, but strict log checks still fail post-spawn movement, Bot-to-Bot combat, and C4 plant/defuse; Nav diagnostics report `roam_no_intent`/`Stuck`. Debian Linux x86 live evidence, the pinned reference trace, and the remaining gameplay/lifecycle cases are pending.

## Goal-backward evidence

| Criterion | Status | Evidence |
|-----------|--------|----------|
| Windows/Linux x86 artifacts and exact exports | Passed offline | `08-01-SUMMARY.md`, artifact matrix, current x86 builds |
| Source-independent observable differential replay | Passed as contract only | `08-02-SUMMARY.md`, `tests/phase8_differential.py` |
| Windows x86 plugin load/profile/two-team creation | Passed live | `docs/evidence/phase8-live/windows-x86.md` |
| Windows x86 forced death and `sv_restart` recovery | Passed live | Post-restart `Spawned_With_The_Bomb` and `Round_Start` |
| Windows x86 human damage/death | Partial live | `+ARUKARI-` damaged and killed `Bert` with `usp`; autonomous Bot combat and respawn parity remain unverified |
| Windows x86 TeamInfo/readiness gap closure | Passed offline | Current-source RED/GREEN regression and CTest `41/41`; live deployment/retest pending |
| Windows x86 automated movement/combat/C4 log gates | Failed live | Fresh interval: no post-spawn sustained horizontal movement, zero Bot-to-Bot attacks, zero Bot C4 plant/defuse events; `roam_no_intent`/`Stuck` recorded |
| PAR-06 lifecycle/reconnect/multi-Bot stability | Pending | The Windows run ended with four `Game_idle_kick` removals; autonomous action and the Linux live run remain incomplete |
| TEST-03 pinned CSBot/AstraBot differential evidence | Blocked | No captured reference trace; checked-in trace is synthetic |
| TEST-04 real-server Windows/Linux x86 acceptance | Pending | Windows partial evidence exists; Linux live evidence is absent |

## Offline verification

- Windows x86 Metamod CTest: 37/37 passed.
- Debian WSL Linux x86 Metamod CTest: 37/37 passed.
- PE/ELF verifier, Python artifact/manifest checks, Phase 6 verification, Phase 7 scenarios, and the Phase 8 differential contract remain passed at their respective offline layers.
- Offline results do not establish live physics, combat, radio, objective, or multi-Bot parity.

## Boundary audit

Production AstraBot source has no ReGameDLL-CS private symbols, headers, classes, or API dependency. Runtime uses public Metamod-P, HLSDK Engine, GameDLL, and AstraBot-owned Core boundaries. Existing `.nav` input remains read-only; AstraNav generation, optimized persistence, Wallbang data, and adaptive AI remain deferred.

## Resume condition

## 2026-09-18 plan 08-07 checkpoint

The missing production action boundary is now implemented offline: Core
`CombatController` and `RoundObjectivePlanner` feed a public action adapter
that preserves movement input and emits `IN_ATTACK`, `IN_USE`, or the public
reload command. TeamInfo-derived T/CT roles are used when raw entity team
fields remain zero. The current x86 focused suite is 5/5 and the required
boundary verifier passes.

This checkpoint does not promote live acceptance. The final DLL is staged on
the stopped Windows HLDS target, but the latest five-minute interval (qconsole
startup offset `87247`, an earlier action-sensor build) produced no flushed
action diagnostics or fresh Bot-to-Bot/C4 GameDLL events. PAR-03/PAR-04 live
gates, Linux x86, the pinned reference trace, and PAR-06/TEST-04 remain open.

Resolve or explicitly classify the autonomous-action failure, run the Debian Linux x86 real-server scenarios and remaining Windows lifecycle/gameplay cases, and add a pinned captured reference trace before closing TEST-03. Do not mark Phase 8 or PAR-06/TEST-03/TEST-04 complete from offline evidence alone.

## 2026-09-18 movement-boundary continuation

## 2026-09-19 offline implementation checkpoint

- Windows x86 Debug complete build and CTest: 42/42 passed.
- Windows x86 Release complete build and CTest: 42/42 passed.
- Focused Nav tests and ActionAdapter test passed after directed portal and
  action projection changes.
- Python contract/PE verification remains environment-unverified because
  `py -3` cannot create the configured Python process.
- Live HLDS/ReHLDS acceptance remains pending; offline results do not establish
  Bot movement, C4 plant/defuse, combat, or crash-free lifecycle behavior.

- Root-cause evidence: historical live logs contained NavRoam target/intent and `RunPlayerMove` dispatch, but post-dispatch velocity remained zero and the controller reached `LocomotionResult::Stuck`.
- Fix: synchronize `edict_t::v.button` and `edict_t::v.impulse` before the public `pfnRunPlayerMove` call; regression test observes the entvars state.
- Offline gate: full Windows x86 Debug CTest `42/42` passed; action-boundary contract passed; PE verifier was not run because the installed Python launcher could not create its Python process.
- Current live status: not accepted. The local HLDS launch path terminated before a post-spawn observation window, so movement and C4 plant/defuse remain pending.
