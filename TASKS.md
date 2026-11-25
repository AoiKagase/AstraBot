# AstraBot CSBot準拠タスク一覧

更新：2026-11-25。今回の統合前基点：`68726e1`（ローカルmaster）。

**大項目49件：限定完了2件、未完47件。部分対応は未完に含む。** チェック済みの修正単位があっても、大項目の未完数は減らない。S14は現対戦milestone外の差分として残し、対応済みには数えない。

正本：[全ソース監査](docs/parity/FULL_SOURCE_AUDIT.md)／[全項目・ソース台帳](docs/parity/FULL_SOURCE_INVENTORY.json)。以下の内訳は監査根拠を作業に分解したもので、内訳件数を独立した監査件数として加算しない。実機受入れ・オフライン合格・実装済みを区別する。

| 群 | 未完大項目 |
|---|---|
| 1. 視線・戦闘・武器 | 11 |
| 2. 状態・目的・人質・VIP | 16 |
| 3. NAV・移動・知覚 | 6 |
| 4. 公開観測・イベント・発話・管理・profile | 12 |
| 5. 固定参照リプレイ・両OS実機・継続性能 | 2 |

## 検証の実施単位

2026-11-24のユーザー指示により、上記5群を大分類として全体検証をまとめる。各修正単位では変更対象のビルド・重点テストと必要な実機確認に絞り、検証済み範囲を記録してコミット・ローカルmasterへマージする。両OSの全CTest・PE/ELF export検証・固定履歴比較・広範な実機回帰は大分類完了時に実施する。重点合格を全体合格や未実施の実機受入れとして扱わない。共有fixtureのCTest逐次実行、HLDS実PID終了確認、同一検証サーバーの再利用、push禁止は継続する。指示前に開始したNoise順序修正の全ビルド・後続検証は既存処理として完走させる。

## ブランチの後始末

各修正単位のローカルmasterマージと指紋確認が完了したら、masterへ切り替えて当該作業ブランチを安全削除する。未マージ、別作業用、他worktreeで使用中のブランチは削除しない。次の修正はmasterを基点に新しい`codex/`ブランチで始める。

## 現在の作業
C03-5：Huntかつ厳密well-past-safeかつgrenadeのMUST_EQUIPを水没後/unsafe前へ接続。normal HuntのAttack重複を含み、調整済みsafeTime、公開active優先/fallback、callback失効の旧full中断、共通装備時計を保持。成立RED→Windows重点31/31・25入力、独立review指摘0。最終DLLの同じ既存as_oilrigで公開HE入力→AK47を132/132実commandで保持。dry確認、equipBest old4→28 sent1、実HLDS31604 exit0・実PID[]・core/fixture復元・保護11一致。 自然発生/全状態/profile/blind/武器/Linuxと既存C03/V02未完範囲は残し、大項目未完47を維持。[範囲と証拠](docs/parity/UPDATE_HUNT_EQUIPMENT_C03_5.md)。

C03-4a：IdleのNAV確保・zombie拒否後、safeなら一意grenadeを優先し、選べなければskill厳密>.33でknifeを選択。所持sniper拒否はactive grenade判定より先、smoke可、装備時計/cvar許可非依存。成立RED→Windows重点30/30・28入力と装備時計両方向、独立review指摘0。最終DLLの同一既存as_oilrigで非VIP Wolfを公開入力し、HEは184実command中184更新、knifeは178中178更新で選択を確認。各equipSafe old28→4/29 sent1を照合、両実exit0・PID[]・core/fixture復元/保護11一致。 複数grenadeの所持リスト先頭は公開maskだけでは確定できず、自然発生/全境界/LinuxとV02旧原因を残す。大項目未完47を維持。[範囲と証拠](docs/parity/IDLE_SAFE_EQUIPMENT_C03_4A.md)。

C03-3：round IsSafeとは独立した敵未視認>5秒のpistol→使用可能primaryを通常Updateへ接続。成立RED→最終29重点/24入力合格、実機loaded pistol→AK47を2回限定機能採録。quit後Steam待機で正常終了不合格、特定PID14156のユーザー許可後に終了0xFFFFFFFF・実PID[]・設定復元/保護11一致。early-safe・自然戦闘/全境界とV02旧原因は残す。[範囲と証拠](docs/parity/UPDATE_PISTOL_REARM_C03_3.md)。

C03-2：unsafe総弾薬枯渇/既知activeなし・退避外knifeを通常Updateへ接続。正規RED2種→最終28重点合格、実機は両切替を限定機能採録。quit後Steam待機で正常終了不合格、ユーザー閉鎖後実PID[]・設定復元/保護11一致、終了コード未取得。safe5秒/early-safe・未採録範囲とV02は残す。[範囲と証拠](docs/parity/UPDATE_UNSAFE_C03_2.md)。

C03-1：通常Updateの水没MUST_EQUIP接続を実装。正規RED→最終27重点シナリオ合格、実as_oilrigの高skill公開入力でwater3の132更新中131更新Glock保持、最終両実exit0・復元一致。その他C03装備分岐・自然遊泳/全境界・Linuxは残す。[範囲と証拠](docs/parity/UPDATE_UNDERWATER_C03_1.md)。

C02-2d12：公開足音と専用NAVでFetchBomb保持Noiseを限定受入れ。移動gate16更新、静止ゼロClear1更新、候補1の抽選後LOW拒否2更新、各両exit0・復元一致。通常NAV自然発生・blind独立採録/全境界とV02旧原因を残す。[範囲と証拠](docs/parity/FETCH_NOISE_LIVE_C02_2D12.md)。

C02-2d11：専用encounter候補0/1の最終DLL対照を限定受入れ。候補0のintervalのみ2更新・同fullの静止ゼロClear読点なし、候補1のspot抽選正例、各両exit0・復元一致。保持Noise/自然発生と過去Steam待機原因は残す。[範囲と証拠](docs/parity/FETCH_ZERO_LIVE_C02_2D11.md)。

C02-2d10：最新製品・専用NAVのFetchBombで、他条件が開いたNative保持13更新（後の正例と同じencounterは6更新）の抽選前拒否、静止ゼロClear、解除後の候補1件抽選を機能採録。静止ゼロの先行2試行は両exit0。最終移動対照を含むlive6/7/8はquit後Steam待機で正常終了不合格、各PIDのユーザー許可後に終了・通常NAV/core復元・保護一致。製品変更なし。[範囲と証拠](docs/parity/FETCH_ADMISSION_LIVE_C02_2D10.md)。当該単位時点では保持Noise/移動候補ゼロ/通常NAV自然発生とV02が未完。移動候補ゼロは後続C02-2d11で限定受入れ済み。

C02-5b：専用NAVで全可視5点/4点・同一area自明2点経路を最終DLLで採録し、長短/reset履歴と25刻み・36+10・4方向bufferを因果対照。固定関数再生4/4、endpoint-only mutationはexit1。全3試行の両プロセスexit0、通常NAV/core復元、製品変更なし。[限定実機の証拠](docs/parity/GRENADE_VISIBLE_LIVE_C02_5B.md)。通常NAV自然発生や失敗履歴/RemovePath/梯子suffix等を完了には数えない。

C02-5a：失敗した固定ComputePathの位置書込みを実関数19対照で監査。成功経路のみの履歴モデルをmutationで反証したが、再現入力は欠落梯子parentの明示不整合。通常の固定探索/Astra公開入力で同じ失敗に至る経路は未確認のため、製品RED・修正完了とは数えない。[監査と残条件](docs/parity/GRENADE_FAILED_TAIL_C02_5A.md)。後続C02-5bで全可視/reset/rebuildの限定実機を追加。失敗履歴は到達入力の確定から継続する。

直前のC02-5では成功経路の全可視tail履歴を限定実装し、Windows重点32/32。実機は不可視cornerの25full保持→attack解放のみ限定受入れ、両exit0・復元/指紋一致。全可視実機、失敗生成の書込み済みslot、NAV削除RemovePath、はしごsuffix等は未完。[詳細と証拠](docs/parity/GRENADE_VISIBLE_TAIL_C02_5.md)。
## 完了した修正単位（大項目の完了とは別）

| 単位 | 完了範囲・証拠 |
|---|---|
| ☑ C03-5（実装・限定機能） | [Hunt MUST装備](docs/parity/UPDATE_HUNT_EQUIPMENT_C03_5.md)。Windows重点31/31・25入力。最終DLLの同じ既存as_oilrigで公開HE入力→AK47を132/132実commandで保持。dry確認、equipBest old4→28 sent1、実HLDS31604 exit0・実PID[]・core/fixture復元・保護11一致。 未採録範囲は残す |
| ☑ C03-4a（実装・限定機能） | [Idle early-safe一意grenade/knife](docs/parity/IDLE_SAFE_EQUIPMENT_C03_4A.md)。Windows重点30/30・28入力と時計両方向。最終DLLの同一既存as_oilrigで非VIP Wolfを公開入力し、HEは184実command中184更新、knifeは178中178更新で選択を確認。各equipSafe old28→4/29 sent1を照合、両実exit0・PID[]・core/fixture復元/保護11一致。 複数grenade等は残す |
| ☑ C03-3（実装・限定機能） | [未視認5秒超のpistol再装備](docs/parity/UPDATE_PISTOL_REARM_C03_3.md)。Windows重点29/29・24入力、実loaded pistol→AK47を2回。正常終了はV02不合格、許可済み終了後復元一致 |
| ☑ C03-2（実装・限定機能） | [unsafe再装備](docs/parity/UPDATE_UNSAFE_C03_2.md)。Windows重点28/28、実機両切替を採録。正常終了はV02不合格、閉鎖後実PID[]・復元一致 |
| ☑ C02-2d12（限定実機） | [FetchBomb保持Noise](docs/parity/FETCH_NOISE_LIVE_C02_2D12.md)。移動gate16更新・静止ゼロ1更新・候補1 LOW拒否2更新、各両exit0。製品変更なし |
| ☑ C02-2d11（限定実機） | [FetchBomb移動候補ゼロ](docs/parity/FETCH_ZERO_LIVE_C02_2D11.md)。候補0/1の因果対照、各両exit0。V02旧原因は未確定 |
| ☑ C02-2d10（限定機能） | [FetchBomb移動Native受付・静止ゼロ](docs/parity/FETCH_ADMISSION_LIVE_C02_2D10.md)。抽選前拒否→Clear→再開を採録。製品変更なし、最終移動試行の正常終了はV02不合格 |
| ☑ C02-5b（限定実機） | [全可視・自明経路tail](docs/parity/GRENADE_VISIBLE_LIVE_C02_5B.md)。専用NAVで長短/reset履歴・25刻み・bufferを採録、固定再生4/4。製品変更なし |
| ☑ C02-5a（監査のみ） | [失敗経路tail監査](docs/parity/GRENADE_FAILED_TAIL_C02_5A.md)。固定19対照と誤モデルmutation。製品変更なし、失敗履歴の実装/通常入力到達性は未完 |
| ☑ C02-5 | [成功経路の全可視tail履歴](docs/parity/GRENADE_VISIBLE_TAIL_C02_5.md)。正規RED→GREEN、Windows重点32/32。実機は不可視corner限定、全可視/失敗生成等は残す |
| ☑ C02-4 | [全可視/自明経路tail監査](docs/parity/GRENADE_VISIBLE_TAIL_C02_4.md)。固定実関数11対照、Windows corner重点7/7。製品変更なし、tail履歴の実装は未完 |
| ☑ C02-1 | [保持・認識敵への投擲・独立look/投擲時計](docs/parity/GRENADE_LIFECYCLE_C02_1.md)。各記録の限定範囲で検証・コミット・masterマージ済み |
| ☑ C02-2a | [手榴弾視線中の破壊物LOW受付拒否](docs/parity/GRENADE_OBSTACLE_ADMISSION_C02_2A.md)。各記録の限定範囲で検証・コミット・masterマージ済み |
| ☑ C02-2b | [音要求拒否時のRNG・抑制時計](docs/parity/GRENADE_NOISE_ADMISSION_C02_2B.md)。各記録の限定範囲で検証・コミット・masterマージ済み |
| ☑ C02-2c1 | [静止候補ゼロでのClearLook](docs/parity/GRENADE_ZERO_APPROACH_C02_2C1.md)。各記録の限定範囲で検証・コミット・masterマージ済み |
| ☑ C02-2c2 | [静止候補ありで抽選後にLOW拒否](docs/parity/GRENADE_HUNT_ADMISSION_C02_2C2.md)。各記録の限定範囲で検証・コミット・masterマージ済み |
| ☑ C02-2c3 | [moving Huntで遭遇抽選前にNative視線受付を確認](docs/parity/GRENADE_MOVING_ADMISSION_C02_2C3.md)。各記録の限定範囲で検証・コミット・masterマージ済み |
| ☑ C02-2c4 | [accepted-BEGINでの旧Hunt LOW所有解除](docs/parity/GRENADE_OWNER_RETIREMENT_C02_2C4.md)。Win182/Linux181＋各7exports、同full実機採録、C4 0/7/8合格。`889c35c` コミット・master FF済み |
| ☑ C02-2d1 | [新規Noise順序・再入停止・Hide姿勢](docs/parity/LOOK_NOISE_ORDER_C02_2D1.md)。重点14/14、Windows初回182/184＋修正後3/3、Linux183/183、各7exports、実機同full順序を限定受入れ。`438842e` コミット・master FF済み |
| ☑ C02-2d2 | [保持NoiseのHunt受付](docs/parity/RETAINED_NOISE_ADMISSION_C02_2D2.md)。Windows重点12/12、同じ実機の静止保持条件を限定受入れ。moving/zero実機・Guardは未完 |
| ☑ C02-2d3 | [Guard受付・独立Ahead](docs/parity/GUARD_LOOK_ADMISSION_C02_2D3.md)。重点16/16＋追加1/1、実機Native受付31更新。抽選時計・Noise/zero実機は未完 |
| ☑ C02-2d4 | [Guard/Hunt共有視線時計](docs/parity/GUARD_LOOK_CLOCK_C02_2D4.md)。重点16/16＋再入補強1/1、実機Native保持63更新で期限を確認。状態間・Noise/zero実機は未完 |
| ☑ C02-2d5 | [Guard保持Noise実機](docs/parity/GUARD_NOISE_LIVE_C02_2D5.md)。製品変更なし、54更新で時計・抽選後拒否・Noise保持を限定確認 |
| ☑ C02-2d6 | [Guard候補ゼロ実機](docs/parity/GUARD_ZERO_LIVE_C02_2D6.md)。専用NAVでNative1更新・Noise1更新のClear、Native投擲待機保持を限定確認 |
| ☑ C02-2d7 | [Hunt/Guard共有時計の状態間継続](docs/parity/LOOK_CLOCK_TRANSITIONS_C02_2D7.md)。通常NAV・C4落下/回収で往復の期限保持を実機確認 |
| ☑ C02-2d8 | [後段Noise再入停止](docs/parity/LATE_NOISE_ABORT_C02_2D8.md)。正規RED→GREEN、Windows重点18/18、同一実機で通常Noise継続3更新を限定確認 |
| ☑ C02-2d9 | [FetchBombと直接Root再帰](docs/parity/FETCH_ROOT_REENTRY_C02_2D9.md)。重点20/20、静止Native LOW拒否4回。実機は機能のみ、正常終了は不合格・復元済み |
| ☑ C02-3 | [定義済みcorner投擲](docs/parity/GRENADE_CORNER_C02_3.md)。正規RED→GREEN、最終重点29/29＋追加9/9、通常NAV自然safe境界のflash実機入力を限定受入れ。全可視tail・HE/進行後drop/梯子実機等は残す |

既存Plantの早期判断・MoveTo開始順・末尾入力/退出・Plant前Idle経路抑止・cold Idle/Buy視線供給も限定完了。残る自然拾得・実水没・標準GameDLL受入れはS13/C14等へ分離している。過去の完了範囲は[STATUS](docs/parity/STATUS.md)を参照。

## 1. 視線・戦闘・武器（11件未完）

### ☐ C02 — 手榴弾開始条件と保持/投擲の実行経路

状態：部分対応。

- [x] accepted-BEGIN時の旧Hunt LOW所有を退役する（C02-2c4、`889c35c`、検証・master FF済み）
- [x] 新規Noiseのhandled-return・限定的な再入停止・Hide姿勢を合わせる（C02-2d1、`438842e`、検証・master FF済み）
- [x] 保持NoiseのHunt静止/移動受付を合わせる（C02-2d2、重点12/12、実機静止条件のみ限定受入れ）
- [x] GuardのNoise/Native受付・ゼロ候補Clear・独立Aheadを実装する（C02-2d3、重点16/16＋追加1/1、実機Native受付のみ限定受入れ）
- [x] 記憶敵候補・geometry再構築等によるGuard抽選時計のresetを修正する（C02-2d4、共有時計、重点16/16＋1/1、実機Native保持63更新）
- [x] Hunt↔Guardをまたぐ共有静止時計の継続を実機対照で確認する（C02-2d7、通常NAV・C4落下/回収、期限全更新との照合）
- [x] Guard保持Noiseの実機受入れを確認する（C02-2d5、54更新の限定対照）
- [x] Guardゼロ候補の実機機能を確認する（C02-2d6、専用NAVでNative1更新・Noise1更新。Noise試行の正常終了は不合格、V02で追跡。通常NAV自然発生は未確認）
- [x] 他状態の後段Noise再入停止を合わせる（C02-2d8、MoveToの正規4条件・Windows重点18/18。実機は通常継続smokeのみ）
- [x] FetchBomb静止Native受付と直接同時刻Root再帰を限定検証する（C02-2d9、重点20/20。nested fullはschedulerで受付抑止。実機45静止/4拒否/8移動Native保持、正常終了はV02不合格）
- [x] FetchBomb移動Nativeの抽選前受付拒否と解除後の再開を限定機能採録する（C02-2d10、最終13更新・同一encounter厳密6更新→静止Clear→候補1件。最終試行の正常終了はV02不合格）
- [x] FetchBomb静止候補ゼロでNative視線Clearを限定確認する（C02-2d10、同full前後の1→0と期限更新。先行2試行は各両exit0、通常NAV自然発生やpin保持は含まない）
- [x] FetchBomb移動候補ゼロを専用NAVで限定受入れする（C02-2d11、intervalのみ2更新・spot抽選/同fullの静止ゼロClear読点なし・期限継続、候補1正例1更新、各両exit0）
- [x] FetchBomb保持Noiseを公開足音/専用NAVで限定受入れする（C02-2d12、移動gate16更新・静止ゼロClear1更新・候補1の抽選後LOW拒否2更新。静止期限前13/22更新、各両exit0）
- [ ] FetchBomb保持Noise/候補ゼロの通常NAV自然発生・blind内部の独立採録・全境界等の未採録実機範囲を検証する（C02-2d10/2d11/2d12は専用NAVの限定対照。全safe/blind/skill/期限境界と過去の終了原因は残す）
- [x] 安全時間終了時の定義済みcorner投擲を実装・限定検証する（C02-3、最終Windows重点29/29＋追加契約/武器/float9/9、通常NAVの自然flash境界で開始→22full保持→実attack解放）
- [x] 全可視/自明経路で固定参照が読む未使用tailの挙動を監査する（C02-4、固定実関数11対照。初期zero・履歴保持・RemovePath消去・最大長slot256境界を確定。製品変更なし）
- [x] 成功経路の全可視tail履歴を限定実装する（C02-5、初期zero・長短経路再利用・ResetValues/DestroyPath/round保持・新bot消去、容量/旧map拒否。重点32/32、当該時点では全可視実機未受入れ。後続C02-5bの限定受入れを参照）
- [x] 失敗ComputePathPositionsのground書込み・未書込みsuffix保持を固定実関数で監査する（C02-5a、19対照。探索stubの不整合入力であり製品REDではない）
- [ ] 失敗生成の書込み済みslot履歴の製品対応を実装・検証する（C02-5aで固定失敗の意味を確定。通常探索/Astra公開入力の到達例と正規製品REDは未確認。任意のNoRouteを同一視しない）
- [ ] NAV area削除通知のRemovePath/新tail消去、はしごsuffix/容量超過のnative対応を実装・検証する（C02-5は成功経路限定、配列外slot256は推定しない）
- [x] 全可視/自明経路と長短経路・reset履歴を最終DLLの実機で限定因果対照する（C02-5b：専用NAVの5点/4点・同一area2点、固定再生4/4。通常NAV自然発生/全mapや着弾の受入れは含まない）
- [ ] 進行後drop上下段・梯子index、HE corner実機、着弾/跳ね返り等の未受入れ範囲を確認する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:469`、`include/astrabot/compat/weapon_actions.hpp:43`、`src/core/compat/state_machine.cpp:900`。参照：`regamedll/dlls/bot/cs_bot_update.cpp:347`、`regamedll/dlls/bot/cs_bot_update.cpp:490`、`regamedll/dlls/bot/cs_bot_weapon.cpp:576`。

既存テスト／検証上の注意：astrabot_native_grenade_lifecycle real full/RunPlayerMove path; weapon_actions native clock tests; compat_state_machine Clear receipt before reentrant callback. Valid Clear product RED→GREEN retained; earlier false-positive test excluded.

### ☐ C03 — 通常Updateの装備再選択・安全時間中の装備

状態：部分対応（C03-1 水没、C03-2 unsafe、C03-3 敵未視認5秒超pistol再装備、C03-4a Idle一意grenade/knife、C03-5 Hunt MUSTを接続）。

- [x] 水没禁止の装備中武器に一般MUST_EQUIPを接続する（C03-1、通常full Update正規RED→27重点合格、実water3/使用可能XM1014→Glock、最終両実exit0。実active0/古いknife観測の再装備もオフライン確認）
- [ ] 水没切替の自然遊泳・全profile/閾値/blind・Linux実機を検証する（C03-1は公開位置保持、skill100のWindows限定正例）
- [x] unsafe時の総弾薬枯渇/既知activeなし・退避外knife再装備を接続する（C03-2、正規RED→28重点合格、実機両切替は限定機能採録。正常終了不合格と復元を区別）
- [ ] unsafe再装備の通常戦闘自然発生・全状態/profile/blind・Linux実機を検証する（C03-2は制御入力、正常終了もV02未受入れ）
- [x] 敵未視認が厳密に5秒超のpistol→使用可能primaryを接続する（C03-3、round IsSafeと独立。正規成立RED→29重点/24入力、実loaded pistolの2切替を機能採録、正常終了不合格/許可済み終了後復元一致）
- [ ] 敵未視認5秒超再装備の通常戦闘自然発生・全状態/profile/blind・全武器/Linux実機を検証する（C03-3は制御公開入力のWindows限定受入れ）
- [x] early-safe Idleの一意grenade/knife選択を接続する（C03-4a、正規RED→30重点/28入力、NAV/zombie/sniper/skill境界・共通時計非依存・再入を確認。限定実機は[記録](docs/parity/IDLE_SAFE_EQUIPMENT_C03_4A.md)）
- [ ] 複数種類grenadeの所持リスト先頭を公開観測で確定し、early-safe選択を実装・検証する（maskのID順を仮定しない。非sniperのactive grenadeは保持）
- [ ] early-safe選択の自然発生・全profile/blind/境界・Linux実機を検証する（C03-4aは制御公開入力のWindows限定採録）
- [x] Huntかつwell-past-safe中のgrenadeにMUST_EQUIPを接続・限定検証する（C03-5、固定Update:526-529、成立RED→31重点/25入力、[限定実機と範囲](docs/parity/UPDATE_HUNT_EQUIPMENT_C03_5.md)）
- [ ] Hunt MUST装備の自然発生・全状態/profile/blind/全武器・Linux実機を検証する（C03-5は制御公開入力のWindows限定採録）

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:8`、`src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:74`、`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1374`。参照：`regamedll/dlls/bot/cs_bot_update.cpp:519`、`regamedll/dlls/bot/cs_bot_update.cpp:526`、`regamedll/dlls/bot/cs_bot_update.cpp:533`、`regamedll/dlls/bot/cs_bot_update.cpp:547`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:66`。

既存テスト／検証上の注意：C03-5は[Hunt専用記録](docs/parity/UPDATE_HUNT_EQUIPMENT_C03_5.md)。C03-4aは[Idle専用記録](docs/parity/IDLE_SAFE_EQUIPMENT_C03_4A.md)。C03-3の厳密lastSaw境界・共通装備時計/再入と限定実機は[専用記録](docs/parity/UPDATE_PISTOL_REARM_C03_3.md)。C03-1の正規製品RED/最終27シナリオ/限定実機は[専用記録](docs/parity/UPDATE_UNDERWATER_C03_1.md)。C03-2の17入力・共通5秒境界と限定機能は[専用記録](docs/parity/UPDATE_UNSAFE_C03_2.md)。unsafeの固定参照は総弾薬枯渇であり、clip0＋reserveありのReloadCheckと区別する。weapon_equipment_tests.cpp:268-336はhelper/timer/water matrix、weapon_switch_runtime_tests.cpp:184はprimary-empty→pistolのみ。旧14.51 compiler実体なしのcacheを保持し、今回14.52専用buildを使用。全CTest/両OS/export/固定履歴は未実施。

### ☐ C04 — SilencerCheck要求の実行時接続

状態：差確定。

- [ ] profile/skillに基づくsilencer希望を生成する
- [ ] ReloadCheck後のSilencerCheckとattack/defuse/reload抑止を接続する
- [ ] skill>.7・敵非接触時間・weapon readinessを検証する

根拠：`include/astrabot/compat/weapon_actions.hpp:225`、`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1434`。参照：`regamedll/dlls/bot/cs_bot_weapon.cpp:760`、`regamedll/dlls/bot/cs_bot_update.cpp:556`。

既存テスト／検証上の注意：weapon_actions_tests.cpp:315-325 tests synthetic mode input only.

### ☐ C05 — 遠距離ショットガンから拳銃への切替

状態：差確定。

- [ ] shotgun距離strict1000超でEquipPistolを要求する
- [ ] 発砲照準整合に依存せず切り替わることを検証する

根拠：`include/astrabot/compat/weapon_actions.hpp:236`、`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1374`。参照：`regamedll/dlls/bot/states/cs_bot_attack.cpp:319`。

既存テスト／検証上の注意：weapon_actions_tests.cpp sniper switching coverage; no positive shotgun long-range production case found.

### ☐ C06 — 狙撃銃のscope解除/事前zoom/resumeZoom

状態：差確定。

- [ ] resumeZoomの実行時producerを接続する
- [ ] 静止時の事前scopeと移動時unzoomを合わせる
- [ ] 非Attack・非hiding時のscope解除を接続する
- [ ] 発砲整合より前のAdjustZoomとretry方針を合わせる

根拠：`include/astrabot/compat/weapon_actions.hpp:250`、`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1428`、`src/adapter/metamod/runtime/behavior/bot_behavior_guard.cpp:437`。参照：`regamedll/dlls/bot/states/cs_bot_attack.cpp:331`、`regamedll/dlls/bot/cs_bot_vision.cpp:453`、`regamedll/dlls/bot/cs_bot_update.cpp:593`。

既存テスト／検証上の注意：weapon_actions_tests.cpp:219,270,349-360 covers synthetic scope and retries, not native global/sight range lifecycle.

### ☐ C07 — 敵を見失った後の回避・距離調整

状態：差確定。

- [ ] 敵非可視でもchase開始までdodgeを継続する
- [ ] skill<.66または非可視で距離調整する
- [ ] 再捕捉時にAttack再進入なしのfirstDodge再初期化を防ぐ

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1245`、`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1281`、`include/astrabot/compat/combat_movement.hpp:314`。参照：`regamedll/dlls/bot/states/cs_bot_attack.cpp:481`。

既存テスト／検証上の注意：combat_policy_tests.cpp:233 tests visible range correction, no LOS-loss persistent dodge case.

### ☐ C08 — Combat軽更新/全更新の視認・照準積分順

状態：差確定。

- [ ] Upkeepが直前fullの可視/部位を消費するよう順序を合わせる
- [ ] full側とのaim offset/spring二重更新を解消する
- [ ] 途中遮蔽・friendly trace回数・目標所有を検証する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:270`、`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1238`、`src/core/combat/combat_intent.cpp:304`。参照：`regamedll/dlls/bot/cs_bot_update.cpp:46`、`regamedll/dlls/bot/cs_bot_update.cpp:441`、`regamedll/game_shared/bot/bot.cpp:71`。

既存テスト／検証上の注意：combat_target_log_tests.cpp existing actual aim/cached-light coverage; add independent expected native per-upkeep trajectory and trace-count case.

### ☐ C09 — friendly-fire・敵の盾を含む発砲/脅威判定

状態：差確定。

- [ ] friendly-fire有効時のrecoil＋view10000ray判定を合わせる
- [ ] 遅延shield保護状態と正面盾への発砲抑止を接続する
- [ ] 盾を含む脅威順と一般FFA探索を照合する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:1177`、`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:318`、`include/astrabot/compat/combat_behavior.hpp:16`。参照：`regamedll/dlls/bot/cs_bot_weapon.cpp:54`、`regamedll/dlls/bot/cs_bot_weapon.cpp:78`、`regamedll/dlls/bot/cs_bot_weapon.cpp:849`。

既存テスト／検証上の注意：combat_target_log_tests.cpp:2107 proves obstruction behavior; shield fixture1318 verifies OWN shield control, not enemy shield suppression.

### ☐ C10 — 破壊物対応の装備時計を共通EquipBestへ接続

状態：差確定。

- [ ] 破壊物の独立装備時計を共通EquipBest/DoEquipへ接続する
- [ ] ammo不明・水没・weapon flag・slot選択を合わせる
- [ ] 通常装備直後と破壊物装備後の共通5秒時計を検証する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_obstacles.cpp:44`、`src/adapter/metamod/runtime/behavior/bot_behavior_obstacles.cpp:258`。参照：`regamedll/dlls/bot/cs_bot.cpp:318`、`regamedll/dlls/bot/cs_bot_weapon.cpp:437`。

既存テスト／検証上の注意：obstacle_interaction_runtime_tests.cpp:151-212 isolated throttle/ammo fallback positive, no shared-clock interleave/submersion case.

### ☐ C11 — 敵非可視で再装填中の最終移動停止

状態：差確定。

- [ ] 状態処理の最後にreload中かつ敵非可視なら移動を消す
- [ ] 既存cover選択と最終停止の順序を検証する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:186`、`src/adapter/metamod/runtime/command/bot_command_runtime.cpp:1399`、`include/astrabot/compat/weapon_actions.hpp:220`。参照：`regamedll/dlls/bot/cs_bot_update.cpp:788`。

既存テスト／検証上の注意：weapon equipment/reload-cover tests cover reload request and cover state, not final no-LOS command stop.

### ☐ C13 — BendLineOfSightの整数角・地面探索

状態：差確定。

- [ ] BendLineOfSightの整数VecToYawとBotSIN/COSを合わせる
- [ ] 1〜8192の独自長さ制限を照合・修正する
- [ ] 参照GetGroundHeightによる地面探索を合わせる

根拠：`src/adapter/metamod/runtime/behavior/bot_look_geometry.cpp:15`、`src/adapter/metamod/runtime/behavior/bot_look_geometry.cpp:48`。参照：`regamedll/dlls/bot/cs_bot_vision.cpp:609`。

既存テスト／検証上の注意：bomb_guard_look_tests.cpp geometry cases; noise_look_runtime_tests.cpp; add independently computed native corner rays.


## 2. 状態・目的・人質・VIP（16件未完）

### ☐ S01 — T設置先のzone選択・重複area抽選

状態：差確定。

- [ ] 最寄りzoneを参照PathCostで選ぶ
- [ ] 重複NAV areaを抽選する
- [ ] 重複領域の中点を使い、目標とRNG順を照合する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:1033`、`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:1042`、`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:1106`、`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:1151`、`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:1231`。参照：`regamedll/dlls/bot/states/cs_bot_idle.cpp:183`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:186`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:190`、`regamedll/dlls/bot/cs_bot_manager.cpp:1684`、`regamedll/dlls/bot/cs_bot_manager.cpp:1728`。

既存テスト／検証上の注意：round_objectives_tests.cpp:179 and :223 exercise generic bounded legacy helper; they do not establish native production zone/area choice. Need adversarial multi-area/multi-zone production fixture and matched native target/RNG trace.

### ☐ S02 — CT探索siteの距離・経路コストの区別

状態：差確定。

- [ ] 経路選択用コストと返却する幾何学的距離を分離する
- [ ] CROUCH/JUMP/ladderの参照重みを適用する
- [ ] 2000境界・最寄りsite・遠方ランダムfallbackを検証する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1219`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1221`、`include/astrabot/compat/bomb_search.hpp:98`。参照：`regamedll/dlls/bot/states/cs_bot_idle.cpp:312`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:313`、`regamedll/game_shared/bot/nav_area.h:572`、`regamedll/game_shared/bot/nav_area.h:597`、`regamedll/game_shared/bot/nav_area.h:604`。

既存テスト／検証上の注意：Need two bombsites whose geometric and weighted order differ and exact 2000 boundary, comparing actual production selected target/RNG.

### ☐ S03 — Hunt30秒退出時のrogue解除

状態：差確定。

- [ ] Hunt strict30秒退出でrogueを解除する
- [ ] 残存rogue timerが次Idleへ持ち越されないことを検証する

根拠：`src/core/compat/state_machine.cpp:642`、`src/core/compat/state_machine.cpp:769`、`include/astrabot/compat/teamwork_policy.hpp:105`、`include/astrabot/compat/teamwork_policy.hpp:137`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1778`。参照：`regamedll/dlls/bot/states/cs_bot_hunt.cpp:56`、`regamedll/dlls/bot/cs_bot.h:1029`。

既存テスト／検証上の注意：compat_state_machine_tests.cpp:385 verifies only state timeout, not TeamworkPolicy value. teamwork_policy_tests.cpp:149 verifies resetValues but timeout never calls it. Need production timer-survives-timeout test without resetting timer/RNG.

### ☐ S04 — 攻撃側Idleの狙撃待機抽選

状態：差確定。

- [ ] 攻撃側各scenarioのfriends＋sniper＋10%待機抽選を接続する
- [ ] Hide10〜30秒・範囲2000・後続Fetch等との順序を検証する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:342`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1600`、`src/adapter/metamod/runtime/behavior/bot_behavior_radio.cpp:1032`、`src/adapter/metamod/runtime/behavior/bot_behavior_radio.cpp:1136`。参照：`regamedll/dlls/bot/states/cs_bot_idle.cpp:205`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:529`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:663`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:867`。

既存テスト／検証上の注意：No assigned scenario test asserts offensive sniper RNG/preemption. Need cases for all four branches including loose C4 priority.

### ☐ S05 — CT人質収集/救助・護送数・移動比較

状態：部分対応。

- [ ] 現在の人質leader・護送数を確定観測する
- [ ] 救助地点と次の人質への経路距離を比較する
- [ ] 追加収集・喪失・route/disposition・Help leaderを参照に合わせる

根拠：`include/astrabot/compat/scenario_behavior.hpp:613`、`include/astrabot/compat/scenario_behavior.hpp:624`、`include/astrabot/compat/scenario_behavior.hpp:635`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:913`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1908`。参照：`regamedll/dlls/bot/states/cs_bot_idle.cpp:672`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:685`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:713`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:723`、`regamedll/dlls/bot/states/cs_bot_move_to.cpp:197`、`regamedll/dlls/bot/states/cs_bot_use_entity.cpp:61`。

既存テスト／検証上の注意：scenario_behavior_tests.cpp:152-246 positively tests inference/retries; these establish current subset, not native parity. Need authoritative public bridge assessment plus multi-hostage competing-CT collect/rescue/loss replay and live rescue.

### ☐ S06 — TのHunt/Hide中の人質警備変更

状態：部分対応。

- [ ] Hunt中に全人質護送を察知して救助を阻止する
- [ ] 安全時間後の可視free hostage警備へ切り替える
- [ ] Hide救助地点警備からfree hostage警備へ切り替える

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1517`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1605`、`src/adapter/metamod/runtime/behavior/bot_behavior_radio.cpp:1032`、`include/astrabot/compat/scenario_behavior.hpp:621`。参照：`regamedll/dlls/bot/states/cs_bot_hunt.cpp:126`、`regamedll/dlls/bot/states/cs_bot_hunt.cpp:148`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:591`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:225`。

既存テスト／検証上の注意：Existing T report/defense tests do not prove spontaneous Hunt/Hide visible-free-hostage transition. Need authority-qualified native event+visibility fixtures and runtime acceptance.

### ☐ S07 — TのHide中の解除者迎撃

状態：差確定。

- [ ] T Hideで距離2000以内の解除者を調べる
- [ ] 爆弾可視ならAttack、非可視ならFASTEST MoveToを選ぶ
- [ ] LookAround抑制10秒を接続する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:684`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1825`、`include/astrabot/compat/scenario_behavior.hpp:746`。参照：`regamedll/dlls/bot/states/cs_bot_hide.cpp:190`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:195`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:198`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:204`。

既存テスト／検証上の注意：Need T guarding near/far defuser, bomb visible/occluded, death/cancel/reuse and public identity tests; live defuse interception not established.

### ☐ S08 — VIP/Escapeの20秒境界・区域/戦術選択

状態：部分対応。

- [ ] VIP/逃走Tの20秒前ランダムzone・以後route最寄りzoneを実装する
- [ ] 重複NAV area抽選を接続する
- [ ] 防御側のsniper/rogue/rush/morale分岐を合わせる

根拠：`include/astrabot/compat/scenario_behavior.hpp:456`、`include/astrabot/compat/scenario_behavior.hpp:462`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:829`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:940`。参照：`regamedll/dlls/bot/states/cs_bot_idle.cpp:432`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:496`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:776`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:812`。

既存テスト／検証上の注意：scenario_behavior_tests.cpp:296 only asserts one known VIP escape action; add multiple zones,20s boundary, defenders, no VIP/zone and RNG order; live as_/es_ acceptance absent.

### ☐ S09 — entityによるscenario判別とCT旧救出地点fallback

状態：差確定。

- [ ] map接頭辞依存をentityクラスによるscenario判別へ合わせる
- [ ] CT人質救助地点がない場合のinfo_player_start fallbackを接続する
- [ ] 既存T防御側fallbackを保持する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:744`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:936`。参照：`regamedll/dlls/bot/cs_bot_manager.cpp:1383`、`regamedll/dlls/bot/cs_bot_manager.cpp:1399`、`regamedll/dlls/bot/cs_bot_manager.cpp:1461`。

既存テスト／検証上の注意：Need nonstandard map prefix with bomb/hostage/VIP entities, mixed class ordering, no objective prefix match, and legacy info_player_start rescue scenario.

### ☐ S10 — T爆弾警備の隠れ場所選定・寿命

状態：部分対応。

- [ ] 設置zoneのPlace全体からT警備hiding spotを抽選する
- [ ] Hideのambush/hold遅延・占有retry・再進入寿命を合わせる
- [ ] 到着・経路失敗時の代替点/Idleを合わせる

根拠：`include/astrabot/compat/bomb_guard.hpp:122`、`include/astrabot/compat/bomb_guard.hpp:181`、`include/astrabot/compat/bomb_guard.hpp:191`、`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:344`、`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:411`。参照：`regamedll/dlls/bot/states/cs_bot_idle.cpp:134`、`regamedll/dlls/bot/states/cs_bot_idle.cpp:138`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:33`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:343`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:413`、`regamedll/dlls/bot/cs_bot_statemachine.cpp:120`。

既存テスト／検証上の注意：scenario_behavior_tests.cpp:551+ tests current BombGuardPlanner selection/cache/no-cover and performance bounds; no native same-condition Place-vs-radius/occupancy/failure/lifetime replay. Live guarding remains unverified.

### ☐ S11 — BuyのCareer・状態進入/装備観測の厳密性

状態：部分対応。

- [ ] Career観測をBuyContextへ接続しkit購入条件を合わせる
- [ ] Buy OnEnterと初回updateのRNG時刻を照合する
- [ ] slot先頭・active/default pistol・購入後freeze装備/reloadを検証する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_buying.cpp:113`、`src/adapter/metamod/runtime/behavior/bot_behavior_buying.cpp:149`、`include/astrabot/compat/buy_controller.hpp:43`、`src/core/compat/buy_controller.cpp:228`、`src/core/compat/buy_controller.cpp:605`。参照：`regamedll/dlls/bot/states/cs_bot_buy.cpp:48`、`regamedll/dlls/bot/states/cs_bot_buy.cpp:68`、`regamedll/dlls/bot/states/cs_bot_buy.cpp:79`、`regamedll/dlls/bot/states/cs_bot_buy.cpp:257`。

既存テスト／検証上の注意：Assigned tests omit buy-specific fixture; parent domain may supply existing economy/live evidence. Need career kit exclusion and buy entry/reset RNG timing/current public inventory equivalence cases.

### ☐ S12 — 人間leaderのauto-follow許可の観測

状態：部分対応。

- [ ] 人間leaderのauto-follow許可を確定観測する
- [ ] 到達不能travel=-1を含む参照300未満条件を照合する
- [ ] 公開radio代理と参照権限の差を解消する

根拠：`include/astrabot/compat/teamwork_policy.hpp:68`、`include/astrabot/compat/teamwork_policy.hpp:206`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1373`。参照：`regamedll/dlls/bot/states/cs_bot_idle.cpp:107`、`regamedll/dlls/bot/states/cs_bot_follow.cpp:103`、`regamedll/dlls/bot/cs_bot_update.cpp:693`、`regamedll/dlls/bot/cs_bot_update.cpp:705`、`regamedll/dlls/player.h:602`。

既存テスト／検証上の注意：teamwork_policy_tests.cpp:299 tests inferred60s public radio policy; need native player allowance comparison and multi-human eligibility replay.

### ☐ S13 — Plant修正後に残る自然拾得/標準GameDLL受入れ

状態：実機未受入。

- [ ] inventory bit同期fixtureに依存しない自然C4拾得を受け入れる
- [ ] 初回未装備C4と敵割込みを実機確認する
- [ ] 実waterlevel3・標準GameDLL・複数slot authorityを関連項目と連携して確認する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1721`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:2229`、`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:91`、`src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp:1918`。参照：`regamedll/dlls/bot/states/cs_bot_idle.cpp:169`、`regamedll/dlls/bot/states/cs_bot_move_to.cpp:263`、`regamedll/dlls/bot/states/cs_bot_plant_bomb.cpp:45`。

既存テスト／検証上の注意：compat_state_machine_tests.cpp:406,:751 cover order and boundaries. See STATUS.md:17,:51,:153-175 and csbot-idle-buy-look-supply-20261122/evidence-final-v2/acceptance.json. Historical tests were not rerun in this audit.

### ☐ S15 — 通常Hideの占有・経路失敗時の場所再選択

状態：差確定。

- [ ] Hide移動中の最寄りplayer占有による場所再選択を実装する
- [ ] 非PROGRESSING経路結果で代替hiding spotを探す
- [ ] 候補なしの現在位置fallback・再経路失敗のIdleを合わせる

根拠：`src/core/compat/state_machine.cpp:397`、`src/core/compat/state_machine.cpp:680`、`src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp:1490`、`src/adapter/metamod/runtime/behavior/bot_behavior_radio.cpp:329`、`src/adapter/metamod/runtime/behavior/bot_behavior_radio.cpp:420`。参照：`regamedll/dlls/bot/states/cs_bot_hide.cpp:343`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:348`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:413`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:419`、`regamedll/dlls/bot/states/cs_bot_hide.cpp:442`。

既存テスト／検証上の注意：Need production Hold/Guard/retreat Hide fixture where another visible player occupies spot after admission, plus path failure/replacement unavailable/current-position fallback/failed replacement; existing scenario guard tests cover BombGuardPlanner only.

### ☐ C12 — 複数武器slotの先頭を確定する公開観測

状態：公開観測調査。

- [ ] 複数primary/pistolのlinked slot先頭を公開観測で確定する
- [ ] 固定数値順や全拒否との差を解消する
- [ ] custom inventoryで実機検証する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:77`、`src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:102`、`include/astrabot/compat/weapon_pickup.hpp:43`。参照：`regamedll/dlls/bot/cs_bot_weapon.cpp:443`、`regamedll/dlls/bot/cs_bot_weapon.cpp:482`。

既存テスト／検証上の注意：weapon_equipment_tests.cpp core cases; weapon_pickup_runtime_tests.cpp public identity cases; no private-head live equivalence.

### ☐ C14 — C4初回未装備・実水没の直接受入れ

状態：実機未受入。

- [ ] 初回未装備C4で敵が割り込む実機受入れを行う
- [ ] 実際の水没状態で最適武器選択を受け入れる
- [ ] 人工inventory同期・合成waterテストと区別する

根拠：`src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp:74`、`src/adapter/metamod/runtime/command/bot_command_runtime.cpp:539`。参照：`regamedll/dlls/bot/cs_bot_weapon.cpp:420`。

既存テスト／検証上の注意：combat_target_log_tests.cpp;weapon_equipment_tests.cpp:315-321;artifacts/local/debug/csbot-idle-buy-look-supply-20261122/evidence-final-v2/acceptance.json


## 3. NAV・移動・知覚（6件未完）

### ☐ NAV-01 — 段差ジャンプ80/30/10と地面probeの順序/方向

状態：差確定。

- [ ] 現在の要求方向でfar80→near30→gap10をprobeする
- [ ] near上下・MUST_JUMPと成功後の打切り順を合わせる
- [ ] 旧route方向/freshness・独自step16や落下vetoとの差を解消する

根拠：`src/core/nav/locomotion.cpp:336`、`src/core/nav/locomotion.cpp:346`、`src/core/runtime/nav_roam_controller.cpp:82`、`src/adapter/metamod/runtime/navigation/bot_navigation_runtime.cpp:1190`、`src/adapter/metamod/runtime/navigation/bot_navigation_runtime.cpp:1246`。参照：`regamedll/dlls/bot/cs_bot_nav.cpp:126`、`regamedll/dlls/bot/cs_bot_nav.cpp:199`、`regamedll/dlls/bot/cs_bot_nav.cpp:223`、`regamedll/dlls/bot/cs_bot_nav.cpp:237`、`regamedll/dlls/bot/cs_bot_nav.cpp:250`。

既存テスト／検証上の注意：['tests/locomotion_tests.cpp:543', 'tests/locomotion_tests.cpp:568', 'tests/locomotion_tests.cpp:574']

### ☐ NAV-02 — 足元左右feelerによる壁回避操舵

状態：差確定。

- [ ] 床法線に合わせた左右足元feelerを追加する
- [ ] precise/jump/near-endの条件を合わせる
- [ ] 片側閉塞時の150/300横方向補正を接続する

根拠：`src/adapter/metamod/runtime/navigation/bot_navigation_runtime.cpp:1211`、`src/core/runtime/nav_roam_controller.cpp:2593`、`src/core/nav/locomotion.cpp:430`。参照：`regamedll/dlls/bot/cs_bot_pathfind.cpp:1169`、`regamedll/dlls/bot/cs_bot_pathfind.cpp:1461`、`regamedll/dlls/bot/cs_bot_pathfind.cpp:1469`、`regamedll/dlls/bot/cs_bot_pathfind.cpp:1515`。

既存テスト／検証上の注意：['tests/locomotion_tests.cpp:521', 'tests/nav_query_tests.cpp:1616']

### ☐ NAV-03 — 生存可能な有痛落下を拒むzero-damage gate

状態：差確定。

- [ ] zero-damage限定の落下gateを参照の生存可能判定へ合わせる
- [ ] health>damage+10とroute/aggressionの重みを接続する
- [ ] 実落下で経路選択・着地・被害を検証する

根拠：`src/adapter/metamod/runtime/navigation/bot_navigation_runtime.cpp:1073`、`src/core/nav/locomotion.cpp:360`、`src/core/runtime/nav_roam_controller.cpp:3060`、`src/core/nav/jump_drop.cpp:165`。参照：`regamedll/dlls/bot/cs_bot.h:1632`、`regamedll/dlls/bot/cs_bot.h:1640`、`regamedll/dlls/bot/cs_bot.h:1645`、`regamedll/dlls/bot/cs_bot_pathfind.cpp:1515`。

既存テスト／検証上の注意：['tests/locomotion_tests.cpp:739', 'tests/locomotion_tests.cpp:751', 'tests/jump_drop_tests.cpp:152', 'tests/jump_drop_tests.cpp:165']

### ☐ NAV-04 — 人質護送数をpath costへ接続

状態：実行時接続が部分対応。

- [ ] 確定した現在の人質護送数をPathCostContextへ渡す
- [ ] 護送時crouch penalty3倍を実行経路で検証する
- [ ] 推測カウンタで代用せずPUB-HOSTAGE-OWNERSHIPと連携する

根拠：`include/astrabot/nav/nav_query.hpp:107`、`src/core/nav/nav_query.cpp:770`、`src/adapter/metamod/runtime/navigation/bot_navigation_runtime.cpp:210`。参照：`regamedll/dlls/bot/cs_bot.h:1667`。

既存テスト／検証上の注意：['tests/nav_query_tests.cpp:1208', 'docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:36']

### ☐ NAV-05 — NAV v1–4の.loc place付随データ読取り

状態：差確定。

- [ ] NAV v1〜4の.loc付随データを読む
- [ ] place名解決とareaへのPlace付与を接続する
- [ ] legacy placeを使う発話/scenarioを検証する

根拠：`src/core/nav/legacy_nav_reader.cpp:360`、`src/adapter/metamod/navigation/nav_loader.cpp:236`、`src/adapter/metamod/navigation/nav_loader.cpp:283`。参照：`regamedll/game_shared/bot/nav_file.cpp:639`、`regamedll/game_shared/bot/nav_file.cpp:697`、`regamedll/game_shared/bot/nav_file.cpp:900`、`regamedll/game_shared/bot/nav_file.cpp:902`。

既存テスト／検証上の注意：['tests/legacy_nav_reader_tests.cpp', 'tests/nav_loader_tests.cpp']

### ☐ NAV-06 — A* open-node減額→同値時の並び順

状態：算法差確定・対照未実行。

- [ ] open-node減額後の同値順序を参照の安定順へ合わせる
- [ ] 先発A20・後発B10・A→10でBが先になる対照を追加する
- [ ] 既存初期同値テストに加え経路/RNG差を照合する

根拠：`src/core/nav/nav_query.cpp:825`、`src/core/nav/nav_query.cpp:840`、`src/core/nav/nav_query.cpp:918`、`src/core/nav/nav_query.cpp:932`。参照：`regamedll/game_shared/bot/nav_area.cpp:2446`、`regamedll/game_shared/bot/nav_area.cpp:2494`、`regamedll/game_shared/bot/nav_area.cpp:2497`。

既存テスト／検証上の注意：['tests/nav_query_tests.cpp:1129']


## 4. 公開観測・イベント・発話・管理・profile（12件未完）

### ☐ S14 — NAV学習/解析/編集/保存（現対戦milestone外）

状態：未実装・現milestone外。

- [ ] NAV node学習を実装・検証する
- [ ] hiding/approach/encounter/sniper解析を実装・検証する
- [ ] NAV編集・保存を実装・検証する（現対戦milestone外、完了扱いにはしない）

根拠：`docs/parity/IMPLEMENTATION_BACKLOG.md:21`。参照：`regamedll/dlls/bot/cs_bot_learn.cpp:169`、`regamedll/dlls/bot/cs_bot_learn.cpp:385`、`regamedll/dlls/bot/cs_bot_learn.cpp:478`。

既存テスト／検証上の注意：No live NAV authoring acceptance sought; do not add to current work scope without user decision.

### ☐ OBS-01 — 任意/非player soundからの不要な調査音生成

状態：差確定。

- [ ] generic音の非player・敵/味方・可聴event whitelistを合わせる
- [ ] 任意sampleをOther/WeaponFire調査音にする過剰受付を修正する
- [ ] 過剰受付を要求する既存テストを参照に合わせる

根拠：`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:636`、`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:668`、`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:1531`。参照：`regamedll/dlls/bot/cs_bot_event.cpp:275`、`regamedll/dlls/bot/cs_bot_event.cpp:280`、`regamedll/game_shared/bot/bot_util.cpp:639`、`regamedll/game_shared/bot/bot_util.cpp:764`。

既存テスト／検証上の注意：['tests/gunshot_runtime_tests.cpp:440', 'tests/gunshot_runtime_tests.cpp:464', 'tests/gunshot_runtime_tests.cpp:466']

### ☐ OBS-02 — 標準武器eventの受信時listener/局在化処理

状態：差確定。

- [ ] 標準PlaybackEvent武器音を受信時にlistener距離/team/source判定する
- [ ] 局在化RNGを受信時へ合わせる
- [ ] listener移動・range境界・RNG割込み順を検証する

根拠：`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:503`、`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:517`、`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:569`、`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:1531`、`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:1561`。参照：`regamedll/dlls/bot/cs_bot_event.cpp:357`、`regamedll/dlls/bot/cs_bot_event.cpp:389`、`regamedll/dlls/bot/cs_bot_event.cpp:419`。

既存テスト／検証上の注意：['tests/gunshot_runtime_tests.cpp:472', 'tests/gunshot_runtime_tests.cpp:559', 'tests/pm_move_footstep_runtime_tests.cpp:59']

### ☐ M01 — quota縮小時の死亡Bot優先

状態：差確定。

- [ ] quota縮小時に死亡Botを先に走査する
- [ ] 死亡Botがいなければ全Bot走査へ進む
- [ ] 既存team/score同数抽選を保持する

根拠：`src/adapter/metamod/plugin/plugin_runtime.cpp:1657`。参照：`regamedll/game_shared/bot/bot_util.cpp:209`。

既存テスト／検証上の注意：同teamの低slot生存/高slot死亡対照で実際のkick対象を検証し、実機quota縮小を受け入れる。

### ☐ M02 — 自動追加失敗時のquota補正

状態：差確定。

- [ ] 自動追加のprofile枯渇/AddBot失敗でquotaを1減らす
- [ ] 手動追加失敗では同じ補正を行わない対照を検証する

根拠：`src/adapter/metamod/plugin/plugin_runtime.cpp:1651`、`src/adapter/metamod/plugin/plugin_runtime.cpp:2422`、`src/adapter/metamod/plugin/plugin_runtime.cpp:2434`。参照：`regamedll/dlls/bot/cs_bot_manager.cpp:819`、`regamedll/dlls/bot/cs_bot_manager.cpp:866`。

既存テスト／検証上の注意：profile全使用/create拒否の自動/手動対照、quotaと次0.3秒更新の再試行・乱数消費を検証。

### ☐ M03 — Any-team追加前のprofile絞込み

状態：差確定。

- [ ] Any-team追加で先にSelectDefaultTeamする
- [ ] 決定teamでprofile候補を絞って抽選する
- [ ] team限定profileの候補集合とRNG順を検証する

根拠：`src/adapter/metamod/config/compat_surface.cpp:93`、`src/adapter/metamod/plugin/plugin_runtime.cpp:2422`、`src/adapter/metamod/plugin/plugin_runtime.cpp:2447`。参照：`regamedll/dlls/bot/cs_bot_manager.cpp:798`、`regamedll/dlls/bot/cs_bot_manager.cpp:818`。

既存テスト／検証上の注意：T/CT限定profileと偏った人口、bot_join_team指定/Anyでprofile候補とjoin先を比較。

### ☐ M04 — profile Skin/custom Skin・bot_prefix

状態：差確定。

- [ ] profile Skin属性・custom Skinブロックを解析する
- [ ] 選択skinをappearanceへ渡す
- [ ] bot_prefixを生成名に反映する

根拠：`include/astrabot/compat/profile_catalog.hpp:43`、`src/adapter/metamod/config/profile_loader.cpp:378`、`src/adapter/metamod/config/profile_loader.cpp:874`、`src/adapter/metamod/plugin/plugin_runtime.cpp:2434`。参照：`regamedll/game_shared/bot/bot_profile.cpp:109`、`regamedll/game_shared/bot/bot_profile.cpp:347`、`regamedll/game_shared/bot/bot_util.cpp:358`、`regamedll/dlls/bot/cs_bot_manager.cpp:1630`。

既存テスト／検証上の注意：標準/独自SkinのDB読取り、prefix付き名前の重複判定、appearance選択を実行時で検証。境界の安全上限と不正DB方針は別途明示。

### ☐ M05 — bot_mimic入力複製・診断コマンド面

状態：差確定。

- [ ] bot_mimicのusercmd複製とyaw offsetを実装する
- [ ] entity_dump等の診断コマンド効果を照合する
- [ ] bot_aboutのconsole/hint表示を実装する
- [ ] NAV編集部分はS14へ集約し二重計上しない

根拠：`src/core/compat/command_registry.cpp:69`、`src/adapter/metamod/config/compat_surface.cpp:128`、`src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp:907`、`src/adapter/metamod/runtime/command/bot_command_runtime.cpp:711`、`src/adapter/metamod/plugin/plugin_runtime.cpp:2469`。参照：`regamedll/game_shared/bot/bot.cpp:278`、`regamedll/game_shared/bot/bot.cpp:301`、`regamedll/dlls/bot/cs_bot_manager.cpp:271`、`regamedll/dlls/bot/cs_bot_manager.cpp:366`。

既存テスト／検証上の注意：有効/無効slotのmimic view/msec/buttons/axesとyaw offsetを比較。編集/診断機能は通常対戦の完了範囲と分け、未実装を記録する。

### ☐ PUB-STD-DAMAGE — 標準GameDLLの判定前inflictor/被弾時刻の確定観測

状態：公開観測調査。

- [ ] 標準GameDLLで判定前inflictorと被弾時刻を確定する
- [ ] FriendlyFire/injury/panicへ同等のreceiptを接続する
- [ ] HUD集計値との違いを検証する

根拠：`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:3`、`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:16`、`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:18`。参照：`regamedll/dlls/bot/cs_bot.cpp:98`。

既存テスト／検証上の注意：既存provider/fallback試験と取得経路監査はある。nativeが消費する時刻/identity/順序の等価な公開bridgeを確立し実機確認する。

### ☐ PUB-STD-BLIND — 標準GameDLLの正確なgameplay失明期限

状態：公開観測調査。

- [ ] 標準GameDLLの正確なgameplay失明期限を確定する
- [ ] ScreenFade由来fallbackとの相違を検証する

根拠：`include/astrabot/compat/public_observation.hpp:155`、`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:1202`。参照：`regamedll/dlls/bot/cs_bot_vision.cpp:1135`。

既存テスト／検証上の注意：既存provider/fallback試験と取得経路監査はある。nativeが消費する時刻/identity/順序の等価な公開bridgeを確立し実機確認する。

### ☐ PUB-STD-ANIMATION — 標準GameDLLの武器animationイベント列

状態：公開観測調査。

- [ ] 標準GameDLLのfired/empty/reload animation列を確定観測する
- [ ] 既存mapped fire/sample fallbackとの差を検証する

根拠：`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:20`、`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:22`。参照：`regamedll/game_shared/bot/bot_util.cpp:639`。

既存テスト／検証上の注意：既存provider/fallback試験と取得経路監査はある。nativeが消費する時刻/identity/順序の等価な公開bridgeを確立し実機確認する。

### ☐ PUB-HOSTAGE-OWNERSHIP — 人質の現在leader/追従終了/護送数の確定観測

状態：公開観測調査。

- [ ] 現在leader・追従終了・護送数を確定観測する
- [ ] use-start eventだけでは分からない継続状態を接続する
- [ ] S05/S06/NAV-04へ同じauthorityを供給する

根拠：`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:34`、`docs/parity/PUBLIC_EVENT_INTAKE_AUDIT.md:36`、`src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp:1943`。参照：`regamedll/dlls/bot/cs_gamestate.cpp:572`。

既存テスト／検証上の注意：既存provider/fallback試験と取得経路監査はある。nativeが消費する時刻/identity/順序の等価な公開bridgeを確立し実機確認する。


## 5. 固定参照リプレイ・両OS実機・継続性能（2件未完）

### ☐ V01 — 全体の固定参照リプレイ・共有RNG順

状態：実機未受入。

- [ ] 固定CSBotと複数Bot生成/round/event/state/音声を通した実行列を比較する
- [ ] 共有RNGの型・範囲・値・消費順を照合する
- [ ] Astra旧履歴一致を参照全体一致と扱わない

根拠：`src/adapter/metamod/bridge/engine_random_source.cpp:49`、`src/adapter/metamod/plugin/plugin_runtime.cpp:1791`。参照：`regamedll/game_shared/bot/bot.cpp:225`、`regamedll/dlls/bot/cs_bot_update.cpp:186`。

既存テスト／検証上の注意：同一条件の固定参照/最終製品でイベント順、目標、移動、RNGを採録。baselineは再生成しない。

### ☐ V02 — 両OS実機・照準/旋回・多人数継続性能

状態：実機未受入。

- [ ] Linux実機で最終製品を受け入れる
- [ ] 人間クライアントでAimせず通過・旋回カクつきを確認する
- [ ] 多数Bot・長時間・map切替・FPS/lagの継続性能を測定する
- [ ] C02-2d6 Noise試行のquit後Steam stalled cross-thread pipeを切り分け、正常終了を再確認する（手動終了0xC000013A、元NAV/core復元済み）
- [ ] C02-2d9/2d10 FetchBomb試行のquit後Steam待機原因を確定する（C02-2d11は子PATHの既存Debug CRT・BPなし/8読点で正常終了を限定確認。旧環境CRT同一性と待機原因は未確定、通常起動時のDebug CRT欠落も残す。[対照](docs/parity/FETCH_ZERO_LIVE_C02_2D11.md)）。過去の終了不合格を保持する（2d9のHLDS11228/CDB30732に加え、2d10 live6/7/8も機能採録のみ。各回の明示許可で終了、各両0xFFFFFFFF、実PID0・通常NAV/core復元・保護一致。[最新証拠](docs/parity/FETCH_ADMISSION_LIVE_C02_2D10.md)）
- [ ] C03-2のquit後Steam BMainLoop待機・pipes fatal assertを切り分け、正常終了を再確認する（ユーザー閉鎖後HLDS37588/CDB6520の実PID[]・core/fixture復元・保護11一致。終了コード未取得/null、過去試行と同一原因とは断定しない。[記録](docs/parity/UPDATE_UNSAFE_C03_2.md)）
- [ ] C03-3のCDBなしquit後Steam BMainLoop待機を切り分け、正常終了を再確認する（機能採録後110秒でも実HLDS14156残存。特定PIDのユーザー許可後に終了0xFFFFFFFF、実PID[]/core/fixture復元/保護11一致。[記録](docs/parity/UPDATE_PISTOL_REARM_C03_3.md)）
- [ ] C03-4a live-3の入力準備前message79未送信fatalを切り分ける（実HLDS28568 exit0xFFFFFFFF、実PID[]・設定復元/保護11一致、強制終了なし。最終HE/knife両exit0とは分離。[記録](docs/parity/IDLE_SAFE_EQUIPMENT_C03_4A.md)）
- [ ] Windowsの限定受入れを全体合格と扱わない

根拠：`src/adapter/metamod/diagnostics/runtime_profiler.cpp:314`、`src/adapter/metamod/plugin/plugin_runtime.cpp:854`。参照：`regamedll/dlls/bot/cs_bot_vision.cpp:57`、`regamedll/dlls/bot/cs_bot_update.cpp:186`。

既存テスト／検証上の注意：既存実機再配備を基本に、人数/時間/マップ/製品SHAと観測値を明記して確認。隔離は固定1コピーのみ再利用する。 cold/post-combat停止で有限Aheadがない場合のlook初期化は正規RED/固定参照比較を別途確認する。C16の暖機後有限Ahead保持試験で閉じない。

## 限定完了した大項目（2件）

### ☑ C01 — 全look ownerの角速度共有・Guardの定速旋回

[完了範囲と受入れ証拠](docs/parity/SHARED_LOOK_C01.md)。全owner・全map・Linux実機・継続性能までの完了を意味しない。関連する未完はC08/V01/V02等で管理する。

### ☑ C16 — full移動commandのUpkeep外raw視線公開

[完了範囲と受入れ証拠](docs/parity/FULL_LOOK_PUBLICATION_C16.md)。全owner・全map・Linux実機・継続性能までの完了を意味しない。関連する未完はC08/V01/V02等で管理する。

## 各修正単位の完了条件

1. 固定参照との差と対象範囲を確定し、必要な正規RED/GREENを採録する。
2. 最終ソースで両OS全build後、Windows/Linux CTestを逐次実行し、PE/ELF各7exportを確認する。
3. 既存実機を再利用して対象を受け入れる。HLDSは実PID終了確認後に次を起動し、設定を原文復元する。
4. 旧記録と本一覧を更新し、対象パスだけをstage、コミット、ローカルmasterへFFする。
5. ソース・製品・ユーザーファイル指紋を再確認し、大項目数と修正単位の完了/残件を報告する。pushなし。

## 網羅照合

監査正本の全49 IDと照合済み：未完47 IDを各1回、限定完了2 IDを各1回記載。群別未完11/16/6/12/2。元のソース台帳・歴史証拠は保持する。
作成時の監査正本SHA256：`1c1feff8aacf7910180c274ec6488dac2f259eec1217646560258296d600e74e`。
