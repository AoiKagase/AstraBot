# Codex向け追補 — 入力同一性と実NAVの反例

P07.6を継続する。P08へ進まない。製品の既存dirty/untrackedを破棄しない。

## まず基準を照合

今回の `AstraBot(1).zip` は、前回 `AstraBot.zip` と共通430ファイルが全て同一だった。新ZIPには `.planning` 等の追加はあるが、製品ソース・ヘッダー・テストには内容変更がない。
さらに最新 `qconsole(3).log` の `effective_team` / `raw_team` / `registered_site_count` は、このZIPのCPPにはない。

実装前に現branch/HEAD/dirty、今回ZIPの主要ファイルSHA-256、build対象source directory、配置DLL SHA-256を照合する。申告SHAへreset/checkoutして最新変更を捨てない。今回ZIPが旧ソースで、現worktreeですでに修正済みなら同じ箇所を再修正せず、反例テストを現worktreeで再実行する。

## 実NAVの入力

`de_dust2.nav` SHA-256:
`53b9889c0a5b45da7c1284b13db7d7b1218c185ea768dc261ce4c4d6e13372c2`

production readerでは737 Area、2876 directed links、79 AreaにJUMP属性。今回BSPは未提供。NAVをrepoへcommitせず外部fixtureとする。

## 今回の優先反例

1. `1778/1779→619→93`: NAV上で高さ48.031→96.031へ連続する面。境界段差はほぼ0なのに、Area平均差24を `StepTooHigh` として拒否する。床高さと境界評価を正す。
2. `46→119`: 下り接続であり、NAV矩形間にはY方向25unitの隙間がある。有効な46→54 corridorを持つ状態で、feet=(-1575,-151,128.031)では119方向、feet.y=-149かつairborneではrecovery46への逆向き入力になる。計画された一時的area lossとgeneric off-mesh recoveryを分ける。
3. `5→Area1520上の3D点`: raw A*は5→1520に成功するが、runtimeのnearestが同XYの高所Area351へ目的地を置き換える。selectedPath.back()==351を確認済み。目的地の3D/Area対応を維持する。
4. `feet=(-1187.5,1250,-32)`: nearest1505が頭上にあり、direction=(0,0,1)のWalk IntentReadyを返す。製品ActionAdapterへ通すとforward/sideとも0。実行できない復帰を成功扱いしない。これはNAV入力から構成した試験で、ログの実足元座標ではない。
5. `275→642`: destination JUMP属性があるが、Jump分岐より前にStepTooHighで拒否。NAV属性だけで無条件Jumpさせるのではなく、歩行/Jump判定と地形観測の契約を正す。

今回のraw howは0/1/2のみで、現実装がJumpDrop開始に使う4/5/6はない。ただしlocomotionのNAV_JUMP分岐は別にあり、全Jump不可能と解釈しない。how=2をDropへ直接置換せず固定ReGameDLLのenum・NAV属性・物理観測の役割を照合する。

## 実施手順

- `README_JA.md` の独立harnessを現在ソースでビルドし、REDを再確認。
- 意味と更新時点の合うdiagnosticsを使う。古い `decision.targetPosition` を実行中local targetと混同しない。
- shared高さ/segment/traversal/recovery契約を必要最小限修正し、6個の実NAV安全性テストと旧synthetic REDを閉じる。
- 以前の階段0→16→32で位置不変のまま二段目へ進む不整合、曲がり角飛ばし、途中Jump切替の回帰も維持。
- sourceを調べる必要がある場合 `source_evidence.md` と `AUDIT_JA.md` の関数・行を入口にする。行番号はZIP基準なので現HEADで再確認。
- raw A* 41→90 Found / explicit budget256 ResourceLimit / controlsを維持。A*全面再設計や任意cooldown、水増しstep上限、velocity/origin直接変更はしない。
- P02 timing、P03 RNG隔離、Objective検索storm修正、path persistenceを維持。
- 正式x86ビルドと現在全CTest、Phase8、外部NAVを実施。今回の独立18件を公式全CTestと混同しない。
- BSP/HLDSがない場合、NAV-onlyとliveを分けてPARTIALと報告。実機では1Botで上記地点、次に4Bot+C4で性能を確認。
- 最後に入力同一性、変更関数、RED/GREEN、残る未検証、DLLハッシュ、commit/dirtyを報告して停止する。

ログとZIPの対応が未確定でも、現在ソース上で反例が再現するかを確認する作業は進める。入力の不足を理由に推測修正を重ねず、実装済みの修正を消して旧コードへ戻さない。
