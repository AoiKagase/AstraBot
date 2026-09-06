# P4-09 — Phase 4 offline integration gate

Status: P4-01〜09の実装・適用可能なオフライン検証完了。P4-09のmain統合後全gateも完了。
Base main `cedbfdc`。専用branch `codex/p409-offline-gate`。
Plan: [P4計画](../plans/phase-4-perception-world-model.md)。

## 再現可能な統合証跡

`tests/perception/scenarios.json`と[再現手順](../../tests/perception/README.md)を追加した。
Core/NAV18条件、Fake-engine15条件をそれぞれ別プロセスで2回実行し、
順序付きsnapshot／診断列全体を比較する。CoreはLinux x86でも実行する。
元の視認位置と時刻、視認・匿名音・伝聞の独立した減衰、世代／round／map、
NAV候補AreaとunknownMass、frame処理量、固定modelサイズを記録する。
P3のsource revision・dirty content・fixture／実行ファイルhash・compiler・CMake設定・
x86ヘッダ・source freshness検査を再利用し、実行前後で文脈不変も検証する。
checkerは期待checkpoint、完全なmatrix、記憶の出所・寿命、処理上限を独立検査し、
10種の意図的な破損を拒否する。raw2回分とlogは各buildの`perception-evidence/`へ残す。

視認x100→遮蔽→減衰→匿名音→明示的報告→再視認x210を実行する。
観測者2は直接視認なしで始まり、報告後にだけ伝聞を知る。
音だけの受信checkpointでは人物の位置を知らず、その後の明示報告で知識が変わることも検査する。
2.5秒の直接確信度0.5／伝聞0.25と、元の視認から5秒での失効を検査する。
NAVあり／なし、999出辺の分布処理、公平性、roundと世代の64回反復退役も含む。
Fake-engineではStartFrameと実際のpre/post hookを通し、煙／flash、音266件のoverflow、
遮蔽中の位置移動、受聴者分離、再視認、round／切断／再利用／map退役を組み合わせる。
視認callback中の効果更新・切断・無効化・報告者切断の既存再入テストもproducer内で再実行する。

負荷matrixは1／8／16観測者×8／16／100ms。移動との同時実行は1 Bot×同frame3種で、
通常到着と700ms停止からの復旧を各8秒実行し、到着後10frameの追加移動dispatchがないことを確認する。
音は毎frame、視認／遮蔽は200msごとに切り替える。多Bot移動そのものは全gateの既存P3 matrixで維持する。
これらはsynthetic処理量の証跡であり、実機16-Bot FPS・CPU上限を保証しない。
音の捕捉callback頻度はengineが決めるため、キュー排出のframe上限と混同しない。
本項目はテスト・証跡・CI artifact・文書の変更のみで、production sourceとDLL exportは変更しない。

## 現在の能力・既定値・読み取りAPI

SDK pin: `7ec9b014f8c0a947a724644aebe34eb33706e44b`。
参照ReGameDLL_CS: `679973265e1ac99a43193119e0da212ee568f5f9` (MIT)。本項目でもHEADを再確認した。
詳細なsymbol・wire形式と限界は[P4-04](p4-04-anonymous-sound.md)・[P4-05](p4-05-visual-interference.md)参照。

| 能力／経路 | 実装とオフライン証拠／制約 |
|---|---|
| 視認 | 4096 units、full FOV90°、100ms間隔、最大4観測者／frame。頭部／胴体trace、最大248trace／frame |
| 視認記憶 | 32×31、初期1.0から既定5sの線形減衰。保持時間は非zeroで設定可能 |
| 所属／round | 全playerの世代付きTeamInfo、未知はUnknown。HLTV round開始で退役、ResetHUD単独では判定しない |
| EmitSound／EmitAmbientSound | 対応sample名のみ。足音familyとC4／flashbang／smoke爆発音。匿名256-unit領域 |
| PM_PlaySound足音 | sample分類は対応。実機で上記hookへ届く全経路は未確認 |
| PlaybackEvent銃声 | PrecacheEventの返却IDと25 exact nameを対応付け。遅延・未知形式は除外 |
| HE TE_EXPLOSION | 未対応。createexplo.scのprecacheだけでは捕捉を証明しない |
| 匿名音 | 32×16、初期0.5／3s、FIFO256件、排出32件／frame、受聴検査最大1024／frame。PASや壁反射は再現しない |
| 煙 | 初回createsmoke.scの検証済み形式のみ。半径115、22s、32領域。反復particleで寿命延長しない。球交差近似 |
| Flash | MSG_ONE、ScreenFadeの3short＋RGBA、白色／flags0。duration＋hold中抑制。描画同等性やgrenade起因の完全識別は未確認 |
| NAV分布 | 200msごとに半分残留・半分隣接へ。上位32Area＋unknownMass。256接続／frame、32mapping／frame、2048job visit／frame |
| 味方報告 | 明示的送信のみ。直接視認500ms以内、現在有効な味方Bot限定。初期上限0.5／元の視認から5s。FIFO256recipient、32処理／frame |

`LifecycleCoordinator::world().latest(observer)`は次回model変更までの借用WorldSnapshot。
`visual`／`sounds`／`reports`は別々の記憶、`distributions[i]`は直接視認記憶に対応する候補。
`known(target)`は直接視認を優先し、伝聞なら元のsourceとreporterを保持する。
音から人物を特定しない。`teams()`と各`diagnostics()`は読み取り専用。
world／visual／sound／reportの失効・拒否・処理量、sound待機数、元観測年齢／受信遅延、
分布処理遅延、effect能力・拒否・overflowを取得できる。

明示送信は`LifecycleCoordinator::report(reporter,target)`またはserver operatorの
`astrabot_report <slot:generation> <slot:generation>`。任意座標は入力できず、
consoleは`report status=<reason> recipients=<n>`を記録する。
新しい常時ログ・自動送信・AMXX公開APIは追加しない。

## 検証記録

最終追加した出所別snapshot／64回退役の対象検証:
Windows portable1/1 (107.75s)、Linux portable1/1 (73.34s)、Metamod1/1 (15.81s)通過。
その後checkerに順序・出所座標・配信先検査を追加した状態を全gateで検証する。
初期開発でGCC misleading-indentationとSDK close/writeマクロ衝突を検出し、テスト側を修正した。
Graphに有用な関係がなくsource fallback。FocalSpanを初期化・queryしてから変更した。
全事前／main統合後の結果は後述へ追記する。

独立音checkpoint追加前の全gateはWindows portable52/52 (154.62s)、
Metamod67/67 (205.94s)、Linux51/51 (99.34s)通過、Release PE32/x86／指定6 exportを確認。
監査で音だけの知識差を独立checkpointへ分離し、最終版の全gateを再実行する。
固定objectサイズはWindows x86でWorldModel1,129,296／DistributionModel1,761,384 bytes、
Linux x86で940,696／1,470,560 bytes。共有NAV topology／allocator／process RSSは含まない。

最終版（独立音checkpoint、順序・出所・配信先検査を含む）の事前gate:
Windows portable52/52 (156.36s)、Metamod67/67 (212.90s)、Linux51/51 (99.67s)通過。
Releaseはmachine14C／magic10B (PE32)、GetEngineFunctions、GetEntityAPI2、GiveFnptrsToDll、
Meta_Attach、Meta_Detach、Meta_Queryの指定6 exportのみ。

## main統合後の最終検証と完了判定

実装コミット`17ae95a6b364e1b1d53b5f50305936966d9dc75f`をmainへfast-forwardした。
全証跡のcontextはこのrevision、dirty=falseであることを確認した。
以下をmain上で再実行し、ソースを固定したまま通過した。

| Gate | main結果 |
|---|---|
| Windows x86 portable Debug /WX・inspector ON | 52/52、153.70s |
| Windows x86 Metamod Debug /WX・inspector ON | 67/67、195.62s |
| Linux x86 portable Debug・warnings-as-errors・inspector ON | 51/51、91.23s |
| Release DLL | PE32、machine14C、magic10B、指定6 exportのみ |

main DLL SHA256:
`624df77ad6df0abf456629ba86995bba68c75a2036f58b3df64f0dbb90040f7d`。
P4-09はproduction sourceを変更しないため、P4-08のmain artifactと同一hashである。
全CTestのログは各`build-*-x86-test/Testing/Temporary/LastTest.log`。
統合証跡は`build-portable-x86-test/perception-evidence/portable.json`、
`build-metamod-x86-test/perception-evidence/adapter.json`、
`build-linux-x86-test/perception-evidence/portable.json`と隣接raw／logに保持する。
この後の完了記録コミットは文書3ファイルのみで、検証したコードを変更しない。

mainの実測最大値: Core/NAVは16入力処理／frame、256接続／frame、35分布job visit／frame、
元観測からの最大受信遅延300,000µs。密なNAVの最大処理遅延は4,700,000µsで、
完了済み分布を保持しつつ古い記憶は既定期限で失効する。Windows／Linuxで同じ値を確認した。
Fake-engineは516入力処理、512受聴検査、8視認trace／frame、受信遅延最大800,000µs。
全値は既定の有限予算内であり、実機性能測定ではない。

完了監査ではP4-01〜08の各報告・実装・Core/Adapter CTest登録、P4-09の6スライス、
独立音→報告の知識差、全33条件の独立プロセス一致と破損拒否、既存移動回帰を確認した。
Graphは新規関係の十分な証拠を提供しなかったためsource/diffを確認し、両worktreeでFocalSpanを更新した。
専用branch・worktreeは保持し、rootのP3-07系未コミット変更を保護した。
P4の適用可能なオフライン作業を完了とし、下記実機受入は引き続き未完了とする。

## 残る受入

[P4計画のpost-Finish表](../plans/phase-4-perception-world-model.md#post-finish-acceptance-matrix)
を未チェックで維持する。実機の音到達、煙／flashの描画・知覚照合、round／map／再参加、
報告console、16-Bot FPS／メモリ、Windows／Linux HLDS・ReHLDSは未検証。
実NAV互換性の未完了項目もP3報告の状態を維持する。
ローカルCTest結果はhosted CIの実行結果ではない。pushは行わない。
P4オフライン完了はプロジェクト全体Finishではなく、実機検証を開始する許可にもならない。
