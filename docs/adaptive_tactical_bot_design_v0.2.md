# CS 1.6 Adaptive Tactical Bot
## Architecture & Implementation Specification v0.2

> Target: Counter-Strike 1.6 / GoldSrc  
> Primary runtime: ReHLDS/HLDS + Metamod + ReGameDLL_CS  
> Project license: MPL-2.0  
> Status: Architecture specification for Codex implementation planning

---

## 1. Project Goal

Counter-Strike 1.6向けに、従来のWaypoint追従型Botとは異なる「高思考型AI Bot」を実装する。

本Botは単純な射撃精度や反応速度の強化を主目的にしない。

中心となる特徴は以下。

- NavMeshベースのnavigation
- 不完全情報を扱うWorld Model
- 視覚・音・味方情報を統合するPerception
- Tactical / Strategic planning
- チーム単位の役割分担
- マップごとのPersistent Experience
- 人間およびBotのプレイ結果からの学習
- Metamod経由のGoldSrc統合
- AMX Mod Xから制御可能なAPI
- AIとGoldSrc依存部分の分離
- 将来的な他MOD・特殊ゲームモードへの拡張性

Botの強さは「人間より正確に撃つこと」ではなく、
「人間のように情報を集め、推測し、経路・戦術・行動を選択すること」によって実現する。

## 2. Non-Goals

初期バージョンでは以下を目的としない。

- Neural Networkによるaim制御
- Deep Reinforcement Learning
- LLMによる毎フレーム判断
- Waypointとの完全互換
- YaPB/SyPB/E-BOTとのバイナリ互換
- ReGameDLL内蔵zBotとの完全挙動互換
- Counter-Strike以外のGoldSrc MOD対応
- Zombie Mod専用AI
- NPC entity制御
- 人間を超える反応速度
- wallhack的な完全情報AI

初期ターゲット:

```text
Counter-Strike 1.6
+
ReHLDS / HLDS
+
Metamod
+
ReGameDLL_CS
```

Vanilla GameDLL対応は後続フェーズとする。

## 3. Upstream Reference Strategy

### 3.1 YaPB

YaPBは直接forkして全面改造する前提にしない。

役割は「現代的なGoldSrc外部Bot実装のreference implementation」。

参考対象:

- Metamod integration
- FakeClient生成
- Engine hook
- Client message解析
- movement command生成
- weapon handling
- combat
- grenade処理
- sound処理
- visibility / perception
- server lifecycle
- configuration
- C++17でのGoldSrc統合

原則として採用しないもの:

- Waypoint graph
- Waypoint index中心のgoal表現
- Waypoint pathfinding
- Waypoint experience data
- Waypointを前提とした高レベルdecision logic

### 3.2 SyPB

主に以下を参考にする。

- AMXX API設計
- 外部スクリプトからBotを操作する発想
- Bot/NPC control boundary
- YaPBから独自AIへ発展させた構造

NavigationはWaypointベースなので採用しない。

### 3.3 E-BOT

主に以下を参考にする。

- pursuit
- escape
- group movement
- Zombie系MODでのdynamic target追跡
- 通常CS以外の状況へのAI適応

通常CS向けBot Coreの基盤にはしない。

### 3.4 RealBot

コードそのものより思想を重要視する。

採用する思想:

- プレイを重ねることでマップについて学習する
- navigationとexperienceを分離する
- 人間プレイヤーの行動を観測する
- static map knowledgeとlearned knowledgeを分ける
- persistenceを前提にする

RealBotのNodeMachineをそのまま移植しない。
NavMesh上のExperience Layerとして再設計する。

### 3.5 ReGameDLL_CS / zBot

NavMesh設計の主要referenceとする。

参考対象:

- Nav Area
- Area connection
- Hiding Spot
- Place
- approach information
- encounter information
- Nav file
- Nav generation / analysis
- ladder
- crouch / jump等のarea attribute
- path finding

GameDLL内部のCCSBotそのものは移植しない。

## 4. Architecture Overview

```text
┌─────────────────────────────────────┐
│           Team Director             │
│ strategy / roles / coordination     │
└─────────────────┬───────────────────┘
                  │
┌─────────────────▼───────────────────┐
│          Tactical Planner           │
│ attack / defend / rotate / retake   │
└─────────────────┬───────────────────┘
                  │
┌─────────────────▼───────────────────┐
│           Action Planner            │
│ peek / reload / grenade / retreat   │
└─────────────────┬───────────────────┘
                  │
┌─────────────────▼───────────────────┐
│             World Model             │
│ belief / enemy / objective / team   │
└───────┬─────────────────────┬───────┘
        │                     │
┌───────▼───────┐     ┌───────▼──────────┐
│  Perception   │     │ Experience Model │
│ vision/sound  │     │ learned knowledge│
└───────┬───────┘     └───────┬──────────┘
        │                     │
        └──────────┬──────────┘
                   │
┌──────────────────▼──────────────────┐
│        Tactical Navigation          │
│ navmesh / route / danger cost       │
└──────────────────┬──────────────────┘
                   │
┌──────────────────▼──────────────────┐
│         Local Navigation            │
│ steering / avoidance / traversal    │
└──────────────────┬──────────────────┘
                   │
┌──────────────────▼──────────────────┐
│        Motor / Combat Control       │
│ usercmd / aim / fire / move         │
└──────────────────┬──────────────────┘
                   │
┌──────────────────▼──────────────────┐
│          GoldSrc Adapter            │
│ Metamod / ReGameDLL / Engine        │
└─────────────────────────────────────┘
```

## 5. Core Design Rules

### 5.1 GoldSrc型をBot Coreへ漏らさない

Coreでは可能な限り以下を使用しない。

- `edict_t*`
- `entvars_t*`
- `CBasePlayer*`
- `CBaseEntity*`
- `CCSPlayer*`
- ReAPI concrete types
- global engine state

代わりに独自Handle / Snapshotを使用する。

```cpp
using EntityId = uint32_t;
using PlayerId = uint32_t;
using NavAreaId = uint32_t;

struct PlayerSnapshot;
struct WeaponSnapshot;
struct WorldSnapshot;
struct TraceResult;
```

### 5.2 Engine access is interface-driven

```cpp
class IGameHost {
public:
    virtual ~IGameHost() = default;

    virtual PlayerSnapshot getPlayer(PlayerId id) const = 0;
    virtual WorldSnapshot getWorld() const = 0;

    virtual TraceResult trace(
        const Vec3& from,
        const Vec3& to,
        TraceMask mask) const = 0;

    virtual void submitCommand(
        PlayerId id,
        const BotCommand& command) = 0;
};
```

目的:

- Engineなしでunit test可能
- ReGameDLL dependencyの局所化
- Vanilla adapter追加余地
- AIの長期保守性向上

## 6. Runtime Scheduling

推奨周期:

```text
Motor Controller        30 Hz程度
Local Navigation        20-30 Hz
Perception              10-20 Hz
Action Planner           5-10 Hz
Tactical Planner         1-5 Hz
Team Director            1-2 Hz
Experience Update        event driven
Persistence              round/map boundary
```

重要イベント時にはplannerを即時invalidate可能とする。

## 7. NavMesh

Static Nav Dataにはgeometry、connection、crouch、jump、ladder、hiding spots、place、traversal typeを保持する。

```cpp
struct NavArea {
    NavAreaId id;
    Bounds bounds;
    Vec3 center;
    NavAttributes attributes;
    std::vector<NavConnection> connections;
    std::vector<HidingSpot> hidingSpots;
    PlaceId place;
};
```

危険度等のlearned dataはNavMeshへ直接書き込まない。

```cpp
struct AreaExperience {
    uint64_t visits;
    float dangerT;
    float dangerCT;
    float encounterRate;
    float deathRate;
    float grenadeThreat;
    float sniperThreat;
    float pushSuccess;
    float retakeSuccess;
    float humanTraffic;
    float botTraffic;
};
```

```text
NavMesh = どこへ行けるか
Experience = そこへ行った結果どうなったか
```

## 8. Tactical Pathfinding

```text
totalCost =
    distance
  + danger
  + exposure
  + congestion
  + objectivePenalty
  + rolePenalty
```

Personalityによってweightを変更する。

## 9. Local Navigation

PathfinderがArea corridorを返し、Local Navigationが以下を担当する。

- doorway通過
- corner handling
- wall avoidance
- player avoidance
- stuck recovery
- ladder
- crouch
- jump
- dynamic obstacle
- teammate congestion

Area centerを単純に辿るだけの実装は禁止。

## 10. Perception

Vision、Sound、Team InformationをObservationへ変換し、World Modelへ渡す。
敵を見失っても即座に消さず、confidence付きのbeliefとして保持する。

## 11. World Model

BotはEngineの完全情報ではなく「自分が知っている世界」を使用する。

```cpp
struct EnemyBelief {
    PlayerId player;
    Vec3 lastKnownPosition;
    NavAreaId lastKnownArea;
    double lastSeenTime;
    double lastHeardTime;
    float confidence;
    std::vector<AreaProbability> possibleAreas;
};
```

敵を見失った場合、last known areaからNavMesh connectivityとelapsed timeに基づいてpossible areaへbeliefを拡散する。

## 12. Experience / Learning

### 12.1 Persistence

マップ単位で保存する。

```text
addons/<bot>/experience/de_dust2.*
```

v1ではdebuggabilityを優先しSQLiteを第一候補とするが、Phase 0で依存コストと配布性を検証する。

### 12.2 Learning Events

- area entered
- damage dealt
- damage received
- kill
- death
- enemy encounter
- grenade explosion
- plant
- defuse
- successful attack
- failed attack
- successful retake
- round win/loss
- human traversal
- bot traversal

### 12.3 Human / Bot separation

```text
Human observations
Bot observations
```

を別カウンタとして保存する。

初期weight例:

```text
human = 1.0
bot   = 0.25
```

### 12.4 Decay

古い経験にはdecayを適用する。

## 13. Action Planner

初期実装ではLLMを使用しない。
Utility AIを第一候補とする。

候補:

```text
MoveToGoal
Hold
Peek
TakeCover
Retreat
Reload
Engage
ThrowFlash
ThrowSmoke
ThrowHE
Plant
Defuse
GuardBomb
RescueHostage
FollowTeammate
RecoverBomb
```

## 14. Tactical Planner

候補Intent:

```text
ATTACK_SITE
DEFEND_SITE
ROTATE
RETAKE
SAVE
FLANK
LURK
SUPPORT
ENTRY
TRADE
HOLD
ESCORT
```

Plannerは直接movement commandを生成しない。

## 15. Team Director

役割例:

```text
Bot A = Entry
Bot B = Trade
Bot C = Support
Bot D = Lurk
Bot E = Flank Watch
```

全BotをHive Mindにしない。

## 16. Combat

Combat AIはStrategic AIから独立する。

Difficultyは以下で調整する。

- reaction time
- observation error
- prediction error
- aim noise
- decision quality

## 17. Personality

最低限:

```text
Aggression
Caution
Teamwork
Patience
Curiosity
RiskTolerance
```

## 18. GoldSrc / Metamod Adapter

Bot本体はMetamod pluginとしてロードする。

GameDLL private dataを独自CCSBotへ置換しない。

```text
GameDLL
  CBasePlayer

Metamod Bot
  BotAgent
      │
      └ PlayerId
```

## 19. ReGameDLL Adapter

ReAPI依存は`src/adapter/regame/`へ閉じ込める。
Coreから直接includeしない。

## 20. AMX Mod X API

AMXXは必須依存にしない。

```text
Bot Core
   ↑
Metamod plugin
   ↑
AMXX bridge
```

## 21. Recommended Repository Structure

```text
/
├─ CMakeLists.txt
├─ README.md
├─ LICENSE
├─ NOTICE.md
├─ docs/
│  ├─ architecture.md
│  ├─ navigation.md
│  ├─ learning.md
│  ├─ amxx-api.md
│  └─ research/
├─ src/
│  ├─ core/
│  ├─ world/
│  ├─ perception/
│  ├─ nav/
│  ├─ experience/
│  ├─ ai/
│  ├─ combat/
│  ├─ adapter/
│  │  ├─ metamod/
│  │  ├─ regame/
│  │  └─ amxx/
│  └─ debug/
└─ tests/
   ├─ nav/
   ├─ world/
   ├─ experience/
   ├─ planner/
   └─ simulation/
```

## 22. Debug / Observability

最低限取得可能にする。

```text
Current Area
Current Goal
Current Intent
Current Action
Role
Enemy Beliefs
Selected Route
Route Cost
Danger Cost
Experience Values
Utility Scores
Reason for replanning
```

Debug instrumentationを後回しにしない。

## 23. Determinism

```text
same snapshot
same seed
=
same decision
```

を可能な範囲で保証する。

## 24. Performance Goal

32 slot serverを想定。

初期目標:

```text
16 Bots
60+ server FPSを阻害しない
```

全Botのfull replanを同一frameに集中させない。

## 25. Testing Strategy

Unit:
- NavMesh graph
- A*
- tactical cost
- enemy belief propagation
- experience update
- utility scoring
- personality weighting
- team role assignment

Simulation:
- WorldSnapshotから期待Intentを検証

Integration:
- join
- movement
- combat
- bomb
- hostage
- ladder
- grenade
- map change
- reconnect

## 26. Implementation Phases

### Phase 0 — Research & Baseline
production codeを大きく書かない。

### Phase 1 — Bot Host Skeleton
Metamod load、FakeClient、join、disconnect、basic movement。

### Phase 2 — Independent Nav Core
`.nav` load、area query、nearest area、connection graph、A*、offline tests。

### Phase 3 — Nav Movement
`bot_goto <area>`で目的Areaまで移動。
corridor following、obstacle、ladder、crouch、jump、stuck recovery。

### Phase 4 — Perception + World Model
visible observation、lost enemy memory、sound、confidence。

### Phase 5 — Combat Baseline
detect、aim、fire、reload、weapon switch。

### Phase 6 — Action Planner
Utility-based actions。

### Phase 7 — Tactical Planner
attack、defend、rotate、retake、save、flank。

### Phase 8 — Team Director
role assignment / reassignment。

### Phase 9 — Persistent Experience
map DB、danger、traffic、human/bot separation、restart persistence。

### Phase 10 — Adaptive Tactical Navigation
Experienceをpath costへ導入。

### Phase 11 — AMXX API

### Phase 12 — Advanced Learning
directional danger、weapon-conditioned danger、opponent profiling等。

## 27. Phase Gate Rules

各Phase終了時に必須:

- unit tests
- integration test
- benchmark
- documentation
- debug observability
- explicit acceptance criteria

次Phaseの機能を先取りして巨大実装しない。

## 28. Licensing Policy

### 28.1 Project License

本プロジェクトの新規コードは原則として **Mozilla Public License 2.0 (MPL-2.0)** で公開する。

意図:

- file-level copyleft
- 改変されたMPL対象ファイルのソース公開を促す
- surrounding code全体へ強いcopyleftを要求しない
- 将来的な外部統合の柔軟性を残す

### 28.2 Upstream Policy

MPL-2.0採用は、他ライセンスのコードを自由にコピーできることを意味しない。

各upstreamについて確認する。

- repository-level license
- individual file header
- exception clause
- linking condition
- derived-work condition

Codexは候補コードを以下に分類する。

```text
reference-only
clean-room reimplementation
adapted with attribution
copied / derived
unclear; requires confirmation
```

### 28.3 Default Rule

ライセンス条件が曖昧なコードは直接コピーしない。

特にGPL系コードからMPL-2.0 fileへ実装を移す場合は個別確認し、不明なら

```text
reference-only
+
independent reimplementation
```

を選ぶ。

### 28.4 Required Metadata

repoには最低限:

```text
LICENSE
NOTICE.md
docs/research/license-matrix.md
```

を置く。

## 29. Codex Rules

1. Current checkoutを唯一のsource of truthとする。
2. 推測でupstream APIを作らない。
3. upstream sourceを実際に確認する。
4. Waypoint abstractionを新規Coreへ持ち込まない。
5. GoldSrc型をCoreへ漏らさない。
6. Engine integrationとAIを分離する。
7. 各Phaseを小さなcommit単位にする。
8. test可能な箇所はtest firstを優先する。
9. debug/telemetryを同時に実装する。
10. 学習AIを一度に巨大実装しない。
11. offline test可能性を優先する。
12. optimizationは測定後に行う。
13. upstream licenseをファイル単位で確認する。
14. 直接コピーと参考実装を区別する。
15. Phase scopeを越える実装を行わない。

## 30. First Major Success Criterion

```text
MetamodからFakeClientを生成
        ↓
.navを読み込む
        ↓
現在Areaを特定
        ↓
A*でArea corridorを生成
        ↓
BotがNavMesh上を目的地まで移動
```

ここまで完成してから高レイヤーAIへ進む。

## 31. Final Vision

```text
SEE
 ↓
REMEMBER
 ↓
ESTIMATE
 ↓
PLAN
 ↓
MOVE / FIGHT
 ↓
OBSERVE RESULT
 ↓
LEARN
```

このloopを本プロジェクトの核とする。
