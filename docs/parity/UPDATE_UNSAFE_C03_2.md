# C03-2 通常Updateのunsafe装備再選択（2026-11-25）

通常full Updateのnative grenade・水没装備処理の後へ、unsafe時の総弾薬枯渇/現武器なしと、爆弾退避外のknifeからの `EquipBestWeapon()` を接続した。固定参照はReGameDLL_CS `b0889847fe6d03898be88acc9e366660efb40ab5` の `cs_bot_update.cpp:533–543`、`game_shared/bot/bot.cpp:232–245`、`cs_bot_weapon.cpp:411–494`。`artifacts/local/debug/csbot-update-unsafe-20261125/reference.json` に固定4blobのGit blob IDと保存bytesのSHA256を保持する。

現武器は公開UpdateClientDataを優先し、取得不能時だけCurWeaponへ戻す。総弾薬枯渇はGetWeaponDataと公開予測弾薬、またはWeaponList/CurWeapon/AmmoXの既知情報で判定する。clip0でも予備弾があれば枯渇ではなく、clipが負の非弾倉アイテムも枯渇にはしない。UnknownをEmptyへ変換しない。HE/flash/smokeは除外し、knifeの爆弾退避は維持する。通常要求のため既存共通5秒時計を通す。safe5秒後のpistol→primaryとearly-safe選択は今回の対象外であり、C03の残件として保持する。

Actor/edict serial/frame/time/full/revision/normal/Attack/inventory/water/unsafeの条件がcallback後に変われば旧更新を中断する。独立レビューI1では、装備helper内のpermission callbackが現武器をgrenadeへ変更してもprimary要求を送る経路を正規製品REDで確認した。任意のcaller admissionをhelperへ渡し、初期activeと送信直前のfresh active/公開fallbackに適用する。失効はstickyに保持して旧full Updateのscenario続行も中断する。選択送信後は元active入力でsnapshotを照合し、意図した現武器変更は許容する。既存callerの動作を追加制約で変えない。

## オフライン証拠

証拠rootは `artifacts/local/debug/csbot-update-unsafe-20261125`。`red-1` は安全側8対照の後にunsafe弾切れpistolがselected0/expected28でexit1。`red-review-i1` は同actor/frame/inventoryのallowance callbackでactive17→4に変わり、selected28/expected0でexit1。失敗試行は削除していない。

最終 `build-final` は重点EXEとDLL/PDBをMSVC14.52.37025 HostX86/x86、NMake Debugの `build/windows-x86-c03-v1452` でbuildし、exit0。対象source202ファイルのbuild前後指紋が一致する。旧14.51 cache/旧DLL/PDBは保持した。`candidate.json` と `final/` は最終ソースと製品を固定する。最終DLL SHA256は `f7a90ed1d89f38d6a157b2b7df9862a3caf8218fe142417e69a9ea125a8ae3dc`、PDBは `db944aba61d6b2e70f2288cb1fe8189ca93553f62520fa26b22358b8fd2f5779`。

`focused-final` の28シナリオは専用cwd・逐次で全てexit0。unsafeは17入力と4.999/5.0秒境界を含む。safe直前/strict境界、clip0＋予備あり、clip正、爆弾退避中knife、grenade、active/弾薬読取り中serial変更、Unknown、予測APIなしでの公開fallback2対照、I1 callback変更、pistol/primary総枯渇、knife、既知active0、爆弾退避中の総枯渇を対照した。既存水没/Plant/Combat/共有視線/native grenadeも含む。

全build/全CTest、両OS、PE/ELF各7exports、固定履歴・広範回帰は実施していない。重点合格を全体合格として扱わない。新しいRNG要求は追加していない。

## 最終候補の限定実機機能と終了不合格

同じ最終DLL/PDBを既存一台 `D:/SteamCMD/cstrike_rehlds` に検証再配備した。新規サーバーコピー0、push/通常配布なし。`live-1-final` は通常as_oilrig BSP/NAV、公開 `bot_add_t Wolf`（既存Elite profile100）とCrusherを使用し、NOTARGET/被害抑止/他bot凍結/乾燥位置保持の制御条件で検証した。公開ReAPIでAK47とGlockを与え、Glockをclip1で装備してからclip0/reserve0にする。7秒後に公開knife選択を要求する。Astra私有状態の書込みなし、CDBはpassive・製品BP0。DebugCRTは子PATHだけに指定した。

準備直後のAMXX `get_user_weapon` は旧28で、次のCurWeaponとcommandで実17を確認した。AMXX時刻25.067346にphase1 weapon17、30.164718にweapon28。phase2は32.100585にweapon29、35.203662にweapon28。profile時計の選択は `now=30.183771 old=17 selected=28` と `now=35.185230 old=29 selected=28`、両方sent1で、同じprofile時計内の差は5.001459秒。profileとAMXXの時計を混ぜた遅延比較は行わない。`UN_COMPLETE phase=2 samples1=188 switched1=50 samples2=182 switched2=102` を採録した。自然戦闘の受入れを主張するものではない。

quit後にSteam `CSteamEngine::BMainLoop` の15秒待機assertが出た。110秒時点でも実HLDS37588/CDB6520が残り、runnerはexit1で `restore-pending.json` を保存した。正常終了は未受入れであり、過去V02と同じ原因とは断定しない。ユーザーの「閉じました」後にfresh実PID[]を確認した。監視session81282は既に失われ、終了コードreceiptもないため、HLDS/CDBの終了コードは未取得/nullと記録しexit0を推測しない。末尾にはSteam pipes fatal assert/application exitingも残る。エージェントの強制終了操作は0回。実PID消滅後にcore原文復元・専用plugin/list除去・通常NAV/server保護11の指紋一致を確認した（`user-close-observation.json` / `exit-after-stop.json`）。`functional-final.json` はsource202/製品/28件/固定4blob/限定機能/復元を照合し、normal_exit_gate=falseを保持する。統合後の正本は `functional-merged.json` / `documentation-merged.json` / `preservation-merged.json`。

## 残件

1. quit後Steam待機の原因切分けと正常終了の再受入れ（V02）。今回の機能採録を正常終了合格へ読み替えない。
2. unsafe再装備の通常戦闘自然発生、全状態/profile/blind、Linux/長時間実機と正常終了受入れ。
3. C03のsafe5秒pistol→primary、early-safe grenade/knife、水没の未採録範囲。大項目未完47（群11/16/6/12/2）を維持する。残件正本はTASKS.md。
