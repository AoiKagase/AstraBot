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

[config/](config/README.md)にローカル設定、[artifacts/](artifacts/README.md)に手元の配布物・生成物を分類しています。
元の6ファイルは内容を変更せず、[移動記録](artifacts/local/source-root-moves.json)に旧パス・新パス・SHA256を保存しています。
検証後に再生成されたプロフィールは`artifacts/local/test-fixtures/`に保存しています。
サーバーへの配置は別途行います。

## 設計・作業記録

現行の責任表・互換性資料は`docs/`、作業状態は`.planning/`にあります。
`README_FIRST.md`・`ROADMAP.md`・`CODEX_START_PROMPT.md`・`plans/`・`MANIFEST.sha256`は既存のParity Roadmap Packです。
取り込み時の資料と監査証跡を保つため配置を維持します。C/C++の編集規約は[C_CPP_REFACTOR_RULES.md](C_CPP_REFACTOR_RULES.md)を参照してください。
