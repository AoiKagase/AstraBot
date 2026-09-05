// SPDX-License-Identifier: MPL-2.0
#include <cstdio>
#include <cmath>
#include <fstream>
#include <string_view>
#include "adapter/cstrike/nav/console.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include "debug/nav_command.hpp"
#include "nav/io/mesh_loader.hpp"
#ifdef snprintf
#undef snprintf
#endif
#ifdef read
#undef read
#endif

namespace astrabot::adapter::cstrike {
namespace {
constexpr std::size_t mib=1024*1024;
constexpr std::size_t inputLimit=64*mib;
const nav::io::NavMeshReadLimits meshLimits{inputLimit,{100000,65535,65535,8*mib},
    {100000,4096,255,255,65536,255,1000000,1000000,1000000,1000000,1000000},256*mib};
void run(NavCommand command) noexcept {
    auto& owner=metamod::lifecycleCoordinator(); owner.navConsole().execute(command,owner);
}
void loadCommand() { run(NavCommand::Load); }
void gotoCommand() { run(NavCommand::GoTo); }
void statusCommand() { run(NavCommand::Status); }
void cancelCommand() { run(NavCommand::Cancel); }
std::string_view bounded(const char* text,std::size_t max) noexcept {
    if (!text) return {};
    std::size_t n=0; while(n<=max && text[n]) ++n;
    return n>max ? std::string_view{}:std::string_view{text,n};
}
}
void NavConsole::configure(enginefuncs_t* engine,mutil_funcs_t* utility,globalvars_t* globals) noexcept {
    engine_=engine; utility_=utility; globals_=globals;
    if (!engine_ || !engine_->pfnAddServerCommand || !engine_->pfnCmd_Argc || !engine_->pfnCmd_Argv) return;
    static char loadName[]="astrabot_nav_load",gotoName[]="astrabot_goto";
    static char statusName[]="astrabot_nav_status",cancelName[]="astrabot_nav_cancel";
    engine_->pfnAddServerCommand(loadName,&loadCommand);
    engine_->pfnAddServerCommand(gotoName,&gotoCommand);
    engine_->pfnAddServerCommand(statusName,&statusCommand);
    engine_->pfnAddServerCommand(cancelName,&cancelCommand);
}
void NavConsole::sink(void* ctx,const char* text) noexcept { static_cast<NavConsole*>(ctx)->line(text); }
void NavConsole::line(const char* text) noexcept {
    if (utility_ && utility_->pfnLogConsole) utility_->pfnLogConsole(PLID,"%s",text);
}
void NavConsole::printUpdate(const nav::runtime::SessionUpdate& update) noexcept {
    for(std::size_t i=0;i<update.count;++i) debug::printNavTrace(update.events[i],&sink,this);
    if (!update.count && !update.accepted) {
        char text[96]{}; std::snprintf(text,sizeof(text),"nav rejected reason=%u",unsigned(update.reason)); line(text);
    }
}
void NavConsole::invalidate(nav::runtime::SessionReason reason) noexcept {
    if(inRequest_) { deferredInvalidation_=reason; return; }
    if(session_) {
        auto update=session_->cancel();
        for(std::size_t i=0;i<update.count;++i) update.events[i].reason=reason;
        printUpdate(update);
    }
    session_.reset(); navigation_={}; index_.reset(); queryingEntity_=nullptr;
}
void NavConsole::reset() noexcept {
    invalidate(nav::runtime::SessionReason::Cancelled); engine_=nullptr; utility_=nullptr; globals_=nullptr;
}
nav::diagnostics::NavError NavConsole::publish(core::MapGeneration map,
    std::shared_ptr<const nav::model::NavMeshSnapshot> mesh) noexcept {
    invalidate(nav::runtime::SessionReason::GoalReplaced);
    if (!map.isValid()) return {nav::diagnostics::NavErrorKind::InvalidInput};
    const auto index=nav::query::NavSpatialIndex::build(mesh,{100000,199999,256*mib});
    if(!index) return index.error;
    const auto graph=nav::query::NavGraph::build(std::move(mesh),{100000,1000000,256*mib});
    if(!graph) return graph.error;
    index_=*index.value; navigation_={map,*graph.value}; return {};
}
bool NavConsole::load(const char* path,core::MapGeneration map) noexcept {
    invalidate(nav::runtime::SessionReason::GoalReplaced);
    try {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if(!input) { line("nav load=InputUnavailable"); return false; }
        const auto end=input.tellg();
        if(end<0 || static_cast<std::uint64_t>(end)>inputLimit) { line("nav load=InputSizeLimit"); return false; }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
        input.seekg(0);
        if(!bytes.empty()) input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        if(!input || input.peek()!=std::char_traits<char>::eof()) { line("nav load=InputChanged"); return false; }
        const auto mesh=nav::io::NavMeshLoader::load({bytes.data(),bytes.size()},meshLimits);
        auto error=mesh ? publish(map,*mesh.value):mesh.error;
        if(!error.isNone()) {
            char text[160]{};
            std::snprintf(text,sizeof(text),"nav load=Rejected kind=%u record=%u field=%u offset=%llu",
                unsigned(error.kind),unsigned(error.record),unsigned(error.field),static_cast<unsigned long long>(error.offset));
            line(text); return false;
        }
        line("nav load=Ready profile=compatibility-v1 input_limit=67108864 areas_limit=100000 memory_limit=268435456"); return true;
    } catch(...) { line("nav load=AllocationOrInputFailure"); return false; }
}
nav::runtime::MovementSnapshot NavConsole::snapshot(metamod::LifecycleCoordinator& owner) noexcept {
    nav::runtime::MovementSnapshot s;
    auto& registry=owner.registry(); auto& fake=owner.fakeClient();
    s.actor=fake.activePlayer(); s.agent=owner.agents().findByPlayer(s.actor).agent;
    s.map=registry.mapGeneration(); s.tick=registry.currentTick();
    if(globals_ && std::isfinite(globals_->frametime) && globals_->frametime>=0 && globals_->frametime<=60)
        s.elapsedUs=static_cast<std::uint64_t>(double(globals_->frametime)*1000000.0);
    s.connected=s.actor.isValid() && registry.currentPlayer(s.actor.slot)==s.actor;
    s.joined=owner.joinState().phase()==JoinPhase::Joined && owner.joinState().player()==s.actor;
    auto* entity=fake.activeEntity();
    if (!entity || entity->free || !engine_ || !engine_->pfnIndexOfEdict ||
        engine_->pfnIndexOfEdict(entity)!=s.actor.slot || owner.agents().mappingCount()!=1 || fake.removalPending()) return s;
    s.kind=(entity->v.flags&FL_FAKECLIENT) ? nav::runtime::ActorKind::ManagedBot:nav::runtime::ActorKind::Human;
    const auto& v=entity->v;
    s.alive=v.deadflag==DEAD_NO; s.grounded=(v.flags&FL_ONGROUND)!=0; s.ducked=(v.flags&FL_DUCKING)!=0;
    s.position=nav::model::NavVector3{v.origin.x,v.origin.y,v.origin.z};
    s.velocity=nav::model::NavVector3{v.velocity.x,v.velocity.y,v.velocity.z};
    s.view=nav::model::NavVector3{v.v_angle.x,v.v_angle.y,v.v_angle.z};
    s.hull=nav::runtime::HullDimensions{{v.mins.x,v.mins.y,v.mins.z},{v.maxs.x,v.maxs.y,v.maxs.z}};
    if (std::isfinite(v.maxspeed) && v.maxspeed>=0) s.speedLimit=v.maxspeed;
    return s;
}
void NavConsole::observe(metamod::LifecycleCoordinator& owner) noexcept {
    if(inRequest_) return;
    if(session_ && session_->executable()) printUpdate(session_->observe(snapshot(owner)));
}
void NavConsole::execute(NavCommand command,metamod::LifecycleCoordinator& owner) noexcept {
    if(inRequest_) return;
    if(!engine_ || !engine_->pfnCmd_Argc || !engine_->pfnCmd_Argv) return;
    const int expected=(command==NavCommand::Load || command==NavCommand::GoTo) ? 2:1;
    if(engine_->pfnCmd_Argc()!=expected) { line("nav error=InvalidArguments"); return; }
    if(command==NavCommand::Status) {
        observe(owner);
        if(session_) debug::printNavTrace(session_->trace(),&sink,this); else line("nav state=Idle");
        return;
    }
    if(command==NavCommand::Cancel) {
        if(session_) printUpdate(session_->cancel()); else line("nav state=Idle"); return;
    }
    if(!owner.registry().isMapActive()) { line("nav error=NoActiveMap"); return; }
    if(command==NavCommand::Load) {
        const auto path=bounded(engine_->pfnCmd_Argv(1),1024);
        if(path.empty()) { line("nav error=InvalidPath"); return; }
        (void)load(path.data(),owner.registry().mapGeneration()); return;
    }
    const auto goal=debug::parseNavGoal(bounded(engine_->pfnCmd_Argv(1),10));
    if(!goal) { line("nav error=InvalidGoalArgument"); return; }
    const auto s=snapshot(owner);
    if(s.kind!=nav::runtime::ActorKind::ManagedBot || !s.agent.isValid() || s.connected!=true || s.joined!=true) {
        observe(owner); line("nav error=NoUniqueJoinedManagedActor"); return;
    }
    if(!session_ || session_->trace().actor!=s.actor || session_->trace().agent!=s.agent || session_->trace().map!=s.map)
        session_.emplace(s.agent,s.actor,s.map);
    queryingEntity_=owner.fakeClient().activeEntity();
    nav::runtime::RouteOptions options; options.limits={100000,256*mib};
    auto navigation=navigation_;
    if(!navigation.graph) navigation.map=s.map;
    inRequest_=true;
    auto update=session_->request(s,*goal,navigation,*this,options);
    inRequest_=false;
    queryingEntity_=nullptr;
    if(deferredInvalidation_) {
        const auto reason=*deferredInvalidation_; deferredInvalidation_.reset();
        for(std::size_t i=0;i<update.count;++i) {
            update.events[i].reason=reason;
            update.events[i].state=nav::runtime::SessionState::Cancelled;
            update.events[i].terminal=true;
        }
        printUpdate(update);
        invalidate(reason); return;
    }
    printUpdate(update);
}
nav::runtime::WorldQueryResult NavConsole::query(const nav::runtime::QueryRequest& request) {
    nav::runtime::WorldQueryResult result;
    result.stamp=request.stamp; result.kind=request.kind;
    if (!index_ || !engine_ || !engine_->pfnTraceLine || !queryingEntity_ || !request.hull ||
        request.kind!=nav::runtime::QueryKind::GroundedArea) return result;
    const float feet=request.start.z+request.hull->minimum.z;
    if (!std::isfinite(feet)) return result;
    const float start[]{request.start.x,request.start.y,feet+2};
    const float end[]{request.start.x,request.start.y,feet-4};
    TraceResult hit{}; engine_->pfnTraceLine(start,end,1,queryingEntity_,&hit);
    if(deferredInvalidation_) return result;
    result.error=nav::runtime::QueryError::None;
    if(hit.fAllSolid || hit.fStartSolid || !std::isfinite(hit.flFraction) || hit.flFraction<0 || hit.flFraction>=1 ||
        !std::isfinite(hit.vecPlaneNormal.z) || hit.vecPlaneNormal.z<0.7f ||
        !std::isfinite(hit.vecEndPos.z) || std::abs(hit.vecEndPos.z-feet)>4) return result;
    // Only a containing area at the observed support height; no geometric nearest fallback.
    const auto match=index_->containing({request.start.x,request.start.y,hit.vecEndPos.z},2);
    if(!match || !*match.value) return result;
    result.ground=nav::runtime::GroundedAreaObservation{(**match.value).areaId,
        nav::runtime::FloorObservation{hit.vecEndPos.z,{hit.vecPlaneNormal.x,hit.vecPlaneNormal.y,hit.vecPlaneNormal.z},true}};
    return result;
}
}
