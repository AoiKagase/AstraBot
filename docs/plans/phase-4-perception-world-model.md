# Phase 4 plan — Perception and World Model

Status: P4-01・P4-02は実装と適用可能なオフライン検証を完了し、main
`5c3a0be`へ統合済み。P4-03も`3251106`でmainへ統合し、統合後の全gateを完了。
P4-04も`837e0d3`でmainへ統合し、統合後の全gateを完了。
P4-05も`d7228ab`でmainへ統合し、統合後の全gateを完了。
P4-06は実装・オフライン検証完了。P4-07〜P4-09は承認済み計画であり、実装未着手。
この文書の作成は後続項目の実装完了を意味しない。

P3と同じく、番号付き項目は作業単位、番号なしチェックリストは独立して
レビュー・コミットできる実装スライスとする。スライスから新しいP4番号を作らない。

## Goal and authority

視認・音・所属情報・煙／フラッシュ・明示的な味方報告を統合し、Botが
「自分の知っている世界」を読み取り専用World Modelから取得できるようにする。
敵の完全な現在位置をEngineから取得して知識として公開しない。

[architecture](../architecture.md)と
[全体設計](../../adaptive_tactical_bot_design_v0.2.md)のPerception／World Modelを基準とする。
P4-01・02の既存契約は下記参照先に保持し、後続項目での変更を明示する。

- 音は匿名の音源領域とする。発音者のEngine IDで未視認の相手を特定しない。
- 味方情報は明示的な報告だけで共有し、常時自動共有しない。
- サブエージェントは使用しない。項目ごとに専用ブランチ・worktreeで作業する。
- 戦闘、行動選択、永続学習、AMXX公開APIは後続Phaseとする。
- **この計画ではFinishを宣言しない。** Linux x86のオフライン検証は継続して実行する。
  実機HLDS／ReHLDS検証は、別途プロジェクト全体のFinish確認後に実施する。

## Planned modules

| Location | Responsibility |
|---|---|
| `src/core/perception.*`, `visual_memory.*` | 実装済み視認・視認記憶。後続ではラウンドと共通観測識別へ接続 |
| `src/core`の知覚・World Model契約 | geometry-freeの所属／ラウンド情報、SoundObservation、TeamReport、WorldSnapshot |
| `src/adapter/cstrike`, `src/adapter/metamod` | メッセージ／音／イベントの変換、ライフサイクル、視覚妨害、受信能力の診断 |
| `src/nav/query`とWorld Model連携部 | 読み取り専用NAV snapshot、位置からAreaへの対応付け、候補分布 |
| `src/debug` | 記憶・出所・年齢・予算診断とoperator-onlyの明示的報告 |
| `tests`のportable／adapter／replay | 単体、fake-engine、統合リプレイ、処理上限・決定性の検証 |
| `docs/reports`, post-Finish受入資料 | 項目別証跡、対応イベント表、未対応能力、実機受入チェックリスト |

未実装のモジュール位置は提案である。CoreへSDK型を持ち込まず、NAV連携も
移動セッション内部に依存させない。機能追加に伴うDLL export追加は予定しない。

## Common commit and verification protocol

各P4項目は次の順序で進める。既存の未コミット変更、ブランチ、worktreeを保持する。

1. mainと作業状態を確認し、`codex/p4NN-<purpose>`と専用worktreeをmainから作成する。
2. Graphを先に確認する。対象を収録していない場合だけソース検索へ移る。
   FocalSpan status、必要なupdate、関連契約のqueryを実施してから編集する。
3. 対象スライスの失敗するテスト、最小実装、関連検証、差分レビューを行う。
   FocalSpanを更新し、明示したパスだけstageしてcached diff check後にコミットする。
4. 項目の全スライスと事前検証が揃ったら、main-integrationへfast-forwardする。
   mainが進んでいる場合は専用ブランチ側で統合・再検証し、履歴を書き換えない。
5. マージ後のmainで共通検証を再実行し、報告書に検証したコミット、ログ、制約を記録する。
   失敗は専用ブランチで修正・再統合する。証跡だけの最終変更は文書差分として確認する。
6. コミット成功、`git log -1 --oneline`、両worktreeの状態を確認して報告する。

共通検証はAGENTS.mdのVS 2026／NMake／x86手順に従う。
Windows portable・Metamod Debugの全CTestとwarnings-as-errors、Linux x86
portable Debug、Release DLLのPE32／指定6 exportを確認する。DebugのinspectorはON。
新規テストはCTestへ登録し、既存移動・視認の回帰も維持する。
push、ブランチ削除、worktree削除は行わない。FocalSpanのローカルindex/configはコミットしない。

実装・適用可能なオフライン検証と、post-Finishの実機受入は別に管理する。
未完了項目を推論やsynthetic fixtureの成功だけで完了扱いしない。

## P4-01 — Visible observation（完了）

- **Goal:** 世代付きの幾何学的な視認結果を、SDK非依存の値として公開する。
- **Interfaces/modules:** Vision、ObservationBatch、VisionAdapter、StartFrame。
- **Implementation / evidence:**
  - [x] 距離・視野・頭部／胴体トレース、有限予算、公平な走査を実装。
  - [x] Adapterへの接続とライフサイクル再検証を実装。
  - [x] [個別計画](p4-01-visible-observation.md)と
    [検証報告](../reports/p4-01-visible-observation.md)を記録。
- **Acceptance:** 直接視認が証明された位置だけを公開し、無効化後に再公開しない。
- **Deferred:** 煙・フラッシュはP4-05。記憶はP4-02。実機受入はpost-Finish。

## P4-02 — Visual memory and confidence decay（完了）

- **Goal:** 視認した全相手の最終位置・時刻を保持し、確信度を減衰させる。
- **Interfaces/modules:** VisualMemoryModel、MemoryFrame、MemorySnapshot、VisionAdapter。
- **Implementation / evidence:**
  - [x] 固定32観測者×31対象、確信度1.0から5秒で線形失効を実装。
  - [x] 重複・逆順・世代・時計ガード、再視認、ライフサイクル退役を実装。
  - [x] 専用ブランチからmainへ統合し、マージ後も全検証を実施。
    [個別計画](p4-02-visual-memory.md)／[検証報告](../reports/p4-02-visual-memory.md)。
- **Acceptance:** 未視認の現在位置で更新しない。マージ後Windows portable 43/43、
  Metamod 51/51、Linux portable 42/42は当時の証跡であり、後続変更後の結果ではない。
- **Deferred:** 所属／ラウンドはP4-03、統合はP4-06、候補AreaはP4-07。

## P4-03 — 所属・ラウンド・観測の識別基盤（完了）

- **Goal:** 別人・別ラウンド・別イベントの知識を混同しない。
- **Why now:** 音・報告・World Modelの共通識別と敵味方判定の前提となる。
- **Files/modules:** 所属情報、共通観測値、CSメッセージ、lifecycle、視認記憶。
- **Interfaces:** RoundGeneration、観測ID、観測元、元の観測時刻、受信時刻、
  `Self / Ally / Opponent / Unknown`。P4-02は全相手を記憶する契約を維持する。
- **Implementation outline / commit slices:**
  - [x] 人間を含む全プレイヤーの世代付き所属をTeamInfoから管理し、未知を敵扱いしない。
  - [x] 検証済みラウンド開始通知でRoundGenerationを更新し、前ラウンドの記憶と待機情報を破棄する。
    ResetHUD単独では全体のラウンド開始を判定しない。
  - [x] 共通観測識別と元の時刻／受信時刻を追加する。遅延で寿命を延長しない。
  - [x] 切断・所属・世代・ラウンド変更を位置情報なしで通知し、テスト・報告を揃える。
- **Evidence:** [検証報告](../reports/p4-03-perception-identity.md)。main統合後Windows portable 44/44、
  Metamod 53/53、Linux portable 43/43、Release PE32/x86と指定6 exportを確認。
- **Tests:** 未知チーム、途中参加、チーム変更、スロット再利用、重複ラウンド通知、古い観測。
- **Acceptance criteria:** 誤った敵対判定やラウンドをまたぐ記憶復活がない。
- **Dependencies:** P4-01・02、既存TeamInfo decoderとhost identity。
- **Risks:** メッセージの欠落・重複・途中接続。未知状態は明示して失敗を隠さない。
- **Deferred / live validation:** 標準CSの実際の通知順序の確認はpost-Finish。戦闘判断はPhase 5以降。

## P4-04 — 匿名の聴覚観測（完了）

- **Goal:** 受信した音イベントから、聞こえた可能性のある領域を生成する。
- **Why now:** 視認以外の観測を統合する前に、情報境界と有限処理を確立する。
- **Files/modules:** Metamod音／イベントhook、CS対応表、Core聴覚値、固定イベントキュー。
- **Interfaces:** SoundObservationは音種、観測ID、元の時刻、量子化領域、不確実性を持つ。
  発音者ID・正確な音源座標はAdapter外へ公開しない。
- **Implementation outline / commit slices:**
  - [x] 標準の音・再生イベントhookを接続し、precache名とイベント番号の対応を管理する。
    固定イベント番号を埋め込まない。
  - [x] 足音・銃声・爆発について取得経路と対応表を記録する。未知イベントは診断して除外し、
    速度や武器状態から音を捏造しない。
  - [x] Adapter内で距離・音量・attenuationから受聴可否を判定し、初期幅256 unitsで匿名化する。
    壁の透過・反射は再現したと扱わない。
  - [x] 固定256件キュー、最大32件/frame、観測者あたり最大16件保持を実装する。
    古い順で処理し、満杯時は新規イベントを破棄して計数する。
  - [x] 不正値、重複、距離境界、overflow、退役のテストと対応能力の報告を追加する。
- **Evidence:** [検証報告](../reports/p4-04-anonymous-sound.md)。main統合後Windows portable 46/46、
  Metamod 56/56、Linux portable 45/45、Release PE32/x86と指定6 export。
  PM_PlaySoundの足音網羅性とHE TE_EXPLOSIONの取得は未対応／未確認として残す。
- **Tests:** 無音、範囲外、同一音再送、未知sample/event、キュー超過、世代／ラウンド変更。
- **Acceptance criteria:** 匿名音源を未視認プレイヤーの記憶に自動結合しない。
  取得できない音種は未対応として明示し、対応済みと主張しない。
- **Dependencies:** P4-03。
- **Risks:** 足音やクライアント側再生音が同じhookで取得できるとは限らない。
- **Deferred / live validation:** 実機イベント網羅性・聴覚調整はpost-Finish。音響伝播の高度化は後続。

## P4-05 — 煙・フラッシュによる視認制約（完了）

- **Goal:** 幾何学的な視認に、CSの視覚妨害を接続する。
- **Why now:** 妨害中の誤った直接視認をWorld Modelへ取り込まないため。
- **Files/modules:** 煙イベント変換、ScreenFade decode、期限付き領域、VisionAdapterのサンプル判定。
- **Interfaces:** 有限の視覚妨害入力・拒否理由・能力診断。既存ObservationBatchの成功契約を維持する。
- **Implementation outline / commit slices:**
  - [x] 発生経路を確認した煙イベントだけを期限付き領域へ変換する。
    一般的な煙エフェクトをすべてスモークグレネードと解釈しない。
  - [x] 半径・持続時間を設定値として近似モデルを定義し、有効な煙と交差する視線サンプルを拒否する。
    具体値と対象イベントは出所を確認した対応表へ記録してから実装する。
  - [x] 観測者宛ての検証済み白色ScreenFadeを効果時間付きで扱い、その間の新規視認を抑制する。
    黒フェード等はフラッシュと扱わない。
  - [x] 煙領域は最大32件とする。超過時は新規視認を保守的に抑制し、超過状態も期限付きで管理する。
  - [x] 妨害中の記憶減衰、効果終了、能力不足、マップ／ラウンド退役のテスト・報告を追加する。
- **Evidence:** [検証報告](../reports/p4-05-visual-interference.md)。初回createsmoke.scのみ、
  既定半径115 units／22秒。main統合後Windows portable 48/48、Metamod 59/59、
  Linux portable 47/47、Release PE32/x86と指定6 export。実描画との一致はpost-Finish受入。
- **Tests:** 煙の発生・失効・重なり、頭部／胴体、フラッシュ終了、受信先違い、未知形式、overflow。
- **Acceptance criteria:** 妨害によって最終確認位置を更新しない。未取得効果を対応済みと主張しない。
- **Dependencies:** P4-03、P4-04のイベント番号管理、P4-01・02。
- **Risks:** クライアント描画と幾何学的近似の差、ゲーム版ごとのイベント差。
- **Deferred / live validation:** 実際の煙の見え方・効果時間との照合はpost-Finish。

## P4-06 — 観測統合とWorldSnapshot（完了）

- **Goal:** 後続Plannerが利用できる一つの知識モデルへまとめる。
- **Why now:** 直接視認・匿名音・後続の報告を同じ完全情報と誤解させない。
- **Files/modules:** World Model reducer、WorldSnapshot、Adapter公開部、診断。
- **Interfaces:** 観測者別read-only WorldSnapshot。P4-02のVisualMemoryModelを内部で再利用し、
  既存読み取りAPIは維持する。同じ視認記憶を別ownerで二重管理しない。
- **Implementation outline / commit slices:**
  - [x] 視認記憶と匿名音源のownerと公開snapshotを追加する。
  - [x] 出所と元の時刻を保持し、音でlastSeenを更新しない。受信数だけで確信度を加算しない。
  - [x] 視認は1.0／5秒、音は0.5／元の発生時刻から3秒の線形失効を実装する。
  - [x] 更新順を「無効化→減衰→観測ID順の取り込み→公開」に固定する。
  - [x] 件数・年齢・出所・拒否理由・キュー超過・遅延の診断と決定的replayを追加する。
- **Evidence:** [検証報告](../reports/p4-06-world-snapshot.md)。main統合後Windows portable49/49、
  Metamod61/61、Linux portable48/48、Release PE32/x86と指定6 export。
- **Tests:** 遅延、重複、逆順、同時入力、別観測者、寿命境界、古い位置の再送。
- **Acceptance criteria:** 古い情報で位置を巻き戻したり寿命を延長したりしない。
  匿名音源と個人の視認記憶を区別でき、同じ入力列から同じsnapshotを得られる。
- **Dependencies:** P4-03〜05、P4-02。
- **Risks:** source別時刻の混同、古いcached batchの再取り込み、snapshot寿命。
- **Deferred / live validation:** 戦闘・Planner接続は後続Phase。実機時系列はpost-Finish。

## P4-07 — NAV上の候補位置分布

- **Goal:** 見失った相手の候補Area分布を、最終確認位置と併せて提供する。
- **Why now:** 有効期限・出所が確立した記憶を入力にして、不確実性を表現する。
- **Files/modules:** World ModelのNAV連携、既存NavSpatialIndex／NavGraph、候補分布と有限scheduler。
- **Interfaces:** マップ世代付きread-only NAV snapshot、最大32 Areaの重み、unknownMass、更新時刻・遅延。
- **Implementation outline / commit slices:**
  - [ ] 移動セッション内部に依存せずNAV snapshotを渡し、最終確認位置をAreaへ対応付ける。
    NAVなし・対応Areaなしでは位置を保持し、分布は利用不可とする。
  - [ ] 200ms刻みの離散拡散を実装する。重みの半分を現在Areaへ残し、残りを既知の有向接続先へ等分する。
    出口がなければ全量保持する。対象の実速度や動的障害物を知っているとは扱わない。
  - [ ] 1対象最大32 Areaとし、省略した重みをunknownMassへ移す。
    残った候補を勝手に確信度1.0へ正規化しない。安定したID順で同点を解決する。
  - [ ] 全体最大256接続/frame、公平な観測者／対象間の繰り越しを実装する。
    未完了更新は公開せず、更新時刻と遅延を報告する。期限切れジョブは破棄する。
  - [ ] 再視認で観測Areaへ戻し、記憶失効・NAV差し替え・ラウンド変更で退役させる。
- **Tests:** 分岐、循環、一方通行、孤立Area、NAVなし、候補切り詰め、予算超過、再視認、NAV差し替え。
- **Acceptance criteria:** 決定的に動作し、既知重みと不明重みの合計を維持する。
  未視認の現在位置を照会しない。確信度と位置分布の重みを混同しない。
- **Dependencies:** P4-06、Phase 2の静的NAV queryと検証済みsnapshot。
- **Risks:** 大規模graphの遅延、拡散近似と実際の移動との差、範囲外の外部接続。
- **Deferred / live validation:** 動的障害物・速度を含む高度な推定は後続。実際の敵移動との比較はpost-Finish。

## P4-08 — 明示的な味方報告

- **Goal:** 味方からの知識を直接視認と区別して利用する。
- **Why now:** 所属とsource別World Modelを使って、伝聞の権限・寿命を制御する。
- **Files/modules:** TeamReport、有限配信キュー、World Modelの伝聞保持、operator command。
- **Interfaces:** 報告者、受信者、元の観測ID／時刻、対象、報告位置。
  後続の行動選択用の報告関数と`astrabot_report <reporter> <target>`を追加する。
  actor指定は既存と同じslot:generationとし、任意座標の入力経路にはしない。
- **Implementation outline / commit slices:**
  - [ ] 報告者が500ms以内に直接視認した相手だけを報告可能とする。
    伝聞の再転送、匿名音の個人への結合を禁止する。
  - [ ] 現在同チームの有効なAstraBotだけに配信する。未知所属、敵、別ラウンド、古い世代を拒否する。
  - [ ] 明示的な関数／console操作でのみ送信し、常時自動共有しない。
  - [ ] 上限確信度0.5、元の観測から5秒で失効する伝聞を直接視認と別に保持する。
    直接視認を優先し、再送や受信時刻で寿命を延長しない。
  - [ ] 重複・不正送信・所属変更・切断・出所保持のテストと報告を追加する。
- **Tests:** 報告前後の知識差、味方／敵／未知への送信、再送、遅延、報告者切断、世代・ラウンド変更。
- **Acceptance criteria:** 報告後にだけ受信者へ伝聞が現れ、出所・元の時刻を確認できる。
- **Dependencies:** P4-03・06、先行実装済みP4-07と回帰統合。
- **Risks:** 再送による確信度増幅、退役した報告者のID再利用、受信時刻との混同。
- **Deferred / live validation:** 人間の自由文chat／radioからの座標推定、自動報告の行動判断は対象外。
  実機console操作と配信の確認はpost-Finish。

## P4-09 — Phase 4オフライン完了ゲート

- **Goal:** 個別機能を一連の知覚・記憶フローとして再現可能な証跡で閉じる。
- **Why now:** 項目単体の成功だけでは統合時の漏洩・遅延・処理集中を確認できない。
- **Files/modules:** 統合replay、負荷matrix、証跡checker、対応能力表、完了報告、post-Finish受入資料。
- **Interfaces:** 入力観測列、source revision、設定、snapshot／診断列と期待結果を持つ再現可能な証跡。
- **Implementation outline / commit slices:**
  - [ ] 視認→遮蔽→減衰→音→味方報告→再視認を統合replayにする。
  - [ ] 煙／フラッシュ、ラウンド／マップ変更、切断、世代再利用、NAVなし、キュー超過を組み合わせる。
  - [ ] 1／8／16 Botと8／16／100ms frameのmatrixで、上限、公平性、遅延、メモリを記録する。
  - [ ] 既存移動の到着・停止・復旧と並行して検証し、知覚追加による移動回帰を検出する。
  - [ ] 全共通gateとマージ後検証を実施し、対応イベント表、未対応能力、既定値、API、ログを報告する。
  - [ ] 実機受入を未完了の別表に残し、P4全体の実装・適用可能なオフライン検証完了を判定する。
- **Tests:** 全P4契約、同一入力の決定性、処理集中、長時間の退役・再参加、移動との共存。
- **Acceptance criteria:** P4-03〜08の実装・テスト・文書が揃い、情報漏洩、無制限処理、
  世代をまたぐ記憶復活がないことをオフラインで検証できる。
- **Dependencies:** P4-01〜08。
- **Risks:** synthetic合格を実機互換性・性能保証に読み替えること。対応表の未取得経路を隠すこと。
- **Deferred / live validation:** 下表を維持する。Phase 4完了はプロジェクト全体Finishではない。

## Dependency order and compatibility evidence

実施順はP4-03→04→05→06→07→08→09とする。各項目は全スライスの完了後に閉じる。
P4-01・02の完了状態は再定義しないが、後続変更時には回帰検証する。

音・煙・フラッシュの対応表には検証したSDK／GameDLLの版、イベント名／message形式、
出所、対応／未対応とテストを記載する。標準Metamod経路を基準にし、ReAPIを必須にしない。
計画調査時のMetamod-P SDK pinは`7ec9b014f8c0a947a724644aebe34eb33706e44b`。
ローカルReGameDLL_CSは`679973265e1ac99a43193119e0da212ee568f5f9`であり、既存研究資料のpinとは異なる。
参照時は差を明示し、実装開始時に対象のsymbol／版／licenseを再確認する。
イベント取得の未確認部分や煙の調整値は、該当スライスの証拠確認事項であり実装済み仕様ではない。

## Post-Finish acceptance matrix

| Status | Scenario | Required evidence |
|---|---|---|
| [ ] | 実機で足音・銃声・爆発を取得 | engine／GameDLL／Metamod版、対応イベント表、受聴と除外の記録 |
| [ ] | 煙・フラッシュ中の視認抑制と復帰 | map、効果イベント、実描画との照合、時系列、近似の制約 |
| [ ] | round／map切り替え、再参加、所属変更 | 世代とsnapshotの退役、古い観測の不復活 |
| [ ] | 明示的報告前後の味方知識 | console入力、送受信者、元の観測と出所、非共有時の状態 |
| [ ] | 32-slot／16-Botと移動の共存 | server FPS、観測遅延、予算超過、メモリ、移動結果 |
| [ ] | Linux HLDS／ReHLDS実機動作 | 正確な構成、起動・運用手順、ログ、失敗時のfollow-up |

これらはプロジェクト全体のFinish確認前に開始しない。オフライン合格でチェックを付けない。

## Recommended next session

次は**P4-07の全スライス**。この計画を読み、mainと指示の現状、WorldSnapshotとread-only NAVの
契約を確認して専用worktreeで開始する。P4-08以降を先取りして実装しない。
各完了項目に報告書リンクを追加し、このファイルのチェックとSTATEを更新する。
