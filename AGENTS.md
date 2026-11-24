# Agent Instructions

@C:\Users\SandS\.codex\RTK.md

## 手動指示の継続ルール

以下はユーザーがこの作業で明示した運用方針。再開時にも適用し、その後の明示指示があれば更新する。停止記録の古いHEAD・PID・テスト件数を現在の事実として扱わない。

### 再開と参照基準

- 最初にこのファイルと `C:/Users/SandS/.codex/RTK.md` を読み、シェル操作にはRTKを使用する。
- 作業場所は `H:/sourcecode/003.Game/amxmodx/AstraBot`。停止記録、現在のgit status/diff、既存処理の実プロセスとログを確認し、未コミット実装を引き継ぐ。最初から作り直さない。
- 固定参照は `H:/sourcecode/003.Game/amxmodx/ReGameDLL_CS` の `b0889847fe6d03898be88acc9e366660efb40ab5`。CSBot参照挙動を優先し、旧Astraの挙動維持を正解にしない。
- `TASKS.md` と `docs/parity/STATUS.md`、`TRACE_REPLAY.md`、`CSBOT_SOURCE_AUDIT.md`、全ソース監査・台帳を照合する。資料が別作業で整理・移動されていたら現在の配置を確認し、削除を勝手に取り消さない。
- セッション操作が使えなくても実プロセスとログを調べる。観測のタイムアウトだけで処理を停止扱いにしたり、同じビルド・検証を再起動したりしない。

### 修正単位とGit

- 残件を修正単位に分け、必要な検証、証拠・記録更新、対象パスの明示stage、commit、ローカルmasterへのマージまで進める。可能な場合はFFを用いる。各単位の完了時に残リストを示し、そのまま次の単位へ進む。
- masterマージ後にソース・製品・保護対象の指紋を確認し、masterへ切り替えて完了した当該作業ブランチを `git branch -d` で削除する。未マージ、別作業用、他worktreeで使用中のブランチは保持する。次の単位はmasterを基点とした新しい `codex/` ブランチで始める。
- pushと実環境への配布は行わない。下記の既存サーバーへの検証用再配備・一時設定切替は、ユーザーが許可した検証作業として区別する。
- 無関係なユーザー編集、未追跡ファイル、worktree、設定、既存資料・証拠を保持する。特に `.gitignore`、`.planning/PROJECT.md`、`AGENTS.md` の既存編集と `BotProfile.db`、`.serena`、`config/local` を保護する。対象ファイルを更新する明示指示がある場合も既存内容を確認してから編集する。
- stageは今回の対象パスだけ。reset/clean、一括stageは禁止。資料整理のために追跡解除されたローカル資料を勝手に再追加しない。

### 検証の頻度と証拠

- テストは大分類の完了ごとにまとめ、その間は簡略化する。現在の大分類は `TASKS.md` の5群。個別修正では変更対象のビルド、重点テスト、必要な実機確認に絞る。
- 両OSの全build・全CTest、PE/ELF各7export、固定履歴比較、広範な実機回帰は大分類完了時に実施する。重点合格を全体合格として報告しない。全CTestを実施する際は対象ディレクトリを全buildしてから実行する。
- Windows/LinuxのCTestは共有fixtureがあるため同時実行禁止。他の共有fixture検証とも競合させない。オフライン合格、実装済み、実機受入れ済みを明確に区別する。
- 検証結果はそのソース・製品の指紋に結び付ける。ビルド中や検証後に追加編集した結果を、最終ソースの合格に流用しない。文書だけの明示更新は変更前後と範囲を記録し、製品ソース変更と区別する。
- 既存証拠を上書きしない。履歴差は固定baselineを再生成せず、変更箇所とRNG消費差を説明する。失敗した試行も保持し、入力不成立・観測失敗と製品不具合を区別する。

### 実機確認と再配備

- 確認は最終候補を実機へ再配備して行う。既存の `D:/SteamCMD/cstrike_rehlds` を継続利用し、サーバーコピーを追加しない。隔離環境が必要な場合もコピーは1回だけとし、その同じ環境を使い続ける。
- HLDSは必ず1台ずつ。実PIDの終了を確認してから次を起動する。CDBを使う試行も実プロセスを追跡し、親ラッパーの終了だけでHLDS終了と判断しない。
- 検証用DLL/PDBと必要な専用fixtureのみを扱う。core.iniなどを一時切替する場合は原文を保存し、実PID終了後に復元・指紋確認する。既存fixtureは再利用し、通常設定と無関係なファイルを保持する。
- CDBは対象DLLのmodule-load待機後にBPを設定し、最終DLL/PDBに対応した読取り採録を行う。読込み前のBP設定や別製品のシンボルによる失敗を受入れ証拠に含めない。
- 過去に特定PIDの強制終了へ与えられた許可を、他プロセスの強制終了許可へ拡張しない。破壊的操作は事前確認する。

### 残件台帳と報告

- 全ソースを対象に残件の漏れを確認し、内訳を含む全リストを `TASKS.md` に維持する。大項目と部分修正の完了を混同せず、未検証・対象外の差分も明示して残す。
- 現在のソースと証拠から完了と判断できるものは、古い未完記録も更新する。古い記録の記述だけを根拠に完了を主張しない。
- 日本語で結論または次の操作から短く報告する。工程の進捗と検証済み／未検証を明示し、単位のcommit・masterマージ完了ごとに残リストを表示する。
- 複数手順は番号付きで1手順1操作、通常のリストは5項目以内にする。詳細説明を求められた場合は必要な内訳を省略しない。見積りを示す場合は具体的な時間単位を使い、最後に2分以内で着手できる次の操作を示す。
- 実行再開の指示を受けたら、既に許可された作業を確認待ちで止めない。曖昧な場合は短い質問を1つに絞る。同じ修正が3回失敗した場合は、疑わしい前提を特定してから進める。

### サブエージェントを利用する場合

依頼がある場合や適用する手順が必要とする場合に利用する。担当者は開始、主要な処理の終了、30秒を超える処理の前、完了、失敗・阻害・入力が必要になった時点で親へ短く報告する。長時間作業では少なくとも30秒ごとに現状・完了事項・次の操作・阻害要因または証拠を伝える。親は報告をユーザーへの進捗に反映し、報告が途切れたら状態を確認する。

## Source review

Use current source, focused file reads, `rg`, compiler diagnostics, relevant tests, and runtime evidence to review architecture, dependencies, and changes.
Confirm important conclusions against the current source and build/test identity. Keep offline verification distinct from live acceptance.

## Change hygiene

- Preserve unrelated edits, untracked files, worktrees, configuration, and generated evidence.
- Stage explicit paths only; never use broad staging for a focused task.
- Keep implementation and offline verification distinct from live HLDS/ReHLDS or real-device acceptance.

## Windows x86 / NMake toolchain recovery

The Windows x86 CMake caches use the NMake generator and an MSVC HostX86/x86 compiler. Build and test commands must run inside the same Visual Studio developer environment that initialized the cache. On this machine, initialize it with:

```cmd
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x86
```

Then, in that same `cmd.exe` process, verify `where cl` resolves to the cached `Hostx86\x86\cl.exe`. For the full verification gate, build the complete target set before running CTest. The current verification directories are `build/windows-x86-23a1-clean` and `build/linux-x86-debug`:

```cmd
cmake --build build/windows-x86-23a1-clean --config Debug
ctest --test-dir build/windows-x86-23a1-clean -C Debug --output-on-failure --parallel 1
```

If MSVC reports `fatal error C1083: 'windows.h': No such file or directory`, treat it as an uninitialized or mismatched developer environment first. Do not add ad-hoc Windows SDK include paths or change the CMake cache. Reopen/reinitialize the developer command environment, confirm the compiler architecture against `CMakeCache.txt`, and rebuild the affected target. A focused target build can leave other CTest executables absent; a full CTest run will then report those cases as `Not Run`, not as source regressions. Build the complete directory before interpreting the full CTest result.

The PE verifier is a separate gate. If `py -3` cannot create its Python process, record the verifier as environment-unverified and do not replace that result with an offline CTest claim.
