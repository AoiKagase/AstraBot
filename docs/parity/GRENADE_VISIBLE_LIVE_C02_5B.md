# C02-5b 全可視・自明経路tailの限定実機対照

2026-11-24。基点 `53d973427ec7f82e8ea07e32ef852f05f45be803`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。**製品変更なし。専用NAVと公開AMXX入力で全可視・自明経路／長短・reset履歴の実機対照を追加した。C02全体は未完。**

## 成立した入力と因果関係

証拠root: `artifacts/local/debug/csbot-visible-live-20261124/`。既存 `D:/SteamCMD/cstrike_rehlds` 一台を逐次使用。Bサイトground上の専用6area NAV、公開origin保持・flash装備・round restart／C4 dropだけを入力し、Astra内部の経路・cache・時刻・safe状態は書き換えていない。通常NAVでの自然発生や全mapの受入れではない。

| 試行 | 採録した経路／tail | 確認結果 |
|---|---|---|
| live-2-two-rounds 第1round | actor3、length5/next1、i=length。終点(-1695,2640,.03125)、slot5=(-1695,2680,.03125) | 0/25標本、36+10、4方向buffer後target=(-1677.96875,2665,46.03125)、full136でNative begin |
| 同 第2round | actor3、length4/next1、i=length。同じ終点、slot4=(-1695,2575,.03125) | 0/25/50標本、target=(-1677.96875,2590,46.03125)、full336でbegin。終点だけでは説明できない履歴差 |
| live-4-trivial-visible | 公開C4 weaponboxを同一area内へ置き視線を向ける。actor1、5点→2点、next1、i=length。slot2=(-1695,2640,.03125)を保持 | 自明経路終点(-1700,2580,.03125)から0/25/50標本、target=(-1677.96875,2629.827392578125,46.03125)、full172でbegin |
| live-3-trivial | C4 dropは成立したが視認入力不十分、6点Huntの全可視経路 | 自明経路の受入れには使わない。zero tailへの延長標本の最後が不可視となる別対照として保持 |

live-2の履歴書込み13回・長→短7回・nonzero cacheのreset1回を読取り採録し、書込み前cacheと直前prefix更新／未更新suffixをraw binary32で照合。経路長は2〜7。live-4も書込み2回、5→2短縮とnonzero reset1回が整合した。actor0で消去された終了時cacheは通常resetと区別した。2点経路は公開C4 drop後に得たもので、失敗したComputePathの履歴再現ではない。

`tools/replay_csbot_visible_live.py` は固定履歴から取り出した **無改変のFindGrenadeTossPathTargetとVector** に採録済み経路／tailを与え、各FVisible座標・各wall Traceの開始点と方向を照合してから実機の応答を返す。4入力すべてで標本要求・trace数・最終targetが一致（reference-final-2/3/4）。tailを同じ経路の終点に置換する誤モデルは `wall request start` でexit1（mutation-endpoint-only）。製品REDではなく、tail非依存仮説の反証である。NAV探索やGameDLL全体の同値性は主張しない。

## 製品・復元・検証

最終DLL `2215e8a1fa7ad305ac2efaf6c73ec7241cbb3e551dc288594d80d2cd2097d86f`、PDB `12363c7975cdf4da6c36ae3b43e574983d8c10294c967f8096f71f2e1ca1c956` を各試行で検証再配備。module-load後の読取りBPを使用した。live-2はHLDS46444/CDB4400、live-3はHLDS42864/CDB14968、live-4はHLDS46384/CDB43736、すべてexit0。各実PID終了後、core原文 `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b` と通常NAV `53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2` を復元、専用fixture/listを除去、server保護9を照合した。

今回の検証は専用fixtureコンパイル、指定MSVC Hostx86/x86で現LegacyNavReaderの6area/424bytes受理、採録式のoffline資格確認、最終DLL限定実機、固定関数再生である。製品ソース不変のため追加の全build／全CTestは実施せず、旧重点32/32・22/22を今回の実行結果とは数えない。全体gateは5群の大分類完了時に実施する。

## 失敗・修正を含む証拠

1. offline-probe.log: CDBの省略member式が解決できず大量反復。ユーザーが今回のPID7020/23380の停止を許可し、停止後exit1を保存。v2はscenario未指定、v3はC++参照型fromの読取り不能、v4でthis指定・float位置差・正しい局所スコープへ修正し資格確認した。
2. この中断で試験のExistingProfileGuardデストラクタが走らず、repoの保護対象BotProfile.dbが一時fixture内容になった。live-1-two-roundsは開始前指紋で拒否、server書込み0。既存preserved-inputsの88byte原文と開始時SHA `be5c4692f5dfc4e10888d47475b6b6fe4d6663fb9b2df348ccf86c3354a98b31` が完全一致することを確認して復元した。変更された344byteも保存（profile-recovery.json/interrupted-test-BotProfile.db）。以後のoffline試験は専用cwdで行う。
3. live-3のC4 dropだけでは自明経路が成立しなかった。live-4で公開weaponbox位置・視線を明示し成立。解析時の終了actor0消去と延長標本の最後の不可視を通常reset／全経路点の可視性から分離した。失敗試行を製品回帰と数えない。

## 残る境界

失敗生成の書込み済みslot（通常到達入力未確定）、NAV area削除通知→RemovePath、はしごsuffix／容量超過のnative対応、HE・進行後drop/梯子・着弾/跳ね返り、FetchBombの他の未採録範囲、V02を残す。live-4はNative beginまでの採録で、投射物着弾や待機後attack解放の追加受入れではない。通常NAV／全map／Linuxへの一般化、全体gateの完了を意味しない。大項目未完47、群別11/16/6/12/2は維持する。
