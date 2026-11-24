# C02-2d4 Guard/Hunt共有視線時計

2026-11-24。C02の部分修正。大項目の完了ではない。

## 参照と変更

固定参照 b0889847fe6d03898be88acc9e366660efb40ab5 の cs_bot_init.cpp:251 は m_lookAroundStateTimestamp を0初期化し、cs_bot_vision.cpp:473–480 は期限を確認してordinary1〜2秒、sniper5〜10秒を設定する。cs_bot_nav.cpp:395–431のComputeApproachPointsは点を再構築するだけでこの時計をresetしない。

Bot所有のlookAroundStateTimestampをHuntとGuardで共有し、geometryの寿命から分離した。actor/round/execution lifecycleで初期化し、有効な記憶敵だけを理由としたcompatibility Guard planner resetを解除。旧Astra経路は維持。任意の外部時計を使わない単独plannerの仕様は維持した。

## 検証

正規Rootテストで非可視の生存記憶敵を保持すると、旧実装は次の0.1秒fullでinterval/index各1を再抽選するREDを再現。修正後は各0。NAV再読込直後に期限resetを仮定していた旧zero fixtureは、実際のRNG期限まで待つ対照へ変更した。これは固定baselineの再生成ではない。

Windows関連ターゲットをビルドし重点16/16合格。テストのみ追加後、Guard1/1合格。geometry再構築で期限が変わらず、RNG callbackでplannerが置換された場合は旧処理が共有期限を上書きせず、置換後だけ11.5へ設定する対照を含む。最終PDBで読取りCDB観測点を検証済み。全build/CTest・Linux・exports・固定履歴比較は大分類完了時に行う。

同じ D:/SteamCMD/cstrike_rehlds へ最終DLL/PDBを再配備し、module-load後にBP設定。自然Plant・Guard到着後、HE装備/敵の一時公開/view補正でNative保持を作るcontrolled fixture。actor1の63更新でNative保持を確認し、期限前57更新は抽選なし、期限到達6更新はinterval→pointの順で抽選しLOWを拒否した。次回期限は1.0881/1.7779/1.2923/1.9357/1.9983/1.5811秒。HLDS22024/CDB35852ともexit0、実PID消滅、core.ini原文復元、一時fixture削除、保護対象指紋一致。コピー追加0。

証拠：artifacts/local/debug/csbot-guard-clock-20261124/ の final/freeze.json、offline/、live-1-native/、acceptance.json。ソース200件・保護対象12件と検証入力を製品へ紐付けた。

- DLL SHA256: e786db4a200f87ffa095a32f06048a432bef13be40b858ce3005bfe6a234a02a
- PDB SHA256: 91c4632f56aa8ec7987cc4095cefb70b316a77f48433158ae2a2e542d641d69f

## 限界と残件

HuntとGuardの共有接続はソース確認と関連オフライン回帰で確認したが、状態間遷移の実機対照は未実施。保持Noise/zero実機、他状態の後段Noise再入、FetchBomb/同時刻full再入、corner投擲も残す。大項目47件は未完のまま。

意図したRNG差は、記憶敵・geometry再構築による期限前interval/pointを消し、共通期限到達時だけ消費すること。固定baselineは再生成せず、全履歴の消費差数は大分類検証まで未計測。
