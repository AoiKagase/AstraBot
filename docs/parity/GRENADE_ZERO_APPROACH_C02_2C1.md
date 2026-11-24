# C02-2c1 静止・接近候補ゼロ時の手榴弾視線解除

状態：オフライン・controlled NAV実機の限定受入れ合格。C02全体と自然NAVでの全挙動一致は未完。基点 `f58be94a3ed3105b0205c36ca0471a8046d4d22a`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。

## 参照と変更

参照 `cs_bot_vision.cpp:470–495` は静止時に間隔RNGを消費し、接近候補がゼロなら無条件にClearLookAtする。手榴弾の優先度も例外にしない。adapterの同分岐でnative grenade spotを解除する。ピン待機・投擲期限・共有ばねは変更しない。RNG callback後の古い更新が時計や解除を公開しないよう、既存Actor/frame/revision/normal/route/NAV/config検査にAttack世代とfull更新番号を加えた。全helper callbackの即時中断を保証する変更ではない。

## RED/GREENとオフライン

正規 `astrabot_native_grenade_zero_approach` は実Root更新・実NAV読込み・公開Touchで、同Hunt世代、間隔RNG1回、候補RNG0回、Primary保持を対照にする。旧製品RED（`zero-red-canonical-1.log`）の後、clear追加でGREEN。`astrabot_native_grenade_zero_reentry` は間隔RNG中の正規Attack開始・終了で通常状態と最終overlayを同じに保ち、Attack世代が変わった旧更新の時計公開を検出する。再入RED後、ガード追加でfocused2/2 GREEN。同時刻full再入そのものの直接対照は未実行であり、実行済みとは主張しない。

初期試行はNAVなし開始後の移動ウォームアップ2回が不足し、別試行にはNAV経路と接触箱の非交差によるプローブ拒否があった。旧RED/GREENを製品証拠から除外して保持し、現在位置を含む接触箱と正常full2回のcanonicalケースを採用した。診断CDBの初回追加式には改行エスケープ誤りがあり、実機に使わず保存し、修正版でオフラインの正例を確認してから実機へ移した。

両OS全target build終了0。共有fixtureのためCTestをWindows→Linuxの順に実行し、Windows178/178＋PE7、Linux177/177＋ELF7合格。既存履歴は両OSとも前単位C02-2bと151933行・順序付きRNG510行を含め差分0。固定baselineは再生成していない。この既存履歴に新zero fixtureは含まれず、差分0を全lookaround順序の一致へ拡張しない。今回のclear自体はRNGを追加せず、間隔抽選→候補0→Clearの既存順序を保つ。

Linux履歴補助処理に旧Temp shell参照が残り、新規出力先が見つからず停止した。旧Temp出力と変更していない前単位の圧縮原証拠は完全一致を確認した。専用新規パスへ修正後に再採録して差分0、経緯とSHAは `linux-history-path-incident.json` に保存した。

Windows正規テストEXEの読取りCDBは終了0。native spot非ゼロ→ゼロの1組、同Actor/full、ピン待機1・期限保持・RunPlayerMove呼出し境界の入力を正例確認した。これはオフラインであり、以下のHLDS受入れと区別する。

| 製品 | SHA-256 |
|---|---|
| DLL | `c76563d679cf37227ec7b289a51ac37049507d81c6499c414971c23edeea7401` |
| PDB | `cf51a57a557433f8f5d8c21b6850cf2703fca526f0c07b14217a13b6ec63e2de` |
| ELF | `4af3e8f7c96a6cd0d138f61503ebc87e93cc4f1274395530dd14068ef96f93e2` |

証拠は `artifacts/local/debug/csbot-grenade-zero-approach-c02-2c1-20261123/`。最終source200件・保護対象12件・検証入力・製品は `final/freeze.json`。大きいログは原文SHAを保持して圧縮保存した。固定参照HEADと使用したvisionソースの一致も再確認し、参照側の既存編集は保持した。

## 実機の限定受入れ

2026-11-24のユーザー明示許可で、最終DLL/PDB・専用公開fixtureを既存 `D:/SteamCMD/cstrike_rehlds` へ再配備した。サーバーコピー追加0。観測者の手榴弾装備、敵の露出・NOTARGET、位置・速度0とview25度/model pitch=-view/3を公開fixtureで整える。攻撃ボタンや製品内部状態は注入せず、CDBはmodule-load待機後に確認済みの式で読む。自然な旋回や全軌跡の受入れではない。

元NAVの `zero-1` / `zero-2` は実HLDS/CDB終了0・core原文復元を確認したが、zero分岐の正例がなく除外した。読取り診断はHunt9/Attack0/速度XYZ0/候補3で、元NAV全737areaにも登録候補0のareaはない。そのため形状・接続・隠れ場所・遭遇・場所情報とBSP identityを保持し、接近候補4803件だけを0にした有効な専用NAV入力を使う。Pythonの範囲限定raw変換と、既存C++ LegacyNavReaderによる独立した全737area・全非approachフィールド比較を通した。これは定義済みのzero条件入力であり、自然de_dust2の全経路一致ではない。

原文NAV SHA `53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2`、専用入力 SHA `49d0d7892132bec2192eba69dcd966094ac1f7145e80f9fe4fd9c2c8e0a0b478`。起動前に実データSHA/scopeをlaunch receiptへ固定し、専用入力宣言も保存する。実PID0後に原文bytesとmetadataを戻し、原文NAVでなければC4を起動しない。未知の変更は保存して上書きせず、NAVの復元エラー時も既知coreの復元を独立して試みる。専用入力の受入れは入力・復元の両receiptを必須とする。

`zero-3` はactor index 0 / object `0078d544` / full 85、同pairでcount0・look1→0・waiting1・投擲期限bit不変を採録した。解除後の保持32入力、期限後解放1入力、違反0。最後の保持のNative判定 `14.206944465637` は期限 `14.209413528442` 以下、解放のNative判定 `14.315778732300` はstrictに期限後だった。RunPlayerMove内のCmdStart時計がNative判定より前に戻るため、同じActor/native object/期限と生ログ順で結び付け、CmdStart時計だけで早期解放とは判定しない。生ログ値や時間窓を補正・除外していない。

| 試行 | 実HLDS PID | CDB PID | 終端 |
|---|---|---|---|
| zero-3 | 34072 | 23712 | 実HLDS/CDB終了0・core原文復元 |
| c4-0 | 37820 | 37392 | 実HLDS/CDB終了0・core原文復元 |
| c4-7 | 16868 | 30328 | 実HLDS/CDB終了0・core原文復元 |
| c4-8-balanced | 28652 | 10016 | 実HLDS/CDB終了0・core原文復元 |

zero試行後に原文NAV・core.iniを復元し、元NAVでC4 0/7/8の自然設置・実zone退出・実C4喪失を逐次確認した。各実PID終了後に次を起動し、全4受入れ試行で終了0・core原文復元を確認した。旧製品・設定・ユーザー編集・旧証拠を保持し、pushなし。

解析補助は解除イベント・native object・期限を単位として保持と期限後解放を結び付ける。解除後記録欠落、保持入力欠落、期限前の待機消失、別解除イベントの合算を拒否する対照を確認した。追記consoleは起動前分を除き、modeも起動記録と照合する。旧C4の形式対照はaccepted=falseであり今回品の受入れへ流用しない。zero専用schema、同pairの保持・解放、無違反、製品・実PID終了・raw証拠SHAをstage前からcommit前・マージ後まで照合する。

## 残件

1. 非ゼロ選択後の拒否は後続[C02-2c2](GRENADE_HUNT_ADMISSION_C02_2C2.md)で静止Huntに限定対応。moving encounterの抽選前抑止、FetchBomb実機、Guard/Noiseのhandled-returnと後続順、直接同時刻full再入対照は残件。
2. C02-3：安全時間終了後の経路投擲と点列境界。
3. C08/C10：Aim所有、共通装備時計・遅延Touch・C4/USE例外。
4. V01/V02：参照全RNG/全軌跡、Linux実機、多数Botの継続性能。

49歴史項目／47残件を維持し、C02全体は部分対応とする。統合の最終証拠は本単位の `post-merge.json` に記録する。
