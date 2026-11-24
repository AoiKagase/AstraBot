# C02-2c3 moving Huntの遭遇視線受付

状態：オフライン・Windows実機の限定受入れ合格。C02全体は部分対応。基点 `205cb038b94270c9dc6c5418c6834f8bbf0a6393`、固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。

## 修正と参照順序

参照 `cs_bot_vision.cpp:499–515` は移動中にunsafeと `!IsLookingAtSpot(LOW)` を確認してから、間隔10〜30秒と遭遇spotの抽選を行う。UNINTERRUPTABLEの手榴弾spotがあれば、この抽選へ進まない。

adapterのmoving Hunt/FetchBomb helperに `actor.nativeGrenade.looking()` の早期条件を1つ加えた。待機pinではなく独立したlookの存続を使うため、投擲期限を過ぎてもspotが残れば拒否する。区間・時計・点・既存checked・出力を変更せず、RNGの前に戻る。stationary側の前単位の「抽選後に拒否する」順序は別のまま保持する。

## 正規RED/GREENとオフライン

新規 `astrabot_native_grenade_moving_admission` は公開武器・敵観測でNativeを開始する。実NAVv3の接続3areaと有向encounterを読み、unsafe時刻、Hunt・非Attack、moving physicsを明示してhelperへ渡す。普通の対照は間隔→spotを各1回消費しTrack。Native対照は両抽選0・Unchanged。公開upkeepでAtへ進めstrict3秒後のlook失効を確認すると、同じhelperで間隔→spotを再開する。任意速度を渡すこの試験はhelper境界検証であり、実Root移動の証明とは区別する。

正規CMake target/CTestの `red-moving-1.log` は関連11件中Nativeの抽選前拒否assertだけRED。1条件追加後11件GREEN、Hunt/非Attackと抽選IP順の明示補強後も11件GREEN。ソースを固定し全target buildの終了0後、Windows181/181→Linux180/180を逐次実行し、PE/ELF各7exports合格。両OSの履歴は151933行・順序付きRNG510行で親との差0、固定baseline再生成なし。新規遭遇対照は旧履歴に含まれず、対象条件下でのRNG抑止は正規RED/GREENと実機で検証する。

| 製品 | SHA256 |
|---|---|
| DLL | `9f1815da0af2cb708478ca738d69d4e812d3c2fbb20189869660c3e628e7afc8` |
| PDB | `ee41c21a09d5acf4afc6ad1e4336490e8901587adb23632bd409365895111e6f` |
| ELF | `d13f0ff3c32e6a0f751dbf49172a1b72b9aefe8419411d3d121e6d5fe55d3e7f` |

## 実機の限定受入れ

既存 `D:/SteamCMD/cstrike_rehlds` を再使用、サーバーコピー追加0。ユーザーのC02-2c3明示許可の範囲で最終DLL/PDBと小さい専用公開fixtureを配備し、core.iniを一時切替した。元NAVは全試行不変（SHA `53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2`）。各実PIDの終了後、所有fixtureはV2、coreは原文SHA `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b`へ復元した。

V4は公開FL_FROZEN・武器・一度の配置・view・1.2秒の敵露出を使い、解除後は通常NAVで移動する。observerのvelocity注入・Astra private state書換えはない。V5は観測窓8→30ゲーム秒、fallback50→70秒だけの差で、露出や動作条件は同じ。最終の `moving-7-long-window` はFrozen中のcold BEGINから、解除後の実位置849種類・moving Huntへ進み、元NAV上のencounterへ到達した。

module-load後に最終PDBのBPを設定し、オフラインで式を資格確認してから読取り採録した。74組でHunt・非Attack・速度10以上・encounter非null・旧LOW所有0・Native look非0・selection期限到達・unsafe・inhibit通過を確認。Gate→Skipの26値が同一で、間の遭遇RNG呼出し0、同frameの後段実InputDispatcher入力へ結んだ。Native objectとtargetはFrozen・oldwait0・cachedLOW0の初回BEGINと結び、fixture入力SHA、DLL/PDB、launch/exit、原文復元も照合した。初回BEGINの点は遅延認識後のparking位置(-832,-704,164.03125)であり、露出中の位置を保持したという証拠ではない。waiting1の2組は実入力IN_ATTACKを保持した。waiting0の72組をピン保持の証拠とはしない。実機の通常Track対照は0件で、通常採用・look失効後の再開はオフライン対照の範囲。

実機ではNative条件の後ろにあるblind判定のfalseを採録していない。今回の製品がNative条件でshort-circuitして抽選前に戻ったことは確認したが、「この条件を除いた旧製品なら74組すべてで必ず抽選した」とは主張しない。旧条件との動作差は正規helper RED/GREENで検証する。

| 試行 | CDB / HLDS実PID | 結果 |
|---|---|---|
| moving-7-long-window | 21496 / 1372 | 両終了0・core原文復元 |
| c4-0 | 14488 / 26328 | 両終了0・core原文復元 |
| c4-7 | 36208 / 15688 | 両終了0・core原文復元 |
| c4-8-balanced | 36460 / 28396 | 両終了0・core原文復元 |

C4は同一最終製品・元NAVで自然設置0、zone退出7、C4喪失8を別々に検証した。全試行で前の実PID終了確認後に次を起動した。Windows実機の限定受入れとオフラインLinux合格を区別し、Linux実機合格とはしない。pushなし。

## 除外・診断の記録

moving-1はCDB固定indexと公開observerの不一致、moving-2は旧LOW所有1が先に拒否するため新条件の因果を示さず、受入れから除外。moving-3は起動前のPython indentation errorで実HLDS未起動、修正後にV2/coreを確認した。moving-4はBEGIN0で中間値未採録のため、原因を断定しない。moving-5-diagnosticではHE identity4・可視候補5回でもnoticed0。moving-6-notice-diagnosticはFrozen中の通知成功（roll16.399＜chance31.528）→認識→BEGINを確認したが、解除後moving7件のencounterが全nullで除外した。これらを合格へ読み替えず、raw/excluded/diagnosisを保持する。

「Frozen中は認識不能」「武器切替失敗」は確認された原因ではない。3回のconditioning試行後にfixtureの推測修正を止め、独立ソースレビューと読取り中間値の採録へ切替えた。moving-7では通知抽選4回のうち最後が成功（20.706＜34.028）し、同じ1.2秒露出でも認識・BEGINが成立した。各失敗試行の未採録値をこの成功で遡及断定しない。CDB式v5の `blinded` は同名symbolで曖昧だったため、その式を外してv6/v7で全行を資格確認した。製品変更や製品REDではない。

## 残件

Nativeのaccepted BEGINは旧command側のHunt LOW保持を退役させない（bot_behavior_combat.cpp:554–558、bot_command_runtime.cpp:1405–1411、plugin_runtime.cpp:2018–2019のhelper引数）。moving-2で旧所有1が今回のguardより先に拒否した事実と整合する。Native失効後の旧LOW再開の可能性は残るが、その再開自体は未再現。この所有残件をC02内で明示し、今回のlegacy0受入れと混同しない。

Guard/Noiseのhandled-returnと後続順、FetchBomb実機、直接同時刻full再入対照、安全時間終了後の経路投擲、自然起動・全経路/全角度一致、Linux実機・継続性能は未完。49歴史項目／47残件を維持する。

証拠：`artifacts/local/debug/csbot-grenade-moving-admission-c02-2c3-20261124/`。`final/freeze.json` が200製品ソース・12保護ファイル・7検証入力と最終製品を固定する。受入れは `moving-7-long-window/acceptance.json`、各C4 acceptance、除外・原文復元・通知diagnosis・rawログ・検証スクリプトを保持する。commit/local master FF後の指紋は `final/post-merge.json`。
