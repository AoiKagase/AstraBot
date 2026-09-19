# Join / physics fix implementation evidence

Status: `implemented-offline-live-partial`  
Date: 2026-09-17 (Asia/Tokyo)

## Implementation identity

- Source HEAD: `433235f02383d601406d27ccbbfed2de98465693` with existing working-tree changes.
- Windows x86 Debug build: `build-metamod-x86-test`.
- Windows PE artifact: x86, seven exact exports.
- Debian Linux x86 ELF artifact: x86, seven exact exports.
- Windows build SHA-256: `F505D032BCABE1F3545A70C90E9AC596B79E6511E7C5F83107CD00B397376A52`.
- Deployed Windows DLL SHA-256: identical to the build artifact.
- Deployed backups: `D:/SteamCMD/cstrike_rehlds/cstrike/addons/astrabot/dlls/astrabot_mm.dll.pre-final-20260917-192230.bak`, `pre-final-20260917-194244.bak`, and `pre-teaminfo-20260917-194827.bak`.

## Offline verification

- Windows x86 configure/build: passed.
- Windows x86 CTest: `41/41 passed`.
- Debian Linux x86 configure/build: passed.
- Debian Linux x86 CTest: `41/41 passed`.
- Movement integration contract: passed.
- Phase 6 verification: `OK (9 checks)`.
- Source manifest: `OK (124 entries, 117 C/C++ files)`.
- Synthetic Phase 8 differential contract: passed; `TEST-03` reference capture remains pending.
- Reentrant disconnect cleanup regression: passed; the original actor is released even when the disconnect callback clears the shared handle.
- TeamInfo payload diagnostics now record the payload slot, name, requested team, entity team, deadflag, spectator flag, generation, phase, and frame.

## Live verification

- Unmodified Windows x86 HLDS/ReGameDLL-CS/Metamod-P started with the deployed DLL.
- AstraBot plugin load was observed.
- `de_dust2` Nav load was observed as valid.
- Bot add, TeamInfo/class confirmation, SpawnReady, `sv_restart`, movement progress, and physics sample acceptance were not completed.
- HLDS Console was not exposed to the computer-use target list and RCON returned no usable challenge, so no Bot command was inferred or synthesized.
- The test-only server process was stopped and the temporary cfg was removed.

## Acceptance boundary

This evidence does not close `PAR-01`, `PAR-06`, `TEST-03`, `TEST-04`, or Phase 8. The current implementation is verified offline and deployed for a subsequent operator-controlled live run.
