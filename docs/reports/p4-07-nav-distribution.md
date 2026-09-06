# P4-07 — NAV候補位置分布

Status: 実装・全事前gate通過。main統合後gateは未完了。
Base: main `3f9af62`。専用branch `codex/p407-nav-distribution`。
Plan: [P4計画](../plans/phase-4-perception-world-model.md#p4-07--nav上の候補位置分布)。

## 契約と公開

WorldModelが候補分布の公開値を所有する。WorldSnapshotの`distributions[i]`は
`visual->memories[i]`に対応するconst PositionDistributionへの借用ポインタ。
観測者・対象の世代と元の観測IDが一致する場合だけ公開し、次のmodel変更まで有効。
最大32 Areaのweight、unknownMass、NAV revision、更新時刻、遅延、availableを持つ。
確信度は既存視認記憶の値であり、位置分布のweightとは別である。

SDK非依存のDistributionTopologyはNAV公開時に構築する。検証済みNavGraphと
NavSpatialIndexを共有所有し、接続先IDを昇順・重複なしへ整理する。
NavConsoleの移動セッション・route cursorには依存しない。検証済み外部接続も含む。
NAV差し替え／無効化時は公開分布を直ちに取り下げ、次のframeで新revisionへ再構築する。

初期Areaは最終確認位置のXY包含と最大128 unitsの垂直差で選ぶ。NAVなしはnull、
対応Areaなしはavailable=false。位置は維持し、遠いAreaへ勝手に寄せない。
現在の非視認対象位置・実速度・動的障害物を照会せず、追加engine traceはない。

## 拡散と予算

元の視認時刻から200,000µs刻みで、半分を現在Area、残りを既知の有向接続先へ等分する。
出口なしは全量保持する。最大32本の昇順接続列と現在Area列をmergeし、同じ接続先への
寄与を完全に合算してから上位32件を残す。同weightは小さいArea IDを優先する。
除外した重みをunknownMassへ加え、既存unknownMassはそのまま持ち越す。
既知weight＋unknownMassの合計1を保ち、残った候補を再正規化しない。

- 接続への寄与を処理する回数は全体256/frame、1対象の巡回あたり最大8。
- 元の接続列の比較と重み合算は固定32候補以内。未完了stepの結果は公開しない。
- 対象を巡回するcursorを持ち越し、最大2048対象訪問/frame。初期Area queryは32/frame。
- 200msごとのstepを飛ばさず繰り越す。公開時刻と現在時刻の差を遅延として公開する。
- 再視認で初期Areaへ戻し、失効・死亡・世代・round・map・NAV変更で古いjobを退役する。
- x86固定サイズ: DistributionModel 1,761,384 bytes、WorldModel 890,104 bytes。
  NAV公開時だけ追加のoffset／target vectorを確保し、frame中は動的確保しない。
  最大100,000 Area／1,000,000 edgesの既存上限内で、補助配列の要素分は最大4,400,004 bytes。

## 検証

単体: 分岐、循環、一方通行、孤立、重み保存、同点切り詰め、逆順Area／接続のreplay一致、
999接続×16観測者の公平な繰り越し、途中結果非公開、NAVなし／Areaなし、再視認、
NAV差し替え、世代・round・map・時計巻き戻り、失効中job退役、不正公開値。
Fake-engine: StartFrame→WorldSnapshot、遮蔽後の位置保持、NAV退役時の即時取り下げ、
1／8／16観測者×8／16／100msの上限、既存移動の到着との共存。
対象テスト8/8 (1.33s)。初回失敗は差し替え用synthetic NAVのdangling edgeを修正した。

初回事前Windows portable50/50、Metamod63/63、Release PE32/6 exportsを確認。
Linuxのsign-conversionエラーはiterator差分型への明示変換で修正し、未処理jobを
最大遅延診断へ含める補強とともに全gateを再実行した。
最終事前gate: Windows portable50/50 (31.69s)、Metamod63/63 (55.04s)、
Linux portable49/49 (21.41s)。Release PE32/x86・指定6 exportsのみ。
main統合commit・統合後gateは続いて記録する。
Graphは対象関数を収録せずsource fallback、FocalSpanでNAV所有／query契約を確認した。
実機対応や敵の実際の移動分布への適合は未検証。P4-08・09は未完了、Finishは宣言しない。
