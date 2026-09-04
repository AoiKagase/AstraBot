# P1-07 removal and map-change live evidence

This checklist is post-Finish validation only.  P1-07 implementation and
adapter fixture tests must pass before this scenario is attempted.  Linux
`-m32` and live-server validation remain prohibited until the project-wide
Finish state is explicitly confirmed.

## Scope and trigger split

- The adapter-private `LifecycleCoordinator::removeActive()` path is covered
  by the x86 fixture tests.  It must queue one numeric userid kick and let the
  later `ClientDisconnect` hook own normal cleanup.
- The live server path uses the server console's numeric `kick #<userid>` to
  exercise the external disconnect path.  No new production export, cvar, or
  server command hook is required for P1-07.
- The scenario covers the existing single FakeClient mapping only.  Nav,
  planner, combat, ReAPI, AMXX, and multi-bot scheduling are excluded.

## Pinned inputs

- Metamod-P checkout: `H:\sourcecode\003.Game\amxmodx\metamod-p`
- Metamod-P commit: `7ec9b014f8c0a947a724644aebe34eb33706e44b`
- GoldSrc ABI: 32-bit x86
- Runtime: pinned ReHLDS/HLDS + Metamod-P + ReGameDLL_CS
- Adapter artifact: `astrabot_mm.dll` or `astrabot_mm.so`

## Reproduction

1. Record server, GameDLL, Metamod-P, OS/architecture, compiler, adapter
   commit, artifact hash, and map name.
2. Load `astrabot_mm` and confirm exactly one existing attach identity line:

   ```text
   astrabot version=0.1.0 adapter=metamod-p interface=5:13 outcome=attached
   ```

3. On the first frame after map activation, confirm one FakeClient allocation,
   GameDLL player setup, join request, confirmed team/class, and external
   mapping.
4. Submit one safe forward command and record one non-zero movement result.
5. Read the FakeClient's numeric userid from the server state and run the
   server console command `kick #<userid>`.  Confirm `ClientDisconnect` clears
   the agent mapping, player slot, pending movement, join state, message
   decoder, and adapter entity reference.
6. Repeat the kick or disconnect callback once.  Confirm no second kick,
   entity removal, mapping transition, cleanup trace, or crash occurs.
7. Change map while the slot is occupied or a command is pending.  Confirm
   teardown stops command dispatch before the map deactivates and leaves no
   stale mapping or entity reference.
8. On the new map, confirm a newer map generation and a new PlayerId/player
   generation.  Replay the T/1 request, then repeat the join and movement
   checks.
9. Run an explicit CT request (`CounterTerrorist / class 1..4`) in a separate
   controlled case.  Confirm the requested team from `TeamInfo CT`; do not
   infer it from a name, title, or userid.
10. Unload the adapter, unload it a second time, and confirm no crash,
    duplicate cleanup, or duplicate attach identity line.
11. Save the complete server log and structured fixture/live trace.  Confirm
    there are no error or crash entries and no raw pointer/private-data address
    in the evidence.

## Evidence template

| Item | Observation / artifact | Pass |
| --- | --- | --- |
| server/runtime versions |  | [ ] |
| Metamod-P SHA | `7ec9b014f8c0a947a724644aebe34eb33706e44b` | [ ] |
| OS and architecture |  | [ ] |
| adapter artifact/hash |  | [ ] |
| five exports | `dumpbin /exports` or `nm -D --defined-only` output | [ ] |
| attach identity | exactly one line | [ ] |
| T/1 join and mapping |  | [ ] |
| one movement call |  | [ ] |
| numeric userid kick | command and resulting `ClientDisconnect` | [ ] |
| mapping/entity cleanup | no stale state | [ ] |
| duplicate disconnect | no duplicate cleanup or crash | [ ] |
| first map generation |  | [ ] |
| replay map generation | newer than first map | [ ] |
| replay PlayerId generation | newer than first player | [ ] |
| CT explicit JoinRequest | team/class confirmation | [ ] |
| unload twice | no crash and no duplicate identity line | [ ] |
| GameDLL behavior | no unintended suppression | [ ] |
| server log | no error/crash entries | [ ] |

## Result

- Date/time:
- Operator:
- Server/runtime:
- Adapter commit:
- Result: `PASS` / `FAIL` / `UNVERIFIED`
- Follow-up issue (if any):
