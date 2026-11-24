# C02-2d5 Guard保持Noiseの実機対照

2026-11-24。C02-2d3/2d4の未検証部分を限定受入れ。製品ソース変更なし。固定参照cs_bot_vision.cppの静止時計・候補抽選後のSetLookAt受付を確認する対照。

## 入力と結果

同じD:/SteamCMD/cstrike_rehldsへC02-2d4最終DLL/PDBを再配備。自然Plant/Guard到着後、敵音源の通常RunPlayerMove足音を入力する。観測Botは位置とviewを補正したcontrolled fixtureであり自然な多人数対戦の受入れではない。

旧admission/live-4では音源がGuardへ接近し、最初の足音直後にHide.Spot再選択が発生した。今回、音源のvelocityを維持しながら位置を毎回同じspotへ戻して移動範囲を制限し、保持NoiseとGuardが継続した。旧試行の原因を位置変化のみに断定するものではない。

actor2の54更新で保持Noiseを採録。期限前49更新はGuard内のRNGなし、期限到達5更新はLookAround.StateInterval→LookAround.ApproachPoint→LOW拒否（Unchanged=0）でNoiseが継続した。新規Noise受付とは別の後続更新を対象とした。

初回の抽出器はGA_AFTER後の同一full足音位置RNGまで範囲に含めて失敗。GA_AFTERで区間を閉じ、同じログを再検証した。製品不具合・実機再試行ではない。抽出失敗メモと最終抽出器を証拠に保存した。

## 証拠と製品

artifacts/local/debug/csbot-guard-noise-20261124/ の final/freeze.json、live-1-spatial-control/{debugger.log,launch.json,exit.json,guard-fixture.sma,guard-fixture.amxx}、acceptance.json、acceptance.py。既存証拠は上書きしていない。

- DLL SHA256: e786db4a200f87ffa095a32f06048a432bef13be40b858ce3005bfe6a234a02a
- PDB SHA256: 91c4632f56aa8ec7987cc4095cefb70b316a77f48433158ae2a2e542d641d69f

C02-2d4のWindows重点16/16＋追加Guard1/1に対応する同一製品ソース/テスト指紋を確認し、読取りCDBを再資格確認して実機module-load後にBPを設定。今回CTestの新規全実行は行っていない。fixtureはAMXXコンパイル成功。HLDS37820/CDB26824はexit0、実PID消滅、core.ini原文復元・保護指紋一致、一時fixture削除。コピー追加0。pushなし。

## 残る範囲

候補ゼロの実機Clear、Hunt↔Guardの時計継続、他状態の後段Noise再入、FetchBomb/同時刻full再入、corner投擲は未完。大項目47件を維持。製品変更/RNG仕様変更はなく、固定baseline再生成なし。

後続更新：候補ゼロの実機は[C02-2d6](GUARD_ZERO_LIVE_C02_2D6.md)の専用NAV対照でNative/Noise Clearを限定受入れ。通常NAVでの自然発生・状態間時計の実機を証明するものではない。
