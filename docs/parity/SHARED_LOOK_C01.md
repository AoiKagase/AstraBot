# C01 共有視線角速度の限定完了（2026-11-23）

Compatibilityの通常/NAV・Guard・UseEntity/C4・Noise・Combat・梯子・障害物の積分経路をactor所有の共有yaw/pitch速度へ接続した。Guardのfull更新240°/秒旋回はgoal供給へ変更し、次のUpkeepでnativeばねを使う。Enhanced/独立coreの従来経路は維持する。

同時刻の同等以下priorityは再積分せず、高priorityは保存した更新前角度/速度から1回分を置換する。通常template破棄は運動量を保ち、実spawn/actor再作成のresetはゼロ化する。beginRoundGenerationだけでゼロになるとは扱わない。保持Aheadの既存round規則は維持した。RNG callback内のround/actor/epoch変更後に古いCombatが新receiptの共有速度を書き換えるREDも再現し、command操作直前の既存receipt/epoch検査で遮断した。

## オフライン検証

最終全target build後、Windows x86 **173/173・PE7**、続いてLinux x86 **172/172・ELF7**合格。新規正規CMake回帰は`astrabot_shared_look_velocity`と`astrabot_shared_look_reentry`。実cached command、非ゼロ速度のowner切替、同時刻置換、template破棄、round再入を検証する。Guard/Noise/FriendDeath/梯子の旧単調角度assertは速度を保持したnative数値対照へ変更した。FriendDeathは負Xで味方可視フラグを拾うfixture誤設定を修正し、隠れた移動敵と記憶地点の対照を残した。

DLL SHA-256 `a3a7b7803505abbb5ebd7f4a0efe7c98964dbe27cfa4142f72f47e69a4272936`。PDB `99ae51c28ec55314356a8db1a4216188af639277a2e8364aed1343e63e56ddc1`、ELF `02f019c7492554a821746700777b1cdc213434e2491180b6ce6c763e79fe175d`。基点`6688c472dc5828be39b829514f69c5a5cb2c4779`、固定参照`b0889847fe6d03898be88acc9e366660efb40ab5`。

固定baselineは再生成していない。既存34補正後151999行に対し、各OS290 exact-offset blockを追加し全byteを再構成した。差350行はmove260＋ground probe90、残り151649 raw行は同じoffsetで一致。変更フィールドはpitch256/yaw98/forward9/side9/方向button17、actor/roll/up/msec/非方向buttonは不変。90レイ（30組の80/30/10 probe）のXYのみ変化しZ/種類/順序/80・30・10距離を維持した。視線変更に伴う既存yaw基準の入力投影・probe方向差であり、移動全体同値ではない。RNG510行の順序・raw値は両OSとも不変。独立read-onlyレビュー済み。

## 既存実機への再配備と限定受入れ

既存`D:/SteamCMD/cstrike_rehlds`へ最終DLL/PDBだけを旧版backup付きで再配備。サーバーコピー作成0回、実HLDSは常に1台。元のserver.cfg/GameDLL/NAV/profile等の指紋を保持した。検証用cfgは`exec server.cfg`の後で人間待ちを解除し、通常quota4へ固定する小さな追加ファイルである。通常の設定ファイルを書き換えていない。

module-load後に最終PDBの型/ABIを確認して読取りBPを設定。live3の実PID33836で**2000 step**、非ゼロ速度を伴う**327 owner変更**、**164同時刻高priority置換**を採録した。全数のnative数値対照は最大誤差`1.52587890625e-5`、解析エラー0。次時刻のstartSpringと直前速度はraw一致。実RunPlayerMove角のraw一致は**1843件**（通常priority0=1600、Noise3=163、Combat5=80）。同じpriorityの複数RPPがあるためstep数とは別に数える。Guard/UseEntity/C4/梯子/障害物の個別live owner対照まで合格へ昇格しない。

CDBは上限で正常detach/exit0。HLDSは同じPIDで継続し、実ロード先とDLL SHAを再確認した。最初のliveは72通常stepのみ、live2は採録開始前に無人時population条件でBotが消えたためowner切替証拠に流用しない。liveのmodule-load初回script解釈失敗とcommand引数offset失敗は除外し、同一process内で修正した。応答しなくなった最初のHLDS/CDBはユーザー許可後に終了、実PID0台確認後に次を起動した。UI操作ツール期限切れ、RCON資格情報利用の自動承認拒否も受入れ証拠に使っていない。

## 残る境界：C16とC08

実機には先行priority0共有resultと最終RPPの不一致が**86件**あった。最初は`live3/debugger.log:4690`（先行yaw7.59674072/pitch-1.51635194、最終yaw100.60433197/pitch0）。現ソース`beginMovementCommand:1273–1275`にはpitch0/移動yawのdraft、`finishMovementCommand:1389,1396–1398,1489–1496`には同期しないviewをentityへ直接公開する経路が残る。共有ばねの再積分/resetではない。全86件のbranch帰属は未検証であり、**新残件C16**として別単位で修正・対照する。C08のCombat視認/offset/goal更新順、V01/V02の全参照RNG・両OS実機・継続性能も未完。

C01は「共有角速度と1回相当の積分」の限定完了であり、唯一のview書込み経路、全ownerの実機受入れ、全角度軌跡一致を意味しない。台帳は49歴史項目、C01を除いて**48残件**。次はC16のfull/light公開順をREDから確認する。

証拠：`artifacts/local/debug/csbot-shared-look-c01-20261122/`の`final/freeze.json`、両OS最終ログ、`history-review.md`、`live3/analysis-current.json`、`live3/loaded-modules-final.json`、`live3/continuity.json`。失敗/旧候補は上書きせず保持。今回の対象パスのみcommitし、local masterへFFした同一性の最終receiptは同rootの`post-merge.json`に記録する。pushなし。
