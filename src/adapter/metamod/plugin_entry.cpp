// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/plugin_entry.hpp"

#include "adapter/metamod/lifecycle.hpp"
#include "debug/host_trace.hpp"

#include <cstring>

namespace {

char kPluginInterfaceVersion[] = META_INTERFACE_VERSION;
char kPluginName[] = "AstraBot Metamod-P adapter";
char kPluginVersion[] = "0.1.0";
char kPluginDate[] = "2026/09/04";
char kPluginAuthor[] = "AstraBot contributors";
char kPluginUrl[] = "";
char kPluginLogTag[] = "ASTRABOT";

struct AdapterState {
    bool queried{false};
    bool attached{false};
    mutil_funcs_t* queriedUtility{nullptr};
    META_FUNCTIONS* functionTable{nullptr};
    GETENTITYAPI2_FN previousEntityApi2{nullptr};
    GET_ENGINE_FUNCTIONS_FN previousEngineFunctions{nullptr};
};

AdapterState gState{};
enginefuncs_t gPluginEngine{};
globalvars_t* gEngineGlobals{};

bool hasRequiredGameDllTables(const gamedll_funcs_t* gameDllFunctions) noexcept {
    return gameDllFunctions != nullptr &&
           gameDllFunctions->dllapi_table != nullptr &&
           gameDllFunctions->newapi_table != nullptr;
}

bool hasRequiredUtilityTable(const mutil_funcs_t* utilityFunctions) noexcept {
    return utilityFunctions != nullptr &&
           utilityFunctions->pfnLogConsole != nullptr &&
           utilityFunctions->pfnGetHookTables != nullptr &&
           utilityFunctions->pfnGetUserMsgID != nullptr;
}

bool hasRequiredFakeClientEngine(const enginefuncs_t* engineFunctions) noexcept {
    return engineFunctions != nullptr &&
           engineFunctions->pfnIndexOfEdict != nullptr &&
           engineFunctions->pfnRunPlayerMove != nullptr &&
           engineFunctions->pfnCreateFakeClient != nullptr &&
           engineFunctions->pfnGetInfoKeyBuffer != nullptr &&
           engineFunctions->pfnSetClientKeyValue != nullptr &&
           engineFunctions->pfnRemoveEntity != nullptr &&
           engineFunctions->pfnGetPlayerUserId != nullptr &&
           engineFunctions->pfnServerCommand != nullptr &&
           engineFunctions->pfnServerExecute != nullptr;
}

bool hasRequiredFakeClientGameDll(
    const DLL_FUNCTIONS* gameDllFunctions) noexcept {
    return gameDllFunctions != nullptr &&
           gameDllFunctions->pfnClientConnect != nullptr &&
           gameDllFunctions->pfnClientPutInServer != nullptr &&
           gameDllFunctions->pfnClientDisconnect != nullptr &&
           gameDllFunctions->pfnClientCommand != nullptr;
}

bool hasRequiredFakeClientUtility(
    const mutil_funcs_t* utilityFunctions) noexcept {
    return utilityFunctions != nullptr &&
           utilityFunctions->pfnCallGameEntity != nullptr;
}

bool resolveUserMessageIds(
    mutil_funcs_t* utilityFunctions,
    astrabot::adapter::cstrike::UserMessageIds& ids) noexcept {
    if (utilityFunctions == nullptr ||
        utilityFunctions->pfnGetUserMsgID == nullptr) {
        return false;
    }
    int messageSize = 0;
    ids.vguiMenu = utilityFunctions->pfnGetUserMsgID(
        PLID, "VGUIMenu", &messageSize);
    ids.showMenu = utilityFunctions->pfnGetUserMsgID(
        PLID, "ShowMenu", &messageSize);
    ids.teamInfo = utilityFunctions->pfnGetUserMsgID(
        PLID, "TeamInfo", &messageSize);
    return ids.valid();
}

void logAttachedIdentity(const char* line) noexcept {
    if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr) {
        gpMetaUtilFuncs->pfnLogConsole(PLID, "%s", line);
    }
}

void resetState() noexcept {
    gState = {};
    gpMetaGlobals = nullptr;
    gpGamedllFuncs = nullptr;
    gpMetaUtilFuncs = nullptr;
    gPluginEngine = {};
    gEngineGlobals = nullptr;
}

} // namespace

plugin_info_t Plugin_info = {
    kPluginInterfaceVersion,
    kPluginName,
    kPluginVersion,
    kPluginDate,
    kPluginAuthor,
    kPluginUrl,
    kPluginLogTag,
    PT_ANYTIME,
    PT_ANYPAUSE,
};

meta_globals_t* gpMetaGlobals = nullptr;
gamedll_funcs_t* gpGamedllFuncs = nullptr;
mutil_funcs_t* gpMetaUtilFuncs = nullptr;

extern "C" void WINAPI GiveFnptrsToDll(enginefuncs_t* engine, globalvars_t* globals) {
    if (gState.attached) return;
    gPluginEngine = engine && globals ? *engine : enginefuncs_t{};
    gEngineGlobals = engine && globals ? globals : nullptr;
}

C_DLLEXPORT FORCE_STACK_ALIGN int Meta_Query(
    char* interfaceVersion,
    plugin_info_t** pluginInfo,
    mutil_funcs_t* metaUtilFunctions) {
    if (interfaceVersion == nullptr || pluginInfo == nullptr ||
        metaUtilFunctions == nullptr) {
        return 0;
    }
    if (std::strcmp(interfaceVersion, META_INTERFACE_VERSION) != 0) {
        return 0;
    }
    if (gState.attached) {
        return 0;
    }
    if (gState.queried && gState.queriedUtility != metaUtilFunctions) {
        return 0;
    }

    *pluginInfo = &Plugin_info;
    if (!gState.queried) {
        gState.queried = true;
        gState.queriedUtility = metaUtilFunctions;
        gpMetaUtilFuncs = metaUtilFunctions;
    }
    return 1;
}

C_DLLEXPORT FORCE_STACK_ALIGN int Meta_Attach(
    PLUG_LOADTIME /* now */,
    META_FUNCTIONS* functionTable,
    meta_globals_t* metaGlobals,
    gamedll_funcs_t* gameDllFunctions) {
    // The pinned Meta_Attach ABI has no enginefuncs_t argument.  Metamod-P
    // supplies the live engine table through the pinned utility callback.
    if (gState.attached || !gState.queried || functionTable == nullptr ||
        !gEngineGlobals || !gPluginEngine.pfnAddServerCommand ||
        !gPluginEngine.pfnCmd_Argc || !gPluginEngine.pfnCmd_Argv ||
        metaGlobals == nullptr || !hasRequiredGameDllTables(gameDllFunctions) ||
        !hasRequiredUtilityTable(gpMetaUtilFuncs)) {
        return 0;
    }

    enginefuncs_t* engineFunctions = nullptr;
    DLL_FUNCTIONS* hookDllFunctions = nullptr;
    NEW_DLL_FUNCTIONS* hookNewDllFunctions = nullptr;
    gpMetaUtilFuncs->pfnGetHookTables(
        PLID, &engineFunctions, &hookDllFunctions, &hookNewDllFunctions);
    if (engineFunctions == nullptr ||
        engineFunctions->pfnIndexOfEdict == nullptr ||
        hookDllFunctions == nullptr || hookNewDllFunctions == nullptr ||
        hookDllFunctions != gameDllFunctions->dllapi_table ||
        hookNewDllFunctions != gameDllFunctions->newapi_table ||
        !hasRequiredFakeClientEngine(engineFunctions) ||
        !hasRequiredFakeClientGameDll(gameDllFunctions->dllapi_table) ||
        !hasRequiredFakeClientUtility(gpMetaUtilFuncs)) {
        return 0;
    }

    astrabot::adapter::cstrike::UserMessageIds userMessageIds{};
    if (!resolveUserMessageIds(gpMetaUtilFuncs, userMessageIds)) {
        return 0;
    }

    const GETENTITYAPI2_FN previousEntityApi2 = functionTable->pfnGetEntityAPI2;
    const GET_ENGINE_FUNCTIONS_FN previousEngineFunctions =
        functionTable->pfnGetEngineFunctions;

    functionTable->pfnGetEntityAPI2 = &GetEntityAPI2;
    functionTable->pfnGetEngineFunctions = &GetEngineFunctions;

    gState.attached = true;
    gState.functionTable = functionTable;
    gState.previousEntityApi2 = previousEntityApi2;
    gState.previousEngineFunctions = previousEngineFunctions;
    gpMetaGlobals = metaGlobals;
    gpGamedllFuncs = gameDllFunctions;
    astrabot::adapter::metamod::lifecycleCoordinator().configure(
        engineFunctions,
        gpMetaUtilFuncs,
        gameDllFunctions->dllapi_table,
        userMessageIds,
        gEngineGlobals);
    // The bootstrap table contains Metamod's command-registration wrapper.
    // GetHookTables returns a different table that bypasses unload tracking.
    astrabot::adapter::metamod::lifecycleCoordinator().navConsole().configure(
        &gPluginEngine, gpMetaUtilFuncs, gEngineGlobals);

    astrabot::debug::emitAttached(&logAttachedIdentity);
    return 1;
}

C_DLLEXPORT FORCE_STACK_ALIGN int Meta_Detach(
    PLUG_LOADTIME /* now */, PL_UNLOAD_REASON /* reason */) {
    astrabot::adapter::metamod::lifecycleCoordinator().reset();
    if (gState.attached && gState.functionTable != nullptr) {
        gState.functionTable->pfnGetEntityAPI2 = gState.previousEntityApi2;
        gState.functionTable->pfnGetEngineFunctions =
            gState.previousEngineFunctions;
    }
    resetState();
    return 1;
}

C_DLLEXPORT FORCE_STACK_ALIGN int GetEntityAPI2(
    DLL_FUNCTIONS* functionTable, int* interfaceVersion) {
    if (functionTable == nullptr || interfaceVersion == nullptr) {
        return 0;
    }
    if (*interfaceVersion != INTERFACE_VERSION) {
        *interfaceVersion = INTERFACE_VERSION;
        return 0;
    }

    const DLL_FUNCTIONS emptyHookTable{};
    std::memcpy(functionTable, &emptyHookTable, sizeof(emptyHookTable));
    functionTable->pfnServerActivate =
        &astrabot::adapter::metamod::serverActivateHook;
    functionTable->pfnServerDeactivate =
        &astrabot::adapter::metamod::serverDeactivateHook;
    functionTable->pfnClientDisconnect =
        &astrabot::adapter::metamod::clientDisconnectHook;
    functionTable->pfnStartFrame = &astrabot::adapter::metamod::startFrameHook;
    return 1;
}

C_DLLEXPORT FORCE_STACK_ALIGN int GetEngineFunctions(
    enginefuncs_t* engineFunctions, int* interfaceVersion) {
    if (engineFunctions == nullptr || interfaceVersion == nullptr) {
        return 0;
    }
    if (*interfaceVersion != ENGINE_INTERFACE_VERSION) {
        *interfaceVersion = ENGINE_INTERFACE_VERSION;
        return 0;
    }

    const enginefuncs_t emptyHookTable{};
    std::memcpy(engineFunctions, &emptyHookTable, sizeof(emptyHookTable));
    engineFunctions->pfnMessageBegin =
        &astrabot::adapter::metamod::messageBeginHook;
    engineFunctions->pfnMessageEnd =
        &astrabot::adapter::metamod::messageEndHook;
    engineFunctions->pfnWriteByte =
        &astrabot::adapter::metamod::writeByteHook;
    engineFunctions->pfnWriteChar =
        &astrabot::adapter::metamod::writeCharHook;
    engineFunctions->pfnWriteShort =
        &astrabot::adapter::metamod::writeShortHook;
    engineFunctions->pfnWriteString =
        &astrabot::adapter::metamod::writeStringHook;
    engineFunctions->pfnCmd_Args =
        &astrabot::adapter::metamod::commandArgsHook;
    engineFunctions->pfnCmd_Argv =
        &astrabot::adapter::metamod::commandArgvHook;
    engineFunctions->pfnCmd_Argc =
        &astrabot::adapter::metamod::commandArgcHook;
    return 1;
}
