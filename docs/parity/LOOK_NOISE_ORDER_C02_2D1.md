# C02-2d1 新規Noise処理をHunt/Guardの視線選択より先に行う

2026-11-24。実装・下記検証済み。`438842e` でコミット・ローカルmaster FF済み。検証時原文と製品・保護ファイルの指紋を後確認済み。C02全体は部分対応、全体台帳は49項目／47未完のまま。

## 参照と変更

固定参照は ReGameDLL_CS `b0889847fe6d03898be88acc9e366660efb40ab5`。`cs_bot_vision.cpp:403-446` は新規Noiseを処理すると、上位の視線に要求を拒否されても後続の視線選択へ進まない。`cs_bot_hide.cpp:287-292` の到達済みHideはLookAroundの結果と独立してしゃがむ。

旧adapterはNoise要求より前にHunt/Guardが低優先度の視線選択を行い、不要な間隔抽選やClearを発生させていた。Noiseの「処理済み」と「視線採用」を分離し、Hunt/Guardの選択前に判定する。同fullの判定をreceiptで保持し、Actor・entity・生死・map/round・実行revision・normal/attack epoch・Clearの失効や時刻巻戻りで旧処理を中止する。完了済みの同一実行が前進した場合は再抽選せず、独立した視線期限切れ後の通常処理を許す。

Guardのcallback再入は `ActionProposal::executionAborted` をRootの中立・移動コマンド生成まで伝え、旧RunPlayerMoveを送らない。正常なHideのしゃがみは視線目標の有無と独立して維持する。

## オフライン検証

- 正規RED→GREEN：新規銃声より前の不要抽選、Guard再入後の旧draft送信、同fullで失効した準備の再利用、Noise処理済み時のしゃがみ欠落。重点14/14合格。
- 両OSの全buildはexit 0。Windows全CTest初回は182/184合格。新memberの所有権台帳登録漏れと履歴照合の2件を修正し、該当2件＋補正checkerを3/3で再検証した。製品を再ビルドせず、同一製品で失敗を解消した。単一回の184/184合格とは表記しない。
- Linux全CTestは183/183合格。PE/ELFはそれぞれx86・正確な7exportを独立確認した。
- 両OSの履歴151,933行のうち6行だけ `actor=1 buttons=0→4`。これは既存fixtureの無武装HideがNAV待機地点へ到着した後の6更新であり、新規Noise入力の6件ではない。視線・移動量・msec・順序付きRNG 510行は不変。元baselineを保持し、SHAと完全一致行・6offsetに限定した `csbot-hide-noise-crouch-correction.json` を追加した。
- 独立レビューは製品ソースと履歴差の両方で重大所見なし。文書更新と外部のignore追記は別receiptに記録し、製品ソース200件・検証入力12件・保護対象12件と最終製品を指紋で結んだ。

## 既存Windows実機

`D:/SteamCMD/cstrike_rehlds` を再利用し、最終DLL/PDBを再配備した。サーバーコピー追加0。既存の公開team setup・AK47 PrimaryAttack fixture、元de_dust2 NAVを使用した。実機BPは同じ最終ソースの正規テストで式を事前確認し、DLL module-load後に設定した読取り採録だけを用いた。

`noise-order-1` で実際の弾倉消費を伴う銃声と、Native grenade視線保持中のactor slot 2／full 90を確認。ゲーム時刻12.7958078の音に対し12.8127155の更新で、Startle RNG（log945）→処理済み判定（946）→Hunt打切り（947）の順に進んだ。同fullのRNGは音位置x/yとStartleの3件で、LookAround間隔・候補の抽選は0件。

HLDS PID 42184・CDB PID 25376はともにexit 0、残プロセス0。core.iniは原文SHA `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b` へ復元済み。

| 製品 | SHA256 |
|---|---|
| DLL | `28be73725165d2015156ab8608ce21ab4b831ca39ac6e5c058558b1ed4181e3f` |
| PDB | `0ca2e82f189e17e4f9ce394388d8adb511f7e2496a5df0b8ce4a8e7e4ea1679a` |
| SO | `9d3e1133fee789c20fde1211c5ef64c0e13caeb1ec7d2c9b9e73287a76f5cb5d` |

証拠：[`final/freeze.json`](../../artifacts/local/debug/csbot-look-noise-order-20261124/final/freeze.json)、[`noise-order-1/acceptance.json`](../../artifacts/local/debug/csbot-look-noise-order-20261124/noise-order-1/acceptance.json)。失敗ログ・履歴差・採録プローブ・実PID receiptも同ディレクトリに保持する。

## 残る範囲

- 保持NoiseのHunt受付は後続[C02-2d2](RETAINED_NOISE_ADMISSION_C02_2D2.md)で限定対応。Guard側のactive抑止は[C02-2d3](GUARD_LOOK_ADMISSION_C02_2D3.md)で抽選後受付へ修正。実機moving/zero条件は残る。
- GuardのNative受付・ゼロClear・Ahead再開はC02-2d3で重点検証、Native受付のみ実機限定確認。Guard抽選時計・保持Noise/zero実機と全ownerの退役は残る。
- 他状態の後段Noise処理の再入停止、FetchBomb実機、直接の再帰Root full更新。
- 安全時間終了後の経路corner投擲、通常Noiseのmoving/zero・Guardの実機、Linux実機（通常Noiseの静止保持条件はC02-2d2で限定受入れ）。
- 今回の広範なC4 0/7/8再実行は未実施。ユーザーの検証簡略化指示により大分類完了時へ回す。以前の別製品の受入れを今回の製品へ流用しない。
