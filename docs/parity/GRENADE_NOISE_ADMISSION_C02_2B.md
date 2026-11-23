# C02-2b 手榴弾視線に拒否された音要求

## 後続更新（2026-11-24）

静止・接近候補0の無条件Clearは [GRENADE_ZERO_APPROACH_C02_2C1.md](GRENADE_ZERO_APPROACH_C02_2C1.md) で限定対応した。C02全体は部分対応のまま、非ゼロ・moving・Guard/Noise後続順と経路投擲は残す。以下の製品・実機・RNG数値はC02-2b時点の履歴である。

基点 `663afcc8df96f97cb0278f42365ac29fd4c954fa`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。C02全体の完了ではなく、音要求の受付と独立したRNG・抑制時計の修正単位。

## 参照と変更

`cs_bot_vision.cpp:403–445` の危険銃声はHIGHのSetLookAtを試み、拒否されてもRANDOM_FLOAT(2,4)でlookaroundを抑制してreturnする。`cs_bot_listen.cpp:215–236` の通常音もdurationを引いてからSetLookAtを試みる。手榴弾のUNINTERRUPTABLE spotは視線だけを拒否する。Primary保持は音の生成・RNGを省く条件ではない。

NoiseLookPlannerの視線受付と抑制時計を分離し、拒否時もduration/startleのRNGを消費する。Startleの抑制期限は独立に公開し、拒否された視線を新規保持しない。adapterはnative grenade spotがある間の音視線・移動軸公開を抑え、保持Primaryを理由に音処理全体を止めない。新しいnative投擲開始に成功したときだけ旧noise goalを解除し、抑制時計と共有springを保持する。再入検査はActor/frame/revision/normal/attack/clear/fullで古い公開を拒む。ネストした幾何・team helper内部はbatch境界の検査であり、全callbackからの即時中断を保証したものではない。

## RED/GREENとオフライン検証

正規 `astrabot_native_grenade_noise` は、実Root full更新で公開銃声を局在し、音の経過0.125秒が通常反応0.3秒より短く、手榴弾Primaryが保持されていることを確認する。未修正ではStartle RNG0回、期待1回のRED。修正後GREEN。実upkeepへ渡したview/axes/Primaryが拒否された音で書き換わらない対照も合格した。

初期の通常音試行は製品REDに採用していない。反応待ち未到達の結果と、反応到達後にHuntの調査処理が音を消費した結果を混同しかけたため、時刻・反応時間・activeを直接採録した。参照Hunt `159–167` もShouldInvestigateNoiseがUpdateLookAroundより先なので、この消費は正当である。危険銃声は反応待ち前に分岐し、この曖昧さを除いた。未使用helper警告による最初のbuild失敗も除外して保持した。

core対照は拒否された通常音の1〜2秒RNG、危険銃声の2〜4秒RNGと独立抑制、期限後にgoalが復活しないこと、RNG再入後に古い時計を公開しないことを確認する。重点回帰6/6。全target build後、Windows176/176＋PE7、Linux175/175＋ELF7を逐次実行した。両OS履歴は前回C02-2aと151933行・順序付きRNG510行を含め差0。固定baseline・補正は変更していない。新音fixtureはこの履歴に含まれず、差0を全lookaround RNG順の一致へ拡張しない。

| 製品 | SHA-256 |
|---|---|
| DLL | `257a64a4530fd9fc6960565d49d1539870bec89a6095636368a43cbf800202b4` |
| PDB | `4a900049814453d8b43edbf3020fc526b6ced34105d0601ece12a7e0bbe8bf1f` |
| ELF | `090fb3a186deae0e58bf96d7f9c8939363861b9f61432426e2a79e53ae64817e` |

src/include全200追跡パス（製品C/C++198本）とユーザーファイル12件を照合。ソースレビューに阻害指摘なし。製品・検証入力・レビュー・raw履歴の証拠は `artifacts/local/debug/csbot-grenade-noise-admission-c02-2b-20261123/` に保存した。

## 実機の限定受入れ

既存 `D:/SteamCMD/cstrike_rehlds` に最終DLL/PDBを再配備し、追加サーバーコピーは0。公開fixtureで役割/手榴弾/姿勢を用意し、投擲開始の読取り採録後、敵をNOTARGETとして0.5秒待ち、敵の実AK-47に公開Ham PrimaryAttackを実行し、GameDLLの実発砲を発生させた。攻撃ボタンや製品状態は注入しない。25度のviewとmodel pitch=-view/3を整合させた制御試験であり、自然旋回や自然音源識別全体の受入れではない。

試行 `noise-4`、Actor index 0。音時刻 12.207825、拒否要求 12.207826、抑制期限 14.358248。native spotあり・Startle kind・admit=falseを確認し、拒否後に抑制期限を25標本、goal非activeを25標本確認した。同区間の共有step25件でNoise priority3は0、既存のPrimary保持は49入力で維持された。通常音の拒否RNGはcore対照であり、この実機試行の主張に含めない。

Primary解放のNative判定は14.073584557秒で、期限14.057006836秒の後だった。RunPlayerMove内部では、その判定後にゲーム時刻が13.982656、14.015656、14.048656へ一時的に戻る3入力がある。最初の解析はこれを期限前の解放と誤判定したため、元解析を保持し、同じActorの直前Native primary判定とログ順に結び付けた。49保持入力は期限前のNative判定に属し、3入力は期限後の解放判定に属する。生ログの時刻を補正したり、任意の時間窓を除外したものではない。対応関係は `noise-4/acceptance.json`、元解析は `analyzer-initial.py`、修正版は検証補助 `analyze-noise-v2.py` に保持した。

初回noise-1は採録式の修正待ちでSteam watchdog assertが生じ、正常quitを完了できず除外した。AMXXのemit_soundが元engine関数を直接呼ぶため観測hookへ届く保証もなかった。後続は読取り式を修正済みにし、実GameDLLのclip消費とPlaybackEventを対照とする。旧試行と元fixtureは保持した。

noise-2は実発砲を確認したが適格なStartle要求が採れず除外した。noise-3はCDBが参照型をポインタとして扱うのにdotで読んだ診断が不正で、Steam watchdog assertを伴ったため除外した。通常継続・detach要求でも終了せず、fixture完走・quit要求・全Bot切断後に起動記録と実パスを照合し、検証用HLDS17344だけを停止した。CDB15012は追従終了し、実PID0とcore.ini復元を確認した。受入れ試行4の前に、同じWindows正規テストEXEで参照型・ゲート10項目・音の鮮度/距離/LOSの正例採録を通し、検証済みの式を実機へ移した。失敗試行のデータを合格へ流用していない。

C4実機0/7/8も同じ最終品で逐次確認。受入れたnoise-4とC4の4試行では、各HLDS実PIDとCDBの終了0、元core.iniのraw復元を確認して次を起動した。旧製品・設定・証拠・ユーザー編集を保持した。pushなし。

## 残件

1. C02-2c2：Noiseのhandled returnとHunt/Guard後続RNG順、stationary非0の選択後拒否、moving encounterの抽選前抑止。候補0のClearは後続C02-2c1で限定対応。
2. C02-3：安全時間終了後の経路投擲とnative点列境界。
3. C08/C10：照準所有、共通装備時計・遅延Touch順・既存C4/USE例外。
4. V01/V02：参照全RNG/軌跡、Linux実機と多数Bot継続性能。

49歴史項目／47残件を維持する。
