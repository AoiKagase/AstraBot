// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/plugin_entry.hpp"

#include "adapter/metamod/console_debug.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include "adapter/metamod/sound_hooks.hpp"
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
    GET_ENGINE_FUNCTIONS_FN previousEngineFunctionsPost{nullptr};
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

bool hasRequiredFakeClientHookTable(
    const DLL_FUNCTIONS* hookedGameDllFunctions) noexcept {
    return hookedGameDllFunctions != nullptr &&
           hookedGameDllFunctions->pfnClientConnect != nullptr &&
           hookedGameDllFunctions->pfnClientPutInServer != nullptr &&
           hookedGameDllFunctions->pfnClientDisconnect != nullptr &&
           hookedGameDllFunctions->pfnClientCommand != nullptr;
}

bool hasRequiredFakeClientUtility(
    const mutil_funcs_t* utilityFunctions) noexcept {
    return utilityFunctions != nullptr &&
           utilityFunctions->pfnCallGameEntity != nullptr;
}

void logAttachedIdentity(const char* line) noexcept {
    if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr) {
        gpMetaUtilFuncs->pfnLogConsole(PLID, "%s", line);
    }
}

bool rejectAttach(const char* reason) noexcept {
    if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr) {
        gpMetaUtilFuncs->pfnLogConsole(PLID, "%s", reason);
    }
    return false;
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
    // User message IDs are deliberately resolved after attach. Some GameDLLs
    // do not register them until map activation; refusing attach here makes
    // the adapter appear unloadable without identifying the timing issue.
    if (gState.attached) {
        return rejectAttach("astrabot Meta_Attach rejected reason=already-attached");
    }
    if (!gState.queried) {
        return rejectAttach("astrabot Meta_Attach rejected reason=not-queried");
    }
    if (functionTable == nullptr) {
        return rejectAttach("astrabot Meta_Attach rejected reason=missing-function-table");
    }
    if (!gEngineGlobals || !gPluginEngine.pfnAddServerCommand ||
        !gPluginEngine.pfnCmd_Argc || !gPluginEngine.pfnCmd_Argv) {
        return rejectAttach("astrabot Meta_Attach rejected reason=engine-bootstrap");
    }
    if (metaGlobals == nullptr) {
        return rejectAttach("astrabot Meta_Attach rejected reason=missing-meta-globals");
    }
    if (!hasRequiredGameDllTables(gameDllFunctions)) {
        return rejectAttach("astrabot Meta_Attach rejected reason=missing-gamedll-tables");
    }
    if (!hasRequiredUtilityTable(gpMetaUtilFuncs)) {
        return rejectAttach("astrabot Meta_Attach rejected reason=missing-utility-table");
    }

    enginefuncs_t* engineFunctions = nullptr;
    DLL_FUNCTIONS* hookDllFunctions = nullptr;
    NEW_DLL_FUNCTIONS* hookNewDllFunctions = nullptr;
    gpMetaUtilFuncs->pfnGetHookTables(
        PLID, &engineFunctions, &hookDllFunctions, &hookNewDllFunctions);
    if (engineFunctions == nullptr ||
        engineFunctions->pfnIndexOfEdict == nullptr) {
        return rejectAttach("astrabot Meta_Attach rejected reason=missing-hook-engine");
    }
    if (hookDllFunctions == nullptr || hookNewDllFunctions == nullptr) {
        return rejectAttach("astrabot Meta_Attach rejected reason=missing-hook-tables");
    }
    if (!hasRequiredFakeClientEngine(engineFunctions)) {
        return rejectAttach("astrabot Meta_Attach rejected reason=missing-fake-client-engine");
    }
    if (!hasRequiredFakeClientHookTable(hookDllFunctions)) {
        return rejectAttach("astrabot Meta_Attach rejected reason=missing-fake-client-hook-table");
    }
    if (!hasRequiredFakeClientUtility(gpMetaUtilFuncs)) {
        return rejectAttach("astrabot Meta_Attach rejected reason=missing-fake-client-utility");
    }

    const GETENTITYAPI2_FN previousEntityApi2 = functionTable->pfnGetEntityAPI2;
    const GET_ENGINE_FUNCTIONS_FN previousEngineFunctions =
        functionTable->pfnGetEngineFunctions;
    const auto previousEngineFunctionsPost = functionTable->pfnGetEngineFunctions_Post;

    functionTable->pfnGetEntityAPI2 = &GetEntityAPI2;
    functionTable->pfnGetEngineFunctions = &GetEngineFunctions;
    functionTable->pfnGetEngineFunctions_Post = &astrabot::adapter::metamod::soundEngineFunctionsPost;

    gState.attached = true;
    gState.functionTable = functionTable;
    gState.previousEntityApi2 = previousEntityApi2;
    gState.previousEngineFunctions = previousEngineFunctions;
    gState.previousEngineFunctionsPost = previousEngineFunctionsPost;
    gpMetaGlobals = metaGlobals;
    gpGamedllFuncs = gameDllFunctions;
    // The table passed through gamedll_funcs_t is a private copy of the
    // GameDLL table for this plugin. Fake-client lifecycle calls must use the
    // GetHookTables dispatcher so other Metamod plugins and the GameDLL are
    // reached through the normal hook chain.
    astrabot::adapter::metamod::lifecycleCoordinator().configure(
        engineFunctions,
        gpMetaUtilFuncs,
        hookDllFunctions,
        {},
        gEngineGlobals);
    // The bootstrap table contains Metamod's command-registration wrapper.
    // GetHookTables returns a different table that bypasses unload tracking.
    astrabot::adapter::metamod::lifecycleCoordinator().navConsole().configure(
        &gPluginEngine, gpMetaUtilFuncs, gEngineGlobals);
    astrabot::adapter::metamod::ConsoleDebug::instance().configure(
        &gPluginEngine,
        gpMetaUtilFuncs,
        &astrabot::adapter::metamod::lifecycleCoordinator());

    astrabot::debug::emitAttached(&logAttachedIdentity);
    return 1;
}

C_DLLEXPORT FORCE_STACK_ALIGN int Meta_Detach(
    PLUG_LOADTIME /* now */, PL_UNLOAD_REASON /* reason */) {
    astrabot::adapter::metamod::ConsoleDebug::instance().reset();
    astrabot::adapter::metamod::lifecycleCoordinator().reset();
    if (gState.attached && gState.functionTable != nullptr) {
        gState.functionTable->pfnGetEntityAPI2 = gState.previousEntityApi2;
        gState.functionTable->pfnGetEngineFunctions =
            gState.previousEngineFunctions;
        gState.functionTable->pfnGetEngineFunctions_Post = gState.previousEngineFunctionsPost;
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
    astrabot::adapter::metamod::installSoundPreHooks(*engineFunctions);
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
