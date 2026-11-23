# C02-1 手榴弾保持と認識敵への投擲

基点 `a4b132c0e6d714e86787f2da6b8dfda4230d80d8`。固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。C02全体の完了ではなく、保持・認識敵への投擲・明示Clearを実装した修正単位。低優先度視線の受付/RNGと経路向け開始は未完として残す。

## 変更と参照

公開GameDLL `UpdateClientData.m_iId` で現装備を確認し、既知0を古いCurWeaponで覆さない。Full Updateでは手榴弾装備中にPrimaryAttackを保持する。認識敵には現在originをコピーしたlook spotを作り、投擲待機中はstrict `now > deadline` またはUpkeepで積分前の実視線がstrict 1度未満になったとき解放する。通常Attack獲得や弾道/着弾検証は開始条件にしない。非手榴弾への変更では待機だけを解除し、lookの期限は独立に保持する。同一点は各軸strict10未満、照準到達後の期限はstrict3秒。

参照は `cs_bot_update.cpp:339–350,475–516,143–174`、`cs_bot_weapon.cpp:576–586`、`cs_bot_vision.cpp:333–363`、`player.cpp:9415`、`bot_util.h:73`。共有lookの4番でUpkeepを積分し、保持Aheadは変更しない。Defuse/Hide/Plant/Use退出のClearを、診断sinkに依存しない単調receiptとして公開し、callback前に進める。受信時はspotだけを解除し、投擲待機・タイマー・共有角速度を維持する。再入後の古いpending公開はActor/frame/full/revision/epochとClear receiptで拒む。

## 検証と製品同一性

候補1の実機では、fullの保持入力をlightの旧Combat後処理が消す差を検出した（`grenade-3`）。実full更新がない対照で正規テストもREDとなり、Combat Upkeepのprimary公開だけを抑える修正後GREEN。最新fullが手榴弾装備を確認した区間では、その保持/解放入力をlightが上書きしない。照準の有効性検査・退役・RNGは維持し、盲目・死亡・Enhancedには適用しない。旧tickのreceipt保持は意図的で、actor/round/resetでは解除する。候補1の全件合格を最終ソースへ流用していない。

Windows x86全build後174/174＋PE7、Linux x86全build後173/173＋ELF7。CTestは同時実行していない。最終ソース/検証入力は `artifacts/local/debug/csbot-grenade-c02-20261123/final-v2/freeze.json`。198製品C/C++を含むsrc/include全200追跡パスとユーザーファイル12件を照合した。

| 製品 | SHA-256 |
|---|---|
| DLL | `77b92327d98d50e57825aac3e3e77856c4d09dbd03241b23a755d98e7b678c1b` |
| PDB | `f671ede38bac89308093bca9a35863fa86968fbd6becf81f8cf3d59c8c1341da` |
| ELF | `545a996362c77dfa4ea12a617c96bed9eb86185893728f26f1f8c86540841a2c` |

正規CMake targetで、敵不在保持・GameDLL既知0と古いCurWeapon・実grenadeと古いrifleのAttack差をRED→GREEN。ClearLook試験は当初、旧Combat aimの負のpitchをgrenade Upkeepと誤認して未結線でも通った。この結果は除外し、読取りCDBで未整合を確認した。敵喪失後に2full更新して旧aimを退役させた対照では、実際に早期解放のREDを再現し、receipt結線後GREENになった。タイマー維持・同一点・武器変更・厳密境界はcoreでも確認した。最初のWindows全件実行 `windows-final.log` は173/174で、新ActorState3件の所有権台帳未登録だけが失敗した。台帳登録後の両OS全buildと直列CTestを最終合格に使い、旧ログは保持した。

直前C16との両OS履歴は、それぞれ151933行で差0、順序付きRNG 510行も一致。固定baselineと既存補正は再生成していない。今回の正規追加fixtureにだけ手榴弾の正例があり、履歴差0を経路投擲や全RNG一致の証拠に拡大しない。

## 実機の限定受入れ

既存 `D:/SteamCMD/cstrike_rehlds` に最終DLL/PDBを再配備した。サーバーディレクトリの追加コピー0、元設定と既存ファイル指紋を保持。一時プラグインは公開setup操作で手榴弾装備・敵の公開/退避・姿勢を用意し、攻撃ボタンや製品の待機状態を注入しない。CDBは対象DLLロード後にBPを設定し、読取りのみ。各試行はquitで終了し、実HLDS/CDBの0台を確認して次を開始した。

敵不在で38個の実入力保持を観測。投擲開始 `10.793934`、敵退避 `10.933939`、期限 `13.793934`、最初の実解放 `13.873867`。姿勢を未整合に保った対照なので、実機ではstrict期限による喪失後解放を受け入れる。自然旋回による早期整合解放・全look priorityの実機一致はこの試験の主張に含めない。C4回帰0/7/8は同じ最終製品・配備先で設置/zone退出/実C4喪失を採録した。詳細は各試行の `acceptance.json` と生ログ。

## 残件

最終製品の試行4では敵の実RunPlayerMoveで座標が変化した。解析は配置時の名目座標との一致を要求せず、slot-1で確定した実ActorのNative begin入力、そのraw値を保持した23 Upkeep標本、実入力を対応付けた。試行5は短い公開中に敵を認識せず除外した。case8初回も全BotがCTでT役不在のため除外し、公開team変更/respawnによる検証役準備後のc4-8-balancedを受入れた。製品を変更して合格へ寄せた試行ではない。試験中だけ作成したrootの自動ロード一覧2件は終了後に非ini拡張子へ変更し、通常起動時に試験プラグインを読まないことを確認した。専用小設定と証拠を保持し、追加サーバーコピーは0。


実機試行1/2は、AMXXの既定設定と`plugins-*.ini`自動ロードがC4試験を同時実行し装備を上書きしたため除外した。CLI localinfoだけでは起動時のcore.ini設定を切り替えられるという仮定を棄却した。受入れ試行はcore.iniの2パスだけをrawバックアップ付きで専用小設定へ一時切替し、実PID終了後に完全復元している。元のplugin一覧・map設定は編集していない。試行1の通常quit後のSteamClient終了例外も保持し、後続はエンジンの`-nobreakpad`を指定して実HLDS終了0を直接確認した。CDBの終了コードだけで実HLDS終了0とは判定しない。遅延認識が敵退避後に届いた場合の投擲先は、参照同様、その時点の敵の現在originである。

1. C02-2：低優先度SetLookAtの拒否とRNG消費順、stationary0 approachの無条件Clear、moving encounter抑止、破壊物の装備選択を維持した照準拒否。
2. C02-3：安全時間終了後の経路投擲。固定参照のall-visible分岐はlogical path長の直後を読む。sentinelは存在しないため、終点へ黙って丸めて一致とは扱わない。通常/自明/ladderを含むnative path点列の公開が前提。
3. C08：cached trackedAimとnative fullで保持されるIsAimingAtEnemyの区別。手榴弾だから常にCombatを無効化する修正は行わない。
4. V01/V02：参照側の全RNG/軌跡、Linux実機、人間の照準/旋回、長時間多数Botは未受入れ。

現在49歴史項目／47残件。ユーザー編集・未追跡資料・旧証拠を保持し、対象パスだけをstageする。pushと実環境配布は行わない。
