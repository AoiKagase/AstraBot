# P5-03 — Aim and reaction control

Status: 実装・オフライン検証完了。実機受入は未実施。
Base: P5-02 target selection。専用 branch: `codex/p503-aim-reaction`。

## 実装範囲

`combat::aimTarget(const CombatInput&) noexcept` を追加し、P5-02 の選択結果を
もとに、観測された Vision／TeamReport の target point へ有限な pitch/yaw を計算する。
yaw は最短経路へ正規化し、既存の view-angle 範囲へ clamp する。reaction delay、
observation error、prediction error、aim noise は agent/map/round/tick/target に
依存する決定的な値として適用し、入力不正・非有限座標・同一点では fail-closed する。

照準処理は `Track` または `NoOp` のみを返し、Attack input／`Fire`／`FireMode` を
生成しない。したがって現在の直接視認条件を維持し、TeamReport は追跡・照準準備に
限定される。

射撃契約には `FirePlan`／`FirePattern` を追加した。1回の `Fire` decision が
`Tap`、`Burst`（2〜8発）、`FullAuto` を値として保持でき、単純な永続 bool に
固定されない。`Fire` には有効な計画を必須とし、Track／NoOp への混入は拒否する。
weapon class、距離、aim error、recoil、target movement、visibility、ammo、
difficulty を将来の FireControl が参照できる最小の拡張点に留め、P5-03 では
weapon tuning、recoil model、Wallbang、SuppressiveFire は実装しない。

## 検証

`tests/combat_aim_tests.cpp` に geometry、yaw wrap、pitch clamp、reaction window、
TeamReport fallback、決定性・bounded error、fail-closed、Attack input 非生成を追加。
既存 `combat_contract_tests.cpp` では FirePlan の validity、Track への混入拒否、
DirectFire の必須条件、reserved fire mode の拒否を検証する。

| Gate | 結果 |
|---|---|
| Windows x86 portable Debug、warnings-as-errors | 52/52 CTest、build 成功 |
| Windows x86 Metamod Debug、warnings-as-errors | 67/67 CTest、build 成功 |
| Linux x86 GCC `-m32` portable Debug、warnings-as-errors | 54/54 CTest、build 成功 |
| Metamod x86 Release、tests OFF | build 成功、PE32/x86 |
| Release export | `Meta_Query`、`Meta_Attach`、`Meta_Detach`、`GetEntityAPI2`、`GetEngineFunctions`、`GiveFnptrsToDll` の6件 |
| Metamod-P SDK | `7ec9b014f8c0a947a724644aebe34eb33706e44b` |

Linux の linked worktree では、既存証拠 checker 用に `GIT_DIR`、
`GIT_COMMON_DIR`、`GIT_WORK_TREE` を WSL プロセス限定で指定した。Git 設定や
worktree metadata は変更していない。

## 未対応の受入

本変更は SDK-free Core と offline gates が対象であり、実射撃、reload、weapon switch、
Adapter hook、実サーバー上の direct visibility／combat 動作は未検証である。HLDS／
ReHLDS の live acceptance は project-wide `Finish` 後の別ゲートとして維持する。
