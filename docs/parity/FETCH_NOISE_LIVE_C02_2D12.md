# C02-2d12 FetchBomb保持Noiseの限定実機

2026-11-25。基点 `5cae5dd9737597204cd7b474327e084dd08ccd31`。製品ソース変更なし。C02の部分受入れであり、未完大項目47件を維持する。

## 入力と固定参照

既存一台 `D:/SteamCMD/cstrike_rehlds` に同一最終DLL/PDBを検証再配備。C02-2d10 v6公開fixtureを基に、HEを除き、敵はFL_NOTARGETを保持したままrifle装備とC4 drop/拾得、敵音源の通常RunPlayerMove足音4pulseを入力した。phase2の敵配置とFB_OPEN/CLOSEログ自体は残る。origin/velocity/viewを補正するcontrolled fixtureであり自然対戦ではない。Astra私有状態への書込みなし。最終2試行は同じv3 fixture binaryを使用し、6area NAVの各approach listだけを0件から1件（here6/previous5/south/next0）へ変更。area・接続・hiding spot・encounterは同一。検証器が両NAVを構造比較する。

固定 `ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5` の `cs_bot_vision.cpp:445–585`、`cs_bot_listen.cpp:155`を保存・Git blob照合した。新規Noiseのhandled returnと、その後の保持視線を区別する。固定は静止期限後にinterval→approach抽選→LOW受付、移動時はLOW以上の視線でencounter抽選前に戻る。現ソース対応は `bot_behavior_guard.cpp:259–380`。

## 限定結果

| 専用NAV / actor1 | full / 時刻 | 結果 |
|---|---|---|
| 候補0 / 移動保持 | 299からの16更新、初回37.637191772 | Noise=NotHandled/active1・Native0、encounter/middleあり、LOW0、期限到達・safe経過。同fullの受付/interval/spot読点なし |
| 候補0 / 静止保持 | 269 / 34.079242706 | interval→count0→Clear読点、次fullでNoise0・更新期限35.092056274を保持 |
| 候補1 / 静止保持 | 272 / 34.075222015 | interval→count1→LOW判定→拒否、Noise1/Native0、期限35.664218903 |
| 候補1 / 静止保持 | 286 / 35.742595673 | 同上、期限37.181076050 |

保持静止の期限前は候補0で13更新、候補1で22更新あり、各同fullに静止抽選/Clear/LOW判定読点なし。候補0のClear採録点はnoise.clearLook呼出し直前であり、Noise0は次fullで照合した。静止候補1は固定ソース順にinterval/approach抽選を通過してLOW拒否へ到達し、更新後の期限がnow+採録waitと一致する。全RNG要求を網羅した採録とは主張しない。

移動16更新は採録した受付条件と期限を照合した範囲。fixtureにflash入力はないがblind内部は独立採録していないため、全条件の単独因果分離とは数えない。両試行の同じRNG列・同時刻、2回の移動まで連続した実移動、通常NAVでの自然発生も主張しない。

最終live3 HLDS18004/CDB46704、live4 HLDS11108/CDB29240は各実exit0。phase5/C4回収・Server shutdown、実プロセスゼロ、通常NAV/core原文復元、専用fixture/list除去、保護9件一致を確認した。CRTはC02-2d11と同じ既存VC145 DebugCRTを子PATHだけに指定。強制終了0、サーバーコピー追加0、pushなし。通常探索先のDebugCRT欠落・過去Steam待機原因はV02へ残す。

## 資格・失敗・指紋

読取りCDBは資格済み8読点を継承し、Noise準備後/LOW判定/拒否後の3読点を追加。既存EXEの専用cwd3条件は各exit0、構文/メモリ読取りエラー0。最終DLLでも対応する値・採録順序・期限和を照合し、読取りエラー0を確認。製品build/CTest全実行、両OS/export、固定履歴、広範回帰は今回行っていない。

初回live1はNative視線との競合で保持Noise不成立、live2はfixtureの重複AK47取得からrg_switch_weaponの無効entity errorでphase3停止。両試行とも実HLDS/CDB exit0・復元一致だが受入れから除外。v3で重複取得を除去し、近接100単位の自動Clearを避ける320単位の音源を用いた。失敗ログを保存し製品REDとは数えない。

証拠root：`artifacts/local/debug/csbot-fetch-noise-20261125/`。`baseline.json`、`reference.json`、`qualification.json`、各liveのlaunch/exit/debugger.log/input.nav、`failures.txt`、`verify.py`、`acceptance-final.json`を参照。検証器は保存ログを同actor/state6/full/時刻で結合し、NAV差・期限和・exit・復元・source200/products2/protected10/verification3を照合する。

DLL SHA256 `2215e8a1fa7ad305ac2efaf6c73ec7241cbb3e551dc288594d80d2cd2097d86f`、PDB SHA256 `12363c7975cdf4da6c36ae3b43e574983d8c10294c967f8096f71f2e1ca1c956`。

残件正本はTASKS.md。保持Noiseの通常NAV自然発生、blind/skill/safe/期限の全境界、全map/Linux実機/継続性能、失敗tail/RemovePath/梯子suffix/容量、HE/drop/着弾とV02旧原因は未完。
