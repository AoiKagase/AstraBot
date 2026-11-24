# C02-3 安全時間終了時の経路corner投擲

対象：固定参照 ReGameDLL_CS `b0889847fe6d03898be88acc9e366660efb40ab5`。
継続目標と残件の正本は `TASKS.md`。C02全体の完了ではない。

## 実装範囲

`cs_bot_update.cpp:490` の前回safe→今回unsafeを一度だけ受け付ける。認識敵への投擲が先で、待機中は重複開始しない。HEはさらにstrictなwellPastSafeを必要とし、普通のsafe境界でHEを拒否した後に再試行しない。safe履歴は通常状態をまたいで保持し、life/round resetで初期化する。状態入口は処理順だけを担い、計算は専用の`rememberNativeSafeTime`へ分離した。

現行の経路snapshotを読取り取得し、Huntの既存waypoint/indexを使う。その他の経路では同じnative portal、dropの上段30・下段追加25、梯子の方向付き32、endpointを再構築する。非Huntの終点は「前回移動が確定したendpoint phase」を公開し、今fullの移動を先読みしない。経路revision、NAV revision、actor/frame/full/state epoch、engine trace関数をcallbackの前後で照合する。

`cs_bot_weapon.cpp:590` の定義済みcornerでは、先頭から目線で見える最遠nodeを探し、最初の不可視nodeまで25単位で標本化する。標本はHalfHumanHeight36、最終点に10を加算する。成分除算後に`t*dir`を計算し、斜め経路の1ULPも対照化した。+X/-X/+Y/-Yの順で999.9のtraceを行い、近接壁から50のbufferを取る。可視traceは0x101、buffer traceは0x100。既存の独立look/3秒toss時計とRunPlayerMove入力を使い、RNG仕様・固定baselineを変更しない。

## オフライン証拠

証拠ルート：`artifacts/local/debug/csbot-corner-20261124/`。

| 証拠 | 結果・限定範囲 |
|---|---|
| `offline/red-2-test.log` → `green-1-test.log` | 正規Root、実際の経路生成、敵なしのcorner欠落RED→GREEN |
| `endpoint-red-test.log` → `endpoint-green-test.log` | 正規controller更新でportal→endpoint進行を再現し、古いportal indexの残留を修正 |
| `diagonal-red-test.log` → `focused-4-test.log` | 斜め経路のfloat計算順序差RED→GREEN |
| `focused-5-build.log`・`focused-5-test.log` | 指定MSVC Hostx86/x86、最終DLLと関連targets build成功、関連29/29、2.86秒 |
| `contracts-test.log` → `contracts-2-test.log` | 初回7/9。新safe履歴と既存共有時計の所有台帳漏れ、状態入口行数を修正。閾値を変更せず9/9、2.73秒 |

新規Root対照7件は、遅延HE、普通のHE境界拒否/再試行なし、flash境界、先頭不可視、全可視境界、round再入、斜め計算。controller対照はinactive/途中portal/終点phase/破棄後を確認する。round再入は旧corner処理を中断し、現行世代の中立入力1回だけを送る。全CTestではなく重点検証である。

初回`red-build.log`のC1902はsandboxのPDBマネージャー環境失敗で、同一sourceを指定環境のsandbox外で再buildした`red-2-build.log`は成功。追加fixtureのsafe境界誤消費、再入時no-sendという誤前提、signed比較警告、変数名隠蔽警告の試行も個別ログに保持する。製品の挙動失敗と混同しない。読取りreviewはCritical0/Important1（終点index）で、正規RED→GREEN修正済み。

## 最終実機

最終候補は`final-2/freeze.json`。DLL `7beae0c18beeb570cd657ba361d4b21b80d16b0355c378a7e63da8d2e80c8e50`、PDB `d3ea98678965b17f9b70ae5fd7bd283db43c7f49f836971d6fce17ec554d30ad`。6つの読取り式は`offline-final-probe.log`で最終正規EXEに資格確認し、実機はmodule-load後に設定する。

最終受入れの正本は`acceptance-final.json`、生ログは`live-5-final/debugger.log`、機械採録は`analysis-final.json`と`exit.json`。通常NAVのmoving Huntで公開flashを装備し、敵は公開NOTARGETで露出を抑える。bot_stop、専用NAV、Astra私有状態の書込みは使わない。

actor1/full135のelapsed15.872037秒からfull136の16.012341秒へsafe16秒を跨ぎ、current path20点/next10を読む。20.381042秒にtarget(-382.358948,1620.186279,-81.968750)でcorner開始。primary/waitingを22 full更新保持した後、lookがTowardsのまま3秒を過ぎたfull158/23.455002秒でprimary/waitingを0へ落とした。最初の実RunPlayerMoveは保持時buttons529（attack=1）、解放時buttons16（attack=0）。これは入力経路の限定機能合格であり、投射物の着弾・跳ね返りの受入れではない。

HLDS11112とCDB40156はともにexit0。独立`verify-final.py`で全対象実プロセス0、core原文SHA `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b`への復元、一時fixture/list除去、通常設定/NAV等9項目の保護指紋、最終ソース/検証入力/製品の不変を確認した。既存サーバー1台を再利用し、サーバーコピー追加0、今回の強制終了0。

先行`live-4-corner/analysis-final.json`は構造整理前の別source/productに対する限定機能合格。通常NAVのmoving Hunt、公開flash装備、自然safe16秒の境界を使い、actor1/full135 elapsed15.937618→136 elapsed16.077930で経路23点/next3を採録。20.444599秒にcornerを開始、22full保持後のfull158/23.480059秒で実RunPlayerMove buttons9→8。HLDS42864/CDB6692はexit0・core原文復元・保護一致。ただし最終DLLの合格に流用しない。

`live-1-corner`はTが1人しかいないのに非C4所持者を要求した入力不成立。`live-2/3-corner`は`bot_stop`でsafe履歴と経路を同時保持できるというfixture前提が誤り、`prepareMovement`のmovementUnavailable→ResetRouteで経路0だった。各試行はexit0・設定復元済み。製品corner不具合と扱わない。初期2回のCDB参照記法失敗も保持し、未資格式を実機へ持ち込んでいない。

## 残る差・未検証

固定参照は全node可視の場合に`m_path[m_pathLength]`の未使用tailを読む。今回の実装は全可視を拒否し、tailの内容を推定しない。この境界安全化を完全parityとは扱わずC02へ残す。全可視/自明経路の参照挙動を別途監査する必要がある。

進行後のdrop上下段・梯子index、HE corner実機、着弾/跳ね返り、全経路、Linux実機、長時間多人数は未受入れ。FetchBomb移動encounter/候補ゼロ等の未採録範囲も残す。今回の正常終了結果で、過去にSteam待機で失敗したV02を閉じない。

Linux・両OS全build/全CTest・PE/ELF各7exports・固定履歴比較・広範実機回帰は、大分類完了時に実施する。push・通常配布・追加サーバー作成は行わない。
