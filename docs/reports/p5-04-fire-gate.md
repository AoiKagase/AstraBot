# P5-04 — DirectFire authorization and attack lifecycle

Status: 実装・オフライン検証完了。実機受入は未実施。
Base: `main` `41a5a10db6561eac688cf477c2656d6a0ba8b4c7`。専用 branch:
`codex/p5-04-directfire`。

## 実装範囲

`combat::authorizeFire` を追加し、P5-03 の `aimTarget` 結果を入力として
DirectFire を認可する。入力・map・round・tick・generation、alive、同一 frame の
`WorldSnapshot.visual`、Vision provenance、現在 frame と一致する
`visual.stamp.timeMicros`／`identity.observedMicros`、reaction、有限かつ範囲内の
Aim、Opponent 関係、active weapon、reload、clip、cooldown を順に fail-closed で
検証する。

認可時は `Fire`、`DirectFire`、`FirePlan::tap()`、`Button::Attack`、active weapon
を返す。抑止時は `Track` または `NoOp` と typed `CombatReason` を返し、P5-04 では
Tap pulse のみを生成する。`InvalidVisibility` と `DuplicateAttack` を追加した。
`AttackLifecycleState` は map／round／player／agent 境界、最後の Fire tick／time、
attack-held 状態を値として保持し、`FireAuthorization` が decision と次状態を返す。
同一 tick の再処理は `DuplicateAttack` とし、context 世代の変更時は lifecycle を
初期化する。隠れた singleton／共有 mutable state および adapter export の変更はない。

## 拒否理由とテスト

`tests/combat_fire_tests.cpp` で次を検証した。

| 条件 | 結果 |
|---|---|
| 現在 frame の直接視認、Vision、Opponent、reaction 完了、装填済み | `Fire(DirectFire)`、Tap、Attack |
| visibility loss、stale memory、TeamReport provenance | Fireなし、`InvalidVisibility` |
| Unknown relation／ally | Fireなし、`UnknownRelation`／`Ally` |
| reaction incomplete | Fireなし、`ReactionDelay` |
| reload／empty clip／cooldown | Fireなし、typed reason |
| stale input／stale weapon／invalid active weapon | Fireなし、typed reason |
| 同一 tick の再送 | Fireなし、`DuplicateAttack` |
| map generation reset | lifecycle reset 後の新しい Fire |
| 同一入力・同一 state | decision／次 state が決定的に一致 |

既存の combat contract test では `validateForP5()` が Wallbang／SuppressiveFire を
拒否することを維持している。

## 検証

実行したコマンド:

```text
rtk powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-canonical.ps1 -Profile All
```

| Gate | 結果 |
|---|---|
| Windows x86 portable Debug、warnings-as-errors | build 成功、53/53 CTest |
| Windows x86 Metamod Debug、warnings-as-errors | build 成功、68/68 CTest |
| Metamod x86 Release、tests OFF | build 成功、PE32/x86 |
| Release export | `Meta_Query`、`Meta_Attach`、`Meta_Detach`、`GetEntityAPI2`、`GetEngineFunctions`、`GiveFnptrsToDll` の6件 |
| FocalSpan | `status --json` が `ready=true`、実装後 update 済み |
| 差分検査 | `git diff --check` 通過 |

Canonical gate の `astrabot.combat.fire_gate` は portable／Metamod Debug の両方で
通過した。Linux x86 CI は継続対象だが、このローカル作業では実行していない。

## 未対応の受入

本変更は SDK-free Core と offline gates が対象であり、実射撃、reload／weapon switch、
adapter hook、実サーバー上の direct visibility／combat 動作は未検証である。
Project-wide `Finish` は確認しておらず、HLDS／ReHLDS の live acceptance は
Finish 後の別ゲートとして未完了のまま維持する。
