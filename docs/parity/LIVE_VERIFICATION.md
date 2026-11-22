# 実機検証と再配備

2026-11-22のユーザー指示により、以後の動作確認は既存実機への再配備を基本とする。この検証用再配備は許可済み。pushは行わない。

1. 実機は `D:/SteamCMD/cstrike_rehlds`。Metamod登録先は `cstrike/addons/astrabot/dlls/astrabot_mm.dll`。検証済みDLL/PDBを使い、停止を確認してから旧版をバックアップし、配備元・配備先のSHAを照合する。既存 `hlds_start.bat` の設定で起動し、実PIDの実行パス・ロード済みAstraBot DLL・新規ログを確認する。
2. HLDSは実機・隔離を通じて必ず1台ずつ。次の起動前に先行実PIDの終了を確認する。稼働中のHLDSへDLLを上書きしない。サーバー設定・GameDLL・NAV・BotProfile等を試験都合で恒久変更しない。
3. 隔離が必要なときもサーバーコピーは固定の1か所だけとし、全試行・候補・修正単位で再利用する。使用可能な既存コピーがあればそれを使い、新規作成が必要な場合だけ `C:/Temp/astrabot-hlds-shared` に一度作る。試行名・候補名・日付ごとのサーバー複製は禁止。以後は同じ場所で必要なDLL/PDB・fixtureだけを差し替える。
4. 試行ごとに保存するのはログ・起動引数・PID終了記録・バイナリ/設定の指紋・必要なfixture等の小さな証拠。再利用前に既存ログを保存して識別し、証拠を上書きしない。過去のサーバーコピーや証拠は一括削除しない。
5. オフライン合格・実機ロード/起動確認・個別挙動受入れを区別する。Windows/Linux CTestは共有fixtureのため直列実行する。起動成功だけでPlant/Combat/NAVや多人数性能の受入れ完了としない。

## 2026-11-22の再配備

製品コミット `6fdadbd1afb622159fc1f4d4d59c423279481ee6` の検証済みDLL/PDBを既存実機へ再配備した。DLL SHA-256は `1bdef02f4a3086c5d12c10ee4632ea6de40395fcd72ec17f721d063cc37f1807`。旧DLL/PDBは同ディレクトリの時刻付き `.bak-YYYYMMDD-HHMMSS` に保存し、指紋を検証した。

実HLDS PID31752、`192.168.0.202:27016`、`de_dust2`。実プロセスから配備先のAstraBot DLLロードを確認し、Bot参加とRound_Startを採録した。再配備前後で既存設定・GameDLL・NAV・BotProfileの指紋は不変。サーバーコピー作成は0回。この時点の確認は実機ロード/起動であり、残件の個別挙動受入れは未完。

証拠は `artifacts/local/debug/csbot-live-redeploy-20261122/` の `deployment.json`、`loaded-modules.json`、実機新規ログ、`live-receipt.json`。今後の再配備でPIDやロード状態を改めて確認し、記載PIDの継続稼働を仮定しない。
