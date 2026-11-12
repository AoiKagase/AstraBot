# 責任分離の検証記録（2026-11-12）

責任分離を実装し、分離前の同じスタブ入力との比較とオフライン検証を完了した。
責任・状態寿命・宣言位置の対応は[責任表](../../RESPONSIBILITIES.md)を参照する。
実機受入れ、配備、コミット、stage、push、サーバー設定変更は実施していない。

## 入力と作業コピー

元ツリーは`H:/sourcecode/003.Game/amxmodx/AstraBot`。
未コミット・未追跡ファイルを含む537入力を[manifest](input-manifest.json)で固定した。
manifest SHA256: `7916b19c4a9d154a82835e71455d25b6858f2148333628bb36de819f20af3a74`。
凍結した製品ソースと同一のスタブfixtureで比較した。125製品ファイルの入力ハッシュ一致を別コンテキストのレビューでも確認した。
凍結コピーは`C:/Temp/astrabot-responsibility-20261112/baseline`、検証コピーは同`work`に保持する。
別作業による2テストのファイルハッシュ変化は改行のみで、正規化ソース一致を確認した。
反映対象は各ファイルの入力ハッシュを再照合し、別作業の上書きが起きる場合は転送を拒否する。

## ビルド・テストと成果物

| ゲート | 結果 | 根拠 |
|---|---|---|
| Windows x86 完全ビルド/CTest | 74/74 PASS、skipなし | [最終ログ](final-serial-windows.log) |
| Linux x86 完全ビルド/CTest | 73/73 PASS、skipなし | [最終ログ](final-serial-linux.log) |
| 更新・入力の比較 | 両OSで132,153記録が改修前と完全一致 | `tests/fixtures/runtime_characterization/provenance.json`、CTest `astrabot_runtime_characterization` |
| PE32 / ELF32 | x86、既存7 exportと完全一致 | [PE](final-pe-verification.log)、[ELF](final-elf-verification.log) |
| 公開NavRoamControllerのlayout | Windows 1080 bytes / align 8、Linux 1036 / align 4。改修前後一致 | [Windows](layout-windows.log)、[Linux](layout-linux.log) |

WindowsはVS18の`VsDevCmd.bat -arch=x86 -host_arch=x86`を同じcmdプロセスで実行し、
`14.51.36231/bin/Hostx86/x86/cl.exe`とNMakeを使用した。LinuxはWSLのGNU x86（`-m32`）。
最終ゲートはWindows→Linuxの順で実施し、同一プロフィールfixtureの同時実行を避けた。
Linuxにも外部de_dust2.navを指定し、最終検証では外部NAVテストをskipしていない。
PE/ELF検証はCTestと別に実行した。既存export/config/NAV形式、C++14、CoreのSDK非依存を維持した。

検証コピーのバイナリーSHA256（配備物ではない）:

```
Windows 5899adcffc36c63feb355b97cc318e1b5f5d6ff6fbaff53a93e4a2857e2410c9
Linux   c9edd381ebb1d885d135f0e5b8c80a90b50546863936ef329c1f1cb4fc33e8bf
```

初回Linux比較基準では外部NAV未指定のため1件skip。最終ゲートのskipとは区別する。
制限された実行環境での試行ではMSVC/PDBのC1902、WSLのE_ACCESSDENIED、Store版Python起動不可による4件Not Runが発生した。
これらはソース失敗として扱わず、同じソースを許可された実行環境と指定VS環境で再検証し、上記最終結果を得た。
検証用shのCRLFもLFに修正した。ツールチェインのSDKパスやサーバー設定へ変更を加えていない。

## 最終レビューと修正

別コンテキストの読み取り専用レビューで、製品コードに新規不具合は見つからなかった。
NAV本体は参照ビューのフィールド名置換を戻すと分離前と同一。コンストラクター初期値、診断NaN、
独立したresetの集約、同期経路要求、同じ更新内の順序、ビルド対象への追加を照合した。

指摘された責任表の共有/ActorState分類、切断時の保持条件、宣言位置と初期化根拠を1回の修正工程で修正した。
宣言位置の指摘は軽微との評価だったが、監査可能な責任表が目的のため必須修正として扱った。
拡張した`runtime_ownership_contract.py`は修正前に失敗し、修正後に203状態の宣言位置と保持範囲を照合して通過した。
切断時の武器キャッシュ/lookahead保持は既存挙動として記載し、製品側のresetを追加していない。
修正後にWindows/Linuxの完全ゲートを再実行した。未対応のレビュー指摘はない。

## 実装上の判断と影響

| 判断 | 理由・残る制約 |
|---|---|
| 書込み可能なC:/Tempコピーで隔離 | アプリ管理のworktreeは書込み範囲外だった。コピー間のずれを防ぐため入力manifestと転送ハッシュを照合 |
| Gitへstage/commitせず証跡を保存 | ユーザー指定を優先。Gitコミットによる復元点は作らず、凍結コピーと転送前バックアップを保持 |
| 現行比較記録を同値性の基準に使用 | CSBotの正解データではない。fixture未到達の挙動や実機問題を保証しない |
| 型付きNavigationRequestを同期適用 | 元の位置で適用し、同一更新の判断を維持。インフラポートへの依存は明示的に残る。可変ActorStateは渡さない |
| NAV内部はprivate参照ビューで分離 | 格納順/初期化順とABIを維持。物理ストレージは平坦なままだが、アルゴリズムの状態アクセスは5境界で明示 |

## 検証の範囲

比較記録はEngine providerの順序、乱数型/範囲/結果、状態結果ログ、最終コマンドを検査する。
全引数は記録していない（indexOfEdictは呼出し名のみ、traceは選択した座標）。
fixture外の全状態空間の同値性は証明していない。未記録の部分はソース照合で補った。
FocalSpanは更新・問合せ済み。code-review-graphには古いHEAD/不足する関係があり、0件/低リスクを影響なしの根拠にしなかった。
現行ソース・コンパイラー・比較記録を検証の根拠とした。

レビューで判断対象外としたHLDS/ReHLDS実機受入れ、CSBot互換性の正解判定、既存の落下再試行/姿勢/終点/上下階問題は別ゲート。
最終バイナリー/exportは上記独立ゲートで親側が検証した。今回の整理完了を停止症状の解消とは扱わない。

## 元ツリーへの反映後の確認

入力ハッシュを照合して差分を反映し、元ツリーでも以下を再実行した。

| 対象 | 結果 | 根拠 |
|---|---|---|
| `build-action-adapter-x86-1451` | 指定VS/Hostx86/x86、全ビルド、74/74 PASS | [Windows](original-windows.log) |
| `build-parity-linux-x86` | GNU x86、全ビルド、73/73 PASS | [Linux](original-linux.log) |
| 元ツリーのPE/ELF | x86、既存7 exportと完全一致 | [PE](original-pe-verification.log)、[ELF](original-elf-verification.log) |
| 比較と差分 | 両OSの132,153記録一致、skipなし、`git diff --check`通過 | CTest、[差分検査](original-diff-check.log) |

反映後の入力識別は[ソース全体のハッシュ](delivered-source-identity.json)、元ツリーの生成物は[成果物ハッシュ](delivered-artifact-hashes.json)を参照する。
検証コピーと元ツリーはビルド識別情報が異なるためDLL/SOのハッシュも別に記録した。
元のBotProfile.dbは検証前後で入力ハッシュと一致し、無関係なファイル・設定・作業を保持した。
最後のCMake変更は`git diff --check`が検出した末尾空行だけの削除であり、[ハッシュ付き記録](post-transfer-hygiene.json)を残した。
stage/commit/push/配備は行っていない。ローカルFocalSpan索引は更新したが、索引/configを反映ファイルへ含めていない。
