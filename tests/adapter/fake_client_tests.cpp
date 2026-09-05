// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include <fstream>
#include <cmath>
#include <chrono>
#include <iterator>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "adapter/metamod/fake_client.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include "adapter/metamod/plugin_entry.hpp"
#include "adapter/cstrike/nav/world_queries.hpp"
#include "nav/local/ground_probe.hpp"

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
bool gInvalidateDuringHull=false;
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

int gHullKind=-1, gHullCalls=0, gHullMode=0;
float gStairHeight=0;
bool gStairCeiling=false;
edict_t gNavDoor{}, gNavCompetitor{};
bool gDoorActive=false, gDoorOpen=false, gDoorLocked=false, gDoorAmbiguous=false, gDoorLoop=false;
int gDoorUses=0, gDoorScans=0;
std::uint64_t gDoorOpenAtUs=0;
float supportHeight(float x) { return x+16>100 ? gStairHeight:0; }
void captureNavHull(const float* start, const float* end, int ignoreMonsters, int hull, edict_t* actor, TraceResult* result) {
    assert((ignoreMonsters==0 || ignoreMonsters==1) && actor!=nullptr);
    gHullKind=hull; ++gHullCalls;
    *result={}; result->flFraction=1; result->vecEndPos=Vector(end[0],end[1],end[2]);
    const float minimumZ=hull==3 ? -18.0f:-36.0f;
    const float floor=supportHeight(end[0]);
    if(ignoreMonsters==1) {
        if(gDoorActive && !gDoorOpen && end[0]+16>=100 && end[0]-16<=104 && start[2]<108) {
            result->fStartSolid=1; return;
        }
        if(!gGroundMissing && end[2]+minimumZ<floor) {
            if(start[2]+minimumZ<floor) result->fStartSolid=1;
            else {
                result->flFraction=(start[2]+minimumZ-floor)/(start[2]-end[2]);
                result->vecEndPos.z=floor-minimumZ; result->vecPlaneNormal=Vector(0,0,1);
            }
        }
        if(gInvalidateDuringGround)
            astrabot::adapter::metamod::lifecycleCoordinator().navConsole().invalidate(
                astrabot::nav::runtime::SessionReason::MapChanged);
        return;
    }
    if(end[2]+minimumZ<floor || (start[0]!=end[0] && start[2]+minimumZ<floor) ||
       (gStairCeiling && end[2]>start[2] && start[0]==end[0])) {
        result->flFraction=0.5f; result->vecPlaneNormal=Vector(-1,0,0);
    }
    if(gHullMode==1) result->fAllSolid=1;
    if(gHullMode==2) result->flFraction=0.5f;
    if(gHullMode==3) result->flFraction=2;
    if(gDoorActive && !gDoorOpen && start[0]<=84 && end[0]>=84 && end[0]>start[0]) {
        result->flFraction=(84-start[0])/(end[0]-start[0]);
        result->vecEndPos=Vector(84,start[1]+(end[1]-start[1])*result->flFraction,
            start[2]+(end[2]-start[2])*result->flFraction);
        result->vecPlaneNormal=Vector(-1,0,0); result->pHit=&gNavDoor;
    }
    if(gInvalidateDuringHull)
        astrabot::adapter::metamod::lifecycleCoordinator().navConsole().invalidate(
            astrabot::nav::runtime::SessionReason::MapChanged);
}
void testNavWorldQueries() {
    using namespace astrabot::nav;
    enginefuncs_t engine{}; edict_t entity{};
    engine.pfnTraceLine=&captureGround; engine.pfnTraceHull=&captureNavHull;
    auto mesh=route_test::snapshot({{1,{{0,0,0},{100,100,0},0,0},{}}});
    auto idx=query::NavSpatialIndex::build(mesh,{1,1,1000000}); assert(idx);
    runtime::QueryRequest q{{{1},{2,{3}},{4},{5},6,1},runtime::QueryKind::SweptHull,
        {20,50,36},{36,50,36},runtime::HullDimensions{{-16,-16,-36},{16,16,36}}};
    const auto call=[&] { return astrabot::adapter::cstrike::queryNavWorld(&engine,&entity,idx.value->get(),q); };
    gHullMode=0; gHullCalls=0;
    auto r=call(); assert(r.error==runtime::QueryError::None && r.hull && r.hull->fraction==1);
    assert(r.stamp==q.stamp && gHullKind==1 && gHullCalls==1);
    q.hull=runtime::HullDimensions{{-16,-16,-18},{16,16,18}};
    r=call(); assert(r.hull && gHullKind==3);
    q.hull->maximum.x=17;
    r=call(); assert(r.error==runtime::QueryError::Unavailable && gHullCalls==2);
    q.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}};
    gHullMode=1; r=call(); assert(r.hull && r.hull->startSolid);
    q.kind=runtime::QueryKind::Clearance; r=call(); assert(r.clearance && !r.clearance->clear);
    gHullMode=2; r=call(); assert(r.clearance && !r.clearance->clear);
    gHullMode=0; r=call(); assert(r.clearance && r.clearance->clear);
    gHullMode=3; r=call(); assert(r.error==runtime::QueryError::InvalidResult);
    gHullMode=0; q.kind=runtime::QueryKind::Floor; q.start={20,50,18}; q.end={20,50,-64};
    r=call(); assert(r.floor && r.floor->supported && r.floor->height==0);
    q.end.x=21; assert(call().error==runtime::QueryError::InvalidResult); q.end.x=20;
    q.kind=runtime::QueryKind::Door; r=call(); assert(r.error==runtime::QueryError::None && !r.door);
    struct Port final : runtime::IWorldQueries {
        enginefuncs_t* engine; edict_t* entity; const query::NavSpatialIndex* index;
        Port(enginefuncs_t* e,edict_t* a,const query::NavSpatialIndex* i):engine(e),entity(a),index(i) {}
        runtime::WorldQueryResult query(const runtime::QueryRequest& request) override {
            return astrabot::adapter::cstrike::queryNavWorld(engine,entity,index,request);
        }
    } port(&engine,&entity,idx.value->get());
    runtime::MovementSnapshot s; s.agent={1}; s.actor={2,{3}}; s.map={4}; s.tick={5};
    s.kind=runtime::ActorKind::ManagedBot; s.connected=true; s.alive=true; s.joined=true; s.grounded=true;
    s.position=model::NavVector3{20,50,36}; s.hull=q.hull;
    const local::GroundProbeLimits limits{5,2,32,16,18,18,64,4,2,0.7};
    auto probe=local::GroundProbe::inspect(s,6,{1},52,50,**idx.value,s.map,port,limits);
    assert(probe && probe.queries==5 && probe.target->origin.z==36);
    gHullMode=1;
    probe=local::GroundProbe::inspect(s,6,{1},52,50,**idx.value,s.map,port,limits);
    assert(!probe && probe.reason==local::ProbeReason::Blocked && probe.queries==3);
    gHullMode=0; engine.pfnTraceHull=nullptr; q.kind=runtime::QueryKind::SweptHull;
    assert(call().error==runtime::QueryError::Unavailable);
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
bool gSimulateNav=false;
bool gInjectNavDuplicate=false;
bool gDeactivateDuringMove=false;
bool gReplaceDuringMove=false;
std::vector<astrabot::debug::MovementTrace> gNavTransportTraces;
void navTransportTrace(const astrabot::debug::MovementTrace& t) noexcept { gNavTransportTraces.push_back(t); }
std::uint64_t gNavClockUs=1000000;
std::vector<astrabot::core::BotCommand> gNavMoves;
std::chrono::steady_clock::time_point navNow() noexcept {
    return std::chrono::steady_clock::time_point(std::chrono::microseconds(gNavClockUs));
}
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
        engine.pfnTraceHull = &captureNavHull;
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
    if(entity==&gNavDoor) return 40;
    return entity == &gFixture->entity ? gFixture->index : 0;
}
edict_t* captureDoorEntity(int index) {
    return index==40 ? &gNavDoor:index==gFixture->index ? &gFixture->entity:nullptr;
}
const char* captureDoorString(int index) {
    return index==10 ? "func_door":index==11 ? "func_door_rotating":"unknown";
}
edict_t* captureDoorSphere(edict_t* previous,const float* origin,float radius) {
    ++gDoorScans; assert(radius==64);
    if(gDoorLoop) return &gFixture->entity;
    if(gDoorAmbiguous) return &gNavCompetitor;
    if(previous==&gNavDoor) return nullptr;
    if(!previous) return &gFixture->entity;
    const double dx=102.0-origin[0],dy=50.0-origin[1],dz=36.0-origin[2];
    return std::sqrt(dx*dx+dy*dy+dz*dz)<=radius ? &gNavDoor:nullptr;
}
void configureDoor(Fixture& fixture) {
    gDoorActive=true; gDoorOpen=gDoorLocked=gDoorAmbiguous=gDoorLoop=false;
    gDoorUses=gDoorScans=0; gDoorOpenAtUs=0; gNavDoor={}; gNavCompetitor={};
    gNavDoor.serialnumber=7; gNavDoor.v.classname=10; gNavDoor.v.spawnflags=1<<8;
    gNavDoor.v.absmin=Vector(100,0,0); gNavDoor.v.size=Vector(4,100,72);
    fixture.engineGlobals.maxEntities=128;
    fixture.engine.pfnPEntityOfEntIndex=&captureDoorEntity;
    fixture.engine.pfnSzFromIndex=&captureDoorString;
    fixture.engine.pfnFindEntityInSphere=&captureDoorSphere;
}
void testDoorObservationContracts() {
    using namespace astrabot;
    Fixture fixture{}; configureDoor(fixture);
    fixture.entity.v.origin=Vector(50,50,36); fixture.entity.v.view_ofs=Vector(0,0,28);
    nav::runtime::QueryRequest q{{{1},{1,{1}},{1},{1},1,1},nav::runtime::QueryKind::Door,
        {50,50,36},{98,50,36},nav::runtime::HullDimensions{{-16,-16,-36},{16,16,36}}};
    const auto query=[&] { return adapter::cstrike::queryNavWorld(&fixture.engine,&fixture.entity,nullptr,q,128); };
    auto r=query(); assert(r.door && !r.door->open && r.door->canUse && r.door->useView);
    const auto id=r.door->id; assert(id==((std::uint64_t{7}<<32)|40));
    const double pitch=std::atan2(28.0,52.0)*180/3.14159265358979323846;
    assert(std::abs(r.door->useView->x-pitch)<0.001 && r.door->useView->y==0);
    for(int mode=0;mode<7;++mode) {
        const auto saved=fixture.entity.v.origin; const auto oldSize=gNavDoor.v.size;
        if(mode==0) fixture.entity.v.origin.x=0; // outside 64-unit use search
        if(mode==1) gDoorAmbiguous=true;
        if(mode==2) gDoorLoop=true;
        if(mode==3) gNavDoor.free=1;
        if(mode==4) gNavDoor.v.size.x=-1;
        if(mode==5) gNavDoor.v.spawnflags=0;
        if(mode==6) gNavDoor.serialnumber=8;
        assert(!adapter::cstrike::doorUseView(&fixture.engine,&fixture.entity,id,128));
        fixture.entity.v.origin=saved; gNavDoor.v.size=oldSize; gNavDoor.free=0;
        gNavDoor.v.spawnflags=1<<8; gNavDoor.serialnumber=7; gDoorAmbiguous=gDoorLoop=false;
    }
    assert(!adapter::cstrike::doorUseView(&fixture.engine,&fixture.entity,id,40));
    q.doorId=id; gDoorOpen=true; r=query(); assert(r.door && r.door->open && !r.door->canUse);
    ++gNavDoor.serialnumber; assert(!query().door);
    gNavDoor.serialnumber=7; gDoorOpen=false; gNavDoor.v.classname=11;
    r=query(); assert(r.door && r.door->canUse); // ordinary rotating use door
    gNavDoor.v.classname=12; assert(!query().door);
    gDoorActive=false; gDoorOpenAtUs=0;
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
    edict_t* entity, const float* viewAngles, float forwardMove, float sideMove, float upMove,
    unsigned short buttons, byte impulse, byte msec) {
    ++gRunPlayerMoveCalls;
    if(gSimulateNav) {
        auto& owner=astrabot::adapter::metamod::lifecycleCoordinator();
        assert(owner.registry().currentTick().isAfter(owner.navConsole().motionTrace().commandTick));
        astrabot::core::BotCommand c{{viewAngles[0],viewAngles[1],viewAngles[2]},
            {forwardMove,sideMove,upMove},buttons,impulse,msec};
        gNavMoves.push_back(c); assert(c.validate());
        assert((buttons==0 || (gDoorActive && buttons==IN_USE)) && impulse==0 && upMove==0 && entity->v.deadflag==DEAD_NO);
        if(buttons==IN_USE) {
            assert(forwardMove==0 && sideMove==0 && !gDoorAmbiguous);
            // Independently emulate the pinned player's sphere/direction test;
            // do not call the production selector to justify its own output.
            const double px=102.0-entity->v.origin.x,py=50.0-entity->v.origin.y,pz=36.0-entity->v.origin.z;
            assert(std::sqrt(px*px+py*py+pz*pz)<=64);
            const double dx=px-entity->v.view_ofs.x,dy=py-entity->v.view_ofs.y,dz=pz-entity->v.view_ofs.z;
            constexpr double radians=3.14159265358979323846/180;
            const double pitch=viewAngles[0]*radians,yaw=viewAngles[1]*radians;
            const double dot=(dx*std::cos(pitch)*std::cos(yaw)+dy*std::cos(pitch)*std::sin(yaw)-dz*std::sin(pitch)) /
                std::sqrt(dx*dx+dy*dy+dz*dz);
            assert(dot>0.7 && (gNavDoor.v.spawnflags&(1<<8)));
            ++gDoorUses;
            if(!gDoorLocked) gDoorOpenAtUs=gNavClockUs+120000;
        }
        const double yaw=double(viewAngles[1])*3.14159265358979323846/180;
        const double dt=double(msec)/1000;
        entity->v.origin.x+=static_cast<float>((forwardMove*std::cos(yaw)+sideMove*std::sin(yaw))*dt);
        entity->v.origin.y+=static_cast<float>((forwardMove*std::sin(yaw)-sideMove*std::cos(yaw))*dt);
        entity->v.origin.z=supportHeight(entity->v.origin.x)+36;
        entity->v.v_angle=Vector(viewAngles[0],viewAngles[1],viewAngles[2]);
        if(gInjectNavDuplicate) {
            gInjectNavDuplicate=false;
            assert(owner.submitCommand(owner.fakeClient().activePlayer(),owner.registry().mapGeneration(),
                owner.registry().currentTick(),astrabot::core::BotCommand::neutral(1)).queued());
        }
        if(gDeactivateDuringMove) { gDeactivateDuringMove=false; owner.serverDeactivate(); }
        if(gReplaceDuringMove) { gReplaceDuringMove=false; runNav({"astrabot_goto","1"}); }
    }
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

void navFrame(Fixture& fixture,std::uint64_t us=16000) {
    gNavClockUs+=us; fixture.engineGlobals.frametime=static_cast<float>(us)/1000000.0f;
    if(gDoorOpenAtUs && gNavClockUs>=gDoorOpenAtUs) { gDoorOpen=true; gDoorOpenAtUs=0; }
    astrabot::adapter::metamod::lifecycleCoordinator().startFrame();
}
void prepareNavWalk(Fixture& fixture,enginefuncs_t& hooks) {
    activate(fixture);
    auto& owner=astrabot::adapter::metamod::lifecycleCoordinator();
    owner.setMovementClockForTest(&navNow); gSimulateNav=true; gNavMoves.clear();
    owner.setMovementTraceSink(&navTransportTrace); gNavTransportTraces.clear();
    gGroundMissing=false; gHullMode=0; gInvalidateDuringGround=gInvalidateDuringHull=false;
    gStairHeight=0; gStairCeiling=false;
    int version=ENGINE_INTERFACE_VERSION; assert(GetEngineFunctions(&hooks,&version)); gEngineHooks=&hooks;
    assert(owner.fakeClient().create("AstraBot-Walk").succeeded());
    assert(owner.requestJoin({astrabot::adapter::cstrike::Team::Terrorist,1}).changed);
    sendVguiMenu(hooks,11,&fixture.entity,2,1); navFrame(fixture);
    sendVguiMenu(hooks,11,&fixture.entity,26,1); navFrame(fixture);
    sendTeamInfo(hooks,13,1,"TERRORIST");
    assert(owner.joinState().phase()==astrabot::adapter::cstrike::JoinPhase::Joined);
    fixture.entity.v.flags=FL_ONGROUND|FL_FAKECLIENT; fixture.entity.v.deadflag=DEAD_NO;
    fixture.entity.v.origin=Vector(50,50,36); fixture.entity.v.v_angle=Vector(0,90,0);
    fixture.entity.v.mins=Vector(-16,-16,-36); fixture.entity.v.maxs=Vector(16,16,36); fixture.entity.v.maxspeed=250;
    route_test::Area a{1,{{0,0,0},{100,100,0},0,0}}, b{2,{{100,0,0},{200,100,0},0,0}},
        c{3,{{100,100,0},{200,200,0},0,0}}, d{4,{{200,100,0},{300,200,0},0,0}};
    a.targets[1]={2}; b.targets[2]={3}; c.targets[1]={4};
    assert(owner.navConsole().publish(owner.registry().mapGeneration(),route_test::snapshot({a,b,c,d})).isNone());
}
void awaitMovingQueue(Fixture& fixture) {
    auto& nav=astrabot::adapter::metamod::lifecycleCoordinator().navConsole();
    for(int i=0;i<20;++i) {
        navFrame(fixture);
        const auto& t=nav.motionTrace();
        if(t.event==astrabot::adapter::cstrike::MotionEvent::Queued &&
           std::hypot(t.command.movement.forward,t.command.movement.side)>0) return;
    }
    assert(false && "Walk did not queue movement");
}
bool motionReason(astrabot::adapter::cstrike::MotionReason reason) {
    auto& nav=astrabot::adapter::metamod::lifecycleCoordinator().navConsole();
    for(std::size_t i=0;i<nav.motionHistoryCount();++i) if(nav.motionHistory(i)->reason==reason) return true;
    return false;
}
void testNavWalkArrival() {
    using namespace astrabot;
    for(std::uint64_t us : {8000U,16000U,100000U}) for(const char* goal : {"1","2","3","4"}) {
        Fixture fixture{}; enginefuncs_t hooks{}; prepareNavWalk(fixture,hooks);
        auto& owner=adapter::metamod::lifecycleCoordinator(); auto& console=owner.navConsole();
        runNav({"astrabot_goto",goal}); assert(console.trace()->state==nav::runtime::SessionState::Ready);
        const auto requestTick=owner.registry().currentTick();
        const auto sequence=console.motionTrace().sequence;
        for(int i=0;i<3;++i) runNav({"astrabot_nav_status"});
        assert(console.motionTrace().sequence==sequence && gNavMoves.empty());
        bool arrived=false;
        for(int frame=0;frame<2000;++frame) {
            navFrame(fixture,us);
            const auto& t=console.motionTrace();
            if(t.decision.state==nav::local::WalkState::Failed || t.decision.state==nav::local::WalkState::Aborted) {
                std::fprintf(stderr,"host Walk failure goal=%s frame_us=%llu reason=%u probe=%u motion=%u\n",goal,
                    static_cast<unsigned long long>(us),unsigned(t.decision.reason),unsigned(t.decision.probeReason),unsigned(t.reason));
                assert(false);
            }
            assert(t.decision.queries<=21 && t.decision.samples<=4 && console.motionHistoryCount()<=console.motionHistoryLimit);
            if(t.event==adapter::cstrike::MotionEvent::Dispatched) assert(t.dispatchTick.isAfter(t.commandTick));
            if(t.decision.state==nav::local::WalkState::Arrived) { arrived=true; break; }
        }
        assert(arrived && owner.registry().currentTick().isAfter(requestTick));
        const auto& position=fixture.entity.v.origin;
        const float expectedX=goal[0]=='1' ? 50.0f:goal[0]=='4' ? 217.0f:117.0f;
        const float expectedY=goal[0]<'3' ? 50.0f:117.0f;
        assert(std::hypot(position.x-expectedX,position.y-expectedY)<=1.01f);
        assert(console.motionTrace().decision.support && console.motionTrace().decision.support->area.value==unsigned(goal[0]-'0'));
        navFrame(fixture,us); assert(!gNavMoves.empty() && gNavMoves.back().movement==core::Movement{});
        const auto count=gNavMoves.size(); navFrame(fixture,us); navFrame(fixture,us); assert(gNavMoves.size()==count);
        if(goal[0]=='4' && us==8000) assert(console.motionHistoryCount()==console.motionHistoryLimit);
        gSimulateNav=false; detach();
    }
}
void testNavStairs() {
    using namespace astrabot;
    for(int mode=0;mode<4;++mode) for(std::uint64_t us : {8000U,16000U,100000U}) {
        Fixture fixture{}; enginefuncs_t hooks{}; prepareNavWalk(fixture,hooks);
        auto& owner=adapter::metamod::lifecycleCoordinator(); auto& console=owner.navConsole();
        gStairHeight=mode==3 ? 19.0f:16.0f; gStairCeiling=mode==2;
        route_test::Area a{1,{{0,0,0},{100,100,0},0,0}},
            b{2,{{100,0,gStairHeight},{200,100,gStairHeight},gStairHeight,gStairHeight}};
        a.targets[1]={2}; b.targets[3]={1};
        assert(console.publish(owner.registry().mapGeneration(),route_test::snapshot({a,b})).isNone());
        const bool descending=mode==1;
        fixture.entity.v.origin=descending ? Vector(150,50,52):Vector(50,50,36);
        runNav({"astrabot_goto",descending ? "1":"2"});
        bool terminal=false, stepped=false;
        for(int i=0;i<1000;++i) {
            const auto traces=gHullCalls;
            navFrame(fixture,us); const auto& d=console.motionTrace().decision;
            stepped=stepped || d.steps>0; assert(d.queries<=21);
            if(d.tick==owner.registry().currentTick()) assert(d.queries==static_cast<unsigned>(gHullCalls-traces));
            else assert(gHullCalls==traces);
            if(d.state!=nav::local::WalkState::Running) {
                terminal=true;
                if(mode<2) assert(d.state==nav::local::WalkState::Arrived && d.support && d.support->floor.height==(descending ? 0:16));
                else assert(d.state==nav::local::WalkState::Failed && fixture.entity.v.origin.x<84);
                break;
            }
        }
        assert(terminal); if(mode==0) assert(stepped);
        const auto count=gNavMoves.size(); navFrame(fixture,us); navFrame(fixture,us);
        for(auto i=count;i<gNavMoves.size();++i) assert(gNavMoves[i].movement==core::Movement{});
        gSimulateNav=false; gStairHeight=0; gStairCeiling=false; detach();
    }
}
void testNavDoors() {
    using namespace astrabot;
    for(int mode=0;mode<9;++mode) for(std::uint64_t us : {8000U,16000U,100000U}) {
        Fixture fixture{}; configureDoor(fixture);
        enginefuncs_t hooks{}; prepareNavWalk(fixture,hooks);
        auto& owner=adapter::metamod::lifecycleCoordinator(); auto& console=owner.navConsole();
        fixture.entity.v.view_ofs=Vector(0,0,28);
        gDoorLocked=mode==1; gDoorAmbiguous=mode==2; gDoorLoop=mode==8;
        if(mode==7) gNavDoor.v.spawnflags=0; // touch-only cannot be invented as Use capability
        runNav({"astrabot_goto","2"});
        bool terminal=false,queuedPress=false,waiting=false,changed=false,guardRejected=false;
        std::uint64_t firstWait=0;
        for(int frame=0;frame<2000;++frame) {
            const auto traces=gHullCalls,scans=gDoorScans;
            navFrame(fixture,us);
            guardRejected=guardRejected || motionReason(adapter::cstrike::MotionReason::DoorChanged);
            const auto& t=console.motionTrace(); const auto& d=t.decision;
            assert(d.queries<=21 && gDoorScans-scans<=66);
            if(d.tick==owner.registry().currentTick()) assert(d.queries==static_cast<unsigned>(gHullCalls-traces));
            else assert(gHullCalls==traces);
            if(d.doorState==nav::local::DoorWaitState::Waiting) {
                waiting=true; if(!firstWait) firstWait=gNavClockUs;
                assert(d.intent.speed==0 && fixture.entity.v.origin.x<84.001f);
            }
            if(t.event==adapter::cstrike::MotionEvent::Queued && t.command.buttons==IN_USE) {
                assert(!queuedPress); queuedPress=true;
                if(mode==3) { ++gNavDoor.serialnumber; changed=true; }
                if(mode==4) { gDoorAmbiguous=true; changed=true; }
                if(mode==5) { runNav({"astrabot_nav_cancel"}); changed=true; }
                if(mode==6) { fixture.entity.v.deadflag=DEAD_DEAD; changed=true; }
            }
            if(d.state!=nav::local::WalkState::Running) {
                terminal=true;
                if(mode==0) assert(d.state==nav::local::WalkState::Arrived && gDoorUses==1 && waiting);
                else {
                    assert(d.state==nav::local::WalkState::Failed || d.state==nav::local::WalkState::Aborted);
                    assert(gDoorUses==(mode==1 ? 1:0));
                    if(mode==1 || mode==4) {
                        assert(d.doorReason==nav::local::DoorWaitReason::TimedOut);
                        assert(gNavClockUs-firstWait>=1000000 && gNavClockUs-firstWait<=1000000+us+40000);
                    }
                }
                break;
            }
        }
        assert(terminal);
        if(mode>=3 && mode<=6) assert(changed);
        if(mode==3 || mode==4) assert(guardRejected);
        const auto presses=gDoorUses; navFrame(fixture,us); navFrame(fixture,us); assert(gDoorUses==presses);
        gDoorActive=false; gDoorOpenAtUs=0; gSimulateNav=false; detach();
    }
}
void testNavWalkCancellationAndGuards() {
    using namespace astrabot;
    for(int mode=0;mode<15;++mode) {
        Fixture fixture{}; enginefuncs_t hooks{}; prepareNavWalk(fixture,hooks);
        auto& owner=adapter::metamod::lifecycleCoordinator(); auto& console=owner.navConsole();
        runNav({"astrabot_goto","4"}); awaitMovingQueue(fixture);
        const auto count=gNavMoves.size(); const auto position=fixture.entity.v.origin;
        if(mode==0) {
            runNav({"astrabot_nav_cancel"}); navFrame(fixture); assert(gNavMoves.size()==count);
            navFrame(fixture); assert(gNavMoves.size()==count+1 && gNavMoves.back().movement==core::Movement{});
            navFrame(fixture); assert(gNavMoves.size()==count+1 && fixture.entity.v.origin.x==position.x);
        } else if(mode==1) {
            const auto generation=console.trace()->routeGeneration;
            runNav({"astrabot_goto","1"}); assert(console.trace()->routeGeneration==generation+1);
            navFrame(fixture); assert(gNavMoves.size()==count); navFrame(fixture);
            assert(fixture.entity.v.origin.x==position.x && fixture.entity.v.origin.y==position.y);
        } else if(mode==2 || mode==3) {
            navFrame(fixture,mode==2 ? 120001:0); assert(gNavMoves.size()==count);
            assert(motionReason(adapter::cstrike::MotionReason::StaleCommand));
        } else if(mode==4) {
            fixture.entity.v.origin.y+=20; navFrame(fixture); assert(gNavMoves.size()==count);
            assert(motionReason(adapter::cstrike::MotionReason::Deviation));
        } else if(mode==5 || mode==6) {
            if(mode==5) fixture.entity.v.deadflag=DEAD_DEAD; else fixture.entity.v.flags&=~FL_ONGROUND;
            navFrame(fixture); assert(gNavMoves.size()==count);
        } else if(mode==7) {
            fixture.engine.pfnRunPlayerMove=nullptr; navFrame(fixture); assert(gNavMoves.size()==count);
            assert(motionReason(adapter::cstrike::MotionReason::TransportRejected));
        } else if(mode==8) {
            gInvalidateDuringHull=true;
            for(int i=0;i<6 && console.trace();++i) navFrame(fixture);
            assert(!console.trace()); gInvalidateDuringHull=false;
            const auto before=gNavMoves.size(); navFrame(fixture); navFrame(fixture);
            for(auto i=before;i<gNavMoves.size();++i) assert(gNavMoves[i].movement==core::Movement{});
        } else if(mode==9) {
            owner.serverDeactivate(); navFrame(fixture); assert(gNavMoves.size()==count && !console.trace());
        } else if(mode==10) {
            owner.clientDisconnect(&fixture.entity); navFrame(fixture); assert(gNavMoves.size()==count && !console.trace());
        } else if(mode==11) {
            gInjectNavDuplicate=true; navFrame(fixture);
            assert(console.motionTrace().event==adapter::cstrike::MotionEvent::Rejected &&
                console.motionTrace().transportError==adapter::metamod::MovementError::QueueOccupied);
            navFrame(fixture); assert(gNavMoves.back().movement==core::Movement{});
        } else if(mode==12) {
            const auto hulls=gHullCalls; navFrame(fixture,120000);
            assert(gNavMoves.size()==count+1 && !motionReason(adapter::cstrike::MotionReason::StaleCommand));
            assert(gHullCalls-hulls<=21 && console.motionTrace().missedDecisions>=1);
        } else if(mode==13) {
            gDeactivateDuringMove=true; navFrame(fixture);
            assert(gNavMoves.size()==count+1 && !console.trace());
            assert(console.motionTrace().event==adapter::cstrike::MotionEvent::Dispatched);
            assert(console.motionTrace().dispatchTick.isAfter(console.motionTrace().commandTick));
            assert(gNavTransportTraces.back().engineMsec==16 && gNavTransportTraces.back().frameDeltaUs==16000);
            navFrame(fixture); assert(gNavMoves.size()==count+1);
        } else {
            const auto generation=console.trace()->routeGeneration;
            gReplaceDuringMove=true; navFrame(fixture);
            assert(console.trace()->routeGeneration==generation+1 && console.trace()->goal.value==1);
            assert(console.motionTrace().decision.binding.routeGeneration==generation+1);
            assert(console.motionHistoryCount()==1 && console.motionHistory(0)->event==adapter::cstrike::MotionEvent::Dispatched);
            assert(console.motionHistory(0)->decision.binding.routeGeneration==generation);
            runNav({"astrabot_nav_status"}); navFrame(fixture); navFrame(fixture);
            assert(console.motionTrace().decision.state==nav::local::WalkState::Arrived);
            for(std::size_t i=1;i<console.motionHistoryCount();++i)
                assert(console.motionHistory(i)->sequence>console.motionHistory(i-1)->sequence);
        }
        gSimulateNav=false; detach();
    }
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
    testNavWorldQueries();
    testDoorObservationContracts();
    testSuccessfulCreationAndOpaquePrivateData();
    testFailureRollback();
    testInputAndCapacityRejection();
    testFirstFrameBootstrapAndCleanup();
    testMissingFunctionIsRejectedWithoutEngineCall();
    testMessageDrivenJoinAndCommandContext();
    testNavWalkArrival();
    testNavStairs();
    testNavDoors();
    testNavWalkCancellationAndGuards();
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
