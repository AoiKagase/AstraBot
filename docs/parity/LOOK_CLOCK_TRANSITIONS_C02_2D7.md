# C02-2d7 Hunt/Guard共有時計の状態間継続

2026-11-24。C02-2d4の実機未検証部分を確認。製品ソース変更なし。固定参照のBot所有m_lookAroundStateTimestampに対応する時計が、状態やgeometryの変更で初期化されないことを確認する。

## 対照と結果

同じD:/SteamCMD/cstrike_rehldsの通常de_dust2 NAVを使用。CT観測者がHunt中に、Tが公開GameDLL入力でC4を落とす。CTは落下C4のGuardへ移行し、到着後にTがGameDLL Touchで回収するとIdle経由でHuntへ戻る。配置と初期view補正を含むcontrolled fixtureで、内部状態への書込みは行わない。

live-3-qualified-transitionのactor1は11.219384秒でHunt→Hide（Guard）、18.958193秒でHide→Idle、19.060951秒でIdle→Hunt。Guardへの入口と最初のGuard視線更新16.081657秒で期限12.258146（bits4144215e）を保持した。戻りのIdle/Hunt入口と最初のHunt視線更新19.184696秒でGuardが設定した期限18.720947（bits4195c480）を保持した。

Huntのnow+intervalをfloat32へ丸めた値、Guard更新後の期限、各状態/視線入口の期限を全採録範囲で照合した。戻り時には期限を過ぎているため、この試行だけから戻り後の期限前抽選抑止までは主張しない。期限前抑止はC02-2d4/2d5の別対照に対応する。

## 失敗試行と観測修正

live-1-loose-bombはC4から200離れた初期配置でGuardへ入らず、Hunt→Idle→Huntのみ。入力未成立として往復の受入れから除外した。live-2-loose-visibilityでは80の配置で往復が成立したが、追加SC_LOOSE観測の参照引数を値として扱うCDB式エラーがあった。旧事前チェックがType is notの診断を見逃していたため、数値行の完全形式とエラー検出を追加し、119行で修正を資格確認。live-2は最終受入れから除外し、修正済みlive-3を正本とした。

各試行の実PID終了・core原文復元・保護指紋・一時fixture削除をexit.jsonで確認。最終HLDS47024/CDB9172はexit0。コピー追加0、通常NAV変更なし。観測はmodule-load後の読取りBP。RNG全件BPを外し状態/期限に絞ったが、前回のSteam終了停止の原因を確定したものではなく、V02は未完のまま。

## 指紋と範囲

- DLL SHA256: e786db4a200f87ffa095a32f06048a432bef13be40b858ce3005bfe6a234a02a
- PDB SHA256: 91c4632f56aa8ec7987cc4095cefb70b316a77f48433158ae2a2e542d641d69f

C02-2d4のWindows重点16/16＋Guard1/1と同一ソース・製品を指紋確認。今回はAMXXコンパイル、オフラインCDB資格確認、逐次実機を実施。全CTest/Linux/export再実施は行わず、大分類完了時のゲートへ残す。固定baseline再生成・製品RNG変更なし。

証拠はartifacts/local/debug/csbot-look-clock-transitions-20261124/ のfinal/freeze.json、各live-*、acceptance.json、acceptance.py。全状態・全mapの時計一致は未証明。他状態の後段Noise再入、FetchBomb/同時刻full、corner投擲を次のC02残件として維持する。
