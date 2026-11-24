# C02-5 成功経路の全可視tail履歴

2026-11-24。基点 `ed4d36b2c731c67c7077dba96185716e9e8acca8`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。**成功経路の履歴を限定実装。C02・tail全体は未完。**

## 実装範囲

`NavRoamController::rememberRoute`へ集約される成功経路の位置列を、bot個体の256slotへ記録する。初期zero、短縮時は先頭の有効範囲だけ上書きする。reset/DestroyPath/roundは保持し、作成/除去のresetForNewActor、actor generation/map変更は消去する。getterは経路を進めず、現在length直後のslotをコピーする。

投擲側は全点可視ならこのtailへ25単位で標本化し、既存36+10の高さ・4方向bufferへ接続する。slot256は公開しない。256点を超えるAstra経路はnative切詰めと対応未確定なので、新botまでtailを拒否する。はしご完了後のlocomotion suffixもtailを拒否する。RNG要求・固定baselineは変更しない。

固定RemovePathはcs_bot.cpp:905、OnDestroyNavDataNotify(NAV_NOTIFY_DESTROY_AREA):979から呼ばれる。はしごsuffix短縮と同一視しない。保存本文は今回証拠rootのpinned-cs-bot.cppとC02-4のreference-final/pinned-functions.inc。S14未実装のNAV削除通知へ架空の入口は追加していない。

## オフライン証拠

証拠root: `artifacts/local/debug/csbot-tail-history-20261124/`。

| 証拠 | 結果 |
|---|---|
| red-2-test.log → green-1-test.log | 正規Rootで全可視の一律拒否RED→初期zero tailへの投擲GREEN |
| green-3-test.log | 長→短、DestroyPath/reset/round保持、新bot/generation消去の6対照と実Root再利用tail。重点9/9 |
| capacity-red-test.log → green-4-test.log | 255/256/257点・短縮後の誤tail再有効化RED→GREEN |
| stale-red-test.log → focused-2-test.log | レビューで旧map StaleFrame拒否前のslot消去を発見。受付判定後へ移してRED→GREEN |
| focused-2-build.log / focused-2-test.log | 指定MSVC Hostx86/x86でDLLとcombat/controller/native_ladder/nav_query/grenade_trajectoryをbuild。関連/構造契約32/32、6.84秒 |

最終EXEのoffline-final-probe.logではlength2/next1、旧slot2=(305,32,0)、標本化後(280,32,46)、+X壁buffer後target=(250,32,46)。最終PDBで読取り式を資格確認してから実機へ持ち込んだ。

失敗試行も保持。red-build.logはsandboxのC1902で、同じVS環境のsandbox外red-2-build.logは成功。テスト編集2回の文脈不一致は製品変更前、現物インデントで解消。green-2のcontroller1失敗は世代のない空topologyという入力不成立。capacity-red/stale-redは製品の動作RED。sandbox PythonのNo installed Python foundと解析前verifyのanalysis.json欠落は、実環境で解析→検証の順に解消した。

## 最終実機と指紋

最終freeze: `final/freeze.json`。DLL `2215e8a1fa7ad305ac2efaf6c73ec7241cbb3e551dc288594d80d2cd2097d86f`、PDB `12363c7975cdf4da6c36ae3b43e574983d8c10294c967f8096f71f2e1ca1c956`。

既存D:/SteamCMD/cstrike_rehldsの1台を再利用し、この候補と既存corner-fixture-4を検証用再配備。module-load後の読取りBP、通常NAV・moving Hunt・公開flash・自然safe16.5秒でactor2/full157に開始、25full保持後full182で実RunPlayerMove attackを解放した。経路31点/next6、不可視i10、公開tail zero。**全可視条件は成立せず、tail投擲の実機受入れではない。** 既存cornerの限定回帰として扱う。

正本acceptance.json、生ログ/解析live-1-final/debugger.log・analysis.json・exit.json。HLDS40848/CDB19780ともexit0。独立verifyで実PID0、core原文SHA e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2bへ復元、専用fixture/list除去、通常設定/NAV等9・repo保護10・製品2・ソース200・検証入力3を照合。コピー追加0、強制終了0、push/通常配布なし。

## 残る境界

1. ComputePathPositionsが欠落梯子等でfalseになる前に書いたground slotの履歴。現行cacheは成功経路のみで、失敗履歴のparityは未実装。DestroyPathは書込み済みposを保持する。
2. NAV area削除通知→RemovePathのmemmove/新tail消去、はしごsuffixのnative全経路/index、容量超過時native切詰め。
3. 全可視/自明経路とreset/rebuild履歴の実機因果対照。今回の不可視cornerで代替しない。
4. 進行後drop/梯子・HE・着弾/跳ね返り、FetchBomb未採録、V02過去の正常終了不合格。
5. 両OS全build/全CTest・各7exports・固定履歴・広範実機/多人数は大分類完了時。未完47件、群別11/16/6/12/2を維持する。
