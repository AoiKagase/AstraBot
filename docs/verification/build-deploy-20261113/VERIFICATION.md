# ビルド配置・展開・無人テストの最終記録

対象ソース: `bb63aa7071b36df719bd803fddb50be017f12b9b`。
2026-11-13 JSTに検証。ソースの内容は `source-identity.json` のSHA256で照合した。
後続の最終コミットはビルド配置の説明と本記録のみを追加する。

## ビルド配置

現用ビルドは `build/windows-x86-debug` と `build/linux-x86-debug`。
旧キャッシュの設定を確認して新しい場所で再生成し、全ターゲットをビルドした。
旧ルートのビルドフォルダーは復元用ZIPを照合後、ユーザー承認に基づいて削除した。
ZIPと詳細証跡はローカルの `artifacts/local/build-cleanup/` と
`artifacts/local/build-layout/` に保持する。

## オフライン検証

| 対象 | 結果 |
| --- | --- |
| Windows x86 Debug / MSVC HostX86 x86 / NMake | 全ビルド成功、CTest 90/90 |
| Linux x86 Debug / GCC / Unix Makefiles | 全ビルド成功、CTest 88/89 |
| Windows PE / Linux ELF | x86・既存7 exportの検査通過 |

Linuxの失敗は既知の `astrabot_combat_policy` 境界値1件。
本作業では期待値やコンパイラーフラグを変更していない。
Windowsは `AGENTS.md` のVisual Studio開発環境を初期化し、同じプロセスで
全ビルド後にCTestを実行した。PE検査は別コマンドとして実行した。
実行結果は同梱の `windows-ctest.log`、`linux-ctest.log`、`pe.log`、`elf.log` を参照。

## 展開・実機無人テスト

展開先は `D:/SteamCMD/cstrike_rehlds/cstrike/addons/astrabot/dlls/astrabot_mm.dll`。
DLL/PDBの旧版をバックアップして展開し、ビルド成果物とのSHA256一致と
起動したHLDSが読み込むモジュールの実パスを確認した。

`de_dust2` で8体180秒、20体120秒、ラウンド再起動30秒、マップ再読込30秒を実施。
36回の状態観測で人間0人、各段階で目標BOT数への到達を複数回確認した。
ゲームログにダメージを伴うBOT攻撃273件、爆弾設置4件、解除1件を記録した。

既存の `tools/verify-phase8-live-log.ps1` の判定条件は変更していない。

| 条件 | Movement | Combat | Objective |
| --- | --- | --- | --- |
| 8体 | 証拠不足 | 合格 | 合格 |
| 20体 | 証拠不足 | 合格 | 不合格（解除未観測） |

全員の現世代・現ラウンドの連続移動と直近2秒の証拠が揃わず、
厳密な全員移動ゲートは未通過。期間中の一部BOTの移動記録をもって合格に置き換えていない。
計測条件と実挙動のどちらに原因があるかは未確定。

## 終了時の障害と復元

`quit` 直後の18:37:51 JSTに終了コード `3221226505` (`0xC0000409`) で異常終了。
Windows Application Error 1000が記録した障害モジュールは
`D:/Program Files (x86)/Steam/steamclient.dll`、offset `0x009605ba`。
原因およびAstraBotとの因果関係は未確定。短時間の初回起動プローブは同じDLLで終了コード0だった。
WERの一時ダンプは残っていなかった。障害記録は `windows-error-events.json` に保存した。

テスト側の強制終了は行っていない。サーバーは開始前と同じ停止状態。
`server.cfg`、Metamod/AMXXの `plugins.ini`、`BotProfile.db`、`liblist.gam` の
5ファイルについて元のSHA256との一致を確認した。新DLL/PDBは展開先に保持する。
開始前のコンソールログもサーバーの `logs/` 配下にバイト一致で保全した。
ソースと既存の個別編集は変更していない。秘密を含む設定原本はGitに含めない。

## 判定

ビルド・展開・無人テストの実行は完了したが、全項目合格・完全互換とは判定しない。
未解決項目はLinuxの既知失敗、全員移動の証拠不足、20体時の解除未観測、終了時の異常終了。
ユーザーの「最終コミット・マージ」の指示に基づき、この状態と検証結果を記録してローカル統合する。
詳細ログ・実行スクリプトはローカルの
`artifacts/local/native-validation/build-deploy-test-2569b914827746988f755519787d79bf/` に保持する。
