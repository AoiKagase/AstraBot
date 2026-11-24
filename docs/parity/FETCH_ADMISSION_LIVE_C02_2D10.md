# C02-2d10 FetchBomb移動Native受付・静止候補ゼロの限定実機

2026-11-24。基点 `6558a0f75b1b3c7251a6da28a903d8206a870325`。製品ソース変更なし。C02の部分検証単位であり、大項目未完47件は維持する。

## 採録した範囲

固定参照 `ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5` の `FetchBombState::OnUpdate` は経路準備後に `UpdateLookAround`、次に経路移動を行う。`UpdateLookAround` は移動中のLOW視線受付をinterval抽選前に確認し、静止候補ゼロではinterval抽選後に無条件Clearする。現製品の対応箇所は `bot_behavior_guard.cpp:282` と `:345` 以降である。

既存一台 `D:/SteamCMD/cstrike_rehlds` へ最新DLL/PDBを検証再配備し、専用6area NAVと公開AMXX入力（敵露出、HE→rifle、C4 drop/拾得、origin/velocity）を用いた。内部Astra状態への書込みはない。NAVはBサイトの直線6area、approachゼロ、内部areaの方向付きencounterと専用spot42を持つ合成入力である。spotは遠方の遮蔽位置を与えており、通常NAVや自然発生の受入れへ拡張しない。

最終 `live-8-final` はoriginも40 unit/sで連続移動し、途中で静止、再び同じ移動入力へ戻す。公開入力を保持したまま、module-load後の資格済み読取りBPで次を確認した。

| 条件 | 最終採録 |
|---|---|
| 移動中Native保持 | FetchBomb state6、encounter/middleあり、既存LOWなし、Noiseなし、選択期限到達、unsafe、非blindを満たす13更新で受付/interval/候補/spot採録0。期限は未設定0を保持 |
| 同じencounterの厳密対照 | 上記のうちfull216–221の6更新は、後の正例と同一encounter/middle。全条件を同slot/時刻/state/fullで結合し、Nativeだけが受付を拒否する条件を確認 |
| 静止候補ゼロ | actor3/full262、時刻34.202560425、Native1/count0。interval1.540952682を抽選し、同fullでNative0、期限35.743515015へ更新。rifle装備後のwaitingは0であり、pin保持の受入れではない |
| 再移動の正例 | 同actor/full289、時刻37.739574432。同encounter/middle、Native0で受付→interval24.415054321→候補1/progress1→spot index0を正順採録。次更新は既存LOWあり。次選択期限62.154628754を採録したが、実機のskill全条件・全時計境界を網羅した主張はしない |

先行 `live-7-continuous` も他条件が開いたNative保持10更新、静止full210でClear、解除後full233にinterval→候補1→spot0を採録した。最終とのfixtureバイナリは同一。最終8試行の統合検証は [acceptance-final.json](../../artifacts/local/debug/csbot-fetch-admission-20261124/acceptance-final.json)、実行した検証器は [verify-final.py](../../artifacts/local/debug/csbot-fetch-admission-20261124/verify-final.py)。このreceiptの `passed` は明記した機能・復元照合を意味し、`normal_exit_gate` はfalseである。

## 静止ゼロの正常終了付き対照

同じ最新製品の `live-1-causal`（full263）と `live-2-disjoint`（full268）でも、静止Native1/count0→同full Clear0と1–2秒の期限更新を確認した。この2試行はHLDS/CDBともexit0、通常NAV/core復元・保護指紋一致。証拠は [acceptance-static.json](../../artifacts/local/debug/csbot-fetch-admission-20261124/acceptance-static.json)。ただし移動中encounterは無く、移動抽選抑止の対照には数えない。

## 入力不成立・診断・終了失敗

初期fixtureはoriginを固定したまま速度だけを50とし、その後y2520→2580へ跳ばしたため、移動encounterを得られなかった。実速度監視はorigin変位を使う（`locomotion.cpp:247`）。静止履歴、Stuckと回復時の再startが影響するという独立ソースレビューを受け、実originの連続移動へ変更した。live7/full172のstart stackは `recoverStuckUpdate` からの直接再startを実証する。live6の初回原因すべてを確定した証拠ではない。診断タグ `FA_STUCK` は行417の読点名であり、Stuck成立を表さない。

live3はT側C4提供者不在、live4は観測actorが受付点へ到達せず、live5は接続途中のチーム変更で生存参加条件が不成立。各試行は保持し、製品REDや機能合格へ数えない。live6の通常役割選択とlive7/8の連続移動へ戻し、以後の接続途中チーム変更は廃止した。v1/v2のCDB参照型・重複ローカル名の読取り失敗も保持し、v4/v9で全8読点とblind期限を専用cwdの正規EXEで資格確認した。moving wait/whichの相対位置はlive7の実DLL `dv /v` でも独立照合した。verifier初回のdv区間切出し失敗も別記録し、製品失敗とはしない。

| 試行 | 実HLDS / CDB | 終了 |
|---|---|---|
| live1 / live2 | 17556 / 29796、10392 / 47024 | 各両exit0。静止ゼロ限定 |
| live3 / live4 / live5 | 44600 / 15644、17652 / 45368、35184 / 44208 | 各両exit0。入力不成立・診断 |
| live6 | 33588 / 37520 | quit後Steam待機、各PIDのユーザー許可後に個別終了、両0xFFFFFFFF |
| live7 | 7852 / 45644 | 同上。機能採録のみ |
| live8 | 39564 / 37884 | 最小BPでも同上。最終機能採録のみ |

live6の非侵入stackはmain threadのSteam/tier0 mutex待機を示すが、記号不足から製品原因を断定しない。診断BP削減で正常終了するという前提は成立せず、追加HLDSは起動していない。3件の終了不合格はV02へ残す。各実PID終了後、通常NAV SHA `53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2` とcore.ini SHA `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b` を復元し、専用fixture/list除去・サーバー保護9件一致・実PIDゼロを確認した。サーバーコピー追加0。

## 指紋と未検証範囲

| 製品 | SHA256 |
|---|---|
| DLL | `2215e8a1fa7ad305ac2efaf6c73ec7241cbb3e551dc288594d80d2cd2097d86f` |
| PDB | `12363c7975cdf4da6c36ae3b43e574983d8c10294c967f8096f71f2e1ca1c956` |

`preservation-before.json` のソース200件・製品2件・保護10件・検証入力3件に結び付けて照合した。今回は専用fixtureのコンパイルと正規EXEによるBP資格確認、限定実機・保存済みログ検証を実行。製品build、全CTest、両OS、exports、固定履歴、広範回帰は実行していない。旧重点合格を今回の実行とは数えない。

通常NAVでの自然発生、FetchBomb保持Noise、移動候補ゼロ、safe/blind/期限等の全境界、他map、V02正常終了は未受入れ。失敗tail・RemovePath・梯子suffix/容量・HE/drop/着弾等のC02残件もTASKS.mdへ保持する。
