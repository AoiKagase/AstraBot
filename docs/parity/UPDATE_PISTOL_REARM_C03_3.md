# C03-3 通常Updateの敵未視認5秒超pistol再装備

2026-11-25。統合前基点 `59b4a9932a8c0bc2c23dfbf3666602f358897cfd`。残件正本は [TASKS.md](../../TASKS.md)。C03の部分対応であり、大項目未完47件（11/16/6/12/2）は維持する。

## 固定参照と実装範囲

固定ReGameDLL_CS `b0889847fe6d03898be88acc9e366660efb40ab5` の `cs_bot_update.cpp:547-551` は `!IsActiveWeaponReloading() && IsUsingPistol() && !IsPrimaryWeaponEmpty() && GetTimeSinceLastSawEnemy()>5`。roundのIsSafeとは独立で、直前の水没・unsafe再装備の後に評価される。primaryEmptyはprimary不在または総弾薬枯渇、HasAnyAmmoはclip非0または予備弾>0。lastSaw初期値は0、視認保持時計を使う。固定6blobは独立blobID照合のうえ `reference.json` / `reference-extra.json` と `pinned-*` に保存した。

通常full Updateに `updatePistolRearm` を接続。実active/reload/総弾薬は公開UpdateClientData/GetWeaponDataを優先し、取得不能時は既存CurWeapon/WeaponList/AmmoXへfallbackする。primaryのUnknownを使用可能としない。reload取得不能時は既存の公開reload送信receiptによる推定を使用するため、予測取得不能で未観測の外部reloadを完全観測したとは扱わない。

Actor/serial/frame/time/full/revision/normal/Attack/inventory/waterとlastSawのreceiptを保持し、helperのprediction/permission callback後もreload・primary弾薬・activeを再確認する。条件失効はstickyに旧Update中断へ伝播する。通常EquipBestWeaponの共通5秒時計を使用し、MUST_EQUIPを使わない。成功SelectItemでactiveが変わることは意図した結果なので、helper admissionに送信後phaseを渡し、送信前の資格と送信後のreceipt確定を区別した。既存unsafe callerの制約は保持した。

## オフライン検証

入力成立を確認した正規REDは `red-established`。状態初期化済みテストに対し、通常Update接続だけを基点のruntime.cppへ戻してbuildし、5.001秒のloaded pistolでselected0/expected28、exit1を採録した。候補はfinallyで原文復元した。

最終 `build-final/receipt.json` は指定VsDevCmd内のMSVC14.52.37025 HostX86/x86、NMake Debug、`build/windows-x86-c03-v1452`、exit0/source202前後安定。旧14.51 compiler/cache/製品は保持。`focused-final/results.json` は専用cwdで逐次29/29合格であり、全CTestではない。`equip-rearm` は24入力、shared装備時計4.999/5.0、実perception/reactionでlastSaw25秒を取得した4.999/5.0/5.125秒境界を含む。実視認後now29.999001と30.000000は切替0、30.125000はAK47へ1件。normal/special reload、primary空/不在/Unknown、予備弾だけ、予測実active優先、予測APIなしfallback、serial失効、allowance中grenade/reload/枯渇/Attack変更を検証した。

旧unsafeテストの敵未視認条件5入力は、この独立分岐の追加により正しくprimaryへ戻るため、固定参照547由来の期待値へ更新した。履歴baselineは再生成していない。

初回 `red-1` / `green-1` は状態未初期化でBuy経路を通る入力不成立、`matrix-1` は5.001起点のfloat丸めによる時計入力問題として保持。`matrix-2/3` とsight診断群は、時刻往復でroot schedulerのfull番号が進まず古いperception snapshotを再利用した入力不成立。直接可視判定FOV/LOS合格・snapshot敵不可視/full不変を採録し、手動behaviorテストのfull receiptを進めて解決した。誤ったgeometry仮説は撤回、診断コードと暫定workaroundを製品・最終テストに残していない。製品修正案は初回の1案であり、これら入力失敗を製品不具合や合格へ読み替えない。

## 最終候補の限定実機

同じDLL/PDBを既存 `D:/SteamCMD/cstrike_rehlds` 一台へ検証再配備。新serverコピー0、通常as_oilrig、Wolf/Crusher、公開ReAPIのloaded Glock clip20 reserve120・AK47 clip30 reserve90、公開乾燥位置保持/NOTARGET/他bot凍結を使用。Astra私有状態書込みなし、CDBなし、Debug CRTは子PATHだけに追加した。phase1は167実command中165更新primary、phase2は159中159更新primary。profileのold17→28 sent1はnow=25.157238, 32.142303。公開pistol弾薬が残った入力を確認し、unsafe枯渇分岐との混同を避けた。

この試行もquit後Steam BMainLoop stalled>15秒assertを記録し、110秒後に実HLDS14156が残存した。runner exit1とrestore-pendingを保存し、ユーザーがこの特定PIDの終了を明示許可した後、実行ファイル・creation時刻・専用起動引数を再照合して14156だけを終了した。実終了コード0xFFFFFFFF、正常終了は不合格。実PID[]確認後にcore原文復元・専用plugin/list除去、通常NAV/server保護11一致を確認した。CDBなしの試行でも発生した事実をV02へ記録し、原因は未解決とする。`live-final/launch.json` / `restore-pending.json` / `authorized-termination-before.json` / `authorized-termination-after.json` / `exit-after-stop.json` を保存した。qconsoleはbefore原文prefixを除き今試行の追記だけを検証し、旧製品のprofileを混ぜていない。即時get_user_weaponは遅延CurWeaponにより28を返すが、phase1実commandの17/clip20/reserve120と2回の実active old17→28を照合した。

DLL SHA256 `25e7508a8b45e5c52498734276acd59190bbc8c1000c37af2c6657490551d6ed`

PDB SHA256 `72d3163486b6b153a08a4f56f591da6456fb934473b874b97fa55c4c610df8b1`

## 証拠と残範囲

証拠root `artifacts/local/debug/csbot-update-rearm-20261125`。`candidate.json` / `functional-final.json` / `documentation-final.json`、統合後は同名の `*-merged.json` と `preservation-merged.json` を照合する。source202と最終製品の検証後に文書だけ更新した範囲を指紋で分離。独立reviewは `review.txt`、Critical0/Important0/Minor0。

early-safeのgrenade/knife選択は未実装。今回の通常戦闘自然発生、全状態/profile/blind、全武器、Linux実機/長時間、多人数、外部reload未観測の全網羅は未受入れ。両OS全build/全CTest、PE/ELF各7export、固定履歴比較と広範実機回帰は大分類完了時で、今回未実施。C03-1/2の未採録範囲とV02はTASKSに保持する。
