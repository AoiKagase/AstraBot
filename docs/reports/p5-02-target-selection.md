# P5-02 — Target detection and deterministic selection

Status: 実装・オフライン検証完了。実機受入は未実施。
Base: P5-01 `75b37fa`。専用 branch: `codex/p502-target-selection`。

## 実装範囲

`WorldSnapshot` は現在フレームの `TeamRoster` 値を世代付き `TeamMember`
配列として値コピーする。`relation(observerTeam, target)` は対象 ID の slot と
generation が一致し、対象が Terrorist／CounterTerrorist のときだけ
`Self`／`Ally`／`Opponent` を返す。それ以外（世代再利用、所属不明、観戦者、
未知の observer team）は `Unknown` とする。

`combat::selectTarget(const CombatInput&) noexcept` は入力と snapshot の stamp を
検証した後、直接 `Vision`、次に `TeamReport` の順で候補を調べる。候補は map、
round、generation、source、観測／受信時刻、confidence、有限座標、対象関係を
検証し、`Opponent` だけを残す。TeamReport の reporter も現フレームの Ally に
限定し、匿名 `Sound` から PlayerId は生成しない。

比較順は常に次の通りである。

1. 直接 `Vision`
2. confidence 降順
3. 元観測時刻の新しい順
4. 現在視点からの角度誤差の小さい順
5. `PlayerId` 昇順

選択時は現在の view、対象 ID、source、元観測からの age、confidence、
`Accepted`、入力 tick、現在時刻の validity deadline を持つ `Track` を返す。
buttons と fire mode は空で、`Fire`／Attack input は生成しない。対象位置と eye
position が一致する候補、非有限値、時刻不整合は fail-closed で除外する。
候補がない場合は `UnknownRelation`、`Ally`、`StaleTarget`、`AnonymousSound`、
`NoTarget` の順で `NoOp` reason を選ぶ。

## 検証

`tests/combat_target_tests.cpp` と CTest `astrabot.combat.target_selection` に、
直接視認、TeamReport fallback、視認優先、confidence／時刻／角度／PlayerId の
tie-break、同一入力の反復一致を追加した。stale／未来時刻／map・round 不一致、
世代再利用、不正 confidence、非有限／一致座標、Self／Ally／Unknown／観戦者、
匿名音、TeamReport の reporter／snapshot 不整合、Attack input 不生成を検証する。
既存 `astrabot.world_model` には roster の値コピーと世代不一致 `Unknown` の回帰を
追加した。

| Gate | 結果 |
|---|---|
| Windows x86 portable Debug、warnings-as-errors | 51/51 CTest、build 成功 |
| Windows x86 Metamod Debug、warnings-as-errors | 66/66 CTest、build 成功 |
| Linux x86 GCC `-m32` portable Debug、warnings-as-errors | 51/51 CTest、build 成功 |
| Metamod x86 Release、tests OFF | build 成功、PE32/x86 |
| Release export | 指定された 6 export のみ |
| Metamod-P SDK | `7ec9b014f8c0a947a724644aebe34eb33706e44b` |

Linux の linked worktree では、既存証拠 checker 用に `GIT_DIR`、
`GIT_COMMON_DIR`、`GIT_WORK_TREE` を WSL プロセス限定で指定した。Git 設定や
worktree metadata は変更していない。

## 未対応の受入

Core の SDK-free offline evidence までを対象とする。射撃、照準、リロード、武器
切替、Adapter hook は未実装であり、P5-02 はこれらの契約を変更しない。実機 HLDS／
ReHLDS、実サーバー上の視認・所属・伝聞・combat 動作、Linux live device は
project-wide `Finish` 後の別ゲートとして未検証のまま維持する。
