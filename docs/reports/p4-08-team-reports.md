# P4-08 — 明示的な味方報告

Status: 実装・全事前gate・main統合後gate完了。
Base: main `853cf13`。専用branch `codex/p408-team-reports`。
Plan: [P4計画](../plans/phase-4-perception-world-model.md#p4-08--明示的な味方報告)。

## 送信と公開契約

`LifecycleCoordinator::report(reporter,target)`とserver operator用
`astrabot_report <reporter-slot:generation> <target-slot:generation>`を追加した。
既存operator commandの引数検証・Metamod管理の登録経路を使い、DLL exportは増やさない。
任意座標は入力できず、明示的な関数呼び出し／console操作以外では送信しない。

報告者が元の視認から500,000µs以内の直接視認を持つ場合だけ送信できる。
現在同チームで、有効・生存・参加済みのAstraBotに限って配信する。
報告者本人と対象本人への配信は除く。未知所属・敵・人間への配信、古い世代、
別round、古い時刻、伝聞の再転送は拒否する。匿名音を人物へ結合しない。
現在の身元・生死・観戦・参加状態は位置を含まない情報で再検証し、
全engine callback後に世代と状態を確認する。追加engine traceや未視認の現在位置の参照はない。

WorldModelがTeamReportModelを所有し、WorldSnapshotの`reports`で別の記憶として公開する。
報告者・受信者・対象、報告位置、元の直接視認ID／時刻、報告ID、送信時刻と受信時刻を保持する。
確信度は0.5×(1−元の視認からの年齢/5,000,000µs)で、5秒で削除する。
送信・受信・再送で寿命を延ばさず、確信度を加算しない。直接視認のlastSeenは変更しない。
`WorldSnapshot::known(target)`は直接視認を優先し、伝聞の場合はsourceとreporterを返す。
元の記憶は別々に読み取れる。借用ポインタの寿命は次のmodel変更まで。

## 失効・処理上限

- 伝聞は32観測者×31対象。送信待ち256 recipient入力、配信処理32/frame。
- 一つの送信の全受信者が入らない場合は送信全体を拒否し、overflowを計数する。
- 明示送信時に単調なIDを割り当て、受信者slot順でFIFOへ追加する。
  World更新の無効化・減衰、視認／音ID順取り込み後にTeamReportを配信し、公開する。
- 直接視認IDの再送は拒否。対象ごとに新しい元の観測時刻を優先し、同時刻の
  別報告者は小さいslotを優先。同じ報告者の新しい直接視認IDは同時刻でも更新できる。
- 送信者・受信者・対象の世代、所属、eligibility epochを待機・保持の両方で照合する。
  両者が同時にチーム変更しても古い伝聞は残さない。
- 直接の無効化通知で公開済み伝聞を取り下げ、次frameで古い待機入力を拒否する。
  map／round変更・時計巻き戻りでは待機と保持を消去する。
- ReportDiagnosticsは送信・配信・退役・拒否理由・overflow・queue残・frame処理量を持つ。
  WorldSnapshotは最古伝聞年齢と、音／伝聞の最大受信遅延を公開する。
- x86 WorldModelは1,129,296 bytes。Adapterは静的所有、単体テストはheap所有。
  候補Area分布は直接視認に対応し、伝聞は独立した記憶として保持する。

## 検証

Core: 明示送信前後、出所・元の時刻、500ms境界、5秒失効、再送、伝聞再転送拒否、
直接視認優先、未知／敵、死亡・切断・再活性化、同時所属変更、世代再利用、map／round、
時計巻き戻り、同時刻再視認、queue超過と遅延失効。
Fake-engine: operator command、座標入力拒否、報告後にだけ伝聞が現れること、
非視認対象移動後も報告位置を保持すること、追加traceなし、検索callback中の切断、
1／8／16観測者×8／16／100ms、既存NAV移動との共存。
初期対象テストはWindows10/10 (1.69s)、Linux Core1/1 (0.02s)。
年齢／遅延公開とtrace不増加の追加確認後、対象4/4 (0.49s)も通過した。
全事前gate: Windows portable51/51 (29.65s)、Metamod65/65 (55.03s)、
Linux portable50/50 (19.71s)、Release PE32/x86と指定6 exportを確認した。
mainへ `f68da7c` をfast-forward統合後、Windows portable51/51 (30.72s)、
Metamod65/65、Linux portable50/50 (19.62s)が通過した。
Release DLLはPE32/x86、指定6 exportのみ。SHA256:
`624df77ad6df0abf456629ba86995bba68c75a2036f58b3df64f0dbb90040f7d`。

Graphは対象関数を収録できずsource fallback。FocalSpanで所属・観測ID・operator経路を確認した。
実機consoleと配信の確認はpost-Finish受入として未完了。P4-09は未完了。
サブエージェント、push、削除、実機HLDS／ReHLDS検証は行わず、Finishを宣言しない。
