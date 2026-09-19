# Plan 08-06 follow-up verification

Status: partial; Movement/no-IdleKick improved, Combat/Objective remain failed.

- Reference comparison completed before implementation: ReGameDLL-CS/ZBot
  `StuckCheck`/`Wiggle` and portal-aware path points; YaPB button translation
  and per-frame `pfnRunPlayerMove`.
- Current-source x86 focused CTest: 3/3 passed (`nav_roam_controller`,
  `locomotion`, `compat_actor_command`).
- Deployed DLL SHA-256:
  `8CCBF6E10082A7B0806860305D06E8A812931E8767DCB7B3DE60EA3C5A128EC8`.
- Fresh qconsole offset: `84273..84288`.
- `mp_roundtime` interval: `17:02:52` to `17:07:52`.
- Movement verifier: passed; `ReadySamples=256`, `MovingSamples=3`.
- IdleKick lines: `0`.
- Bot-to-Bot attacks: `0`.
- Bot plant/defuse: `0/0`.
- Adapter audit: no production call path from Core combat/objective planners
  to attack/reload/weapon selection or `IN_USE`/C4 input.

This checkpoint does not close PAR-03, PAR-04, PAR-06, TEST-03, or TEST-04.
See `docs/evidence/phase8-live/stuck-wiggle-20260918.md`.
