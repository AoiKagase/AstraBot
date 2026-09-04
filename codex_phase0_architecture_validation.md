# Codex Task
## Adaptive Tactical Bot — Phase 0 Architecture Validation

このセッションではproduction implementationを開始しないこと。

目的は、`adaptive_tactical_bot_design_v0.2.md` の設計を、
現在のupstream sourceとGoldSrc / Metamod / ReGameDLL環境に照らして独立検証し、
Phase 1〜3の実装計画を確定することである。

## Basic Direction

- YaPB: GoldSrc / Metamod / FakeClient / weapon / perception実装の参考
- SyPB: AMXX API設計の参考
- E-BOT: pursuit / escape /特殊game mode behaviorの参考
- RealBot: persistent learning / adaptation思想の参考
- ReGameDLL_CS zBot: NavMesh / NavArea / HidingSpot / pathfinding / `.nav` formatの参考

Waypointを新Bot Coreの中心概念として採用しない。

プロジェクトの新規コードはMPL-2.0を基本ライセンス候補とする。
ただしupstream由来コードのライセンス条件を優先し、直接コピー可否は個別に検証する。

## Critical Constraints

以下を正しい前提として扱わず反証すること。

- YaPBが最適なGoldSrc host referenceである
- ReGameDLL NavコードをGameDLLから独立可能である
- ReGameDLL `.nav` を外部Metamod pluginから利用可能である
- FakeClientを通常のGameDLL playerとして保持しAI objectだけ外部管理できる
- ReAPIで必要情報を取得できる
- Vanilla GameDLL対応を後から追加できる
- MPL-2.0採用がplanned upstream usageと矛盾しない

## Research Targets

### YaPB
Repository: https://github.com/yapb/yapb

最低限:
`src/module.cpp`, `engine.cpp`, `hooks.cpp`, `linkage.cpp`, `manager.cpp`,
`botlib.cpp`, `control.cpp`, `navigate.cpp`, `graph.cpp`, `planner.cpp`,
`practice.cpp`, `tasks.cpp`, `combat.cpp`, `vision.cpp`, `sounds.cpp`, `message.cpp`

確認:
FakeClient、client lifecycle、Metamod、movement、GameDLL dependency、
waypoint dependency、weapon state、messages、visibility、hearing、C++ standard。

### ReGameDLL_CS
Repository: https://github.com/rehlds/ReGameDLL_CS

最低限:
`regamedll/game_shared/bot/`, `regamedll/dlls/bot/`,
`nav_area.*`, `nav_file.*`, `nav_path.*`, `nav_generate.*`,
`bot.*`, `cs_bot.*`, `cs_bot_manager.*`

確認:
`.nav` format、CNavArea、HidingSpot、approach、encounter、ladder、place、
pathfinding、map analysis、trace、gpGlobals、CBaseEntity/CBasePlayer dependency。

### RealBot
Repository: https://github.com/Fundynamic/RealBot

最低限:
`NodeMachine.cpp`, `NodeMachine.h`, `NodeDataTypes.h`, `bot_navigate.*`,
persistence関連コード。

確認:
learningの実体、player movementからの蓄積、node generation、
path experience、danger、persistence、human/bot observation。

### SyPB
Repository: https://github.com/CCNHsK-Dev/SyPB

確認:
AMXX API、Bot control boundary、SwNPC、YaPB 2.7.2からの公開interface差分。

### E-BOT
Repository: https://github.com/EfeDursun125/CS-EBOT

確認:
SyPBとの差分、Zombie navigation、pursuit、escape、group movement、
obstacle handling。

## License Investigation

各source treeについて:
- repository license
- individual file header
- exception clause
- linking condition
- derived-work condition

分類:
```text
reference-only
clean-room reimplementation
adapted with attribution
copied / derived
unclear; requires confirmation
```

root LICENSEだけで判断しない。

## Architecture Validation

検証対象:

```text
Bot Core
    |
    +-- World Model
    +-- Perception
    +-- Nav
    +-- Experience
    +-- Planner
    +-- Combat
    |
IGameHost
    |
Metamod / ReGame Adapter
    |
GoldSrc
```

Coreから以下を排除できるか確認:
`edict_t*`, `entvars_t*`, `CBasePlayer*`, `CBaseEntity*`, ReAPI concrete types。

## Phase 1 Feasibility

```text
Metamod plugin
↓
FakeClient create
↓
join game
↓
external BotAgent association
↓
basic user movement
```

必要なfunction/class/header/source/hookを列挙する。

## Phase 2 Feasibility

```text
.nav load
↓
NavArea representation
↓
nearest-area query
↓
A*
```

比較:
A. ReGameDLL nav code直接抽出
B. `.nav` format互換clean implementation
C. 必要部分のみadapt + GameDLL-dependent adapter

比較軸:
effort, maintenance, correctness, testability, license, future compatibility。

## Phase 3 Feasibility

```text
Nav path
↓
corridor
↓
local movement
↓
GoldSrc movement command
```

door、ladder、jump、crouch、narrow passage、teammate avoidance、
stuck recoveryを検討する。

## Compare Three Architectures

A. YaPB fork  
B. New Bot + YaPB reference  
C. ReGameDLL fork / bridge

比較軸:
initial effort, maintainability, NavMesh, AMXX, Vanilla compatibility,
GameDLL coupling, testing, license risk。

## Required Outputs

1. `docs/research/upstream-comparison.md`
2. `docs/research/nav-extraction.md`
3. `docs/research/goldsrc-host.md`
4. `docs/research/license-matrix.md`
5. `docs/architecture.md`への修正提案
6. Phase 1〜3 implementation plan案

このセッションではproduction sourceを変更しない。

## Final Report

- Architecture v0.2で正しかった判断
- 誤っていた判断
- 未確認事項
- 最大のtechnical risk
- 最大のlicense risk
- MPL-2.0採用上の注意点
- 推奨architecture
- Phase 1最小scope
- Phase 2最小scope
- Phase 3最小scope

file path、symbol、commit、license header等の再現可能な根拠を示す。

## Do Not

- production codeを書かない
- repository-wide refactorをしない
- Waypoint compatibility layerを先回り実装しない
- GPL等のコードをライセンス確認なしでコピーしない
- READMEだけを根拠にarchitectureを決めない
- upstream APIを推測で作らない
