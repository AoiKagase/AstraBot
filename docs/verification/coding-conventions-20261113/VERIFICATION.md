# C/C++規約修正の検証

基準: `5f847b8`。適用規約: `C_CPP_REFACTOR_RULES.md`。
対象は自作の `src` / `include` / `tests` にある既存C/C++ 228ファイルと、追加した精度回帰テスト1ファイル。
SDK・外部由来コード・生成ソースは対象外。検証完了時点では未コミット・未配備。
その後のユーザーの明示指示により、本修正と検証記録をローカルコミットする。

## 修正内容

- clang-format 11互換の設定と、対象フォルダーだけに適用するEditorConfigを追加。
  タブ幅4・LF・UTF-8 BOMなし、演算子・カンマ・Allman括弧・1行1文・関数間の空行を整理。
- 仕様を表すタイマー、距離、容量、閾値、武器・チーム・メニュー・メッセージの値を定数化。
  元のリテラルの型・サフィックス・値と、使用箇所の式・演算順序を保持。
  同一メソッドだけが使う定数107個は、独立レビューを受けてメソッド内へ移した。
- 既存の命名、コメント言語、公開名、include順序、配列の要素順序を維持。
  公開構造体の非staticメンバー、列挙値、export名は変更しない。
- `docs/source-manifest.json` の移動済み13パスと未登録の自作104ファイルを現行構成へ同期し、精度回帰テスト1ファイルを登録。
  既存の出自・ライセンス審査待ちの記録を保持し、検査側の条件は緩めない。

## 同値性・規約の確認

書式のみの段階では、全228ファイルのC++トークンとマクロ形式を照合した。
プリプロセッサーの行継続はC++の行結合として扱い、文字列リテラルは丸ごと照合。
テスト80ファイルのトークン・期待値・入力データは修正前と一致する。
定数化はリテラルへの展開比較と個別レビューで確認し、既存定数の再利用、
名前付き配列、マスク式、定数だけを格納する名前空間は別途照合した。

### 浮動小数点の精度を維持する修正

最初の全テストで既存のLinux境界値失敗が通るようになったため、比較プローブで調査した。
`accuracy=.4f` の入力で、修正前の直接リテラル比較は胸高さ91、`constexpr float` への置換後は頭高さ100となった。
x87ではリテラルと式を型以上の精度で評価でき、変数への格納は丸めを伴うため、
型・サフィックスの一致だけでは実際の境界比較を維持できない。
[GCC公式説明](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-fexcess-precision)。

新たに定数化した値のうち、二進数で正確に表せない浮動小数点の式オペランド70個を、
元のリテラルへ展開するプロジェクト接頭辞付きマクロへ変更した。各使用関数の後、
または実装ファイル末尾で`#undef`し、他のファイルへ漏らさない。
既存の定数、二進数で正確に表せる値、既にfloatへ格納されるフィールド初期値は維持する。
型を広げるキャスト、評価順序変更、コンパイラーフラグ変更は行っていない。

`astrabot_literal_precision` は実装の定数を共有しないリテラル式を独立した比較基準とする。
Linux x86で修正前のconstexpr版に対する失敗（100対91）と、マクロ版の通過を確認した。
既存の`combat_policy`テストの期待値は変更していない。

意図的に残す表現: 数学的な0・1・係数、座標の成分添字、文脈が明白な空値・真偽値、
名前や目的がある初期設定値、標準表・別名表・独立したテスト入力データ。
これらを数値の言い換えだけの定数へ置換しない。
120桁の推奨を超える6行は例外として記録する。4行は文字列内容の保持を優先し、
2行は精度維持マクロとその理由を説明する既存の行末コメントを同じ行に保持する。
詳細は[書式監査](format-audit.json)。

## 検証結果

| 確認 | 最終結果 | 根拠 |
|---|---|---|
| 自作C/C++の書式・保存形式 | 229/229で再フォーマット差分なし。既存80テストのトークン一致 | [ソース識別](source-identity.json)、[書式監査](format-audit.json) |
| Windows x86全ビルド・CTest | ビルド成功、90/90 PASS | [ビルド](windows-build.log)、[CTest](windows-ctest.log)、[コンパイラー](windows-compiler.log) |
| Linux x86全ビルド・CTest | ビルド成功、88/89 PASS。既存のcombat_policyのみFAIL | [ビルド](linux-build.log)、[CTest](linux-ctest.log) |
| 精度回帰テスト | constexpr版でRED、リテラルマクロ版でGREEN。最終両OSでPASS | [RED](precision-red.log)、[GREEN](precision-green.log)、[精度維持の対応一覧](literal-precision.json) |
| 厳密な実行記録比較 | 両OSとも149,155記録一致。既存baselineと補正ファイルは変更なし | [記録ハッシュ](artifacts.json) |
| PE / ELFの独立ゲート | 両方x86、既存7 export完全一致 | [PE](pe.log)、[ELF](elf.log)、[成果物ハッシュ](artifacts.json) |
| 検査プログラム | 62/62 PASS。P076は定数値変更・比較演算子変更の2負例を拒否 | `python -B -m unittest discover -s tests -p '*checker_tests.py'` |

Windowsは指定VS開発環境を初期化した同一cmdプロセスで、`where cl`、
`cmake --build build-action-adapter-x86-1451 --config Debug`、
`ctest --test-dir build-action-adapter-x86-1451 -C Debug --output-on-failure`を順に実行。
Linuxは`cmake --build build-parity-linux-x86 --parallel 4`の成功後、
WindowsのCTest終了を待って`ctest --test-dir build-parity-linux-x86 --output-on-failure`を実行した。
共有profile fixtureを使用する両OSのCTestは同時実行していない。

P076のソース検査は、目的地のチーム定数の型と値（int 1/2）と、
条件式で使用する定数名の両方を検査する形へ変更。条件式の順序や検査項目は維持した。

Linuxの `astrabot_combat_policy` 境界値失敗は今回の修正前から存在し、
`ac68225` の隔離ビルドでも再現済み。今回の整理に動作変更を混ぜて解消しない。
基準ログ: [修正前の隔離ビルド](baseline-linux-combat-policy.log)。

検証前に存在しなかったルートの`BotProfile.db`はCTestが生成したため、
検証後に`artifacts/local/test-fixtures/coding-conventions-20261113/BotProfile.db`へ移して保持した。
移動前後のハッシュは[生成fixtureの記録](generated-fixture.json)を参照。

HLDS/ReHLDSの実機受入れ、fixture外の全状態空間、CSBot互換性の正解判定は別ゲート。
オフラインのビルド・テスト結果で実機確認を代替しない。

無関係な既存変更 `.gitignore` / `AGENTS.md` / `.planning/PROJECT.md`、
未追跡の設定・成果物、既存ビルドキャッシュを保持する。
