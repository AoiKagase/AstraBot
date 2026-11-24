# C02-2d6 Guard候補ゼロの実機対照

2026-11-24。C02の部分受入れ。製品変更なし。固定参照cs_bot_vision.cppの静止候補ゼロによるClearLookAtを、実際のHLDSと同一最終DLL/PDBで確認した。

## 入力の成立と限界

既存de_dust2 NAVは737領域すべてにapproach定義がある。これはフィルタ後の候補が常に非ゼロという意味ではない。今回の入力はapproach件数を0とし、対応する14byteレコードだけを除いた専用NAV。他の領域・経路・隠れ場所・encounter等は元バイトを保持する。製品LegacyNavReaderはLoaded(0)、737領域、候補0で受理した。通常NAVで自然発生したゼロ候補は主張しない。

同じD:/SteamCMD/cstrike_rehldsで、元NAV/core.ini原文を保存して一時切替。自然Plant/Guard到着後にHE+敵の一時公開、または通常RunPlayerMove足音を入力する。view/位置補正を含むcontrolled fixture。CDBはmodule-load後に読取りBPのみ。入力失敗を含む各試行で実PID終了・NAV/core原文復元・保護指紋・一時fixture削除を確認し、サーバーコピーは0。

## 試行と受入れ

live-1-native-zeroは敵公開0.6秒でNative保持が成立せず除外。live-2-pin-controlはIN_ATTACKを認識敵ありのNative開始と誤解して早く敵を隠した。製品primary()は認識敵なしでもHE装備中の攻撃入力を出すため、この判定は不適切だった。いずれも候補ゼロClear自体は観測したがowner解除の証拠には使わない。

live-3-recognition-windowは敵を3秒公開し、actor1/full190でNative look=1/waiting=1→intervalのみ→Clear(2)→look=0/waiting=1を採録。保持Nativeの受入れ1更新。live-4-noise-zeroでは保持Noise1更新でintervalのみ→Clear→Noise解除を採録。全試行の実PID・終了コード・復元は各exit.jsonを正本とする。失敗入力を後の成功として読み替えない。

終了処理の注意：live-1〜3はHLDS/CDBともexit0。live-4は機能採録とGUARD_COMPLETE後、Steamのstalled cross-thread pipeでquitが完了せず、ユーザーがHLDS45128/CDB44868を手動終了した（両方0xC000013A）。実PID消滅後の原文復元・指紋一致は独立再確認済み。Noiseの証拠は終了前の機能対照のみで、正常終了ゲートは不合格。V02へ残件を追加し、無条件の実機合格とは扱わない。

## 製品・検証

- DLL SHA256: e786db4a200f87ffa095a32f06048a432bef13be40b858ce3005bfe6a234a02a
- PDB SHA256: 91c4632f56aa8ec7987cc4095cefb70b316a77f48433158ae2a2e542d641d69f
- 元NAV SHA256: 53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2
- 専用NAV SHA256: 49d0d7892132bec2192eba69dcd966094ac1f7145e80f9fe4fd9c2c8e0a0b478

C02-2d4の重点16/16＋Guard1/1と同一製品ソース/テストを指紋確認。今回の追加検証はAMXXコンパイル、製品NAV reader入力検証、オフラインCDB資格確認、逐次実機。全CTest再実施やLinux実機合格とは扱わない。固定baseline再生成なし、製品RNG変更なし。

証拠：artifacts/local/debug/csbot-guard-zero-20261124/ のfinal/freeze.json、各live-*、acceptance.json、acceptance-noise.json、acceptance.py、INPUT_TRIALS.md。スクリプトの初回カーソル加算不良もINPUT_TRIALSへ記録した。元ファイルへは生成失敗中に書き込んでいない。

## 残件

Hunt↔Guardの共有時計継続、他状態の後段Noise再入、FetchBomb/同時刻full再入、corner投擲は未完。Clear後のlight Ahead/sharedspringは既存オフライン対照のみで、今回実機の正確な角速度一致まで主張しない。大項目47件を維持する。
