# C16 full移動の視線公開（2026-11-23）

Compatibility full移動がUpkeep後にpitch0/移動yawを直接公開する差を修正した。最終ソースのオフライン合格と、既存Windows実機での同一Actor・実ゲーム時刻598/598 full公開一致を根拠に限定完了とする。固定参照全軌跡、Combat視認/offset更新順、全owner/両OS実機の受入れは含めない。

## 修正と参照

基点は `c379dd96ab83e4df753441e9148c0f7690eec20e`。固定参照は `b0889847fe6d03898be88acc9e366660efb40ab5`。参照 `cs_bot_pathfind.cpp:1458` は次のAheadを保存し、`cs_bot_update.cpp:135–183` のUpkeepでdrift後に視線を更新する。通常MoveTo/Follow/EscapeFromBombもUpdatePathMovementを使う。

AstraのCompatibility移動draftとfull dispatchは現在のUpkeep視線を保持する。3状態を既存NAV視線経路へ接続し、次Upkeepへ新しいAheadを渡す。停止時はresetPathLook後に有限の旧yawだけを戻し、Hunt待機/梯子/解除のowner情報を残さない。共有ばねは保持する。Enhancedの直接移動視線、梯子のローカル入力、Combat/Guard/Use等の優先owner経路を維持する。

## オフライン検証

正規targetで、moving full公開、停止full公開、停止後cached Upkeep、Escapeの次目標不足をRED確認した。実RunPlayerMove/pitch・同更新のばね非再積分・次MoveTo/Escapeの数値漸化式・cached移動軸保持・停止時有限Ahead・Follow・Enhancedと既存梯子を確認。独立レビューのP2を修正し、最終全ビルド後Windows173/173、Linux172/172を逐次実行して合格。PE/ELF各7 exact exportsも合格した。

初回Windows全ビルドはテスト局所変数のshadow警告C4456で停止し、変数名を修正後に全ビルドを完了した。Linux採録helperのCRLF起動失敗は製品実行前のsetup失敗として除外した。

DLL SHA256: `456ccc43b61da6b5c02cd7e39cc9737fafae9ffc2c7fcf17031e078c66095610`

PDB SHA256: `3df98ca4c6a8042386eaa8f0760ffd70476bad5fca71d971090917f58c7c572f`

ELF SHA256: `2ed8fa239cabd03aadcf414b87ab3cf77a245a0f6aa97f2fe6da7ff53a07529b`

製品198ファイルの全パス集合と指紋を再確認し、C01からの変更はcommand実装1本、他197本は不変。元の全ソース監査指紋は履歴として保持した。全分岐や一般GameDLL全行の一致を保証する監査ではない。保護対象12ファイルも元の指紋と一致した。

## 履歴差とRNG

旧baselineと既存35補正は保持した。両OSとも151999→151933行。移動1127件の順序を保持し、279件は視線のみ、53件は視線/移動軸/方向bitsが変化。actor/msec/roll/up/非方向bitsは不変。profile537/client157/生RNG510/非trace call147935は順序・内容が完全一致する。RNG消費差はない。

traceObjectiveVisibilityは1733→1667。視野coneでLOS前に候補が除外されるため、x100/224側からx60側へplayer probeの呼出が移り、ground3本はXYが変わる。単に66本を削除した補正ではない。独立照合後、各OS477ブロックをoffset/前後内容/入力出力SHA固定で36番目の補正へ追加し、再適用一致と全CTestを確認した。150987行は補正ブロック外でそのまま保護する。角度/traceの汎用maskやbaseline再生成は行わない。

## 実機受入れの範囲

既存 `D:/SteamCMD/cstrike_rehlds` へ最終DLL/PDBのみをバックアップ付きで再配備した。元の設定/GameDLL/NAV/profile指紋を維持し、サーバーディレクトリのコピーは0。実HLDS0/CDB0を確認後にHLDS22516とCDB24384を起動し、module-load後にBPを設定した。初回2000stepは数値誤差最大1.52587890625e-5、実RPP2009一致。ただし開始markerが直前の共有時刻だったため、full公開の受入れはこの採録で完了としなかった。

同じHLDS22516へCDB37076を再接続した。最終PDBの実layoutとロード済みmoduleを確認し、globalvars.timeをfull開始と実RunPlayerMove双方で直接読んだ。598開始markerすべてが同一Actor/世代・実時刻で最終共有pitch/yawと一致し、除外/未完markerは0。別途2000共有stepは数値誤差最大1.52587890625e-5、計算エラー0。priority0/1/3/5を観測した。

広い比較の1996実RPPは共有角と一致。89件はすべて過去時刻の同じpriority5共有値と後続時刻のRPPを比べたもので、同時刻不一致0、full移動marker内0。owner/call chainの直接receiptがないためC08原因とは断定しない。共有値を採録していない703RPPと一時的なゼロ区間は計数対象外。ゼロ区間の生存/freeze原因は断定しない。同一PIDで後続stepが再開した。CDBは両方exit0でdetachし、HLDSは同じPIDで継続、最終ロードDLL SHAとソース/ユーザーファイルの不変を再確認した。

## 残件と証拠

C16はfull移動公開の限定完了。cold/post-combat停止で有限Aheadがない初期化は正規RED/固定参照比較が未確認で、V02の明示確認ケースとして残した。C08、V01/V02等を閉じない。49歴史IDからC01/C16を除いて47残件。次はC02（手榴弾開始/投擲経路）。

証拠は `artifacts/local/debug/csbot-full-look-publication-c16-20261123/`。`final/freeze.json`、両OS/exportログ、`history-final-review.md`、`history-correction-summary.json`、`live2/full-publication-analysis.json`、`live2/analysis-current.json`、`live-review.md`、`loaded-modules-final.json`を参照する。旧/失敗証拠は保持。対象パスのみcommitし、local masterへFFした最終指紋は同rootの`post-merge.json`へ記録する。pushなし。
