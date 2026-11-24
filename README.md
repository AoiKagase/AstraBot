# AstraBot

Counter-Strike 1.6向けのMetamod-P Botプラグイン。公開インターフェースとSDK非依存Coreを分け、
CSBot互換動作をオフライン検証と実機検証で確認します。

## ソースの入口

| 場所 | 責務 |
|---|---|
| `src/core/`・`include/astrabot/` | SDK非依存の判断・状態・NAVと公開ヘッダー |
| `src/adapter/metamod/plugin/` | プラグインexport、フック、全体の更新・ライフサイクル |
| `src/adapter/metamod/runtime/` | behavior・observation・navigation・commandの実行担当とshared契約 |
| `src/adapter/metamod/bridge/` | Engine入出力、観測・行動変換、任意GameDLL provider |
| `src/adapter/metamod/{lifecycle,config,navigation,diagnostics}/` | 接続管理、設定、NAVロード、診断 |

`include/astrabot/metamod/`の公開パスは維持しています。
担当の所有権と受け渡しは[責任表](docs/RESPONSIBILITIES.md)を参照してください。

## テストとビルド

`tests/`にCore・Adapterのテストと所有権／依存／配置の静的検査、`tools/`に成果物・実機ログの検証ツールがあります。
Windows x86は[AGENTS.md](AGENTS.md)のVisual Studio環境で、全ターゲットのビルド後にCTestを実行してください。

```cmd
cmake --build build/windows-x86-debug --config Debug
ctest --test-dir build/windows-x86-debug -C Debug --output-on-failure
```

現用ビルドは`build/windows-x86-debug/`と`build/linux-x86-debug/`にまとめています。
新規構成では、旧キャッシュの設定値を確認してCMakeを再生成してください。
絶対パスを含む既存キャッシュをフォルダーごと移動して再利用したり、別アーキテクチャのコンパイラーへ流用したりしないでください。

Linux x86 Debugのビルドとテストは次のコマンドで実行します。

```sh
cmake --build build/linux-x86-debug --parallel 4
ctest --test-dir build/linux-x86-debug --output-on-failure
```

## 設定とローカル成果物

[config/](config/README.md)に設定の扱いを記載しています。
個人の設定は`config/local/`、手元の配布物・生成物は`artifacts/`に保存し、Gitの追跡対象から除外します。
サーバーへの配置は別途行います。

## 設計・作業記録

公開する責任表・互換性資料は`docs/RESPONSIBILITIES.md`と`docs/parity/`、検証ツールは`tools/`にあります。
内部の作業状態・計画は`.planning/`・`plans/`・`docs/superpowers/`に保存し、Gitの追跡対象から除外します。
監査・調査記録（`docs/AstraBot_*_Audit*/`・`docs/nav-bsp-feasibility/`）と検証証跡（`docs/evidence/`・`docs/verification/`）もローカルに保持します。
C/C++の編集規約は[C_CPP_REFACTOR_RULES.md](C_CPP_REFACTOR_RULES.md)を参照してください。
