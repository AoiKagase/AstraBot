# P12 局所移動 Release 実機受入

状態: **受入未実施。P12-03 / プロジェクト全体の Finish は未完了。**

## 識別情報

- 専用branch: `codex/p12-locomotion-redesign`
- 基準commit: `38ca8de18e3269dfd6e04cf564eafb97c45b4e8e`
- 比較資料 ReGameDLL_CS: `b0889847fe6d03898be88acc9e366660efb40ab5`
- Metamod SDK: `7ec9b014f8c0a947a724644aebe34eb33706e44b`
- DLL: `build-p12-native-release/astrabot_mm.dll`。SHA256とbuild identityは `artifact.json` を参照。
- Runtime: `D:/SteamCMD/cstrike_rehlds`
- de_dust2 BSP SHA256: `15945389528d113562ede0a2c80647ebfa799079ed1c05a25379bcf84e4e9286`
- de_dust2 NAV SHA256: `53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2`

## 今回だけの検証順序

ユーザーが承認したP12修正計画に基づき、Release実機受入を先行する。
明示的PASS前にテストターゲットのconfigure/build/run、CTest、canonical、commitを行わない。
テストソースとCMake登録は準備済みだが、未実行の結果をPASSとして扱わない。

新方式PASS後、比較用旧方式・重複処理を撤去し、最終Releaseで影響する実機受入を取り直す。
その明示的PASS後に対象を絞ったオフライン検証、canonical All 1回、FocalSpan更新、commitへ進む。

## 固定条件と記録

`scenarios.csv` の開始位置・目標位置・向きを試行前に固定する。座標は未確定のため空欄であり、推測値ではない。
同じ開始条件で `runs.csv` の120試行を埋める（6シナリオ × 1/2BOT × 各10回）。
各試行にactor、tick範囲、実機ログ、失敗理由を結び付ける。未実施行は `NOT_RUN` のままとする。

試行前に `sv_gravity`、`sv_stepsize`、`sv_maxspeed`、`sv_airaccelerate`、
ジャンプ高さ設定の有無と値、使用するDLL/BSP/NAVのhashを保存する。
`astrabot_debug 2` の診断とサーバーログを保存し、試行後に `astrabot_debug 0` に戻す。
BOTの追加は既存の `astrabot_addbot 1` を使用し、自動生成BOTも含め実際の人数を確認する。
途中のテレポート・押し出しによる救済はFAILとする。

| シナリオ | 受入条件 |
|---|---|
| wall | 壁沿い・角で補正し、外部操作なしで目標に到達 |
| downhill | 坂・小段差の下降で経路を破棄せず、接地追従へ復帰 |
| stairs | 上下とも完走。予算不足を通行不能へ変換しない |
| obstacle_jump | 通常プレイヤーが越えられる障害物を越え、低天井・到達不能は拒否 |
| oscillation | 往復を進捗と認定し続けず、5秒以内に前進または再計画 |
| replan | 要求から探索開始まで250ms以内。別出口を不当に除外しない |

2BOTは接触・譲り合い後の復帰も確認する。成功・失敗の両方を保存する。
探索計算時間、要求から開始までの待機、辺cooldown、地形照会不足を分離して評価する。
120行がすべて成功でも自動的に承認済みとせず、ログとhash提示後にユーザーの明示的PASSを記録する。

## 受入候補の内容

TerrainSampler（床候補とHull支持の分離）、PathFollower（通常境界の連続追従）、
LocomotionController（歩行・地上から空中への移行・下降・局所回避）、
MotionEnvelope / MovementFeedbackをMotorとadapterへ接続した。
経路失敗の除外を有向辺単位にし、探索policyのheuristic/contextを転送する。
扉・しゃがみは専用部品、梯子は既存専用制御へ接続する。

比較用 `ASTRABOT_LEGACY_LOCOMOTION` は既定OFF。自動fallbackはない。
比較用実装の撤去、最終実機PASS、オフライン回帰、commitは受入後の残作業。
静的レビューとReleaseビルドは、6症状が直ったことの実機証拠を代替しない。
