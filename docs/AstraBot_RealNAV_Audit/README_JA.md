# 再実行方法

このパッケージは監査資料と**未変更製品ソースへリンクする独立診断ハーネス**です。
修正済みDLL・パッチ・NAV/BSPは含みません。

1. `AstraBot(1).zip` または確認したいソースを別のフォルダーへ展開する。
2. 自分の `de_dust2.nav` のパスを指定して独立ビルドする。
3. assertionが失敗したケースを読み、固定Reference・現在checkoutと照合して製品側で修正する。

例（CMake使用可能なコンパイラ環境内）:

```text
cmake -S harness -B build -DASTRABOT_SOURCE_ROOT="H:/sourcecode/003.Game/amxmodx/AstraBot" -DASTRABOT_REAL_NAV="D:/SteamCMD/cstrike_rehlds/cstrike/maps/de_dust2.nav" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Windows NMakeの場合は適切なVS developer環境を使う。既存の製品build cacheをこの独立プロジェクトに転用しない。製品CMakeのx86制約を変更しない。

入力NAV SHA-256:

`53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2`

別NAVではArea ID・接続・期待値が変わる。fixture条件不一致を製品回帰と誤認しない。

## テストの意味

- `existing_*`: 元のテスト7件。
- `audit_diagnostic_observations`: 旧synthetic診断の完走。PASSは「問題なし」ではない。
- `audit_core_safety`: syntheticの不整合を検出するRED gate。
- `real_nav_parse_and_dump`, `real_nav_edge_diagnostics`: production reader・queryを実行し結果を生成。後者のPASSは各辺の物理通行成功を意味しない。
- `real_nav_baseline`: 41→90等のraw A*と明示budget256の結果を検証。
- `real_nav_slope_entry`, `real_nav_slope_exit`: 連続したNAV上り面の誤段差判定。
- `real_nav_jump_gate`: Jump属性の候補を通常段差gateだけで先に拒否しない。
- `real_nav_gap_policy`: 計画中の下降接続の隙間を無条件に後退へ置換しない。
- `real_nav_recovery_projection`: vertical-only回復入力の歩行投影がゼロになる契約不一致。
- `real_nav_goal_floor`: 3D目的地を別の階のAreaへ置換する問題。

現入力では18件中11PASS・7FAIL。このREDをWILL_FAILやskipで合格扱いに変更しない。
修正方法によって正当な拒否理由・観測契約が変わる場合は、Referenceを根拠に安全性を保った期待値へ改訂する。今回のハーネスだけを厳密CSBot parityのoracleにしない。

## 個別実行

```text
build/real_nav_regressions /path/to/de_dust2.nav slope_entry
build/real_nav_regressions /path/to/de_dust2.nav gap_policy
build/real_nav_regressions /path/to/de_dust2.nav recovery_projection
build/real_nav_regressions /path/to/de_dust2.nav goal_floor
```

Windowsの場合は実際の配置に合わせて`.exe`や`Debug/`を使用する。

公式external NAVテストも`official_external_nav`としてビルドするが、自動CTest登録はしていない。公式実装は同じフォルダーの `de_dust2.bsp` が必要で、今回は未提供のため実行が停止した。ハーネスのNAV-only検査でその公式gateを通したとは扱わない。

診断入力の位置はNAV境界に合わせて与えた値で、ログから得た実機feet trajectoryではない。BSP衝突・実際の踏切可否・着地・天井の検証はHLDS/ReHLDSで行う。
