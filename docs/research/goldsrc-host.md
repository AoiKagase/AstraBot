# GoldSrc host validation

## 結論

AstraBotはGameDLL内部Botを継承せず、Metamod pluginから通常のFakeClient playerを
作り、外部`BotAgent`をgeneration付き`PlayerId`へ関連付ける構成で成立する。
Phase 1の最小flowにReAPI/ReHLDS private APIは不要である。すべてのengine/GameDLL
callはserver main thread上のadapterに閉じ込める。

## 必要なSDK surface

| Layer | Header / table | 必要なfunction・hook |
|---|---|---|
| Metamod plugin entry | `meta_api.h`, `META_FUNCTIONS` | `Meta_Query`, `Meta_Attach`, `Meta_Detach`, `GetEntityAPI2`, `GetEngineFunctions`。Metamod-Pのstub登録例は[`stub_plugin/meta_api.cpp`](https://github.com/Bots-United/metamod-p/blob/7ec9b014f8c0a947a724644aebe34eb33706e44b/stub_plugin/meta_api.cpp#L41-L124)。 |
| GameDLL hooks | `dllapi.h`, `DLL_FUNCTIONS` | `pfnClientDisconnect`, `pfnServerActivate`, `pfnServerDeactivate`, `pfnStartFrame`; join観測にはGameDLL/engine message hook。 |
| Engine calls/hooks | `engine_api.h`, `enginefuncs_t` | Calls: `pfnCreateFakeClient`, `pfnGetInfoKeyBuffer`, `pfnSetClientKeyValue`, `pfnRunPlayerMove`, `pfnServerCommand`/`pfnServerExecute`. Hooks: `pfnCmd_Args`, `pfnCmd_Argv`, `pfnCmd_Argc` to supply the current fake command context. |
| GameDLL dispatch | Metamod macros/utilities | `MUTIL_CallGameEntity(PLID, "player", &ent->v)`, `MDLL_ClientConnect`, `MDLL_ClientPutInServer`, and `MDLL_ClientCommand` for a fake client's parsed command. |
| Identity | HLSDK entity helpers | `ENTINDEX(edict)`はslot lookupにだけ使い、Coreには`PlayerId{slot,generation}`を渡す。raw `edict_t *`を保持するのはadapterだけ。 |

Metamod interface versionは対象Metamod-P snapshotの`META_INTERFACE_VERSION "5:13"`
（[`meta_api.h`](https://github.com/Bots-United/metamod-p/blob/7ec9b014f8c0a947a724644aebe34eb33706e44b/metamod/meta_api.h#L39-L66)）。Phase 1 buildは推測した宣言ではなく固定SDK headerを使用し、
[license gate](license-matrix.md#release-gate)を満たす必要がある。

## 正確なlifecycle

| Step | Adapter action | Success evidence / failure cleanup |
|---|---|---|
| 1. Plugin load | `Meta_Query`でinterfaceを検証し、`Meta_Attach`でhook tableとengine/GameDLL functionsを保存する。 | load logにAstraBot version、adapter kind、Metamod interface、GameDLL detectionを1行で出す。attach途中の失敗は登録済み資源を解放する。 |
| 2. Map activate | `ServerActivate(edictList, edictCount, clientMax)`でmap generationを進め、player slot registryを初期化する。 | stale agent/nav/experience handleが0件。Phase 1ではnavをまだloadしない。 |
| 3. Allocate client | `pfnCreateFakeClient(name)`を呼びnull/max-player failureを検査する。YaPBのcurrent実装は[`Game::createFakeClient`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/engine.cpp#L1366-L1384)。 | nullならagentを作らずerror resultを返す。entvars/private-data resetを独自に一般化せず、Phase 1 live testで対象engine/GameDLLに必要な初期化だけを確定する。 |
| 4. Construct GameDLL player | `MUTIL_CallGameEntity(PLID, "player", &ent->v)`でMODの`player` entity factoryを呼ぶ。YaPBの境界は[`BotManager::execGameEntity`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/manager.cpp#L127-L145)。 | GameDLL private dataはGameDLLの所有物。AstraBot classを格納・置換しない。failure時はkick/removeをadapterで完結させる。 |
| 5. Client metadata | info bufferへ`_vgui_menus=0`, `_ah=0`, 必要なら`*bot=1`を設定する。 | name/index/flagsをtrace。秘密情報やraw pointerは出さない。 |
| 6. Connect | reject bufferを空にして`MDLL_ClientConnect(ent,name,loopback-address,reject)`を呼ぶ。current YaPB sequenceは[`manager.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/manager.cpp#L1150-L1185)。 | false/reject textならserver kick、slot registry破棄、failure result。 |
| 7. Put in server | `MDLL_ClientPutInServer(ent)`後、fake/client flagを検証し、`PlayerId{ENTINDEX, generation}`と`BotAgentId`を登録する。 | mappingはbidirectionalかつ一意。slot reuseではgenerationが変わり、古いcommandを拒否する。 |
| 8. Team/class join | `VGUIMenu`/`ShowMenu`をparseして`TeamSelect`/`ClassSelect` stateへ進み、fake clientへ`menuselect <team>`、続いて`menuselect <class>`を送る。YaPBのparserは[`message.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/message.cpp#L85-L126)、dispatchは[`manager.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/manager.cpp#L1860-L1954)。 | `TeamInfo`/alive team stateでjoin完了を確認する。固定delayだけで成功扱いにしない。timeout/retry上限後はkickする。 |
| 9. Command | Coreがimmutable frame snapshotに対して`BotCommand`を返し、次のadapter tickがview angle, forward/side/up move, buttons, impulse, elapsed msecへ変換して`pfnRunPlayerMove`を一度送る。 | current YaPB evidenceは[`Bot::runMovement`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/botlib.cpp#L4001-L4045)。duplicate/stale tickをrejectし、実msecをtraceする。 |
| 10. Disconnect | 明示removeはquoted name/useridをserver kick queueへ送り実行する。`ClientDisconnect` hookがmappingを唯一のcleanup pathで破棄し、agentへ`Disconnected`を通知する。 | 途中join、正常kick、人間slot reuseをtestする。hook例は[`linkage.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/linkage.cpp#L216-L255)。 |
| 11. Map change | `ServerDeactivate`でcommand受付停止、全agent mappingとmap-scoped stateを破棄する。次の`ServerActivate`で新generationを開始する。 | cleanup後にraw entity/nav handlesが残らない。YaPBのmap cleanup/start-frame境界は[`linkage.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/linkage.cpp#L328-L416)。 |

## Join commandの境界

`pfnClientCommand`をFakeClientに呼んではならない。これはserverからreal clientへ
commandを送るengine APIであり、YaPBのcurrent hookもclient DLLを持たないBotへの呼出し
はcrash要因としてsupercedeする（[`linkage.cpp`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/linkage.cpp#L30-L68)）。

FakeClientが`menuselect`等をGameDLLへ発行する最小pathは、commandを独自にtokenizeし、
その処理中だけ`pfnCmd_Args`/`pfnCmd_Argv`/`pfnCmd_Argc` hookがtokenを返すようにして、
`MDLL_ClientCommand(fakeEntity)`を呼ぶことである。YaPBの具体例は
[`Game::prepareBotArgs`](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/engine.cpp#L529-L580)と
[`linkage.cpp`のCmd_Arg hooks](https://github.com/yapb/yapb/blob/4967a220ba3a58c461ee1cef8b6fb37c6fd93b5e/src/linkage.cpp#L806-L870)。AstraBot実装ではRAII/guardでcurrent fake command contextをmain threadに限定し、
nested/reentrant dispatchをrejectまたはstack化し、終了時に必ずclearする。tokenizer、quote、
semicolon、empty argumentをunit testし、GameDLL `ClientCommand`が対象entity/contextを
正しく処理することをlive testする。

## ReGameDLLとVanillaの分離

| Capability | Standard Metamod/HLSDK path | ReGameDLL/ReAPI optional path |
|---|---|---|
| FakeClient, player factory, connect, put-in-server, movement | **必須かつ共通**。Phase 1はこのpathだけで成立させる。 | 不要。 |
| Team/class join | CS user messages + GameDLL `ClientCommand`; Vanillaでも成立し得るがversion別message/menu fixtureが必要。 | ReGameDLL hookやprivate menu handlerを直接呼ばない。 |
| Weapon, round, bomb, grenade events | message/entity observationで最低限対応可能。 | 後続phaseで正確なhookを追加できるが、adapter内のoptional capabilityにする。 |
| Nav ladder/world trace enrichment | engine entity enumeration/traceでportable adapterを作れる。 | ReGameDLL dataが有用でもconcrete `CBaseEntity`/`CCSPlayer`は外へ出さない。 |

ReGameDLL内蔵zBotは別architectureである。`CCSBotManager::AddBot`はGameDLL内で
`CCSBot`を生成し、`ClientPutInServer`とprivate menu handlerを直結する
（[`cs_bot_manager.cpp`](https://github.com/rehlds/ReGameDLL_CS/blob/b0889847fe6d03898be88acc9e366660efb40ab5/regamedll/dlls/bot/cs_bot_manager.cpp#L1583-L1642)）。これはArchitecture Cの実装量を減らす反面、Vanilla、offline test、
独自Core境界を損なうため採用しない。

## `IGameHost`設計契約

Phase 0で確定するのは名前ではなく以下の能力である。

- lifecycle: map activated/deactivated、player connected/disconnected、round event;
- observation: immutable `WorldSnapshot`/`PlayerSnapshot`、weapon/message/sound event;
- query: trace、point contents、entity classificationを値型で返す;
- clock/event: monotonic simulation tick、server time、ordered event sequence;
- command: `submit(PlayerId, TickId, BotCommand)`。adapterがmain threadで一度だけ実行;
- observability: host call result、join transition、reject reason、movement msecをstructured trace化。

Core public interfaceに`edict_t`, `entvars_t`, `CBaseEntity`, `CBasePlayer`,
`CCSPlayer`, ReAPI object、Metamod globalsは現れない。

## Phase 1 live gate

ReGameDLL_CSを使うlive serverで、load → FakeClient allocate → GameDLL player生成 →
connect/put-in-server → team/class join →外部mapping →forward movement →kick →
disconnect cleanupを再現する。map change後に同じscenarioを再実行し、stale ID、二重agent、
残存entityがないことをtraceで証明する。Vanillaは後続adapter gateであり、Phase 1の
成功条件には混ぜない。
