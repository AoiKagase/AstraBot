// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/fake_client.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include "adapter/metamod/plugin_entry.hpp"

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

namespace {

using astrabot::adapter::metamod::FakeClientResult;
using astrabot::core::BotAgentId;
using astrabot::debug::FakeClientError;
using astrabot::debug::FakeClientStage;

struct Fixture;
Fixture* gFixture = nullptr;
std::vector<astrabot::debug::FakeClientTrace> gFakeTraces;

void captureLogConsole(plid_t /* pluginId */, const char* format, ...) {
    assert(format != nullptr);
    assert(std::strcmp(format, "%s") == 0);
    va_list arguments;
    va_start(arguments, format);
    (void)va_arg(arguments, const char*);
    va_end(arguments);
}

void captureHookTables(
    plid_t /* pluginId */,
    enginefuncs_t** engineFunctions,
    DLL_FUNCTIONS** dllFunctions,
    NEW_DLL_FUNCTIONS** newDllFunctions);
edict_t* captureCreateFakeClient(const char* name);
int captureIndexOfEdict(const edict_t* entity);
char* captureGetInfoKeyBuffer(edict_t* entity);
void captureSetClientKeyValue(
    int clientIndex, char* infoBuffer, char* key, char* value);
void captureRemoveEntity(edict_t* entity);
qboolean captureCallGameEntity(
    plid_t pluginId, const char* entityName, entvars_t* variables);
qboolean captureClientConnect(
    edict_t* entity,
    const char* name,
    const char* address,
    char rejectReason[128]);
void captureClientPutInServer(edict_t* entity);
void captureClientDisconnect(edict_t* entity);

struct Fixture {
    mutil_funcs_t utility{};
    meta_globals_t globals{};
    DLL_FUNCTIONS dll{};
    NEW_DLL_FUNCTIONS newDll{};
    gamedll_funcs_t gameDll{};
    META_FUNCTIONS callbacks{};
    enginefuncs_t engine{};
    edict_t entity{};
    char infoBuffer[256]{};
    int index{1};
    bool createReturnsNull{false};
    bool factorySucceeds{true};
    bool infoReturnsNull{false};
    bool connectSucceeds{true};
    int createCalls{0};
    int putCalls{0};
    int disconnectCalls{0};
    int removeCalls{0};
    int setKeyCalls{0};

    Fixture() {
        gFixture = this;
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
    }
};

edict_t* captureCreateFakeClient(const char* /* name */) {
    ++gFixture->createCalls;
    return gFixture->createReturnsNull ? nullptr : &gFixture->entity;
}

int captureIndexOfEdict(const edict_t* entity) {
    return entity == &gFixture->entity ? gFixture->index : 0;
}

char* captureGetInfoKeyBuffer(edict_t* /* entity */) {
    return gFixture->infoReturnsNull ? nullptr : gFixture->infoBuffer;
}

void captureSetClientKeyValue(
    int /* clientIndex */, char* /* infoBuffer */, char* /* key */, char* /* value */) {
    ++gFixture->setKeyCalls;
}

void captureRemoveEntity(edict_t* /* entity */) {
    ++gFixture->removeCalls;
}

qboolean captureCallGameEntity(
    plid_t /* pluginId */, const char* /* entityName */, entvars_t* /* variables */) {
    return gFixture->factorySucceeds ? 1 : 0;
}

qboolean captureClientConnect(
    edict_t* /* entity */, const char* /* name */, const char* /* address */, char /* rejectReason */[128]) {
    return gFixture->connectSucceeds ? 1 : 0;
}

void captureClientPutInServer(edict_t* /* entity */) {
    ++gFixture->putCalls;
}

void captureClientDisconnect(edict_t* /* entity */) {
    ++gFixture->disconnectCalls;
}

void captureHookTables(
    plid_t /* pluginId */,
    enginefuncs_t** engineFunctions,
    DLL_FUNCTIONS** dllFunctions,
    NEW_DLL_FUNCTIONS** newDllFunctions) {
    *engineFunctions = &gFixture->engine;
    *dllFunctions = &gFixture->dll;
    *newDllFunctions = &gFixture->newDll;
}

void captureFakeTrace(const astrabot::debug::FakeClientTrace& trace) noexcept {
    gFakeTraces.push_back(trace);
}

void attach(Fixture& fixture) {
    char interfaceVersion[] = META_INTERFACE_VERSION;
    plugin_info_t* pluginInfo = nullptr;
    assert(Meta_Query(interfaceVersion, &pluginInfo, &fixture.utility) != 0);
    assert(Meta_Attach(
               PT_ANYTIME,
               &fixture.callbacks,
               &fixture.globals,
               &fixture.gameDll) != 0);
    astrabot::adapter::metamod::lifecycleCoordinator().setFakeClientTraceSink(
        &captureFakeTrace);
}

void detach() {
    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    gFixture = nullptr;
    gFakeTraces.clear();
}

void activate(Fixture& fixture) {
    attach(fixture);
    assert(
        astrabot::adapter::metamod::lifecycleCoordinator().registry().activateMap(32));
}

void testSuccessfulCreationAndOpaquePrivateData() {
    Fixture fixture{};
    activate(fixture);

    const FakeClientResult result =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot-1");
    assert(result.succeeded());
    const astrabot::core::PlayerId expectedPlayer{
        1, astrabot::core::Generation{1}};
    assert(result.player == expectedPlayer);
    assert(result.agent == BotAgentId{1});
    assert(result.playerRegistration.changed());
    assert(fixture.createCalls == 1);
    assert(fixture.setKeyCalls == 3);
    assert(fixture.putCalls == 1);
    assert(fixture.disconnectCalls == 0);
    assert(fixture.removeCalls == 0);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().agents().mappingCount() == 1);
    assert(fixture.entity.pvPrivateData == nullptr);
    assert(gFakeTraces.size() == 7);
    assert(gFakeTraces.back().stage == FakeClientStage::Published);
    assert(gFakeTraces.back().agent == BotAgentId{1});
    assert(gFakeTraces.back().sequence == result.playerRegistration.event.sequence);

    const FakeClientResult duplicate =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot-2");
    assert(!duplicate.succeeded());
    assert(duplicate.error == FakeClientError::AlreadyCreated);
    assert(fixture.createCalls == 1);
    detach();
}

void testFailureRollback() {
    {
        Fixture fixture{};
        activate(fixture);
        const FakeClientResult result =
            astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
                nullptr);
        assert(result.error == FakeClientError::InvalidName);
        assert(fixture.createCalls == 0);
        assert(fixture.removeCalls == 0);
        detach();
    }
    {
        Fixture fixture{};
        fixture.createReturnsNull = true;
        activate(fixture);
        const FakeClientResult result =
            astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
                "AstraBot");
        assert(result.error == FakeClientError::CreateFailed);
        assert(fixture.removeCalls == 0);
        detach();
    }
    {
        Fixture fixture{};
        fixture.index = 0;
        activate(fixture);
        const FakeClientResult result =
            astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
                "AstraBot");
        assert(result.error == FakeClientError::InvalidSlot);
        assert(fixture.removeCalls == 1);
        detach();
    }
    {
        Fixture fixture{};
        fixture.factorySucceeds = false;
        activate(fixture);
        const FakeClientResult result =
            astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
                "AstraBot");
        assert(result.error == FakeClientError::PlayerFactoryFailed);
        assert(fixture.removeCalls == 1);
        assert(fixture.putCalls == 0);
        detach();
    }
    {
        Fixture fixture{};
        fixture.infoReturnsNull = true;
        activate(fixture);
        const FakeClientResult result =
            astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
                "AstraBot");
        assert(result.error == FakeClientError::InfoBufferFailed);
        assert(fixture.removeCalls == 1);
        detach();
    }
    {
        Fixture fixture{};
        fixture.connectSucceeds = false;
        activate(fixture);
        const FakeClientResult result =
            astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
                "AstraBot");
        assert(result.error == FakeClientError::ConnectRejected);
        assert(fixture.putCalls == 0);
        assert(fixture.disconnectCalls == 0);
        assert(fixture.removeCalls == 1);
        detach();
    }
}

void testInputAndCapacityRejection() {
    Fixture fixture{};
    activate(fixture);
    char tooLong[40]{};
    for (char& character : tooLong) {
        character = 'x';
    }
    const FakeClientResult tooLongResult =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            tooLong);
    assert(tooLongResult.error == FakeClientError::InvalidName);
    detach();

    Fixture occupiedFixture{};
    activate(occupiedFixture);
    const auto connected =
        astrabot::adapter::metamod::lifecycleCoordinator().registry().registerPlayer(1);
    assert(connected);
    const FakeClientResult occupied =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot");
    assert(occupied.error == FakeClientError::SlotOccupied);
    assert(occupiedFixture.removeCalls == 1);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().agents().mappingCount() == 0);
    detach();
}

void testFirstFrameBootstrapAndCleanup() {
    Fixture fixture{};
    attach(fixture);
    DLL_FUNCTIONS hooks{};
    int interfaceVersion = INTERFACE_VERSION;
    assert(GetEntityAPI2(&hooks, &interfaceVersion) != 0);

    hooks.pfnServerActivate(nullptr, 0, 32);
    assert(fixture.createCalls == 0);
    hooks.pfnStartFrame();
    assert(fixture.createCalls == 1);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().agents().mappingCount() == 1);
    hooks.pfnStartFrame();
    assert(fixture.createCalls == 1);

    hooks.pfnClientDisconnect(&fixture.entity);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().agents().mappingCount() == 0);
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().registry().isConnected(1));
    hooks.pfnServerDeactivate();
    hooks.pfnServerDeactivate();
    detach();
}

void testMissingFunctionIsRejectedWithoutEngineCall() {
    Fixture fixture{};
    activate(fixture);
    fixture.engine.pfnGetInfoKeyBuffer = nullptr;
    const FakeClientResult result =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot");
    assert(result.error == FakeClientError::NotConfigured);
    assert(fixture.createCalls == 0);
    detach();
}

} // namespace

int main() {
    testSuccessfulCreationAndOpaquePrivateData();
    testFailureRollback();
    testInputAndCapacityRejection();
    testFirstFrameBootstrapAndCleanup();
    testMissingFunctionIsRejectedWithoutEngineCall();
    return 0;
}
