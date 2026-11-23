# C02-2a 手榴弾視線と破壊物接触

基点 `af1f06a4d496f968056fe58d100f7adc51cfcefb`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。C02の部分修正単位。C02全体とC10は閉じない。

参照 `cs_bot.cpp:285–325` は破壊可能物にHIGHのSetLookAtを要求する。UNINTERRUPTABLEの手榴弾spotには拒否されるが、後続のEquipBestWeaponまたはPrimaryAttackは実行する。手榴弾のPrimary入力をTouchで解除しない。

公開Touchの受付時に、同期済みnative grenade spotの有無を基準として障害物視線の受付結果を保存する。装備中/投擲待機中だけに限定しない。拒否されても接触の攻撃・装備要求を保持し、視線・共有spring・移動再投影を変更しない。Clear後も拒否された古い要求を復活させず、新たなTouchがあれば受付を再判定する。新しい手榴弾spotの開始に成功した場合だけ旧障害物の受付を失効させる。待機中のbegin試行は新規受付を無効化しない。

装備許可CVarのcallback中にround/Actor/状態が再入する場合は旧選択を停止し、古い5秒時計を公開しない。正常状態の世代だけではAttackの開始・終了を検出できなかったため、Attack世代と非Attack状態も確認する。C10に属する既存のローカル装備時計、遅延Touch処理時刻、C4/USE例外の是正は未実施。

## オフライン検証

正規CMake target/CTestで視線拒否と保持入力のRED→GREEN。round再入もRED→GREEN。最終のAttack callbackは通常世代・最終overlay状態を変えずAttack世代だけを進め、旧Float照会/装備dispatchのREDを再現した。局所ガード後GREEN。新規 `astrabot_native_grenade_admission` は、実装の同型テストだけでなく公開material/Touch、実Root full更新、装備要求、Clear後の新規Touchを対照にする。既存障害物回帰の「手榴弾は常にattack0」という旧期待も参照に合わせて保持へ修正した。

最終全build後、Windows175/175＋PE7、Linux174/174＋ELF7を逐次確認。重点4件も合格。読取りレビューで重要指摘なし。src/include全200追跡パス（製品C/C++198本）・保護ユーザーファイル12件を指紋照合。前回確定C02-1との両OS履歴は各151933行、順序付きRNG510行を含め差0。固定baseline/既存補正は変更していない。この履歴には新しい接触対照が含まれず、差0を全RNG受付の一致へ拡張しない。

| 製品 | SHA-256 |
|---|---|
| DLL | `6d662b9aee0a06e325bce8a35f28d899d0a7dad3d921888cebee47ea4c354704` |
| PDB | `e7459cf3c27df81dfccb9fc462a1d937d551e21e3ddf48435062fcd84885717e` |
| ELF | `6ef670f69cc56aaad52a77d5f7e0b93550ff4cd40c199c8d5a77fc6d23bd6db0` |

## 実機の限定受入れ

既存 `D:/SteamCMD/cstrike_rehlds` に最終DLL/PDBをバックアップ付きで再配備した。追加サーバーコピー0。DLL module-load後にBPを設定し、CDBは読取りのみ。公開fixtureは役割/装備/姿勢を用意し、投擲開始採録後に既存マップのfunc_breakableをtriggerとして実接触させる。攻撃入力や製品状態は注入しない。モデルpitchは `-viewPitch/3` で公開viewと整合させ、未整合の25度視線を維持する制御試験である。

受入れ試行 `admission-1`：Actor index 0、実破壊物entity 33、接触開始 11.079221秒。CDBでnative spotあり・実接触pendingあり・障害物視線admitted=falseを3標本確認。接触直後のPrimary保持3入力と接触由来の装備dispatch1件を確認した。自然な全視線軌跡やC10の時刻一致を受入れたものではない。C4回帰0/7/8も同じ最終品で逐次実施した。

共有視線の独立採録も、native spotと実接触を観測した後の3 stepで障害物priority7が0件だった（`admission-1/shared-look-proof.json`）。視線拒否が内部フラグだけでなく、共有stepの呼出しにも反映された限定対照である。

各試行でHLDS実PID終了0とCDB終了0を確認し、core.iniをraw復元して次を起動した。C4 case0/7の起動待ちはコンソールの「選択」モードが原因で、対象PIDとタイトルを照合してEscapeだけで解除し、同一プロセスで続行した。強制終了は行っていない。旧証拠・旧製品・元設定を保持。証拠正本は `artifacts/local/debug/csbot-grenade-obstacle-admission-c02-2a-20261123/` のfreeze、各acceptance/exit、履歴差分、最終レビュー。

## 次の残件

1. C02-2b/c：noise、Hunt/Guard、stationary0/moving approachの受付・Clear・RNG順。
2. C02-3：安全時間後のnative経路投擲。all-visible分岐と点列の境界を参照から確定する。
3. C10：共通EquipBest時計・Touch処理順/期限、既存C4/USE例外。
4. C08/V01/V02：照準所有、全参照リプレイ、Linux実機・多数Bot継続性能。

49歴史項目／47残件を維持する。pushなし。ユーザー編集と未追跡資料を保持し、対象パスのみをコミットする。
