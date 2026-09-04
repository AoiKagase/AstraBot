// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/plugin_entry.hpp"
#include "adapter/metamod/lifecycle.hpp"

#include "debug/host_trace.hpp"

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

namespace {

using astrabot::host::LifecycleEventKind;

std::vector<std::string> gLogLines;
std::vector<std::string> gTraceLines;
std::vector<astrabot::debug::LifecycleTrace> gLifecycleTraces;

edict_t gFakeEntity{};
char gFakeInfoBuffer[256]{};

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
    NEW_DLL_FUNCTIONS newDll{};
    gamedll_funcs_t gameDll{};
    META_FUNCTIONS callbacks{};
    enginefuncs_t engine{};

    Fixture() {
        utility.pfnLogConsole = &captureLogConsole;
        utility.pfnGetHookTables = &captureHookTables;
        utility.pfnCallGameEntity = &captureCallGameEntity;
        engine.pfnCreateFakeClient = &captureCreateFakeClient;
        engine.pfnIndexOfEdict = &captureIndexOfEdict;
        engine.pfnGetInfoKeyBuffer = &captureGetInfoKeyBuffer;
        engine.pfnSetClientKeyValue = &captureSetClientKeyValue;
        engine.pfnRemoveEntity = &captureRemoveEntity;
        dll.pfnClientConnect = &captureClientConnect;
        dll.pfnClientPutInServer = &captureClientPutInServer;
        dll.pfnClientDisconnect = &captureClientDisconnect;
        gameDll.dllapi_table = &dll;
        gameDll.newapi_table = &newDll;
        callbacks.pfnGetEntityAPI2 = &sentinelEntityApi2;
        callbacks.pfnGetEngineFunctions = &sentinelEngineFunctions;
        gHookEngineFunctions = &engine;
        gHookDllFunctions = &dll;
        gHookNewDllFunctions = &newDll;
    }
};

void resetAdapter() {
    assert(Meta_Detach(PT_ANYTIME, PNL_NULL) != 0);
    gLogLines.clear();
    gTraceLines.clear();
    gLifecycleTraces.clear();
}

void query(Fixture& fixture) {
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
    gHookDllFunctions = &fixture.dll;
    assert(gLogLines.empty());
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
    assert(fixture.callbacks.pfnGetEngineFunctions_Post == before.pfnGetEngineFunctions_Post);
    assert(gpMetaGlobals == &fixture.globals);
    assert(gpGamedllFuncs == &fixture.gameDll);
    assert(gLogLines.size() == 1);
    assert(gLogLines.front() == astrabot::debug::attachedIdentityLine());

    META_FUNCTIONS secondCallbacks{};
    const META_FUNCTIONS secondBefore = secondCallbacks;
    assert(Meta_Attach(PT_ANYTIME, &secondCallbacks, &fixture.globals, &fixture.gameDll) == 0);
    assertCallbacksEqual(secondCallbacks, secondBefore);
    assert(gLogLines.size() == 1);

    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assertCallbacksEqual(fixture.callbacks, before);
    assert(gpMetaGlobals == nullptr);
    assert(gpGamedllFuncs == nullptr);
    assert(gpMetaUtilFuncs == nullptr);
    assert(gLogLines.size() == 1);

    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assert(gLogLines.size() == 1);

    query(fixture);
    assert(Meta_Attach(PT_ANYTIME, &fixture.callbacks, &fixture.globals, &fixture.gameDll) != 0);
    assert(gLogLines.size() == 2);
    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assert(gLogLines.size() == 2);
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
    const enginefuncs_t emptyEngineTable{};
    assert(std::memcmp(&engineTable, &emptyEngineTable, sizeof(engineTable)) == 0);
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

} // namespace

int main() {
    testQueryNullMismatchAndIdempotence();
    testAttachValidationIsRollbackSafe();
    testSuccessfulAttachDoubleAttachAndDetach();
    testEmptyHookTablesAndInterfaceChecks();
    testLifecycleHooksAndCoordinatorCleanup();
    testTraceSink();
    return 0;
}
