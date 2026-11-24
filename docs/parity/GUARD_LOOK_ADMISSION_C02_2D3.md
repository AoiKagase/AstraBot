# C02-2d3 Guardの視線受付と独立Ahead

2026-11-24。C02の部分修正。Windows重点16/16と追加対照1/1、実機Native候補受付を限定検証済み。大項目の完了とは扱わない。統合履歴はこの単位のGit履歴を参照。

## 参照と修正

固定参照 `b0889847fe6d03898be88acc9e366660efb40ab5` の `cs_bot_hide.cpp:287` はUpdateLookAroundを呼び、到着時の開放方向を `SetLookAheadAngle` で供給する。同 `cs_bot_vision.cpp:448–495` は静止期限で間隔、候補番号の順に抽選してからLOWを要求し、候補ゼロはClearLookAtを行う。高優先の受付拒否はSetLookAt内で行うため、保持Noise/Nativeを理由に静止抽選自体を省略しない。

- Guardの候補選択と受付を分離し、保持Noise/Nativeによる拒否を両抽選の後へ移した。拒否したLOWを潜在目標として残さない。
- 候補ゼロはNative/Noiseの視線を解除する。投擲の待機時計は解除しない。
- 開放方向は独立したAheadとしてCommandへ渡す。LOWがない場合も共有springによる通常upkeepを再開する。
- geometry/RNG callbackの後に実行世代・状態epoch・NAV revisionを確認し、旧更新の目標公開を止める。
- compatibility時の「銃を持たないならGuard視線を省略」を解除した。参照のHide/UpdateLookAroundにこの所持条件はない。HEのみのRoot対照で、旧条件はinterval/indexとも0、修正後は各1となった。

## オフライン検証

Windows関連ターゲットをビルドし、重点16/16合格。その後、製品変更なしで「NAV準備後もNative視線が生存している」light対照をテストに追加し、Guard 1/1を再実行して合格。全CTest・Linux・exports・固定履歴比較は大分類完了時の検証へ繰り延べる。

Guard対照は候補あり/ゼロのordinary NoiseとNative、Clear前のNative生存、Clear後lightでの正確なAhead+drift/sharedspring、独立したpin保持、拒否LOWの非復活、RNG/planner置換、ray/interval callbackの再入を含む。NAV再読込・公開scenario目標への配置・正規stopAttackを使用したcontrolled fixtureであり、自然な実機遷移の受入れとは区別する。

最終テストPDBの読取りCDBでも、actor1/full `0x76` でNative `look=1` → intervalのみ → 選択Clear `2` → Native `look=0` を採録した。

証拠：`artifacts/local/debug/csbot-guard-admission-20261124/offline/`。最終候補は同 `final/freeze.json` にソース200件・保護対象12件・検証入力とともに固定。

- DLL SHA256: `e1df4a8135550522912af2470456544546dc175da50d37454dc8a42c879a9c9e`
- PDB SHA256: `c79723c780b2918c6690db32c9f1d496c8540444879816a472c2056cf98ac610`

## 実機と失敗試行

同じ `D:/SteamCMD/cstrike_rehlds` を使用。追加サーバーコピー0。DLL module-load後にBPを設定する。

`live-1` は毎frameの座標/速度補正中に設置へ進まず、入力未成立。`live-2` は設置に成功したが、設置直後のしゃがみを待機と誤認して固定したためGuard受付の証拠には使わない。`live-3` は自然な設置後にGuardへ到着し、71回の候補受付を採録したが、保持Noiseの拒否条件は0回だった。足音入力中の速度リセットを見直し、参照の150超の速度条件に沿って追加試行中。

`live-4` は入力速度182.9とNoise観測を採録したが、その後Guard受付が継続しなかったため保持Noise受付の受入れには使わない。`live-5-native` はGuard到着後にHE装備・敵の一時公開・view固定を使い、actor1の31更新でNative保持→interval→point→LOW拒否→Native保持を採録した。full135〜158の20件では投擲待機も継続、full160以降は待機終了後も視線保持を観測した。これは受付順序の限定対照であり、自然な戦闘終了や全状態の時計一致は主張しない。

5試行ともHLDS/CDBはexit0、実PID消滅、core.ini原文復元、一時fixture削除を各exit.jsonで確認。最終試行はHLDS16860/CDB34236。限定受入れの抽出条件と31更新は `acceptance.json` に保存。保持Noise・候補ゼロの実機受入れは未完了。

## 残る範囲

実機の31更新には1〜2秒未満で再抽選する区間がある。`bot_behavior_objectives.cpp` は非可視でも有効な記憶候補があるとGuard plannerをresetし、`bot_behavior_runtime.cpp` は非Guard decisionでresetする。この寿命管理とgeometry再構築による期限更新は別修正として次に調べる。今回、抽選間隔まで一致したとは扱わない。

他状態の後段Noise再入、FetchBomb実機、直接同時刻full再入、経路corner投擲などC02の残件は維持する。旧baselineは再生成していない。意図したRNG差は保持owner中のGuardで従来省略されていた間隔/候補抽選であり、全履歴の差数は未計測。
