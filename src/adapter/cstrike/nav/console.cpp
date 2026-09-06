// SPDX-License-Identifier: MPL-2.0
#include <cstdio>
#include <cmath>
#include <fstream>
#include <string_view>
#include "adapter/cstrike/nav/console.hpp"
#include "adapter/cstrike/nav/world_queries.hpp"
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
std::optional<core::PlayerId> parsePlayer(std::string_view text) noexcept {
    const auto colon=text.find(':');
    if(colon==std::string_view::npos) return {};
    const auto slot=debug::parseNavGoal(text.substr(0,colon));
    const auto generation=debug::parseNavGoal(text.substr(colon+1));
    if(!slot || !generation || slot->value>host::kMaxClientSlots) return {};
    return core::PlayerId{static_cast<std::uint16_t>(slot->value),{generation->value}};
}
}
NavConsole::ActorState* NavConsole::findActor(core::PlayerId player) noexcept {
    if(!player.isValid() || player.slot>actors_.size()) return nullptr;
    auto* actor=actors_[player.slot-1U].get();
    return actor && actor->actor==player ? actor:nullptr;
}
const NavConsole::ActorState* NavConsole::findActor(core::PlayerId player) const noexcept {
    if(!player.isValid() || player.slot>actors_.size()) return nullptr;
    const auto* actor=actors_[player.slot-1U].get();
    return actor && actor->actor==player ? actor:nullptr;
}
bool NavConsole::selectActor(core::PlayerId player) noexcept {
    if(inRequest_ || !player.isValid() || player.slot>actors_.size()) return false;
    auto& actor=actors_[player.slot-1U];
    try { if(!actor) actor=std::make_unique<ActorState>(); } catch(...) { line("nav error=AllocationFailure"); return false; }
    current_=actor.get();
    if(current_->actor!=player) {
        invalidateCurrent(nav::runtime::SessionReason::Disconnected);
        *current_=ActorState{}; current_->actor=player;
    }
    return true;
}
const nav::runtime::DecisionTrace* NavConsole::trace(core::PlayerId player) const noexcept {
    const auto* actor=findActor(player); return actor && actor->session_ ? &actor->session_->trace():nullptr;
}
const MotionTrace* NavConsole::motionTrace(core::PlayerId player) const noexcept {
    const auto* actor=findActor(player); return actor ? &actor->motionTrace_:nullptr;
}
std::size_t NavConsole::motionHistoryCount(core::PlayerId player) const noexcept {
    const auto* actor=findActor(player); return actor ? actor->motionCount_:0;
}
const MotionTrace* NavConsole::motionHistory(core::PlayerId player,std::size_t index) const noexcept {
    const auto* actor=findActor(player);
    return actor && index<actor->motionCount_ ? &actor->motionHistory_[(actor->motionNext_+motionHistoryLimit-actor->motionCount_+index)%motionHistoryLimit]:nullptr;
}
std::optional<MotionTrace> NavConsole::dispatchTicket(core::PlayerId player) const noexcept {
    const auto* actor=findActor(player);
    return actor && actor->pendingMotion_ ? std::optional<MotionTrace>{actor->motionTrace_}:std::nullopt;
}
void NavConsole::beforeDispatch(metamod::LifecycleCoordinator& owner,core::PlayerId player) noexcept {
    auto* actor=findActor(player); if(!actor) return;
    ActorScope scope(current_,actor); beforeDispatch(owner);
}
void NavConsole::afterDispatch(core::PlayerId player,const metamod::MovementResult& result,core::TickId tick,
    const std::optional<MotionTrace>& ticket) noexcept {
    auto* actor=findActor(player); if(!actor) return;
    ActorScope scope(current_,actor); afterDispatch(result,tick,ticket);
}
void NavConsole::moveFrame(metamod::LifecycleCoordinator& owner,core::PlayerId player) noexcept {
    auto* actor=findActor(player); if(!actor) return;
    ActorScope scope(current_,actor); moveFrame(owner);
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
void NavConsole::invalidateCurrent(nav::runtime::SessionReason reason) noexcept {
    current_->replan_={};
    clearPending();
    stopMotion();
    if(current_->session_) {
        auto update=current_->session_->cancel();
        for(std::size_t i=0;i<update.count;++i) update.events[i].reason=reason;
        printUpdate(update);
    }
    current_->session_.reset();
}
void NavConsole::invalidateActor(core::PlayerId player,nav::runtime::SessionReason reason) noexcept {
    auto* actor=findActor(player); if(!actor) return;
    if(inRequest_ && current_==actor) { clearPending(); deferredInvalidation_=reason; return; }
    ActorScope scope(current_,actor); invalidateCurrent(reason);
}
void NavConsole::invalidate(nav::runtime::SessionReason reason) noexcept {
    for(auto& actor:actors_) if(actor) {
        ActorScope scope(current_,actor.get());
        if(inRequest_) clearPending(); else invalidateCurrent(reason);
    }
    if(inRequest_) { deferredInvalidation_=reason; deferredAll_=true; return; }
    navigation_={}; index_.reset(); mesh_.reset(); ladders_.reset(); queryingEntity_=nullptr; queryingPlayers_=nullptr; queryingOwner_=nullptr;
}
bool NavConsole::applyDeferredInvalidation() noexcept {
    if(!deferredInvalidation_) return false;
    const auto reason=*deferredInvalidation_; const bool all=deferredAll_, resetPending=deferredReset_;
    deferredInvalidation_.reset(); deferredAll_=deferredReset_=false;
    if(all) invalidate(reason); else invalidateCurrent(reason);
    if(resetPending) reset();
    return true;
}
void NavConsole::reset() noexcept {
    if(inRequest_) { invalidate(nav::runtime::SessionReason::Cancelled); deferredReset_=true; return; }
    invalidate(nav::runtime::SessionReason::Cancelled); engine_=nullptr; utility_=nullptr; globals_=nullptr;
    movement_=nullptr;
    for(auto& actor:actors_) if(actor) *actor=ActorState{};
    idle_=ActorState{}; current_=&idle_;
}
nav::diagnostics::NavError NavConsole::publish(core::MapGeneration map,
    std::shared_ptr<const nav::model::NavMeshSnapshot> mesh) noexcept {
    if(inRequest_) { deferredInvalidation_=nav::runtime::SessionReason::GoalReplaced; deferredAll_=true;
        return {nav::diagnostics::NavErrorKind::InvalidInput}; }
    invalidate(nav::runtime::SessionReason::GoalReplaced);
    if (!map.isValid()) return {nav::diagnostics::NavErrorKind::InvalidInput};
    const auto index=nav::query::NavSpatialIndex::build(mesh,{100000,199999,256*mib});
    if(!index) return index.error;
    const auto graph=nav::query::NavGraph::build(mesh,{100000,1000000,256*mib});
    if(!graph) return graph.error;
    index_=*index.value; mesh_=std::move(mesh); navigation_={map,*graph.value}; return {};
}
bool NavConsole::load(const char* path,core::MapGeneration map,metamod::LifecycleCoordinator& owner) noexcept {
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
        line("nav load=Ready profile=compatibility-v1 input_limit=67108864 areas_limit=100000 memory_limit=268435456");
        loadCurrentLadders(owner); return true;
    } catch(...) { line("nav load=AllocationOrInputFailure"); return false; }
}
nav::runtime::MovementSnapshot NavConsole::snapshot(metamod::LifecycleCoordinator& owner) noexcept {
    nav::runtime::MovementSnapshot s;
    auto& registry=owner.registry();
    s.actor=current_->actor; s.agent=owner.agents().findByPlayer(s.actor).agent;
    s.map=registry.mapGeneration(); s.tick=registry.currentTick();
    if(globals_ && std::isfinite(globals_->frametime) && globals_->frametime>=0 && globals_->frametime<=60)
        s.elapsedUs=static_cast<std::uint64_t>(double(globals_->frametime)*1000000.0);
    s.connected=s.actor.isValid() && registry.currentPlayer(s.actor.slot)==s.actor;
    const auto* join=owner.joinState(s.actor);
    s.joined=join && join->phase()==JoinPhase::Joined && join->player()==s.actor;
    auto* entity=owner.entityFor(s.actor);
    if (!entity || entity->free || !engine_ || !engine_->pfnIndexOfEdict ||
        engine_->pfnIndexOfEdict(entity)!=s.actor.slot || !s.agent.isValid() || owner.removalPending(s.actor)) return s;
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
    if(current_->session_ && current_->session_->executable()) {
        printUpdate(current_->session_->observe(snapshot(owner)));
        if(!current_->session_->executable()) { current_->replan_={}; stopMotion(); }
    }
}
void NavConsole::execute(NavCommand command,metamod::LifecycleCoordinator& owner) noexcept {
    if(inRequest_) return;
    if(!engine_ || !engine_->pfnCmd_Argc || !engine_->pfnCmd_Argv) return;
    const int expected=(command==NavCommand::Load || command==NavCommand::GoTo) ? 2:1;
    const auto argc=engine_->pfnCmd_Argc();
    if(argc!=expected && (command==NavCommand::Load || argc!=expected+1)) { line("nav error=InvalidArguments"); return; }
    if(command==NavCommand::Load) {
        if(!owner.registry().isMapActive()) { line("nav error=NoActiveMap"); return; }
        const auto path=bounded(engine_->pfnCmd_Argv(1),1024);
        if(path.empty()) { line("nav error=InvalidPath"); return; }
        (void)load(path.data(),owner.registry().mapGeneration(),owner); return;
    }
    if(command==NavCommand::GoTo && !owner.registry().isMapActive()) { line("nav error=NoActiveMap"); return; }
    core::PlayerId player{};
    if(argc==expected+1) {
        const auto requested=parsePlayer(bounded(engine_->pfnCmd_Argv(expected),24));
        if(!requested || owner.registry().currentPlayer(requested->slot)!=*requested ||
           !owner.agents().findByPlayer(*requested).isValid() || !owner.entityFor(*requested)) {
            line("nav error=InvalidActorArgument expected=slot:generation"); return;
        }
        player=*requested;
    } else {
        for(std::uint16_t slot=1;slot<=owner.registry().clientMax();++slot) {
            const auto candidate=owner.registry().currentPlayer(slot);
            if(!owner.agents().findByPlayer(candidate).isValid()) continue;
            if(player.isValid()) { line("nav error=NoUniqueJoinedManagedActor specify=slot:generation"); return; }
            player=candidate;
        }
        if(!player.isValid()) { line("nav state=Idle"); return; }
    }
    if(!selectActor(player)) return;
    if(command==NavCommand::Status) {
        observe(owner);
        if(current_->session_) debug::printNavTrace(current_->session_->trace(),&sink,this); else line("nav state=Idle");
        printMotion(); printReplan();
        return;
    }
    if(command==NavCommand::Cancel) {
        current_->replan_={};
        stopMotion();
        if(current_->session_) printUpdate(current_->session_->cancel()); else line("nav state=Idle"); return;
    }
    if(!owner.registry().isMapActive()) { line("nav error=NoActiveMap"); return; }
    const auto goal=debug::parseNavGoal(bounded(engine_->pfnCmd_Argv(1),10));
    if(!goal) { line("nav error=InvalidGoalArgument"); return; }
    const auto s=snapshot(owner);
    if(s.kind!=nav::runtime::ActorKind::ManagedBot || !s.agent.isValid() || s.connected!=true || s.joined!=true) {
        observe(owner); line("nav error=NoUniqueJoinedManagedActor"); return;
    }
    if(!current_->session_ || current_->session_->trace().actor!=s.actor || current_->session_->trace().agent!=s.agent || current_->session_->trace().map!=s.map)
        current_->session_.emplace(s.agent,s.actor,s.map);
    stopMotion();
    current_->replan_={}; current_->navigationTimeUs_=0; current_->navigationTimeTick_=s.tick;
    nav::runtime::RouteOptions options; options.limits={100000,256*mib};
    options.groundNavTolerance=18;
    requestRoute(s,*goal,owner,options);
}
void NavConsole::requestRoute(const nav::runtime::MovementSnapshot& s,nav::model::NavAreaId goal,
    metamod::LifecycleCoordinator& owner,const nav::runtime::RouteOptions& options) noexcept {
    queryingEntity_=owner.entityFor(current_->actor);
    queryingPlayers_=&owner.registry();
    queryingOwner_=&owner;
    auto navigation=navigation_;
    if(!navigation.graph) navigation.map=s.map;
    inRequest_=true;
    auto update=current_->session_->request(s,goal,navigation,*this,options);
    inRequest_=false;
    queryingEntity_=nullptr; queryingPlayers_=nullptr; queryingOwner_=nullptr;
    if(deferredInvalidation_) {
        const auto reason=*deferredInvalidation_;
        for(std::size_t i=0;i<update.count;++i) {
            update.events[i].reason=reason;
            update.events[i].state=nav::runtime::SessionState::Cancelled;
            update.events[i].terminal=true;
        }
        printUpdate(update);
        (void)applyDeferredInvalidation(); return;
    }
    printUpdate(update);
    if(current_->session_ && current_->session_->executable()) startMotion(s);
}
void NavConsole::printReplan() noexcept {
    char text[512]{};
    const auto fact=current_->replan_.snapshot(navigation_.map,current_->navigationTimeUs_);
    const auto& edge=fact.blocked;
    std::snprintf(text,sizeof(text),"nav replan_actor=%u:%u state=%u attempts=%u max_attempts=%u fact_lifetime_us=%llu reason=%s edge=%u:%u direction=%u link=%llu:%llu:%llu",
        unsigned(current_->actor.slot),unsigned(current_->actor.generation.value),unsigned(current_->replan_.state()),
        current_->replan_.attempts(),nav::runtime::ReplanAttempt::maxAttempts,
        static_cast<unsigned long long>(nav::runtime::ReplanAttempt::factLifetimeUs),
        current_->replan_.state()==nav::runtime::ReplanState::Idle ? "None":"DynamicObstacle",
        edge ? edge->source.value:0U,edge ? edge->target.value:0U,edge ? unsigned(edge->direction):0U,
        static_cast<unsigned long long>(edge && edge->external ? edge->external->sourceId:0),
        static_cast<unsigned long long>(edge && edge->external ? edge->external->generation:0),
        static_cast<unsigned long long>(edge && edge->external ? edge->external->linkId:0));
    line(text);
}
bool NavConsole::runReplan(metamod::LifecycleCoordinator& owner) noexcept {
    if(current_->replan_.state()!=nav::runtime::ReplanState::Pending) return false;
    const auto s=snapshot(owner);
    if(!s.tick.isAfter(current_->motionTrace_.decision.tick)) return true;
    const auto policy=current_->replan_.consume(current_->motionTrace_.decision.binding,s.tick,current_->navigationTimeUs_);
    printReplan();
    if(!policy || !current_->session_ || !current_->session_->executable()) return true;
    const auto goal=current_->session_->trace().goal;
    stopMotion();
    nav::runtime::RouteOptions options; options.limits={100000,256*mib};
    options.groundNavTolerance=18; options.policy=policy->policy();
    requestRoute(s,goal,owner,options);
    return true;
}
nav::runtime::WorldQueryResult NavConsole::query(const nav::runtime::QueryRequest& request) {
    const NavPlayerResolver resolver{queryingOwner_,[](const void* context,edict_t* entity) noexcept {
        return context ? static_cast<const metamod::LifecycleCoordinator*>(context)->playerForEntity(entity):core::PlayerId{};
    }};
    auto result=queryNavWorld(engine_,queryingEntity_,index_.get(),request,globals_ ? globals_->maxEntities:0,queryingPlayers_,resolver);
    if(deferredInvalidation_) {
        result={}; result.stamp=request.stamp; result.kind=request.kind;
    }
    return result;
}
}
