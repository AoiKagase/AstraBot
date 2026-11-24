# C03-1 通常Updateの水没MUST_EQUIP（2026-11-25）

通常full Updateの手榴弾処理直後に、skill > .4・waterlevel == 3・現武器の水中発射禁止（または公開予測で現武器なし）の一般 `EquipBestWeapon(MUST_EQUIP)` を接続した。固定参照はReGameDLL_CS `b0889847fe6d03898be88acc9e366660efb40ab5` の `cs_bot_update.cpp:518–523`、`game_shared/bot/bot.h:334–340`、`cs_bot_weapon.cpp:411–494`。保存3blobのbytesと固定commit:pathのGit blob IDを照合済み（`reference.json`）。

公開UpdateClientDataの現武器を優先し、取得不能時だけCurWeaponへ戻る。WeaponListのNOFIREUNDERWATERを使い、未受信時は既存標準ID5/14/15/21の定義へ戻る。callback後のActor/edict serial/frame/time/full/revision/normal/Attack/inventory/water条件が変われば旧更新を中断する。既存の主武器→拳銃→knife、弾薬Unknown保留、5秒共通時計を使用し、水没要求だけmustで時計を越える。Enhancedの分岐は追加しない。

独立レビューI1で、実active0・knifeのみ所持でも古いCurWeapon29がknife選択を抑止する境界を発見した。通常Update経路でselected0/expected29の製品REDを採録し、既存equipBestWeaponのcurrentWeaponにも再検証済みの公開active（0も含む）を優先した。時計・弾薬方針は変更しない。初回指摘と解決判定を `review.txt` に保持する。

## オフライン

証拠rootは `artifacts/local/debug/csbot-update-equipment-20261125`。`red-2` はskill40/water3とskill41/water2が選択なし、skill41/water3がselected0/expected17で失敗。最小修正後は拳銃1件となった。`red-review-i1` は上記knife-onlyの失敗。最終 `focused-final` は27シナリオすべてexit0（各専用cwd、逐次）。水没7入力はskill40/41、水深2/3、実予測17対古いCurWeapon28、取得中serial差替え、禁止28、実active0＋使用可能拳銃、実active0＋knifeのみ。正例後5秒以内の再要求も選択される。既存best equipment・Plant末尾/進入・Combat・共有視線・Native grenadeの重点回帰を含む。

指定VsDevCmdの現在の実clはMSVC14.52.37025 HostX86/x86、旧 `build/windows-x86-23a1-clean` cacheの14.51.36231は実体がない。旧cache・旧DLL/PDBは変更せず、独立 `build/windows-x86-c03-v1452` NMake Debugで対象EXEと製品をbuildした。最終build（exec78205）はexit0。所有runtime.cppのCRLF→LFのみの正規化を `owned-eol.json` に記録し、その後両対象を再build、I1修正後の最終build/27件へ結び付けた。`candidate.json` はsource200＋CMake/test2、保護10、検証入力3と製品を固定する。検証入力のCMake/test2は今回の意図的変更で、他入力は保持。

全build/全CTest、両OS、PE/ELF各7export、固定履歴比較・広範回帰は今回実施していない。重点EXE27件を全CTest合格とは扱わない。新DLLのPE32 x86だけを確認した。新しいRNG要求は追加していない。

## 最終限定実機

最終DLL SHA256 `db52d1f94dc6bc0af1abd0acc47602508e9a70b9477a8cfef2e5d40a6621ba96`、PDB `1d517754b346fade532d8c0276864ad9a8e2dcd41c6beaf623a6048b3b0adf2f` を既存一台 `D:/SteamCMD/cstrike_rehlds` に検証再配備。新規サーバーコピー0、強制終了0、push/通常配布なし。実在する14.52 DebugCRTを子PATHだけへ指定し、CRTファイルはコピーしない。

`live-3-wolf` は公開 `bot_add_t Wolf`（既存BotProfile.dbのElite、skill100）とCTのCrusherを使用。quota0/kickと作成は別更新に分けた。公開rg_give_item/rg_switch_weaponでXM1014とGlockを準備し、弾薬は使用可能。通常 `as_oilrig` BSP/NAVの水域 `(500,-600,-1012)` へ公開SetOriginで位置を保持し、waterlevelは書き換えずエンジンの実計算を読む。ほかのbotはFL_FROZEN/NOTARGET、対象はNOTARGET・被害抑止、実機受入れはこの制御条件に限る。Astra私有状態の書込みなし。CDBはpassiveで製品BP0、値を強制しない。

乾燥phase1にweapon5を確認。phase2のAMXX時刻26.087984はwater3/weapon5、26.116579はwater3/weapon17。実水没のcommand更新132件、うちGlock保持131件。既存profileログは `equipBest slot=1 now=26.180380 old=5 selected=17 sent=1 primary_ammo=2 pistol_ammo=2`。profileのnowとAMXX時刻を同一時計の順序・遅延として数値比較しない。BSP/NAVは専用品に差替えていないが、自然遊泳による発生や全mapの受入れは主張しない。

実HLDS4588/CDB28880は各exit0。`UE_COMPLETE phase=2 samples=132 wet=132 switched=131`、Server shutdown、製品/読取りエラーなしを確認した。実PID終了後、core原文復元、専用plugin/list除去、通常NAVとserver保護11の指紋一致。`verify.py` のfresh `acceptance-final.json` はsource/products/保護/27件/固定blob/実PID[]を照合したもの。統合後の正本は `acceptance-merged.json` と `preservation-merged.json`。

## 除外試行と残件

1. `live-1-final`：OrinはFair skill25。water3/weapon5の133件は高skill正例の入力不成立。終了時AMXX内アクセス違反、HLDS0xc0000354/CDB0x8007001e。実PID消滅後に復元一致。即時server_execを除いた後続と区別し、過去V02の原因とは同一視しない。
2. `live-2-wolf`：同更新のquota0処理が新規Wolf/Crusherを退場させ、UE_FAIL no_actor。両実exit0/復元一致だが機能受入れ除外。v3で公開コマンドの処理間隔を修正した。製品は全3試行同一。
3. C03全体は未完。unsafe時の全弾薬枯渇/knife、敵非接触5秒のpistol→primary、early-safeのgrenade/knifeは残す。最初の項目は空弾倉だけでなく固定参照の総弾薬枯渇を区別する。
4. 水没の自然遊泳、全profile/閾値/blind/reentrant分岐、Linux実機、継続性能、旧14.51 build環境、通常起動時DebugCRT探索、過去V02の原因は未解決。大項目未完47（群11/16/6/12/2）を維持し、C03の部分修正1件を完了とする。残件正本はTASKS.md。
