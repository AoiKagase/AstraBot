// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/plugin_entry.hpp"
#include "adapter/metamod/console_debug.hpp"
#include "adapter/metamod/lifecycle.hpp"

#include "debug/host_trace.hpp"

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

using astrabot::host::LifecycleEventKind;

std::vector<std::string> gLogLines;
std::vector<std::string> gTraceLines;
struct RegisteredCommand {
    std::string name;
    void (*callback)(){};
};
std::vector<RegisteredCommand> gServerCommands;
std::vector<std::string> gCommandArgs;
std::vector<astrabot::debug::LifecycleTrace> gLifecycleTraces;
std::vector<astrabot::debug::CombatTrace> gCombatTraces;

edict_t gFakeEntity{};
char gFakeInfoBuffer[256]{};
int gRunPlayerMoveCalls = 0;
bool gUserMessageIdsReady = true;

enginefuncs_t* gHookEngineFunctions = nullptr;
DLL_FUNCTIONS* gHookDllFunctions = nullptr;
NEW_DLL_FUNCTIONS* gHookNewDllFunctions = nullptr;
const edict_t* gDisconnectEntity = nullptr;

void captureLogConsole(plid_t /* pluginId */, const char* format, ...) {
    assert(format != nullptr);
    assert(std::strcmp(format, "%s") == 0);

    va_list arguments;
    va_start(arguments, format);
    const char* line = va_arg(arguments, const char*);
    if (line != nullptr) {
        gLogLines.emplace_back(line);
    }
    va_end(arguments);
}

void captureTraceLine(const char* line) noexcept {
    if (line != nullptr) {
        gTraceLines.emplace_back(line);
    }
}

void captureLifecycleTrace(
    const astrabot::debug::LifecycleTrace& trace) noexcept {
    gLifecycleTraces.push_back(trace);
}

void captureCombatTrace(
    const astrabot::debug::CombatTrace& trace) noexcept {
    gCombatTraces.push_back(trace);
}

void captureHookTables(
    plid_t /* pluginId */,
    enginefuncs_t** engineFunctions,
    DLL_FUNCTIONS** dllFunctions,
    NEW_DLL_FUNCTIONS** newDllFunctions) {
    if (engineFunctions != nullptr) {
        *engineFunctions = gHookEngineFunctions;
    }
    if (dllFunctions != nullptr) {
        *dllFunctions = gHookDllFunctions;
    }
    if (newDllFunctions != nullptr) {
        *newDllFunctions = gHookNewDllFunctions;
    }
}

int captureIndexOfEdict(const edict_t* entity) {
    return entity == gDisconnectEntity ? 1 : 0;
}

edict_t* captureCreateFakeClient(const char* /* name */) {
    return &gFakeEntity;
}

char* captureGetInfoKeyBuffer(edict_t* /* entity */) {
    return gFakeInfoBuffer;
}

void captureSetClientKeyValue(
    int /* clientIndex */, char* /* infoBuffer */, char* /* key */, char* /* value */) {}

void captureRemoveEntity(edict_t* /* entity */) {}

qboolean captureCallGameEntity(
    plid_t /* pluginId */, const char* /* entityName */, entvars_t* /* variables */) {
    return 1;
}

qboolean captureClientConnect(
    edict_t* /* entity */, const char* /* name */, const char* /* address */, char /* rejectReason */[128]) {
    return 1;
}

void captureClientPutInServer(edict_t* /* entity */) {}
void captureClientDisconnect(edict_t* /* entity */) {}
void captureClientCommand(edict_t* /* entity */) {}

int captureGetUserMsgID(
    plid_t /* pluginId */,
    const char* messageName,
    int* size) {
    if (size != nullptr) {
        *size = -1;
    }
    if (messageName == nullptr) {
        return 0;
    }
    if (!gUserMessageIdsReady) {
        return 0;
    }
    if (std::strcmp(messageName, "VGUIMenu") == 0) {
        return 11;
    }
    if (std::strcmp(messageName, "ShowMenu") == 0) {
        return 12;
    }
    if (std::strcmp(messageName, "TeamInfo") == 0) {
        return 13;
    }
    if (std::strcmp(messageName, "HLTV") == 0) {
        return 14;
    }
    if (std::strcmp(messageName, "ScreenFade") == 0) {
        return 15;
    }
    return 0;
}

int captureGetPlayerUserId(edict_t* /* entity */) {
    return 1;
}

void captureServerCommand(char* /* command */) {}
void captureServerExecute() {}
void captureAddCommand(char* name, void (*callback)()) {
    if (name != nullptr && callback != nullptr) {
        gServerCommands.push_back({name, callback});
    }
}
int captureArgc() { return static_cast<int>(gCommandArgs.size()); }
const char* captureArgv(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= gCommandArgs.size()) {
        return "";
    }
    return gCommandArgs[static_cast<std::size_t>(index)].c_str();
}
void captureRunPlayerMove(
    edict_t* /* entity */,
    const float* /* viewAngles */,
    float /* forwardMove */,
    float /* sideMove */,
    float /* upMove */,
    unsigned short /* buttons */,
    byte /* impulse */,
    byte /* msec */) {
    ++gRunPlayerMoveCalls;
}

int sentinelEntityApi2(DLL_FUNCTIONS* /* table */, int* /* version */) {
    return 17;
}

int sentinelEngineFunctions(enginefuncs_t* /* table */, int* /* version */) {
    return 19;
}

struct Fixture {
    mutil_funcs_t utility{};
    meta_globals_t globals{};
    DLL_FUNCTIONS dll{};
    DLL_FUNCTIONS hookDll{};
    NEW_DLL_FUNCTIONS newDll{};
    NEW_DLL_FUNCTIONS hookNewDll{};
    gamedll_funcs_t gameDll{};
    META_FUNCTIONS callbacks{};
    enginefuncs_t engine{};
    globalvars_t engineGlobals{};

    Fixture() {
        utility.pfnLogConsole = &captureLogConsole;
        utility.pfnGetHookTables = &captureHookTables;
        utility.pfnCallGameEntity = &captureCallGameEntity;
        utility.pfnGetUserMsgID = &captureGetUserMsgID;
        engine.pfnCreateFakeClient = &captureCreateFakeClient;
        engine.pfnIndexOfEdict = &captureIndexOfEdict;
        engine.pfnGetInfoKeyBuffer = &captureGetInfoKeyBuffer;
        engine.pfnSetClientKeyValue = &captureSetClientKeyValue;
        engine.pfnRemoveEntity = &captureRemoveEntity;
        engine.pfnGetPlayerUserId = &captureGetPlayerUserId;
        engine.pfnServerCommand = &captureServerCommand;
        engine.pfnServerExecute = &captureServerExecute;
        engine.pfnRunPlayerMove = &captureRunPlayerMove;
        engine.pfnAddServerCommand = &captureAddCommand;
        engine.pfnCmd_Argc = &captureArgc;
        engine.pfnCmd_Argv = &captureArgv;
        dll.pfnClientConnect = &captureClientConnect;
        dll.pfnClientPutInServer = &captureClientPutInServer;
        dll.pfnClientDisconnect = &captureClientDisconnect;
        dll.pfnClientCommand = &captureClientCommand;
        hookDll = dll;
        gameDll.dllapi_table = &dll;
        gameDll.newapi_table = &newDll;
        callbacks.pfnGetEntityAPI2 = &sentinelEntityApi2;
        callbacks.pfnGetEngineFunctions = &sentinelEngineFunctions;
        gHookEngineFunctions = &engine;
        gHookDllFunctions = &hookDll;
        gHookNewDllFunctions = &hookNewDll;
    }
};

void resetAdapter() {
    assert(Meta_Detach(PT_ANYTIME, PNL_NULL) != 0);
    gLogLines.clear();
    gTraceLines.clear();
    gServerCommands.clear();
    gCommandArgs.clear();
    gLifecycleTraces.clear();
    gCombatTraces.clear();
    gRunPlayerMoveCalls = 0;
    gUserMessageIdsReady = true;
}

void runServerCommand(
    const std::initializer_list<const char*>& arguments,
    const char* name) {
    gCommandArgs.clear();
    for (const char* argument : arguments) {
        gCommandArgs.emplace_back(argument == nullptr ? "" : argument);
    }
    for (const auto& command : gServerCommands) {
        if (command.name == name) {
            command.callback();
            return;
        }
    }
    assert(false && "server command was not registered");
}

void query(Fixture& fixture) {
    GiveFnptrsToDll(&fixture.engine,&fixture.engineGlobals);
    char interfaceVersion[] = META_INTERFACE_VERSION;
    plugin_info_t* pluginInfo = nullptr;
    assert(Meta_Query(interfaceVersion, &pluginInfo, &fixture.utility) != 0);
    assert(pluginInfo != nullptr);
    assert(std::strcmp(pluginInfo->name, "AstraBot Metamod-P adapter") == 0);
}

void assertCallbacksEqual(
    const META_FUNCTIONS& actual, const META_FUNCTIONS& expected) {
    assert(actual.pfnGetEntityAPI == expected.pfnGetEntityAPI);
    assert(actual.pfnGetEntityAPI_Post == expected.pfnGetEntityAPI_Post);
    assert(actual.pfnGetEntityAPI2 == expected.pfnGetEntityAPI2);
    assert(actual.pfnGetEntityAPI2_Post == expected.pfnGetEntityAPI2_Post);
    assert(actual.pfnGetNewDLLFunctions == expected.pfnGetNewDLLFunctions);
    assert(actual.pfnGetNewDLLFunctions_Post == expected.pfnGetNewDLLFunctions_Post);
    assert(actual.pfnGetEngineFunctions == expected.pfnGetEngineFunctions);
    assert(actual.pfnGetEngineFunctions_Post == expected.pfnGetEngineFunctions_Post);
}

void testQueryNullMismatchAndIdempotence() {
    resetAdapter();
    Fixture fixture{};
    char interfaceVersion[] = META_INTERFACE_VERSION;
    plugin_info_t* pluginInfo = nullptr;

    assert(Meta_Query(nullptr, &pluginInfo, &fixture.utility) == 0);
    assert(Meta_Query(interfaceVersion, nullptr, &fixture.utility) == 0);
    assert(Meta_Query(interfaceVersion, &pluginInfo, nullptr) == 0);
    assert(pluginInfo == nullptr);
    assert(gpMetaUtilFuncs == nullptr);

    char mismatch[] = "5:13-compatible";
    assert(Meta_Query(mismatch, &pluginInfo, &fixture.utility) == 0);
    assert(pluginInfo == nullptr);

    query(fixture);
    mutil_funcs_t otherUtility{};
    pluginInfo = nullptr;
    char exactAgain[] = META_INTERFACE_VERSION;
    assert(Meta_Query(exactAgain, &pluginInfo, &fixture.utility) != 0);
    assert(pluginInfo != nullptr);
    assert(Meta_Query(exactAgain, &pluginInfo, &otherUtility) == 0);
    assert(gpMetaUtilFuncs == &fixture.utility);
}

void testAttachValidationIsRollbackSafe() {
    resetAdapter();
    Fixture fixture{};
    const META_FUNCTIONS beforeQuery = fixture.callbacks;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, beforeQuery);

    query(fixture);
    const META_FUNCTIONS before = fixture.callbacks;

    GiveFnptrsToDll(nullptr,nullptr);
    assert(Meta_Attach(PT_ANYTIME,&fixture.callbacks,&fixture.globals,&fixture.gameDll)==0);
    assertCallbacksEqual(fixture.callbacks,before);
    auto noConsole=fixture.engine; noConsole.pfnAddServerCommand=nullptr;
    GiveFnptrsToDll(&noConsole,&fixture.engineGlobals);
    assert(Meta_Attach(PT_ANYTIME,&fixture.callbacks,&fixture.globals,&fixture.gameDll)==0);
    GiveFnptrsToDll(&fixture.engine,&fixture.engineGlobals);

    assert(Meta_Attach(PT_ANYTIME, nullptr, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, nullptr, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, nullptr) == 0);
    assertCallbacksEqual(fixture.callbacks, before);

    gamedll_funcs_t noDllTable = fixture.gameDll;
    noDllTable.dllapi_table = nullptr;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &noDllTable) == 0);
    assertCallbacksEqual(fixture.callbacks, before);

    gamedll_funcs_t noNewDllTable = fixture.gameDll;
    noNewDllTable.newapi_table = nullptr;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &noNewDllTable) == 0);
    assertCallbacksEqual(fixture.callbacks, before);

    auto* const savedUserMessageLookup = fixture.utility.pfnGetUserMsgID;
    fixture.utility.pfnGetUserMsgID = nullptr;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);
    fixture.utility.pfnGetUserMsgID = savedUserMessageLookup;

    enginefuncs_t noUserId = fixture.engine;
    noUserId.pfnGetPlayerUserId = nullptr;
    gHookEngineFunctions = &noUserId;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);
    gHookEngineFunctions = &fixture.engine;

    enginefuncs_t noRunPlayerMove = fixture.engine;
    noRunPlayerMove.pfnRunPlayerMove = nullptr;
    gHookEngineFunctions = &noRunPlayerMove;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);
    gHookEngineFunctions = &fixture.engine;

    enginefuncs_t noServerCommand = fixture.engine;
    noServerCommand.pfnServerCommand = nullptr;
    gHookEngineFunctions = &noServerCommand;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);
    gHookEngineFunctions = &fixture.engine;

    enginefuncs_t noServerExecute = fixture.engine;
    noServerExecute.pfnServerExecute = nullptr;
    gHookEngineFunctions = &noServerExecute;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);
    gHookEngineFunctions = &fixture.engine;

    assert(Meta_Detach(PT_ANYTIME, PNL_NULL) != 0);
    mutil_funcs_t noLogger = fixture.utility;
    noLogger.pfnLogConsole = nullptr;
    char interfaceVersion[] = META_INTERFACE_VERSION;
    plugin_info_t* pluginInfo = nullptr;
    assert(Meta_Query(interfaceVersion, &pluginInfo, &noLogger) != 0);
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);

    enginefuncs_t noIndex = fixture.engine;
    noIndex.pfnIndexOfEdict = nullptr;
    gHookEngineFunctions = &noIndex;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);
    gHookEngineFunctions = &fixture.engine;

    DLL_FUNCTIONS unrelatedDll{};
    gHookDllFunctions = &unrelatedDll;
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(fixture.callbacks, before);
    gHookDllFunctions = &fixture.hookDll;
    assert(!gLogLines.empty());
    assert(gLogLines.front().find("Meta_Attach rejected") != std::string::npos);
}

void testAttachBeforeUserMessagesAreRegistered() {
    resetAdapter();
    Fixture fixture{};
    gUserMessageIdsReady = false;
    query(fixture);

    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) != 0);
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().flashCapability());
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().perceptionIdentityDiagnostics().roundNotificationAvailable);

    astrabot::adapter::metamod::serverActivateHook(nullptr, 0, 32);
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().flashCapability());
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().perceptionIdentityDiagnostics().roundNotificationAvailable);
    assert(gLogLines.size() == 2);
    assert(gLogLines[1] == "astrabot user-message-ids pending");

    gUserMessageIdsReady = true;
    astrabot::adapter::metamod::startFrameHook();
    assert(astrabot::adapter::metamod::lifecycleCoordinator().flashCapability());
    assert(astrabot::adapter::metamod::lifecycleCoordinator().perceptionIdentityDiagnostics().roundNotificationAvailable);
    assert(gLogLines.size() == 3);
    assert(gLogLines[2] == "astrabot user-message-ids resolved");

    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
}

void testSuccessfulAttachDoubleAttachAndDetach() {
    resetAdapter();
    Fixture fixture{};
    query(fixture);
    const META_FUNCTIONS before = fixture.callbacks;

    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) != 0);
    assert(fixture.callbacks.pfnGetEntityAPI2 == &GetEntityAPI2);
    assert(fixture.callbacks.pfnGetEngineFunctions == &GetEngineFunctions);
    assert(fixture.callbacks.pfnGetEntityAPI == before.pfnGetEntityAPI);
    assert(fixture.callbacks.pfnGetEntityAPI_Post == before.pfnGetEntityAPI_Post);
    assert(fixture.callbacks.pfnGetEntityAPI2_Post == before.pfnGetEntityAPI2_Post);
    assert(fixture.callbacks.pfnGetNewDLLFunctions == before.pfnGetNewDLLFunctions);
    assert(fixture.callbacks.pfnGetNewDLLFunctions_Post == before.pfnGetNewDLLFunctions_Post);
    assert(fixture.callbacks.pfnGetEngineFunctions_Post != nullptr);
    assert(fixture.callbacks.pfnGetEngineFunctions_Post != before.pfnGetEngineFunctions_Post);
    enginefuncs_t soundHooks{}; int soundVersion = ENGINE_INTERFACE_VERSION;
    assert(fixture.callbacks.pfnGetEngineFunctions_Post(&soundHooks,&soundVersion));
    assert(soundHooks.pfnPrecacheEvent && soundHooks.pfnEmitSound && soundHooks.pfnEmitAmbientSound && soundHooks.pfnPlaybackEvent);
    assert(!soundHooks.pfnTraceLine && !soundHooks.pfnRunPlayerMove && !soundHooks.pfnMessageBegin);
    assert(gpMetaGlobals == &fixture.globals);
    assert(gpGamedllFuncs == &fixture.gameDll);
    assert(gpGamedllFuncs->dllapi_table == &fixture.dll);
    assert(gHookDllFunctions == &fixture.hookDll);
    assert(gHookDllFunctions != gpGamedllFuncs->dllapi_table);
    assert(gLogLines.size() == 1);
    assert(gLogLines.front() == astrabot::debug::attachedIdentityLine());

    META_FUNCTIONS secondCallbacks{};
    const META_FUNCTIONS secondBefore = secondCallbacks;
    assert(Meta_Attach(PT_ANYTIME, &secondCallbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(secondCallbacks, secondBefore);
    assert(gLogLines.size() == 2);
    assert(gLogLines[1].find("reason=already-attached") != std::string::npos);

    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assertCallbacksEqual(fixture.callbacks, before);
    assert(gpMetaGlobals == nullptr);
    assert(gpGamedllFuncs == nullptr);
    assert(gpMetaUtilFuncs == nullptr);
    assert(gLogLines.size() == 2);

    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assert(gLogLines.size() == 2);

    query(fixture);
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) != 0);
    assert(gLogLines.size() == 3);
    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assert(gLogLines.size() == 3);
}

void testEmptyHookTablesAndInterfaceChecks() {
    resetAdapter();

    DLL_FUNCTIONS entityTable{};
    entityTable.pfnStartFrame = reinterpret_cast<decltype(entityTable.pfnStartFrame)>(1);
    int entityVersion = INTERFACE_VERSION;
    assert(GetEntityAPI2(nullptr, &entityVersion) == 0);
    assert(GetEntityAPI2(&entityTable, nullptr) == 0);
    int wrongEntityVersion = INTERFACE_VERSION + 1;
    DLL_FUNCTIONS entityMismatch = entityTable;
    assert(GetEntityAPI2(&entityMismatch, &wrongEntityVersion) == 0);
    assert(wrongEntityVersion == INTERFACE_VERSION);
    assert(entityMismatch.pfnStartFrame == entityTable.pfnStartFrame);
    assert(GetEntityAPI2(&entityTable, &entityVersion) != 0);
    assert(entityTable.pfnServerActivate ==
           &astrabot::adapter::metamod::serverActivateHook);
    assert(entityTable.pfnServerDeactivate ==
           &astrabot::adapter::metamod::serverDeactivateHook);
    assert(entityTable.pfnClientDisconnect ==
           &astrabot::adapter::metamod::clientDisconnectHook);
    assert(entityTable.pfnStartFrame ==
           &astrabot::adapter::metamod::startFrameHook);
    assert(entityTable.pfnGameInit == nullptr);
    assert(entityTable.pfnClientConnect == nullptr);
    assert(entityTable.pfnThink == nullptr);
    assert(entityTable.pfnPlayerPreThink == nullptr);

    enginefuncs_t engineTable{};
    engineTable.pfnTime = reinterpret_cast<decltype(engineTable.pfnTime)>(1);
    int engineVersion = ENGINE_INTERFACE_VERSION;
    assert(GetEngineFunctions(nullptr, &engineVersion) == 0);
    assert(GetEngineFunctions(&engineTable, nullptr) == 0);
    int wrongEngineVersion = ENGINE_INTERFACE_VERSION + 1;
    enginefuncs_t engineMismatch = engineTable;
    assert(GetEngineFunctions(&engineMismatch, &wrongEngineVersion) == 0);
    assert(wrongEngineVersion == ENGINE_INTERFACE_VERSION);
    assert(engineMismatch.pfnTime == engineTable.pfnTime);
    assert(GetEngineFunctions(&engineTable, &engineVersion) != 0);
    assert(engineTable.pfnMessageBegin ==
           &astrabot::adapter::metamod::messageBeginHook);
    assert(engineTable.pfnMessageEnd ==
           &astrabot::adapter::metamod::messageEndHook);
    assert(engineTable.pfnWriteByte ==
           &astrabot::adapter::metamod::writeByteHook);
    assert(engineTable.pfnWriteChar ==
           &astrabot::adapter::metamod::writeCharHook);
    assert(engineTable.pfnWriteShort ==
           &astrabot::adapter::metamod::writeShortHook);
    assert(engineTable.pfnWriteString ==
           &astrabot::adapter::metamod::writeStringHook);
    assert(engineTable.pfnCmd_Args ==
           &astrabot::adapter::metamod::commandArgsHook);
    assert(engineTable.pfnCmd_Argv ==
           &astrabot::adapter::metamod::commandArgvHook);
    assert(engineTable.pfnCmd_Argc ==
           &astrabot::adapter::metamod::commandArgcHook);
    assert(engineTable.pfnClientCommand == nullptr);
    assert(engineTable.pfnRunPlayerMove == nullptr);
    assert(engineTable.pfnTime == nullptr);
}

void testLifecycleHooksAndCoordinatorCleanup() {
    resetAdapter();
    Fixture fixture{};
    query(fixture);
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) != 0);

    DLL_FUNCTIONS hooks{};
    int interfaceVersion = INTERFACE_VERSION;
    assert(GetEntityAPI2(&hooks, &interfaceVersion) != 0);
    assert(hooks.pfnServerActivate != nullptr);
    assert(hooks.pfnServerDeactivate != nullptr);
    assert(hooks.pfnClientDisconnect != nullptr);
    assert(hooks.pfnStartFrame != nullptr);
    assert(hooks.pfnGameInit == nullptr);
    assert(hooks.pfnClientConnect == nullptr);
    assert(hooks.pfnThink == nullptr);
    assert(hooks.pfnPlayerPreThink == nullptr);

    astrabot::adapter::metamod::setLifecycleTraceSink(&captureLifecycleTrace);
    gLifecycleTraces.clear();

    fixture.globals.mres = MRES_SUPERCEDE;
    hooks.pfnServerActivate(nullptr, 0, 32);
    assert(fixture.globals.mres == MRES_IGNORED);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().registry().isMapActive());

    fixture.globals.mres = MRES_SUPERCEDE;
    hooks.pfnStartFrame();
    assert(fixture.globals.mres == MRES_IGNORED);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().registry().currentTick() ==
           astrabot::core::TickId{1});

    const auto connected =
        astrabot::adapter::metamod::lifecycleCoordinator().registry().registerPlayer(1);
    assert(connected);
    edict_t entity{};
    gDisconnectEntity = &entity;
    fixture.globals.mres = MRES_SUPERCEDE;
    hooks.pfnClientDisconnect(&entity);
    assert(fixture.globals.mres == MRES_IGNORED);
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().registry().isConnected(1));

    fixture.globals.mres = MRES_SUPERCEDE;
    hooks.pfnServerDeactivate();
    assert(fixture.globals.mres == MRES_IGNORED);
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().registry().isMapActive());

    fixture.globals.mres = MRES_SUPERCEDE;
    hooks.pfnServerDeactivate();
    assert(fixture.globals.mres == MRES_IGNORED);

    assert(gLifecycleTraces.size() == 4);
    assert(gLifecycleTraces[0].kind == LifecycleEventKind::MapActivated);
    assert(gLifecycleTraces[0].accepted);
    assert(gLifecycleTraces[0].sequence == 1);
    assert(gLifecycleTraces[1].kind == LifecycleEventKind::FrameStarted);
    assert(gLifecycleTraces[1].tick == astrabot::core::TickId{1});
    assert(gLifecycleTraces[1].sequence == 2);
    assert(gLifecycleTraces[2].kind == LifecycleEventKind::PlayerDisconnected);
    assert(gLifecycleTraces[2].sequence == 4);
    assert(gLifecycleTraces[3].kind == LifecycleEventKind::MapDeactivated);
    assert(gLifecycleTraces[3].sequence == 5);

    Meta_Detach(PT_ANYTIME, PNL_COMMAND);
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().registry().isMapActive());
    assert(astrabot::adapter::metamod::lifecycleCoordinator().registry().eventSequence() == 0);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().registry().mapGeneration() ==
           astrabot::core::MapGeneration::invalid());
    gDisconnectEntity = nullptr;
}

void testTraceSink() {
    gTraceLines.clear();
    gLifecycleTraces.clear();
    astrabot::debug::emitAttached(&captureTraceLine);
    assert(gTraceLines.size() == 1);
    assert(gTraceLines.front() ==
           "astrabot version=0.1.0 adapter=metamod-p interface=5:13 outcome=attached");
    assert(gLifecycleTraces.empty());
    astrabot::debug::emitAttached(nullptr);
    assert(gTraceLines.size() == 1);
    assert(gLifecycleTraces.empty());
}

void testConsoleDebugCommandAndTracePrefixes() {
    resetAdapter();
    Fixture fixture{};
    query(fixture);
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) != 0);

    bool registered = false;
    bool addBotRegistered = false;
    for (const auto& command : gServerCommands) {
        if (command.name == "astrabot_debug") {
            registered = true;
            assert(command.callback != nullptr);
        }
        if (command.name == "astrabot_addbot") {
            addBotRegistered = true;
            assert(command.callback != nullptr);
        }
    }
    assert(registered);
    assert(addBotRegistered);

    runServerCommand({"astrabot_debug"}, "astrabot_debug");
    assert(gLogLines.back() == "[ASTRABOT][DEBUG][COMMAND] state=off");

    astrabot::debug::JoinTrace join{};
    join.phase = astrabot::adapter::cstrike::JoinPhase::Failed;
    join.error = astrabot::adapter::cstrike::JoinError::MenuOptionUnavailable;
    join.map = {1};
    join.player = {1, {2}};
    join.team = astrabot::adapter::cstrike::Team::CounterTerrorist;
    join.classNumber = 3;
    join.tick = {12};
    join.sequence = 7;
    join.attempts = 2;
    join.changed = true;

    gLogLines.clear();
    astrabot::adapter::metamod::ConsoleDebug::instance().joinTrace(join);
    assert(gLogLines.empty());

    runServerCommand({"astrabot_debug", "1"}, "astrabot_debug");
    assert(gLogLines.size() == 1);
    assert(gLogLines.front() == "[ASTRABOT][DEBUG][COMMAND] enabled=1");

    astrabot::debug::LifecycleTrace lifecycle{};
    lifecycle.kind = astrabot::host::LifecycleEventKind::FrameStarted;
    lifecycle.map = {1};
    lifecycle.tick = {12};
    astrabot::adapter::metamod::ConsoleDebug::instance().lifecycleTrace(lifecycle);
    assert(gLogLines.size() == 1);

    lifecycle.kind = astrabot::host::LifecycleEventKind::MapActivated;
    lifecycle.playerGeneration = {2};
    lifecycle.sequence = 3;
    lifecycle.accepted = true;
    lifecycle.changed = true;
    astrabot::adapter::metamod::ConsoleDebug::instance().lifecycleTrace(lifecycle);

    astrabot::debug::FakeClientTrace fake{};
    fake.stage = astrabot::debug::FakeClientStage::Published;
    fake.map = {1};
    fake.slot = 1;
    fake.playerGeneration = {2};
    fake.agent = {3};
    fake.sequence = 4;
    fake.accepted = true;
    fake.changed = true;
    astrabot::adapter::metamod::ConsoleDebug::instance().fakeClientTrace(fake);

    astrabot::debug::MovementTrace movement{};
    movement.player = {1, {2}};
    movement.engineMsec = 16;
    movement.engineCall = true;
    movement.source = astrabot::debug::MovementTraceSource::Idle;
    movement.callCount = 1;
    astrabot::adapter::metamod::ConsoleDebug::instance().movementTrace(movement);

    astrabot::adapter::metamod::ConsoleDebug::instance().joinTrace(join);

    astrabot::debug::RemovalTrace removal{};
    removal.outcome = astrabot::debug::RemovalOutcome::KickQueued;
    removal.map = {1};
    removal.player = {1, {2}};
    removal.tick = {12};
    removal.sequence = 8;
    removal.mappingPresent = true;
    removal.entityPresent = true;
    astrabot::adapter::metamod::ConsoleDebug::instance().removalTrace(removal);

    assert(gLogLines.size() == 6);
    assert(gLogLines[1].rfind("[ASTRABOT][DEBUG][LIFECYCLE] ", 0) == 0);
    assert(gLogLines[1].find("kind=MapActivated") != std::string::npos);
    assert(gLogLines[2].rfind("[ASTRABOT][DEBUG][FAKECLIENT] ", 0) == 0);
    assert(gLogLines[2].find("stage=Published") != std::string::npos);
    assert(gLogLines[3].rfind("[ASTRABOT][DEBUG][MOVEMENT] ", 0) == 0);
    assert(gLogLines[3].find("source=Idle") != std::string::npos);
    assert(gLogLines[4].rfind("[ASTRABOT][DEBUG][JOIN] ", 0) == 0);
    assert(gLogLines[4].find("error=MenuOptionUnavailable") != std::string::npos);
    assert(gLogLines[5].rfind("[ASTRABOT][DEBUG][REMOVAL] ", 0) == 0);
    assert(gLogLines[5].find("outcome=KickQueued") != std::string::npos);

    runServerCommand({"astrabot_debug", "9"}, "astrabot_debug");
    assert(gLogLines.back().find("error=InvalidArguments") != std::string::npos);
    gLogLines.clear();
    astrabot::adapter::metamod::ConsoleDebug::instance().joinTrace(join);
    assert(gLogLines.size() == 1);

    runServerCommand({"astrabot_debug", "0"}, "astrabot_debug");
    assert(gLogLines.size() == 2);
    astrabot::adapter::metamod::ConsoleDebug::instance().joinTrace(join);
    assert(gLogLines.size() == 2);

    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    gLogLines.clear();
    astrabot::adapter::metamod::ConsoleDebug::instance().joinTrace(join);
    assert(gLogLines.empty());
}

void testCombatLifecycleRejectsInvalidActorWithTrace() {
    auto& lifecycle = astrabot::adapter::metamod::lifecycleCoordinator();
    lifecycle.reset();
    gCombatTraces.clear();
    lifecycle.setCombatTraceSink(&captureCombatTrace);

    astrabot::core::combat::CombatDecision decision{};
    decision.action = astrabot::core::combat::CombatAction::Track;
    decision.view = {12.0F, -45.0F, 0.0F};
    decision.confidence = 1.0;
    decision.reason = astrabot::core::combat::CombatReason::Accepted;
    decision.inputTick = {7};
    const auto result = lifecycle.submitCombatDecision(
        {}, {}, {1}, decision, astrabot::core::BotCommand::neutral(8));

    assert(!result);
    assert(result.error == astrabot::adapter::metamod::CombatSubmitError::InvalidActor);
    assert(result.composition);
    assert(gCombatTraces.size() == 1);
    assert(gCombatTraces.front().action == decision.action);
    assert(gCombatTraces.front().inputTick == astrabot::core::TickId{1});
    assert(gCombatTraces.front().commandBuilt);
    assert(!gCombatTraces.front().commandAccepted);
}

} // namespace

int main() {
    testQueryNullMismatchAndIdempotence();
    testAttachValidationIsRollbackSafe();
    testAttachBeforeUserMessagesAreRegistered();
    testSuccessfulAttachDoubleAttachAndDetach();
    testEmptyHookTablesAndInterfaceChecks();
    testLifecycleHooksAndCoordinatorCleanup();
    testTraceSink();
    testConsoleDebugCommandAndTracePrefixes();
    testCombatLifecycleRejectsInvalidActorWithTrace();
    return 0;
}
