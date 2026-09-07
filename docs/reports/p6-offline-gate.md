# P6 — Action Planner offline gate

Status: 実装・オフライン検証完了。実機受入は未実施。
専用 branch: `codex/p6-action-planner`。

## 実装範囲

`src/core/action_planner.hpp/.cpp` に SDK-free の Action Planner を追加した。
`ActionType`、`ActionIntent`、`ActionScore`、`ActionPrecondition`、
`ActionAbortReason`、`ActionResult` を公開契約として定義し、value-level の
World Model、武器、NAV候補、目的情報だけを入力にする。

Utility AI は有限配列・決定的な tie-break・説明可能な加点／リスク項で実装した。
Engage、Reload、Hold、FollowCurrentIntent、TakeCover、Retreat、Peek、Plant、
Defuse、GuardBomb、RecoverBomb、RescueHostage、EscortHostage、AcquireWeapon を
対象にし、hysteresis、優先度割込み、timeout、候補消失、unsafe route、threat change、
objective urgency を typed reason として扱う。Weapon Acquisition は固定長候補、
map／round／tick／時刻／有限値の stale 検証、route／exposure／objective-delay リスクを
持つ。Core から Engine API や SDK 型は参照していない。

`tests/action_planner_tests.cpp` と CTest `astrabot.action_planner` で、最小アクション、
目的行動、weapon utility、stale candidate、arbitration／abort、1・8・16 Bot load の
決定性を検証する。

## 検証

P6実装中は focused 検証のみを実行し、ゴール地点で次の canonical full 検証を1回だけ
実行した。

```text
rtk powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-canonical.ps1 -Profile All
```

| Gate | 結果 |
|---|---|
| Windows x86 portable Debug、warnings-as-errors | PASS、57/57 CTest |
| Windows x86 Metamod Debug、warnings-as-errors | PASS、75/75 CTest |
| Metamod x86 Release、tests OFF | PASS、PE32/x86 |
| Release export | `Meta_Query`、`Meta_Attach`、`Meta_Detach`、`GetEntityAPI2`、`GetEngineFunctions`、`GiveFnptrsToDll` の6件 |
| FocalSpan | `ready=true`、`stale=false`、更新後の索引を確認 |
| 差分検査 | `git diff --cached --check` PASS |

canonical cache は同一 HEAD `13b099f6aa4f31083d1bcb49cb06df58e8fc323c`、同一 tree
`b25c0c215fdc648ae437ba879e85e958f30ff9da` に対して、PortableDebug、MetamodDebug、
MetamodRelease の各結果を `passed` と記録している。

Focused 検証では Action Planner `1/1`、既存 movement／combat 回帰 `8/8`、Metamod
Debug adapter build を確認済みである。

## 未対応の受入

Linux x86 は継続CIの対象であり、このWindows作業ではローカル実行していない。
HLDS／ReHLDS上のlive acceptance、実ゲームの objective／weapon pickup 動作、
project-wide `Finish` は未確認であり、本フェーズのoffline PASSとは分離する。
