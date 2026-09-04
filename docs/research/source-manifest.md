# Phase 0 source manifest

This manifest freezes the evidence used for AstraBot Phase 0.  All checkouts were
created in a clean temporary directory on 2026-09-04 (Asia/Tokyo).  A link below
therefore names a commit, not a moving branch.  The temporary directory and any
sibling checkout are evidence caches only; they are not project inputs and were
not modified.

## Repository baseline

Before initialization, the project directory was not a Git repository.  It
contained `.gitignore` and the two design inputs
`adaptive_tactical_bot_design_v0.2.md` and
`codex_phase0_architecture_validation.md`; it had no source tree, build system,
or license file.  The inputs were preserved byte-for-byte in baseline commit
`93d5cce19e90f19fda8ed66a375b43d395530ded` on branch `main`.

| Input | SHA-256 |
|---|---|
| `adaptive_tactical_bot_design_v0.2.md` | `E72D03C0CC785CB7A64EB5000D44A0455F1FC02483CA26BB8DA0443181DA8116` |
| `codex_phase0_architecture_validation.md` | `FB222C715DACB63DDDFC6A41CEB27286E320C1F6D31035C5DB1895B1693206D9` |

## Frozen upstreams

| Source | URL | Remote default branch | Frozen commit | Root license evidence | Relevant file-level evidence |
|---|---|---|---|---|---|
| YaPB | <https://github.com/yapb/yapb> | `master` | [`4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e`](https://github.com/yapb/yapb/tree/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e) | [MIT](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/LICENSE) | `SPDX-License-Identifier: MIT` in [`src/manager.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/manager.cpp#L1-L18), [`src/engine.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/engine.cpp#L1-L18), and other inspected current sources |
| SyPB | <https://github.com/CCNHsK-Dev/SyPB> | `master` | [`4c364fbe40d8356154f66527827bb75100aa7265`](https://github.com/CCNHsK-Dev/SyPB/tree/4c364fbe40d8356154f66527827bb75100aa7265) | [GPL-3.0 text](https://github.com/CCNHsK-Dev/SyPB/blob/4c364fbe40d8356154f66527827bb75100aa7265/LICENSE) | AMXX API implementation under [`Project SyPB/SyPB_API/source/base.cpp`](https://github.com/CCNHsK-Dev/SyPB/blob/4c364fbe40d8356154f66527827bb75100aa7265/Project%20SyPB/SyPB_API/source/base.cpp#L572-L645); bundled Metamod headers retain their own notices |
| CS-EBOT | <https://github.com/EfeDursun125/CS-EBOT> | `main` | [`23335620127198c82e060a4215b8d812de28bb24`](https://github.com/EfeDursun125/CS-EBOT/tree/23335620127198c82e060a4215b8d812de28bb24) | [MPL-2.0](https://github.com/EfeDursun125/CS-EBOT/blob/23335620127198c82e060a4215b8d812de28bb24/LICENSE) | [`source/navigate.cpp`](https://github.com/EfeDursun125/CS-EBOT/blob/23335620127198c82e060a4215b8d812de28bb24/source/navigate.cpp#L1-L25) and [`source/basecode.cpp`](https://github.com/EfeDursun125/CS-EBOT/blob/23335620127198c82e060a4215b8d812de28bb24/source/basecode.cpp#L1-L25) carry a YaPB-style MIT notice |
| Fundynamic/RealBot | <https://github.com/Fundynamic/RealBot> | `master` | [`a6649c826ce39912a2670d755671954242ccfaa3`](https://github.com/Fundynamic/RealBot/tree/a6649c826ce39912a2670d755671954242ccfaa3) | No root license file found | [`NodeMachine.h`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeMachine.h#L1-L34), [`NodeMachine.cpp`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeMachine.cpp#L1-L34), and [`NodeDataTypes.h`](https://github.com/Fundynamic/RealBot/blob/a6649c826ce39912a2670d755671954242ccfaa3/NodeDataTypes.h#L1-L34) state generic GPL protection and mixed bot heritage |
| ReGameDLL_CS | <https://github.com/rehlds/ReGameDLL_CS> | `master` | [`b0889847fe6d03898be88acc9e366660efb40ab5`](https://github.com/rehlds/ReGameDLL_CS/tree/b0889847fe6d03898be88acc9e366660efb40ab5) | [MIT](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/LICENSE), with [2025 transition notice](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/LICENSE-TRANSITION.md) | [`nav_area.h`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_area.h#L1-L27) and [`nav_path.h`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_path.h#L1-L27) still say GPL-2.0-or-later plus HL Engine/MOD exception; [`nav_file.cpp`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/game_shared/bot/nav_file.cpp#L1-L18) has no individual license notice |
| ValveSoftware/halflife | <https://github.com/ValveSoftware/halflife> | `master` | [`b1b5cf5892918535619b2937bb927e46cb097ba1`](https://github.com/ValveSoftware/halflife/tree/b1b5cf5892918535619b2937bb927e46cb097ba1) | [Half-Life 1 SDK custom license](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/LICENSE) | Current SDK contains [`game_shared/bot/nav_file.cpp`](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/game_shared/bot/nav_file.cpp#L1-L31), [`nav_area.h`](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/game_shared/bot/nav_area.h#L1-L20), and related zBot navigation files without separate license notices |
| ReHLDS | <https://github.com/rehlds/ReHLDS> | `master` | [`6266cd23faee4a6e9cf3974f9605b2cadd86f0a4`](https://github.com/rehlds/ReHLDS/tree/6266cd23faee4a6e9cf3974f9605b2cadd86f0a4) | [MIT](https://github.com/rehlds/ReHLDS/blob/6266cd23faee4a6e9cf3974f9605b2cadd86f0a4/LICENSE), with transition notice | [`rehlds/public/rehlds/rehlds_api.h`](https://github.com/rehlds/ReHLDS/blob/6266cd23faee4a6e9cf3974f9605b2cadd86f0a4/rehlds/public/rehlds/rehlds_api.h#L1-L27) retains GPL-2.0-or-later plus HL Engine/MOD exception |
| Metamod-P | <https://github.com/Bots-United/metamod-p> | `master` | [`7ec9b014f8c0a947a724644aebe34eb33706e44b`](https://github.com/Bots-United/metamod-p/tree/7ec9b014f8c0a947a724644aebe34eb33706e44b) | [`GPL.txt`](https://github.com/Bots-United/metamod-p/blob/7ec9b014f8c0a947a724644aebe34eb33706e44b/GPL.txt): GPL-2.0 text | [`metamod/meta_api.h`](https://github.com/Bots-United/metamod-p/blob/7ec9b014f8c0a947a724644aebe34eb33706e44b/metamod/meta_api.h#L1-L33) says GPL-2.0-or-later plus HL Engine/MOD exception; the bundled [FAQ](https://github.com/Bots-United/metamod-p/blob/7ec9b014f8c0a947a724644aebe34eb33706e44b/doc/txt/faq.txt#L96-L103) says plugins generally need to be GPL |
| ReAPI | <https://github.com/rehlds/ReAPI> | `master` | [`06862290bb1b15a3a0e3e0821772e148d8080474`](https://github.com/rehlds/ReAPI/tree/06862290bb1b15a3a0e3e0821772e148d8080474) | [GPL-3.0](https://github.com/rehlds/ReAPI/blob/06862290bb1b15a3a0e3e0821772e148d8080474/LICENSE) | [`regamedll_api.h`](https://github.com/rehlds/ReAPI/blob/06862290bb1b15a3a0e3e0821772e148d8080474/reapi/include/cssdk/dlls/regamedll_api.h#L1-L27) retains GPL-2.0-or-later plus HL Engine/MOD exception |
| AMX Mod X | <https://github.com/alliedmodders/amxmodx> | `master` | [`eaab23f0068b34e295987486ed5ca89408326271`](https://github.com/alliedmodders/amxmodx/tree/eaab23f0068b34e295987486ed5ca89408326271) | [`public/licenses/LICENSE.txt`](https://github.com/alliedmodders/amxmodx/blob/eaab23f0068b34e295987486ed5ca89408326271/public/licenses/LICENSE.txt#L1-L34): GPL-3.0-or-later plus HL Engine/MOD and module/plugin exceptions | [`public/sdk/amxxmodule.h`](https://github.com/alliedmodders/amxmodx/blob/eaab23f0068b34e295987486ed5ca89408326271/public/sdk/amxxmodule.h#L1-L13) repeats GPL-3.0-or-later with additional exceptions |

## Source units and symbols used as evidence

This list is intentionally narrower than a vendoring manifest: no upstream source
is part of AstraBot.

| Source | Paths and symbols inspected |
|---|---|
| YaPB | `CMakeLists.txt` C++ standard; `src/engine.cpp::Game::createFakeClient`, `Game::prepareBotArgs`; `src/manager.cpp::BotManager::execGameEntity`, `BotManager::create`, `Bot::Bot`, `Bot::startGame`; `src/botlib.cpp::Bot::runMovement`; `src/linkage.cpp` client/server lifecycle and `Cmd_Arg*` hooks; `src/message.cpp` menu state; `src/graph.cpp`, `navigate.cpp`, `planner.cpp`, `practice.cpp`, `tasks.cpp`, `combat.cpp`, `vision.cpp`, `sounds.cpp`, `control.cpp` |
| SyPB | `Project SyPB/SyPB_API/source/base.cpp` native table and `MF_AddNatives`; `Project SyPB/SwNPC/source/api.cpp` and `base.cpp` |
| CS-EBOT | `source/navigate.cpp::FindGoalZombie`, path cost functions, `FindPath`, `CheckStuck`; `source/basecode.cpp::IsEnemyReachable`, movement scheduling; `source/waypoint.cpp` |
| RealBot | `NodeMachine.{h,cpp}`, `NodeDataTypes.h`, `bot_navigate.cpp`, `bot.cpp`; node/experience persistence, player observation, danger/contact update and path cost |
| ReGameDLL_CS | `regamedll/game_shared/bot/nav.{h,cpp}`, `nav_area.{h,cpp}`, `nav_file.{h,cpp}`, `nav_path.{h,cpp}`, `nav_generate.cpp`, `nav_grid.cpp`, `nav_ladder.cpp`; `regamedll/dlls/bot/cs_bot*`, `cs_bot_manager*` |
| Valve HL1 SDK | `game_shared/bot/nav.h`, `nav_area.{h,cpp}`, `nav_file.cpp`, `nav_path.{h,cpp}`, `bot_manager.{h,cpp}` for provenance and format comparison |
| ReHLDS | `rehlds/public/rehlds/rehlds_api.h` API/version and license header |
| Metamod-P | `metamod/meta_api.h`, `metamod/dllapi.h`, `metamod/engine_api.h`, `stub_plugin/meta_api.cpp`, `doc/txt/faq.txt` |
| ReAPI | `reapi/include/cssdk/dlls/regamedll_api.h`, `reapi/src/*` registration paths |
| AMX Mod X | `public/licenses/LICENSE.txt`, `public/sdk/amxxmodule.h`, native registration APIs, bundled SQLite build inputs |

## Reproduction rules

1. Fetch the repository URL and checkout the exact full SHA above.
2. Verify the path and symbol named by the finding; do not substitute the tip of
   the default branch.
3. Evaluate both root license and the individual file header.  A root transition
   does not erase an explicit retained header for purposes of this engineering
   decision.
4. If a future fetch fails, record the failure and any local snapshot SHA.  Do
   not relabel an older sibling checkout as current.
5. Existing sibling checkouts may be read only after their `HEAD` equals this
   manifest; they must never be altered for Phase 0 evidence collection.
