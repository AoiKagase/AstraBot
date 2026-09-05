// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include <fstream>
#include <iterator>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "adapter/metamod/fake_client.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include "adapter/metamod/plugin_entry.hpp"

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "../nav/route_fixture.hpp"
#include "../nav/evidence/fixture.hpp"

std::map<std::string, void(*)()> gNavCommands;
std::vector<std::string> gNavArgs;
std::vector<std::string> gNavOutput;
bool gGroundMissing=false;
bool gInvalidateDuringGround=false;
void captureAddCommand(char* name, void(*callback)()) { gNavCommands[name]=callback; }
void unexpectedHookRegistration(char*, void(*)()) { std::abort(); }
int captureArgc() { return static_cast<int>(gNavArgs.size()); }
const char* captureArgv(int i) { return gNavArgs.at(static_cast<std::size_t>(i)).c_str(); }
void runNav(std::initializer_list<const char*> args) {
    gNavArgs.assign(args.begin(),args.end());
    gNavCommands.at(gNavArgs[0])();
}
void captureGround(const float*, const float* end, int, edict_t*, TraceResult* result) {
    *result={}; result->flFraction=0.5f;
    if(gGroundMissing) result->flFraction=1;
    result->vecEndPos=Vector(end[0],end[1],0); result->vecPlaneNormal=Vector(0,0,1);
    if(gInvalidateDuringGround)
        astrabot::adapter::metamod::lifecycleCoordinator().navConsole().invalidate(
            astrabot::nav::runtime::SessionReason::MapChanged);
}

namespace {

using astrabot::adapter::metamod::FakeClientResult;
using astrabot::adapter::metamod::RemovalResult;
using astrabot::core::BotAgentId;
using astrabot::debug::FakeClientError;
using astrabot::debug::FakeClientStage;
using astrabot::debug::RemovalOutcome;

struct Fixture;
Fixture* gFixture = nullptr;
std::vector<astrabot::debug::FakeClientTrace> gFakeTraces;
std::vector<astrabot::debug::JoinTrace> gJoinTraces;
std::vector<astrabot::debug::RemovalTrace> gRemovalTraces;
enginefuncs_t* gEngineHooks = nullptr;
int gGameDllCommandCalls = 0;
int gEngineClientCommandCalls = 0;
int gRunPlayerMoveCalls = 0;
int gLastCommandArgc = 0;
std::string gLastCommandArgv0;
std::string gLastCommandArgv1;
std::string gLastCommandArgs;
bool gReentrantDispatchResult = true;

void captureLogConsole(plid_t /* pluginId */, const char* format, ...) {
    assert(format != nullptr);
    assert(std::strcmp(format, "%s") == 0);
    va_list arguments;
    va_start(arguments, format);
    gNavOutput.emplace_back(va_arg(arguments, const char*));
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
void captureClientCommand(edict_t* entity);
int captureGetUserMsgID(
    plid_t pluginId,
    const char* messageName,
    int* size);
int captureGetPlayerUserId(edict_t* entity);
void captureServerCommand(char* command);
void captureServerExecute();
void captureEngineClientCommand(edict_t* entity, char* format, ...);
void captureRunPlayerMove(
    edict_t* entity,
    const float* viewAngles,
    float forwardMove,
    float sideMove,
    float upMove,
    unsigned short buttons,
    byte impulse,
    byte msec);

struct Fixture {
    mutil_funcs_t utility{};
    meta_globals_t globals{};
    DLL_FUNCTIONS dll{};
    NEW_DLL_FUNCTIONS newDll{};
    gamedll_funcs_t gameDll{};
    META_FUNCTIONS callbacks{};
    enginefuncs_t engine{};
    globalvars_t engineGlobals{};
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
    int userId{1};
    int serverCommandCalls{0};
    int serverExecuteCalls{0};
    std::string lastServerCommand;
    bool reenterCommand{false};

    Fixture() {
        gFixture = this;
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
        engine.pfnClientCommand = &captureEngineClientCommand;
        engine.pfnRunPlayerMove = &captureRunPlayerMove;
        engine.pfnAddServerCommand = &captureAddCommand;
        engine.pfnCmd_Argc = &captureArgc;
        engine.pfnCmd_Argv = &captureArgv;
        engine.pfnTraceLine = &captureGround;
        dll.pfnClientConnect = &captureClientConnect;
        dll.pfnClientPutInServer = &captureClientPutInServer;
        dll.pfnClientDisconnect = &captureClientDisconnect;
        dll.pfnClientCommand = &captureClientCommand;
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

void captureClientCommand(edict_t* /* entity */) {
    ++gGameDllCommandCalls;
    if (gEngineHooks == nullptr) {
        return;
    }
    if (gFixture->reenterCommand) {
        gReentrantDispatchResult =
            astrabot::adapter::metamod::lifecycleCoordinator()
                .dispatchMenuForTest(1);
    }
    gLastCommandArgc = gEngineHooks->pfnCmd_Argc();
    gLastCommandArgv0 = gEngineHooks->pfnCmd_Argv(0);
    gLastCommandArgv1 = gEngineHooks->pfnCmd_Argv(1);
    gLastCommandArgs = gEngineHooks->pfnCmd_Args();
}

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
    if (std::strcmp(messageName, "VGUIMenu") == 0) {
        return 11;
    }
    if (std::strcmp(messageName, "ShowMenu") == 0) {
        return 12;
    }
    if (std::strcmp(messageName, "TeamInfo") == 0) {
        return 13;
    }
    return 0;
}

int captureGetPlayerUserId(edict_t* /* entity */) {
    return gFixture->userId;
}

void captureServerCommand(char* command) {
    ++gFixture->serverCommandCalls;
    gFixture->lastServerCommand = command == nullptr ? "" : command;
}
void captureServerExecute() { ++gFixture->serverExecuteCalls; }

void captureEngineClientCommand(
    edict_t* /* entity */,
    char* /* format */,
    ...) {
    ++gEngineClientCommandCalls;
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

void captureJoinTrace(const astrabot::debug::JoinTrace& trace) noexcept {
    gJoinTraces.push_back(trace);
}

void captureRemovalTrace(const astrabot::debug::RemovalTrace& trace) noexcept {
    gRemovalTraces.push_back(trace);
}

void attach(Fixture& fixture) {
    GiveFnptrsToDll(&fixture.engine,&fixture.engineGlobals);
    // The hook table must never be used for plugin command registration.
    fixture.engine.pfnAddServerCommand=&unexpectedHookRegistration;
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
    gEngineHooks = nullptr;
    gFakeTraces.clear();
    gRemovalTraces.clear();
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

void sendVguiMenu(
    enginefuncs_t& hooks,
    int messageId,
    edict_t* recipient,
    int menuType,
    int validSlots) {
    hooks.pfnMessageBegin(0, messageId, nullptr, recipient);
    hooks.pfnWriteByte(menuType);
    hooks.pfnWriteShort(validSlots);
    hooks.pfnWriteChar(-1);
    hooks.pfnWriteByte(0);
    hooks.pfnWriteString("");
    hooks.pfnMessageEnd();
}

void sendTeamInfo(enginefuncs_t& hooks, int messageId, int slot, const char* team) {
    hooks.pfnMessageBegin(0, messageId, nullptr, nullptr);
    hooks.pfnWriteByte(slot);
    hooks.pfnWriteString(team);
    hooks.pfnMessageEnd();
}

void testMessageDrivenJoinAndCommandContext() {
    Fixture fixture{};
    activate(fixture);
    astrabot::adapter::metamod::lifecycleCoordinator().setJoinTraceSink(
        &captureJoinTrace);
    gJoinTraces.clear();
    enginefuncs_t hooks{};
    int engineVersion = ENGINE_INTERFACE_VERSION;
    assert(GetEngineFunctions(&hooks, &engineVersion) != 0);
    gEngineHooks = &hooks;
    gGameDllCommandCalls = 0;
    gEngineClientCommandCalls = 0;
    gLastCommandArgc = 0;
    gLastCommandArgv0.clear();
    gLastCommandArgv1.clear();
    gLastCommandArgs.clear();
    gReentrantDispatchResult = true;

    const FakeClientResult created =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot-Join");
    assert(created.succeeded());
    assert(astrabot::adapter::metamod::lifecycleCoordinator().requestJoin(
                {astrabot::adapter::cstrike::Team::Terrorist, 1})
               .changed);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::WaitingTeamMenu);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().player().slot == 1);

    sendVguiMenu(hooks, 11, &fixture.entity, 2, 0x0001);
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().messageDecoder().active());
    assert(astrabot::adapter::metamod::lifecycleCoordinator().messageDecoder().lastError() ==
           astrabot::adapter::cstrike::MessageDecodeError::None);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().messageDecoder().lastEvent().recipientSlot == 1);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().pendingSelection());
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::WaitingTeamMenu);
    astrabot::adapter::metamod::lifecycleCoordinator().startFrame();
    assert(gGameDllCommandCalls == 1);
    assert(gEngineClientCommandCalls == 0);
    assert(gLastCommandArgc == 2);
    assert(gLastCommandArgv0 == "menuselect");
    assert(gLastCommandArgv1 == "1");
    assert(gLastCommandArgs == "1");

    sendVguiMenu(hooks, 11, &fixture.entity, 26, 0x0001);
    astrabot::adapter::metamod::lifecycleCoordinator().startFrame();
    assert(gGameDllCommandCalls == 2);
    sendTeamInfo(hooks, 13, 1, "TERRORIST");
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Joined);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().agents().mappingCount() == 1);
    assert(gJoinTraces.size() >= 6U);
    const std::size_t joinTraceCount = gJoinTraces.size();
    // Server commands use only the managed joined actor and never emit movement.
    auto& nav=astrabot::adapter::metamod::lifecycleCoordinator().navConsole();
    const auto map=astrabot::adapter::metamod::lifecycleCoordinator().registry().mapGeneration();
    route_test::Area a{1,{{0,0,0},{2,2,0},0,0}}, b{2,{{3,0,0},{5,2,0},0,0}};
    a.targets[1]={2};
    assert(nav.publish(map,route_test::snapshot({a,b})).isNone());
    fixture.entity.v.origin=Vector(1,1,36);
    fixture.entity.v.mins=Vector(-16,-16,-36); fixture.entity.v.maxs=Vector(16,16,36);
    fixture.entity.v.flags |= FL_ONGROUND | FL_FAKECLIENT;
    fixture.entity.v.deadflag=DEAD_NO;
    const auto moves=gRunPlayerMoveCalls;
    runNav({"astrabot_goto","2"});
    assert(nav.trace() && nav.trace()->state==astrabot::nav::runtime::SessionState::Ready);
    assert(nav.trace()->route->total==3 && nav.trace()->route->steps.size()==1);
    const auto generation=nav.trace()->routeGeneration;
    runNav({"astrabot_goto","-1"});
    runNav({"astrabot_goto","4294967296"});
    runNav({"astrabot_goto","2","extra"});
    assert(nav.trace()->routeGeneration==generation);
    runNav({"astrabot_nav_status"});
    assert(!gNavOutput.empty() && gRunPlayerMoveCalls==moves);
    runNav({"astrabot_nav_cancel"});
    assert(nav.trace()->state==astrabot::nav::runtime::SessionState::Cancelled);
    runNav({"astrabot_goto","1"});
    assert(nav.trace()->state==astrabot::nav::runtime::SessionState::Ready);
    fixture.entity.v.deadflag=DEAD_DEAD;
    astrabot::adapter::metamod::lifecycleCoordinator().startFrame();
    assert(nav.trace()->reason==astrabot::nav::runtime::SessionReason::Dead);
    fixture.entity.v.deadflag=DEAD_NO;
    fixture.entity.v.flags &= ~FL_FAKECLIENT;
    runNav({"astrabot_goto","2"});
    assert(nav.trace()->state!=astrabot::nav::runtime::SessionState::Ready);
    fixture.entity.v.flags |= FL_FAKECLIENT;
    gGroundMissing=true;
    runNav({"astrabot_goto","2"});
    assert(nav.trace()->reason==astrabot::nav::runtime::SessionReason::UnknownGround);
    gGroundMissing=false;
    fixture.entity.v.origin=Vector(4,1,36);
    runNav({"astrabot_goto","1"});
    assert(nav.trace()->reason==astrabot::nav::runtime::SessionReason::Unreachable);
    fixture.entity.v.origin=Vector(1,1,36);
    const auto bytes=evidence::fixture(5,false).bytes;
    {
        std::ofstream file("nav-console-fixture.nav",std::ios::binary);
        file.rdbuf()->sputn(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        assert(file.good());
    }
    runNav({"astrabot_nav_load","nav-console-fixture.nav"});
    runNav({"astrabot_goto","1"});
    assert(nav.trace()->state==astrabot::nav::runtime::SessionState::Ready);
    {
        std::ifstream file("nav-console-fixture.nav",std::ios::binary);
        std::vector<std::uint8_t> after{std::istreambuf_iterator<char>(file),{}};
        assert(after==bytes);
    }
    runNav({"astrabot_nav_load","nav-console-no-such-file.nav"});
    runNav({"astrabot_goto","1"});
    assert(nav.trace()->reason==astrabot::nav::runtime::SessionReason::MissingGraph);
    assert(nav.publish(map,route_test::snapshot({a,b})).isNone());
    runNav({"astrabot_goto","2"});
    gInvalidateDuringGround=true;
    runNav({"astrabot_goto","1"});
    assert(!nav.trace());
    gInvalidateDuringGround=false;
    assert(nav.publish(map,route_test::snapshot({a,b})).isNone());
    runNav({"astrabot_goto","2"});
    astrabot::adapter::metamod::lifecycleCoordinator().clientDisconnect(&fixture.entity);
    assert(!nav.trace());
    detach();
    assert(gJoinTraces.size() == joinTraceCount);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Idle);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().activeEntity() ==
           nullptr);
}

void testJoinFailureCleanupAndCommandContextReentry() {
    Fixture fixture{};
    activate(fixture);
    enginefuncs_t hooks{};
    int engineVersion = ENGINE_INTERFACE_VERSION;
    assert(GetEngineFunctions(&hooks, &engineVersion) != 0);
    gEngineHooks = &hooks;
    gGameDllCommandCalls = 0;
    gEngineClientCommandCalls = 0;
    gReentrantDispatchResult = true;

    const FakeClientResult created =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot-Failure");
    assert(created.succeeded());
    fixture.reenterCommand = true;
    assert(astrabot::adapter::metamod::lifecycleCoordinator().requestJoin(
                {astrabot::adapter::cstrike::Team::Terrorist, 1})
               .changed);

    sendVguiMenu(hooks, 11, &fixture.entity, 2, 0x0001);
    astrabot::adapter::metamod::lifecycleCoordinator().startFrame();
    assert(gGameDllCommandCalls == 1);
    assert(!gReentrantDispatchResult);
    assert(gEngineClientCommandCalls == 0);
    assert(gLastCommandArgc == 2);
    assert(gLastCommandArgv0 == "menuselect");
    assert(gLastCommandArgv1 == "1");
    assert(gLastCommandArgs == "1");
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::WaitingClassMenu);

    sendVguiMenu(hooks, 11, &fixture.entity, 26, 0x0000);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Failed);
    assert(fixture.serverCommandCalls == 1);
    assert(fixture.serverExecuteCalls == 1);
    assert(fixture.lastServerCommand == "kick #1\n");
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().registry().isConnected(1));
    assert(astrabot::adapter::metamod::lifecycleCoordinator().agents().mappingCount() == 0);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().activeEntity() == nullptr);
    assert(gEngineClientCommandCalls == 0);
    detach();

    Fixture fallbackFixture{};
    activate(fallbackFixture);
    const FakeClientResult fallbackCreated =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot-Fallback");
    assert(fallbackCreated.succeeded());
    fallbackFixture.userId = 0;
    assert(astrabot::adapter::metamod::lifecycleCoordinator().requestJoin(
                {astrabot::adapter::cstrike::Team::Terrorist, 1})
               .changed);
    sendVguiMenu(
        hooks,
        11,
        &fallbackFixture.entity,
        2,
        0x0001);
    astrabot::adapter::metamod::lifecycleCoordinator().startFrame();
    sendVguiMenu(
        hooks,
        11,
        &fallbackFixture.entity,
        26,
        0x0000);
    assert(fallbackFixture.serverCommandCalls == 0);
    assert(fallbackFixture.disconnectCalls == 1);
    assert(fallbackFixture.removeCalls == 1);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().agents().mappingCount() == 0);
    detach();
}

void testCounterTerroristPrimaryJoinRequest() {
    Fixture fixture{};
    attach(fixture);
    DLL_FUNCTIONS entityHooks{};
    int entityVersion = INTERFACE_VERSION;
    assert(GetEntityAPI2(&entityHooks, &entityVersion) != 0);
    enginefuncs_t engineHooks{};
    int engineVersion = ENGINE_INTERFACE_VERSION;
    assert(GetEngineFunctions(&engineHooks, &engineVersion) != 0);
    gEngineHooks = &engineHooks;
    gGameDllCommandCalls = 0;

    astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().queuePrimaryCreate(
        {astrabot::adapter::cstrike::Team::CounterTerrorist, 2});
    entityHooks.pfnServerActivate(nullptr, 0, 32);
    entityHooks.pfnStartFrame();
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().request().team ==
           astrabot::adapter::cstrike::Team::CounterTerrorist);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().request().classNumber ==
           2U);

    sendVguiMenu(engineHooks, 11, &fixture.entity, 2, 0x0002);
    astrabot::adapter::metamod::lifecycleCoordinator().startFrame();
    assert(gGameDllCommandCalls == 1);
    assert(gLastCommandArgv1 == "2");

    sendVguiMenu(engineHooks, 11, &fixture.entity, 27, 0x0002);
    astrabot::adapter::metamod::lifecycleCoordinator().startFrame();
    assert(gGameDllCommandCalls == 2);
    assert(gLastCommandArgv1 == "2");
    sendTeamInfo(engineHooks, 13, 1, "CT");
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Joined);
    detach();
}

void testJoinTimeoutCleanup() {
    Fixture fixture{};
    activate(fixture);
    const FakeClientResult created =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot-Timeout");
    assert(created.succeeded());
    assert(astrabot::adapter::metamod::lifecycleCoordinator().requestJoin(
                {astrabot::adapter::cstrike::Team::Terrorist, 1})
               .changed);
    for (int frame = 0; frame < 128; ++frame) {
        astrabot::adapter::metamod::lifecycleCoordinator().startFrame();
    }
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Failed);
    assert(astrabot::adapter::metamod::lifecycleCoordinator().joinState().error() ==
           astrabot::adapter::cstrike::JoinError::Timeout);
    assert(fixture.serverCommandCalls == 1);
    assert(fixture.serverExecuteCalls == 1);
    assert(!astrabot::adapter::metamod::lifecycleCoordinator().registry().isConnected(1));
    assert(astrabot::adapter::metamod::lifecycleCoordinator().agents().mappingCount() == 0);
    detach();
}

void testExplicitRemovalKickAndDisconnectAcknowledge() {
    Fixture fixture{};
    activate(fixture);
    const FakeClientResult created =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot-Remove");
    assert(created.succeeded());

    auto& coordinator = astrabot::adapter::metamod::lifecycleCoordinator();
    coordinator.setRemovalTraceSink(&captureRemovalTrace);
    gRemovalTraces.clear();

    const RemovalResult queued = coordinator.removeActive();
    assert(queued.succeeded());
    assert(queued.outcome == RemovalOutcome::KickQueued);
    assert(fixture.serverCommandCalls == 1);
    assert(fixture.serverExecuteCalls == 1);
    assert(fixture.lastServerCommand == "kick #1\n");
    assert(coordinator.fakeClient().removalPending());
    assert(coordinator.registry().isConnected(1));
    assert(gRemovalTraces.size() == 1);

    const RemovalResult repeated = coordinator.removeActive();
    assert(repeated.succeeded());
    assert(repeated.outcome == RemovalOutcome::NoOp);
    assert(fixture.serverCommandCalls == 1);
    assert(fixture.serverExecuteCalls == 1);
    assert(gRemovalTraces.size() == 1);

    coordinator.clientDisconnect(&fixture.entity);
    assert(!coordinator.registry().isConnected(1));
    assert(coordinator.agents().mappingCount() == 0);
    assert(coordinator.fakeClient().activeEntity() == nullptr);
    assert(!coordinator.fakeClient().removalPending());
    assert(gRemovalTraces.size() == 2);
    coordinator.clientDisconnect(&fixture.entity);
    assert(gRemovalTraces.size() == 2);
    detach();
}

void testRemovalCancelsJoinBeforeKick() {
    Fixture fixture{};
    activate(fixture);
    gGameDllCommandCalls = 0;
    const FakeClientResult created =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot-JoiningRemove");
    assert(created.succeeded());

    auto& coordinator = astrabot::adapter::metamod::lifecycleCoordinator();
    assert(coordinator.requestJoin(
                       {astrabot::adapter::cstrike::Team::Terrorist, 1})
               .changed);
    assert(coordinator.joinState().active());

    const RemovalResult result = coordinator.removeActive();
    assert(result.succeeded());
    assert(coordinator.joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Cancelled);
    assert(coordinator.fakeClient().removalPending());
    assert(coordinator.fakeClient().activeEntity() != nullptr);

    coordinator.startFrame();
    assert(gGameDllCommandCalls == 0);
    coordinator.clientDisconnect(&fixture.entity);
    assert(coordinator.fakeClient().activeEntity() == nullptr);
    assert(coordinator.agents().mappingCount() == 0);
    detach();
}

void testRemovalFallbackCleansInvalidUserId() {
    Fixture fixture{};
    fixture.userId = 0;
    activate(fixture);
    const FakeClientResult created =
        astrabot::adapter::metamod::lifecycleCoordinator().fakeClient().create(
            "AstraBot-FallbackRemove");
    assert(created.succeeded());

    auto& coordinator = astrabot::adapter::metamod::lifecycleCoordinator();
    coordinator.setRemovalTraceSink(&captureRemovalTrace);
    gRemovalTraces.clear();

    const RemovalResult result = coordinator.removeActive();
    assert(!result.succeeded());
    assert(result.error == astrabot::debug::RemovalError::InvalidUserId);
    assert(fixture.serverCommandCalls == 0);
    assert(fixture.serverExecuteCalls == 0);
    assert(fixture.disconnectCalls == 1);
    assert(fixture.removeCalls == 1);
    assert(coordinator.agents().mappingCount() == 0);
    assert(!coordinator.registry().isConnected(1));
    assert(coordinator.fakeClient().activeEntity() == nullptr);
    assert(gRemovalTraces.size() == 2);
    assert(coordinator.status().lastRemovalError ==
           astrabot::debug::RemovalError::InvalidUserId);
    detach();
}

void testMapDeactivateReplaysPrimaryCreate() {
    Fixture fixture{};
    attach(fixture);
    auto& coordinator = astrabot::adapter::metamod::lifecycleCoordinator();
    coordinator.serverActivate(32);
    coordinator.startFrame();
    const astrabot::core::MapGeneration firstMap =
        coordinator.registry().mapGeneration();
    const astrabot::core::PlayerId firstPlayer =
        coordinator.fakeClient().activePlayer();
    assert(firstPlayer.isValid());
    assert(coordinator.joinState().active());
    assert(coordinator.status().mapActivations == 1U);
    assert(coordinator.status().mapReplays == 0U);
    assert(coordinator.status().createAttempts == 1U);

    coordinator.serverDeactivate();
    assert(!coordinator.registry().isMapActive());
    assert(coordinator.fakeClient().activeEntity() == nullptr);
    assert(coordinator.agents().mappingCount() == 0);
    assert(coordinator.joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Idle);

    coordinator.serverActivate(32);
    coordinator.startFrame();
    assert(coordinator.registry().mapGeneration() != firstMap);
    assert(coordinator.fakeClient().activePlayer().isValid());
    assert(coordinator.fakeClient().activePlayer().generation !=
           firstPlayer.generation);
    assert(fixture.createCalls == 2);
    assert(coordinator.joinState().active());
    assert(coordinator.status().mapActivations == 2U);
    assert(coordinator.status().mapReplays == 1U);
    assert(coordinator.status().createAttempts == 2U);
    detach();
}

void testMapReplayPreservesExplicitCounterTerroristRequest() {
    Fixture fixture{};
    attach(fixture);
    auto& coordinator = astrabot::adapter::metamod::lifecycleCoordinator();
    coordinator.queuePrimaryCreate(
        {astrabot::adapter::cstrike::Team::CounterTerrorist, 3});

    coordinator.serverActivate(32);
    coordinator.startFrame();
    assert(coordinator.joinState().request().team ==
           astrabot::adapter::cstrike::Team::CounterTerrorist);
    assert(coordinator.joinState().request().classNumber == 3U);

    coordinator.serverDeactivate();
    coordinator.serverActivate(32);
    coordinator.startFrame();
    assert(coordinator.joinState().request().team ==
           astrabot::adapter::cstrike::Team::CounterTerrorist);
    assert(coordinator.joinState().request().classNumber == 3U);
    detach();
}

void testExternalDisconnectResetsJoinedState() {
    Fixture fixture{};
    activate(fixture);
    auto& coordinator = astrabot::adapter::metamod::lifecycleCoordinator();
    const FakeClientResult created = coordinator.fakeClient().create(
        "AstraBot-DisconnectJoined");
    assert(created.succeeded());
    assert(coordinator.requestJoin(
                       {astrabot::adapter::cstrike::Team::Terrorist, 1})
               .changed);
    enginefuncs_t hooks{};
    int engineVersion = ENGINE_INTERFACE_VERSION;
    assert(GetEngineFunctions(&hooks, &engineVersion) != 0);
    sendVguiMenu(hooks, 11, &fixture.entity, 2, 0x0001);
    coordinator.startFrame();
    sendVguiMenu(hooks, 11, &fixture.entity, 26, 0x0001);
    coordinator.startFrame();
    sendTeamInfo(hooks, 13, 1, "TERRORIST");
    assert(coordinator.joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Joined);

    coordinator.clientDisconnect(&fixture.entity);
    assert(coordinator.joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Idle);
    assert(!coordinator.messageDecoder().active());
    assert(coordinator.fakeClient().activeEntity() == nullptr);
    detach();
}

void testRemovalStopsPendingMovement() {
    Fixture fixture{};
    activate(fixture);
    auto& coordinator = astrabot::adapter::metamod::lifecycleCoordinator();
    enginefuncs_t hooks{};
    int engineVersion = ENGINE_INTERFACE_VERSION;
    assert(GetEngineFunctions(&hooks, &engineVersion) != 0);
    gEngineHooks = &hooks;
    gGameDllCommandCalls = 0;
    gRunPlayerMoveCalls = 0;

    const FakeClientResult created =
        coordinator.fakeClient().create("AstraBot-MovementRemove");
    assert(created.succeeded());
    assert(coordinator.requestJoin(
                       {astrabot::adapter::cstrike::Team::Terrorist, 1})
               .changed);
    sendVguiMenu(hooks, 11, &fixture.entity, 2, 0x0001);
    coordinator.startFrame();
    sendVguiMenu(hooks, 11, &fixture.entity, 26, 0x0001);
    coordinator.startFrame();
    sendTeamInfo(hooks, 13, 1, "TERRORIST");
    assert(coordinator.joinState().phase() ==
           astrabot::adapter::cstrike::JoinPhase::Joined);

    coordinator.startFrame();
    astrabot::core::BotCommand command =
        astrabot::core::BotCommand::neutral(100);
    command.movement.forward = 100.0F;
    const auto queued = coordinator.submitCommand(
        created.player,
        coordinator.registry().mapGeneration(),
        coordinator.registry().currentTick(),
        command);
    assert(queued.queued());
    const RemovalResult removed = coordinator.removeActive();
    assert(removed.succeeded());
    coordinator.startFrame();
    assert(gRunPlayerMoveCalls == 0);
    coordinator.clientDisconnect(&fixture.entity);
    detach();
}

void testRepeatedCreateAfterDisconnectUpdatesGeneration() {
    Fixture fixture{};
    activate(fixture);
    auto& coordinator = astrabot::adapter::metamod::lifecycleCoordinator();
    const FakeClientResult first =
        coordinator.fakeClient().create("AstraBot-Reuse-1");
    assert(first.succeeded());
    coordinator.clientDisconnect(&fixture.entity);
    const FakeClientResult second =
        coordinator.fakeClient().create("AstraBot-Reuse-2");
    assert(second.succeeded());
    assert(second.player.slot == first.player.slot);
    assert(second.player.generation != first.player.generation);
    assert(second.agent != first.agent);
    coordinator.clientDisconnect(&fixture.entity);
    detach();
}

void testDetachDirectlyCleansActiveEntityOnce() {
    Fixture fixture{};
    activate(fixture);
    auto& coordinator = astrabot::adapter::metamod::lifecycleCoordinator();
    const FakeClientResult created =
        coordinator.fakeClient().create("AstraBot-Detach");
    assert(created.succeeded());
    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assert(fixture.disconnectCalls == 1);
    assert(fixture.removeCalls == 1);
    assert(coordinator.registry().eventSequence() == 0);
    assert(Meta_Detach(PT_ANYTIME, PNL_COMMAND) != 0);
    assert(fixture.disconnectCalls == 1);
    assert(fixture.removeCalls == 1);
    gFixture = nullptr;
    gEngineHooks = nullptr;
}

} // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    testSuccessfulCreationAndOpaquePrivateData();
    testFailureRollback();
    testInputAndCapacityRejection();
    testFirstFrameBootstrapAndCleanup();
    testMissingFunctionIsRejectedWithoutEngineCall();
    testMessageDrivenJoinAndCommandContext();
    testJoinFailureCleanupAndCommandContextReentry();
    testCounterTerroristPrimaryJoinRequest();
    testJoinTimeoutCleanup();
    testExplicitRemovalKickAndDisconnectAcknowledge();
    testRemovalCancelsJoinBeforeKick();
    testRemovalFallbackCleansInvalidUserId();
    testMapDeactivateReplaysPrimaryCreate();
    testMapReplayPreservesExplicitCounterTerroristRequest();
    testExternalDisconnectResetsJoinedState();
    testRemovalStopsPendingMovement();
    testRepeatedCreateAfterDisconnectUpdatesGeneration();
    testDetachDirectlyCleansActiveEntityOnce();
    return 0;
}
