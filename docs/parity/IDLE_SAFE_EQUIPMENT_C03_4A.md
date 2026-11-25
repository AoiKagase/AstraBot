# C03-4a Idle early-safeの一意grenade/knife選択

2026-11-25。統合前基点 `e01b07e88c66664e4261a88c8989277fdbb63ab3`。正本は[TASKS.md](../../TASKS.md)。C03は部分対応、未完大項目47件（11/16/6/12/2）を維持する。

## 固定参照と範囲

固定ReGameDLL_CS `b0889847fe6d03898be88acc9e366660efb40ab5` の `states/cs_bot_idle.cpp:51-78` は、NAV確保とbot_zombie拒否後のIdle更新でIsSafeならEquipGrenade、falseならskill厳密>.33でEquipKnifeを実行する。通常Update全状態の分岐ではない。`cs_bot_weapon.cpp:340,485,505` は所持inventoryでIsSniperを判定し、この拒否がactive grenade保持判定に先行する。noSmoke既定falseでsmokeも許容する。両helperはEquipBestWeaponの共通5秒時計を読み書きせず、bot_allow_*も参照しない。固定4blobは独立blobID照合しreference.json/pinned-*に保存した。

通常fullの水没/unsafe/未視認pistol再装備の後、scenarioのtask割当と直接Plant開始前へIdle限定のupdateIdleSafeEquipmentを接続した。NAV既知areaまたは現在feetのcontaining area、zombie、IsSafe、blind、Idle/Attackの受付を確認。公開UpdateClientDataによる実activeを優先し、取得不能時はCurWeaponへfallbackする。Actor/serial/frame/time/full/revision/normal/Attack/inventory/NAV revisionをcallback後に再確認し、失効時は旧fullを中断する。SelectItemの意図したactive変更は失効としない。送信拒否時もgrenadeからknifeへ独自fallbackしない。

実装範囲は所持種類が一意のHE/flash/smokeとknife。複数種類のgrenadeのlinked-list先頭は公開weapon maskだけでは復元できないためID順を仮定しない。非sniperでactiveがgrenadeなら保持し、複数grenadeかつactiveが非grenadeなら選択を保留する。この未対応はC03に残す。sniperの場合は参照どおりgrenade選択を拒否し、knife分岐を評価する。

## オフライン検証

red-1は初期化済みIdleの実full更新。NAVなしは選択0、NAV読込み後HE期待4に対し選択0でexit1。製品修正前の成立REDである。green-1は22入力exit0、ただしfallback名の入力がpredicted0（既知active不在）だったため、matrix-1ではpfnUpdateClientDataを一時nullにして真の取得不能fallbackを確認した。旧試行は保持した。

最終build-finalは指定VsDevCmd内のMSVC14.52.37025 HostX86/x86、NMake Debug、build/windows-x86-c03-v1452。exit0/source202の前後指紋一致。旧14.51 cache/製品は保持。focused-finalは専用cwd逐次30/30合格で全CTestではない。equip-safeの28入力は各grenade、skill33/34、4sniper、safe19.999001/20境界、NAV/Idle/Hunt/Attack/zombie、真の公開fallback、serial/Attack再入、knife不在、送信拒否を含む。水没MUSTのpistol装備直後のknifeで時計を読まないこと、HE選択後5.125秒のpistol再装備で時計を開始しないことを確認した。command callbackでAttack開始後に旧Idle taskを実行しないことも確認した。

独立review.txtはCritical0/Important0/Minor0。初回の製品案1つ、暫定診断コードなし。固定履歴baselineは再生成していない。

## 最終候補の限定実機

最終DLLの同一既存as_oilrigで非VIP Wolfを公開入力し、HEは184実command中184更新、knifeは178中178更新で選択を確認。各equipSafe old28→4/29 sent1を照合、両実exit0・PID[]・core/fixture復元/保護11一致。

既存一台D:/SteamCMD/cstrike_rehlds・通常as_oilrig、非VIP Wolfのloaded AK47からHE/knifeを別試行で採録した。公開ReAPIで所持品を準備し、乾燥位置保持/NOTARGET/被害抑止、他bot凍結を使用。knife正例は所持金0でBuyによる後続HE購入を止めた。Astra私有状態を書き換えていない。CDBなし、Debug CRTは子PATHだけ。最終製品のlive採録と正常終了/復元の判定はfunctional-final.jsonに分離記録した。

初期試行は全て保持した。live-1はVIP CrusherへのAK47付与が失敗してrg_switch_weapon Invalid entityでfixtureが中断し、後続Round_Startも入力を変えた。live-2は名前付きbot生成前の初期Round_Startを拾った入力不成立。live-3は入力準備前にNew message started when msg79 has not been sent yetで実HLDS28568が0xFFFFFFFF終了した（エージェント強制終了なし）。原因は未確定でV02へ残す。live-5-knifeは準備後のBuyがHEを追加し、正しくgrenade優先となった入力不成立。live-1/2/5は実exit0、live-3も実PID[]確認後にcore/fixtureを復元、全試行で保護11一致。これらを製品合格へ読み替えず、最終HE live-4とknife live-6の限定正例から分離した。

DLL SHA256 `9d303f6651ec169bfb9dcc4be1fee672ae8bb8ec5e399c1cabeb470d16396d4b`

PDB SHA256 `cdbfeda55d3d392327fa0aab8d333933d8531fadbd17fc62a114689e476ca9b5`

## 証拠と残範囲

証拠root artifacts/local/debug/csbot-idle-safe-20261125。baseline/candidate/reference、red-1/build-final/focused-final/final、live-*とreview.txtを保存。functional-final/documentation-final、統合後functional-merged/documentation-merged/preservation-mergedを照合する。製品build後の文書のみ更新はソース指紋と分離する。

複数grenade順序、通常NAVでの自然装備/通常戦闘、全profile/blind/境界/Linux/長時間/多人数、C03-1/2/3の未採録範囲は残る。両OS全build/全CTest、各7export、固定履歴、広範実機回帰は大分類完了時で今回未実施。今回の部分合格でC03全体やV02を閉じない。
