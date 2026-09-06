# P4-06 — 観測統合とWorldSnapshot

Status: 実装・全事前gate通過。main統合後検証は未完了。
Base: main `abad680`。専用branch `codex/p406-world-snapshot`。
Plan: [P4計画](../plans/phase-4-perception-world-model.md#p4-06--観測統合とworldsnapshot)。

## 契約と実装

`core::world::WorldModel`が既存VisualMemoryModelとSoundMemoryModelを所有する。
VisionAdapter／SoundAdapterは参照で接続し、既存の`vision().memory()`／
`sound().memory()`は同じ記憶を読む。CoreにSDK型や音の発音者IDは追加していない。
LifecycleCoordinatorの`world()`はconst参照だけを公開する。

`latest(PlayerId)`は観測者別のoptional WorldSnapshotを返す。snapshotはstamp、
canonical記憶へのconstポインタ、最古の視認／音の年齢、音の最大受信遅延、
入力キュー残数と累積overflowを持つ。ポインタの有効期間は次の記憶変更まで。
保持して後から利用する場合は呼び出し側で必要な値をコピーする。
件数・出所・元の時刻・受信時刻は各記憶、拒否理由と処理件数は各diagnosticsで取得する。

StartFrame入口で前の公開を取り下げる。視認走査の再検証後に位置を含まない
MemoryFrameで無効化と減衰を行い、視認と匿名音をstageして最後にpublishする。
エンジンcallback中に新WorldSnapshotを取得できない。マップ・tickを再確認し、
退役したフレームは公開しない。既存読み取りAPIの互換性は維持する。

取り込み順はmap／round／source／sequence／receiverの固定順。同じIDの同一入力は
一件にまとめ、異なるpayloadが衝突したgroupは全件拒否する。古い世代・時刻・IDは
既存reducerが検証する。視認は1.0から5秒、音は0.5から元の発生時刻から3秒で失効。
音で視認位置やlastSeenを更新せず、入力件数で確信度を加算しない。

## 上限と検証

- 固定stage容量: 視認32 batch＋匿名音1024 receiver入力。満杯時は新規を拒否。
- 1 frame最大1056入力を固定配列上でsort。視認減衰最大992、音減衰最大512件。
- x86 `sizeof(WorldModel)`: 251,128 bytes。snapshot取得は最大31＋16記憶を走査。
- 音のAdapter上限256 queue／32 events／1024 audience checksは維持。追加traceなし。
- Core: 公開境界、既存APIとの同一owner、減衰、期限、遅延、逆順replay一致、
  重複／競合／NaN／未来受信時刻／古いbatch、世代・round・map・時計巻き戻り、最大容量。
- Fake-engine: StartFrame→公開、音のhook時には未反映、遮蔽位置保持、trace中の公開抑止、
  再入による切断・死亡・map停止、1／8／16観測者×8／16／100ms、既存NAV到着との共存。
- 初回全Metamod Debug回帰60/60 (50.28s)。追加後のCore＋Adapter対象テスト2/2 (0.26s)。

## 完了gateの記録

事前gate: Windows portable49/49 (29.55s)、Metamod61/61 (49.23s)、
Linux portable48/48 (19.90s)。Release DLLは14C/x86、10B/PE32、指定6 exportのみ。
mainへのff-only統合後、同じ全gateを再実行して結果を記録する。
Graphは変更関数を収録できずsource fallbackを使用。FocalSpanでowner／取り込み契約を照会。
SDK pin `7ec9b014f8c0a947a724644aebe34eb33706e44b`、DLL export変更なし。
サブエージェント、push、削除、実機HLDS／ReHLDS検証は実行しない。
P4-07以降とプロジェクト全体Finishは未完了。
