# P1-05 live load/join/unload evidence

This checklist is post-Finish validation only. It must not be used to move the
project-wide Finish gate forward. P1-05 covers the message-driven join
state machine and does not include FakeClient movement or navigation.

## Pinned inputs

- Metamod-P checkout: `H:\sourcecode\003.Game\amxmodx\metamod-p`
- Metamod-P commit: `7ec9b014f8c0a947a724644aebe34eb33706e44b`
- GoldSrc ABI: 32-bit x86
- Runtime: pinned ReHLDS/HLDS + Metamod-P + ReGameDLL_CS
- Adapter artifact: `astrabot_mm.dll` or `astrabot_mm.so`

## Reproduction

1. Record the server, GameDLL, Metamod-P, and adapter hashes.
2. Start a clean server with the adapter enabled and capture the complete
   server log.
3. Activate a map and confirm the adapter remains loaded.
4. Trigger the default `Terrorist / class 1` JoinRequest through the adapter
   harness. For CT, trigger an explicit `CounterTerrorist / class 1..4`
   JoinRequest; do not infer the team from a title or userid.
5. Capture the `VGUIMenu` or accepted `ShowMenu` team prompt, the class prompt,
   and the `TeamInfo` confirmation.
6. Repeat the same prompt in one frame and confirm it does not produce an
   extra dispatch. Confirm a stale/out-of-order prompt cannot reverse state.
7. Change map or disconnect the fake client and confirm the pending join,
   command context, decoder fragment, mapping, and entity state are cleared.
8. Unload the adapter, then repeat unload once more. Confirm there is no crash,
   duplicate cleanup, or duplicate attach identity line.

## Evidence template

| Item | Observation / artifact | Pass |
| --- | --- | --- |
| x86 artifact and exports | `dumpbin /exports` or `nm -D --defined-only` output | [ ] |
| pinned runtime versions | server/GameDLL/Metamod-P hashes | [ ] |
| adapter load | load command and timestamp | [ ] |
| attach identity | exactly one `astrabot version=0.1.0 adapter=metamod-p interface=5:13 outcome=attached` line | [ ] |
| T join | team/class prompts and `TeamInfo TERRORIST` | [ ] |
| CT join | explicit request, prompts, and `TeamInfo CT` | [ ] |
| GameDLL behavior | no unintended hook suppression or changed return path | [ ] |
| duplicate/out-of-order prompt | no state reversal or duplicate dispatch | [ ] |
| map change/disconnect | no stale PlayerId, mapping, entity, decoder, or command context | [ ] |
| unload twice | no crash and no duplicate cleanup/log line | [ ] |
| server log | no error/crash entries | [ ] |

## Result

- Date/time:
- Operator:
- Server/runtime:
- Adapter commit:
- Result: `PASS` / `FAIL` / `UNVERIFIED`
- Follow-up issue (if any):
