# P4-03 — 所属・ラウンド・観測識別

Status: 実装・事前検証完了。main統合と統合後検証は未実施。P4全体・プロジェクト全体のFinishではない。

## Contract

- `TeamRoster`は32スロットの世代付き所属を保持するSDK非依存の値モデル。
  `Self / Ally / Opponent / Unknown`を返す。TERRORISTとCTだけが対戦チームで、
  不明な名前・未受信・観戦は敵と推定しない。同一有効IDはSelf。
- AdapterはTeamInfo受信時と視認フレームの前に、位置を読まずに接続・edict serial・
  世代を同期する。人間、死亡中、観戦中も登録する。FakeClient生成中はその生成処理の
  登録を横取りしない。切断後の同一edict/serialを退役記録で拒否する。
- 所属変更は該当プレイヤーの視認と記憶を退役させる。所属の再送は記憶を消さない。
  視認自体は全相手が対象で、味方の視認記憶も保持する。参加完了後の所属変更は
  参加手順の失敗と解釈しない。SPECTATOR通知も新規視認を抑制する。
- ローカルの`RoundGeneration`はマップ内で1から始まり、検証済み通知ごとに増加する。
  サーバーの実ラウンド番号ではなく、知識の世代である。途中接続時も初期世代で扱う。
  古い世代の視認結果、記憶、待機中の視認走査とメッセージ断片を退役させる。
- `ObservationBatch::identity`は一度の公開走査を識別する。複数対象の結果を含む
  同一走査では同じIDを共有し、対象IDと併せて個別の視認を参照する。
  map/round/source/sequence/元の時刻/受信時刻を保持する。
  sequenceは一時的な視認リセットでも再利用しない。
- `VisualMemory::identity`へ出所を保存する。視認は現在フレームの検証済み公開だけを
  取り込み、遅延した過去の視認を受信時刻で新鮮にしない。古い・重複・不正時刻・
  source不一致を拒否する。寿命は元の視認から5秒の既存契約を維持する。
- `LifecycleCoordinator::teams()`、`round()`、`perceptionIdentityDiagnostics()`と
  既存のread-only視認／記憶APIで状態・更新／変更／拒否／通知能力を公開する。
  新しいDLL export、エンジントレース、敵の現在位置の補完は追加しない。

## Notification evidence and limits

| Input | Validation / behavior | Source |
|---|---|---|
| TeamInfo | byte slot 1..32 + string、message開始／終了間のmap・round・player generation・serialを確認 | 既存decoderを拡張 |
| HLTV | 動的に解決したmessage ID、MSG_SPEC、個別受信先なし、byte 0 + byte 0だけ | ReGameDLL_CS `multiplay_gamerules.cpp:1729-1733`, RestartRound |
| HLTV health reset | 0 + health/flag値はラウンド開始としない | 同ファイル1723-1727 |
| ResetHUD | 単独ではグローバルroundを更新しない | この実装ではround入力に接続しない |
| HLTV未登録 | 能力フラグfalse、他のメッセージからroundを捏造しない | 任意のmessage IDとして扱う |

参照したReGameDLL_CSは`679973265e1ac99a43193119e0da212ee568f5f9`。
そのcheckoutのLICENSEはMIT。過去の研究pinと異なるため、この版の通知形式の
確認に限定している。実装コードの転載はしていない。Metamod-P SDKは既定pin
`7ec9b014f8c0a947a724644aebe34eb33706e44b`。

HLTVのwire形式にイベント連番はない。同一host frameまたは同一simulation時刻の
反復を重複扱いにし、巻き戻った通知時刻を拒否する。別時刻・別frameでまったく同じ
通知が届く場合は新しい境界として保守的に記憶を消去する。これは未知の古い情報を
復活させないが、通知の完全な重複識別や標準CSの実機通知順序の証明ではない。

## Verification

専用ブランチは`codex/p403-perception-identity`、基準mainは`c2ca0f9`。
基礎型の先行コミットは`223d716`。最終実装とmain統合後の証跡は検証終了後に追記する。

- Portable: 世代／退役／map、未知所属、チーム変更、古いround、source・時刻・連番の不正、
  同一batchと一時リセット、全32×31記憶予算。
- Fake engine: 全プレイヤー所属、途中参加の観戦者、味方記憶、所属変更、round境界と
  重複・誤形式、トレース再入中のround／所属変更、メッセージ途中のserial再利用。
- 既存P4-01/02の1/8/16観測者×8/16/100ms、追加トレースなし、移動との共存を再実行。
- 新しい全プレイヤー走査により、古いテストの接続済みダミーが次のケースへ残る問題を
  検出した。ケース開始前のダミー初期化と、生成前スロットのfree状態を修正した。
  移動の期待結果や処理予算は緩めていない。

| Gate | Pre-merge | Merged main |
|---|---|---|
| Windows x86 portable Debug /WX, inspector ON | 44/44 passed | pending |
| Windows x86 Metamod Debug /WX, inspector ON | 53/53 passed, 52.10s | pending |
| Linux x86 portable Debug /WX, inspector ON | 43/43 passed, 15.18s | pending |
| Windows Release PE32/x86, six exports | passed: machine 14C, magic 10B, exactly six names | pending |

指定exportは`GetEngineFunctions`, `GetEntityAPI2`, `GiveFnptrsToDll`,
`Meta_Attach`, `Meta_Detach`, `Meta_Query`の6件。

ログは各worktreeの`build-*-x86-test/Testing/Temporary/LastTest.log`。
VisualMemoryModelの実測サイズはWindows x86で98,024 bytes、Linux x86で89,948 bytes。
P4-02より観測の出所を保持する分が増えたが、32×31固定容量と走査上限は変わらない。
実機HLDS/ReHLDS、標準CS実通知順序、欠落イベントの評価はpost-Finish受入に残す。
音・煙／フラッシュ・WorldSnapshot・NAV分布・明示的報告は後続P4項目。
