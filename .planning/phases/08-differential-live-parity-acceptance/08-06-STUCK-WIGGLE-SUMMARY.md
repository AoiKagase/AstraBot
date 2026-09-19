---
phase: 8
plan: 06
scope: zbot-aligned-stuck-recovery-and-action-boundary
status: partial-live
---

# 08-06 follow-up checkpoint

## Reference-first basis

Before the recovery change, current ReGameDLL-CS/ZBot and YaPB sources were
checked. ZBot retains `m_isStuck` and emits `Wiggle()` while the path remains
active; its path follower uses portal-aware points. YaPB writes `pev->button`
and sends it every frame through `pfnRunPlayerMove`, including action buttons.

## Implemented and verified

- NavRoam diagnostics retain corridor/link/target/intent/velocity data.
- Stuck emits an eight-frame perpendicular recovery intent, restarts the same
  corridor, then bounds the behavior at three attempts before replan.
- Adapter diagnostic output includes target position, intent direction,
  observed velocity, corridor size/index, and link metadata.
- Current-source x86 focused tests passed 3/3:
  `astrabot_nav_roam_controller`, `astrabot_locomotion`, and
  `astrabot_compat_actor_command`.

## Live gate

- Deployed artifact SHA-256:
  `8CCBF6E10082A7B0806860305D06E8A812931E8767DCB7B3DE60EA3C5A128EC8`
- qconsole interval: `84273..84288`
- Round: `17:02:52` `Round_Start` to `17:07:52` `Target_Saved` / `Round_End`
- Movement: `Passed=true`, `ReadySamples=256`, `MovingSamples=3`
- IdleKick: `0`
- Bot-to-Bot attack: `0`
- Bot plant: `0`
- Bot defuse: `0`

## Action boundary classification

`plugin_runtime.cpp` currently dispatches only NavRoam movement, producing
`IN_DUCK`/`IN_JUMP`. It has no production call path from
`CombatController`/`RoundObjectivePlanner` to `RunPlayerMove`, weapon
selection/reload, `IN_ATTACK`, or `IN_USE`. ReGameDLL ZBot uses internal
`PrimaryAttack`/`UseEnvironment`, while YaPB translates button state into its
per-frame `pfnRunPlayerMove` call. Core combat/objective contracts therefore
remain offline-only; PAR-03/PAR-04 are not promoted.

Detailed live evidence: `docs/evidence/phase8-live/stuck-wiggle-20260918.md`.
