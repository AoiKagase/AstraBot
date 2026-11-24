# 固定CSBotの未使用tail監査

`reference_driver.cpp` は固定参照から抽出した実関数を、明示した平坦NAV・可視性・衝突なしtraceのhostで実行する。製品のテストでもGameDLL全体の実行でもない。参照コード自体はコピーせず `git show` から抽出する。

MSVC x86 developer環境を初期化した同じcmd.exeで、リポジトリrootから実行する。出力は毎回新規ディレクトリを指定する。

```cmd
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x86
where cl
rtk proxy python tools\audit_csbot_grenade_tail.py --reference H:\sourcecode\003.Game\amxmodx\ReGameDLL_CS --output artifacts\local\debug\csbot-tail-new-run
```

11対照は経路なし、新規trivial/複数点、長経路→trivial、DestroyPath後再構築、終点=tail、最初が不可視、tail区間の不可視、RemovePath、最大長全可視/最初が不可視。引数・結果・manifest・抽出関数・初期化根拠・build出力を保存する。最大長ではchecked accessorでslot256の読取りをtrapし、配列外読取りを実行しない。その他のケースは期待target・受付・wall trace回数をassertする。

stubのNAV探索は指定parent chainを返し、接続は平坦かつ相互、可視性はx座標閾値、wall traceは全通過、timerは経過済み、RNGは下限を返す。`real_t=float` は非PLAY_GAMEDLL分岐。ポインタ等のhost初期値はGameDLLの初期値を主張しない。位置の初期値は固定Vector constructorを使う。実水没、drop/梯子、実機投擲/着弾、Linux、最適化違いは検証しない。

今回の結論と製品差は [監査記録](../../../docs/parity/GRENADE_VISIBLE_TAIL_C02_4.md) に記録する。
