# C02-2c4 accepted-BEGINでの旧Hunt LOW所有解除

状態：オフライン・Windows実機の限定受入れ合格。独立レビュー済み、`889c35c` でコミット・ローカルmaster FF済み。C02全体は部分対応。製品ソースの基点 `8727c222efa940d802cf1e4821ca31e5d924306d`、資料整理後の統合元 `21a15a817d9abe456c6eda254397de4b88558ad4`。固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5`。

## 修正範囲

参照 `cs_bot_vision.cpp:333–363` は高優先の視線を受理すると単一のspotを置換または昇格し、旧LOW所有を積み重ねない。adapterにはNativeのaccepted-BEGIN後もcommand側のHunt LOW所有が残っていた。

Native開始をcommitした更新内でcommand serviceを呼び、`huntApproachLookActive`、`huntEncounterLook`、`huntEncounterAlignedAt`を解除する。後段のHunt helperより前に実行する。実際のgrenade aimを渡すUpkeepにも同じ解除を置く。aim不在では解除しない。共有角速度・NAV・Ahead・経路・抽選時計はこの操作で変更しない。

## 現時点の証拠

正規CMake targetのcommand試験は、通常Hunt LOWを受理した後のgrenade aimで所有が残るassertにRED。修正後はGREEN。aim不在の保持、明示解除、後からの正規LOW再受付、共有角速度不変の対照を含む。関連10件は追加assertを含め再ビルドして合格。

公開敵認識を使う `astrabot_native_grenade_owner_retirement` は先に通常Hunt LOWの供給を確認し、その後Nativeのpin・viewを確認する。試験assertだけで同fullの所有解除を証明したとはしない。

別の読取りCDB probe2でactor0/full21の旧LOW1→0が最初のgrenade Upkeepより前に起きることを確認した。これはオフラインRoot経路の証拠。probe1は多重継承serviceのthisをRootとして誤読したため除外し、rawを保持した。実際のNative失効後の再開や全ownerの一致を証明したものではない。

両OS全target buildは終了0。Windows182/182、Linux181/181のCTestを逐次実行し合格、PE/ELF各7 exportsも合格。両OSの固定履歴151933行・順序付きRNG510行は親との差0。baselineは再生成していない。この履歴に新規所有解除の対照が含まれるという意味ではなく、対象動作差は上記RED/GREEN・Root採録で確認する。

| 製品 | SHA256 |
|---|---|
| DLL | `6a7b5d33d9ecffd909245e36ff903049b38ac1d59d1e8222d360474b88c285f9` |
| PDB | `1a1c4fba124431c984660745812fc183f17183d524a6bb57f7a867df9df383da` |
| ELF | `53de1e7748b50eccd5714e443abb214eefb41cf31c7b9096f197d8b0b1be9e96` |

## Windows実機の限定受入れ

既存 `D:/SteamCMD/cstrike_rehlds` を再使用し、サーバーコピー追加0。最終DLL/PDBのみバックアップ付きで再配備。既存V2 moving fixture・元NAVを使い、DLL module-load後に最終PDBのBPを設定した。

`moving-owner-1` の公開observer3＝actor2/full191で `OWNER_BEGIN 2 bf 1` → `OWNER_RETIRED 2 bf 0` がraw2819–2820行に連続し、最初のgrenade Upkeepより前の同期解除を確認。オフラインprobe2で確認済みのRoot this取得とservice直後のBPを使用した。製品private stateの書換えはない。

| 試行 | CDB / HLDS実PID | 結果 |
|---|---|---|
| moving-owner-1 | 41092 / 24248 | 同full所有解除・両終了0 |
| c4-0 | 17288 / 31420 | 自然設置・両終了0 |
| c4-7-balanced | 29020 / 41260 | zone退出・両終了0 |
| c4-8-balanced | 27816 / 31944 | C4実喪失・両終了0 |

各実PID終了後に次を起動し、全試行でcore.ini原文SHA `e980858692e38e6d9b2169b5d4cd1639bf20c03f7d4276117774518ac1408c2b`へ復元。元NAV・既存fixture不変。Linuxの合格はオフラインのみ。

C4-7初回（CDB23620/HLDS41340）は `plant-fixture missing t=4 ct=0` でfixture開始前に終了0。必要なCT役が不在で、parserもbegin markerを見つけられず失敗したため受入れから除外。既存の公開team変更/respawn fixtureで役割を確保した別試行 `c4-7-balanced` を受入れ、初回証拠を上書きしていない。

独立レビューで阻害指摘なし。`889c35c` でcommit/master FFを完了し、ソース200・検証8・保護12・製品・実機設定の後指紋一致を確認。実Native失効後の長期非再開・全owner一致・Linux実機は本単位の合格範囲外。Guard/Noiseのhandled-return順、FetchBomb実機、直接同時刻full再入、安全時間終了後の経路投擲は引き続きC02残件。

## 証拠と作業保存

`artifacts/local/debug/csbot-grenade-owner-retirement-20261124/`、`C:/Temp/astrabot-grenade-owner-retirement/`。

資料整理commitとユーザーの追加 `.gitignore` 編集を保持。元ソース固定manifestを改変せず、追加編集のSHAと保存対象の改訂manifestを分けて残す。製品ソース・検証入力の固定は変えない。
