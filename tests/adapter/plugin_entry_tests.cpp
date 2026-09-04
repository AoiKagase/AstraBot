// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/plugin_entry.hpp"

#include "debug/host_trace.hpp"

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::vector<std::string> gLogLines;
std::vector<std::string> gTraceLines;

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

    Fixture() {
        utility.pfnLogConsole = &captureLogConsole;
        gameDll.dllapi_table = &dll;
        gameDll.newapi_table = &newDll;
        callbacks.pfnGetEntityAPI2 = &sentinelEntityApi2;
        callbacks.pfnGetEngineFunctions = &sentinelEngineFunctions;
    }
};

void resetAdapter() {
    assert(Meta_Detach(PT_ANYTIME, PNL_NULL) != 0);
    gLogLines.clear();
    gTraceLines.clear();
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
    const DLL_FUNCTIONS emptyEntityTable{};
    assert(std::memcmp(&entityTable, &emptyEntityTable, sizeof(entityTable)) == 0);

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

void testTraceSink() {
    gTraceLines.clear();
    astrabot::debug::emitAttached(&captureTraceLine);
    assert(gTraceLines.size() == 1);
    assert(gTraceLines.front() ==
           "astrabot version=0.1.0 adapter=metamod-p interface=5:13 outcome=attached");
    astrabot::debug::emitAttached(nullptr);
    assert(gTraceLines.size() == 1);
}

} // namespace

int main() {
    testQueryNullMismatchAndIdempotence();
    testAttachValidationIsRollbackSafe();
    testSuccessfulAttachDoubleAttachAndDetach();
    testEmptyHookTablesAndInterfaceChecks();
    testTraceSink();
    return 0;
}
