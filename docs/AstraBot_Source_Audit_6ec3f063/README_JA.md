# AstraBot ソース監査・再現ハーネス

このフォルダーは修正パッチではありません。別途展開した AstraBot の未変更 SDK-free ソースを読み、ローカルなナビゲーションの反例を実行します。

## 実行

監査フォルダーを作業ディレクトリにして実行します。`ASTRABOT_SOURCE_ROOT` は `include` / `src` / `tests` を含む AstraBot のルートへ置換してください。

```powershell
cmake -S . -B audit-build -DASTRABOT_SOURCE_ROOT="H:/sourcecode/003.Game/amxmodx/AstraBot"
cmake --build audit-build --config Debug
ctest --test-dir audit-build -C Debug --output-on-failure
```

Linux 例:

```sh
cmake -S . -B audit-build -DASTRABOT_SOURCE_ROOT=/path/to/AstraBot -DCMAKE_BUILD_TYPE=Debug
cmake --build audit-build -j 4
ctest --test-dir audit-build --output-on-failure
```

この CMake は AstraBot 本体の CMake を取り込まない、独立した診断用プロジェクトです。本体の x86 制約や Metamod 設定は変更しません。今回検証したのは Linux x86_64 です。Windows 上のハーネスは未実行です。

## 結果

添付ソースでの期待結果:

- 既存 NAV 系 7 tests: PASS
- `audit_diagnostic_observations`: PASS（ハーネスが完走したという意味）
- `audit_core_safety`: FAIL（R3–R6 の欠陥を検出する RED）

`audit_repro` を引数なしで実行すると 8 個の観察結果を出します。`--assert-core-safety` では R3–R6 の危険な経路進行／traversal 条件が残ると exit=1 になります。

**R1 の「複数 Bot が同じ Goal」は単独では FAIL 条件にしていません。** 全 Bot を異なるルートにすることは Compatibility の要求ではありません。R2/R7/R8 も、Reference と API 契約を踏まえた正式テストに分解してから採用してください。

修正後は、単に intent を生成しなくしてテストを通すのではなく、正常な通行・到着・Jump/着地までの正例も追加してください。この小さな RED gate の PASS だけで実機や CSBot parity を認定しないでください。

## 注意

- 原本の製品ソースは修正していません。
- synthetic NAV fixture を使用し、実マップ/BSP/GoldSrc physics は含みません。
- source ZIP と申告 Git SHA の一致は独立には未検証です。
- 正式な Windows x86 DLL、全 CTest、外部 NAV、live 検証は別途必要です。
- Python contract の 1 FAIL は `.planning/STATE.md` が ZIP にないためです。

詳細は `AUDIT_JA.md`、ソース抜粋は `source_evidence.md`、再実行結果は各 `*_results.txt` を参照してください。
