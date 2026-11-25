# C03-5 Huntのwell-past-safe grenade MUST装備

2026-11-25。基点 `68726e1da37c30be0003b5f7bb987c23e23da4c2`。[残件正本](../../TASKS.md)。C03は部分対応、大項目未完47件（11/16/6/12/2）。

## 固定参照と実装

固定ReGameDLL_CS `b0889847fe6d03898be88acc9e366660efb40ab5` のcs_bot_update.cpp:526-529は、水没処理の後、unsafe再装備とpistol再装備の前に、IsHunting && IsWellPastSafe && IsUsingGrenadeでEquipBestWeapon(MUST_EQUIP)を呼ぶ。cs_bot_statemachine.cpp:425のIsHuntingはnormal stateのみを比較し、保持HuntのAttack重複も対象。cs_bot.cpp:818の境界はelapsed round > 1.25 * safeTime。調整済みsafeTimeを使い、roundFreezeUntilではなくroundFreezeStartTimeからの経過を用いる。固定4blobをreference.jsonへ保存し、reviewでも独立blobID一致を確認。

updateHuntEquipmentを通常full Updateの水没後へ接続。公開predictionの実activeを優先し、取得不能時はCurWeaponを使用。HE/flash/smokeを対象に既存equipBestWeapon(true, admission)を使用する。ammo/allowance/水没制限/共通時計はhelperの既存規則を保持し、MUSTで5秒待ちを迂回、受け付けた非knifeの装備要求は時計を更新する。送信前にfresh activeとcaller条件を再確認し、Actor/serial/frame/time/full/revision/normal/Attack/inventory/water失効時は旧fullを中断。送信後の意図したactive切替は許容する。新しい乱数抽選は追加していない。

## オフライン検証

red-1は初期化済みHuntの通常Update、25.000999秒・HE装備・使用可能AK47でselected0 expected28、exit1。最小実装のgreen-1は同じ入力でselected28、exit0。

matrix-1の公開fallbackはprediction無効時の非装備AK47弾薬入力が不足し、Unknownのため選択0だった。製品を変えず、公開inactive CurWeaponでclip30を補ったmatrix-2は25入力すべてexit0。旧試行保持。境界24.999/25/25.001、aggression0の18.75/18.751、round start4で29/29.001、3grenade、Idle除外、Attack保持、真のfallback、active/ammo queryのserial差、allowanceのactive/Attack/inventory/state差、primary/pistol/knife/Unknown、MUST直前の実装備時計、command再入、送信後active不変の時計を確認。成功後4.999/5秒の通常pistol再装備も検証。

build-finalは指定VsDevCmd内MSVC14.52.37025 HostX86/x86、NMake Debug、build/windows-x86-c03-v1452、exit0/source202前後安定。focused-finalは専用cwd逐次31/31、最終実行体一致。旧14.51 cache/製品保持。review.txtはCritical0/Important0/Minor0。初回非escalated Python起動不可、変更なしbuild-matrixを保持し、build-matrix-2/3を実検証に使用した。

## 最終候補の限定実機

最終DLLの同じ既存as_oilrigで公開HE入力→AK47を132/132実commandで保持。dry確認、equipBest old4→28 sent1、実HLDS31604 exit0・実PID[]・core/fixture復元・保護11一致。

既存一台D:/SteamCMD/cstrike_rehlds、通常as_oilrig、非VIP Wolfへ公開ReAPIでloaded AK47/HEを付与。実prepare39.024471でHE deploy受付、39.132915でold4→28、release39.843250時点で既にAK47。凍結解除後のHE保持を観測したとは主張しない。解除後は132/132実commandでAK47保持。乾燥位置保持/NOTARGET/被害抑止、他bot凍結、所持金0。Astra私有状態書込み、CDB、強制終了、新serverコピーは0。Debug CRTは子PATHだけ。grenadeは水没禁止武器でなく、unsafe/pistol/Idle callerもこのold4→28を代替しない。自然な通常戦闘の受入れとは区別する。

DLL SHA256 `6d6c2019de8cf9b69bbf068ef6095895a993dabb187c197b52c1bad19e36ecea`

PDB SHA256 `2a0b02a59a9781bc52a289f54681d1d074a0df26f658865f2696aba1d3d743fb`

## 証拠と残範囲

artifacts/local/debug/csbot-hunt-equip-20261125。baseline/candidate/reference、red-1/green-1/matrix-1/2、build-*/focused-final/final、live-1のlaunch/exit/stdout/stderr/qconsole原文・fixture、functional-final/reviewを保持。qconsoleはbefore原文prefixを除いた今回追記だけを評価。check-liveの保存先/符号付きinventory/切替時刻仮定/Windows改行変換による採録チェック失敗はcheck-live-failures.txtへ記録し、rawバイト照合で解消。製品失敗や実機再試行へ読み替えない。統合後はfunctional-merged/documentation-merged/preservation-mergedを照合する。

複数grenadeのearly-safe所持head、既存C03未採録範囲、今回callerの自然発生・全profile/blind/武器/長時間/多人数/Linux実機は残る。両OS全build/全CTest、各7export、固定履歴・広範実機は大分類完了時で今回未実施。既存V02の終了待機/message79等の原因は未確定で閉じない。
