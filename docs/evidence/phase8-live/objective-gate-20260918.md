# Phase 8 objective gate live observation

Date: 2026-09-18 (Asia/Tokyo)

## Scope

This run validates the log-only Objective gate after a fresh HLDS start. The
gate requires both a BOT `Planted_The_Bomb` line and a BOT
`Defused_The_Bomb` line in the requested line-offset interval. A
`Spawned_With_The_Bomb` line is lifecycle evidence only and does not satisfy
the gate.

## Run identity

- Server: `D:\SteamCMD\cstrike_rehlds\hlds.exe`, PID `9256`
- Map: `de_dust2`
- Port: `27016`
- Deployed DLL SHA-256: `EF5C80401C1295D64CDC3343890861F2246A1753308721E8E704B8F74DE7FCF5`
- qconsole offset: `82606`
- Round start: `2026-09-18 15:53:24`
- Observation: through the next round transition at `15:58:29` (`mp_roundtime 5`)
- Extended observation through `16:03:19` recorded four `Game_idle_kick`
  events and still no Plant or Defuse event.

## Result

The fresh interval contained `Spawned_With_The_Bomb`, but no BOT plant or
defuse event.

```text
Objective.Passed=false
Objective.PlantLines=0
Objective.DefuseLines=0
Verifier: -Require Objective -Json
```

The result is an intentional live acceptance failure, not an offline test
failure. Core objective proposals and synthetic fixtures are not promoted to
live success. The runtime also recorded `roam_no_intent`; autonomous movement
and combat remain separate pending gates.
