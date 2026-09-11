// SPDX-License-Identifier: MPL-2.0
#include <cstdio>
#include <cmath>
#include <array>
#include <fstream>
#include <limits>
#include <string_view>
#include "adapter/cstrike/nav/console.hpp"
#include "adapter/cstrike/nav/world_queries.hpp"
#include "adapter/metamod/console_debug.hpp"
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
constexpr std::uint64_t currentAreaFallbackMaxAgeTicks=2;
const nav::io::NavMeshReadLimits meshLimits{inputLimit,{100000,65535,65535,8*mib},
    {100000,4096,255,255,65536,255,1000000,1000000,1000000,1000000,1000000},256*mib};
bool hullFits(const nav::model::NavExtent& extent,
              const nav::runtime::HullDimensions& hull) noexcept {
    if(!extent.isFinite() || extent.southEast.x<=extent.northWest.x ||
       extent.southEast.y<=extent.northWest.y || !hull.minimum.isFinite() ||
       !hull.maximum.isFinite() || hull.minimum.x>=hull.maximum.x ||
       hull.minimum.y>=hull.maximum.y)
        return false;
    const double halfX=(std::max)(std::abs(double(hull.minimum.x)),std::abs(double(hull.maximum.x)));
    const double halfY=(std::max)(std::abs(double(hull.minimum.y)),std::abs(double(hull.maximum.y)));
    return double(extent.southEast.x)-extent.northWest.x >= 2*halfX &&
           double(extent.southEast.y)-extent.northWest.y >= 2*halfY;
}

// Route searches are actor-local, while the resulting corridors are executed
// in the same world. Keep a small synchronous view of other actors' leading
// edges so newly planned bots do not all select the same narrow entry. This
// is a cost preference, not a hard exclusion: if the map has no alternative,
// the route remains executable.
constexpr std::size_t trafficLookAhead=6;
struct TrafficReservation final {
    nav::query::NavDirectedEdge edge{};
    std::size_t depth{};
};
struct TrafficRoutePolicy final {
    nav::query::NavRoutePolicy base{};
    std::array<TrafficReservation, host::kMaxClientSlots*trafficLookAhead> occupied{};
    std::size_t count{0};
};

bool sameRouteEdge(const nav::query::NavDirectedEdge& left,
                   const nav::query::NavDirectedEdge& right) noexcept {
    if (left.source != right.source || left.target != right.target ||
        left.traversal != right.traversal ||
        left.external.has_value() != right.external.has_value()) return false;
    if (!left.external) return left.direction == right.direction;
    return left.external->sourceId == right.external->sourceId &&
           left.external->generation == right.external->generation &&
           left.external->linkId == right.external->linkId &&
           left.external->direction == right.external->direction;
}

nav::query::NavCostDecision trafficCost(
    const nav::query::NavCostContext& input, const void* opaque) noexcept {
    const auto* policy=static_cast<const TrafficRoutePolicy*>(opaque);
    if (!policy) return {true,{}};
    auto result=policy->base.cost
        ? policy->base.cost(input,policy->base.context)
        : nav::query::NavCostDecision{
              false,{input.geometricDistance,0.0,0.0,
                     input.edge.external ? input.edge.external->additionalCost:0.0,
                     0.0}};
    if (result.blocked) return result;
    double waitSeconds=0.0;
    for (std::size_t i=0; i<policy->count; ++i)
        if (sameRouteEdge(input.edge,policy->occupied[i].edge))
            // Convert a bounded courtesy wait into the same distance-like
            // units used by the default route cost (CS run speed ~=160).
            waitSeconds += 0.25 + 0.08*static_cast<double>(policy->occupied[i].depth);
    if (waitSeconds>0) result.components.danger += waitSeconds*160.0;
    return result;
}

double trafficHeuristic(const nav::query::NavHeuristicContext& input,
                        const void* opaque) noexcept {
    // The congestion surcharge is not represented in the base heuristic.
    // Return zero so the wrapped policy remains admissible for every base
    // cost function, including custom policies supplied by callers.
    (void)input;
    (void)opaque;
    return 0.0;
}

void run(NavCommand command) noexcept {
    auto& owner=metamod::lifecycleCoordinator(); owner.navConsole().execute(command,owner);
}
void loadCommand() { run(NavCommand::Load); }
void gotoCommand() { run(NavCommand::GoTo); }
void statusCommand() { run(NavCommand::Status); }
void cancelCommand() { run(NavCommand::Cancel); }
void reportCommand() { run(NavCommand::Report); }
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
void NavConsole::applyRuntimeNavigation(
    metamod::LifecycleCoordinator& owner,
    const metamod::RuntimeDecision& decision) noexcept {
    RuntimeNavigationStatus status{};
    status.map = owner.registry().mapGeneration();
    status.round = owner.round();
    status.tick = owner.registry().currentTick();
    status.decisionMap = decision.team.shared.map;
    status.decisionRound = decision.team.shared.round;
    status.decisionTick = decision.team.shared.tick;
    status.player = decision.player;
    status.agent = decision.agent;
    status.goal = decision.navigationGoal;
    const bool roamDecision =
        decision.tactical.intent.type == core::tactical::IntentType::Roam;
    const bool explicitDecision =
        decision.tactical.intent.type == core::tactical::IntentType::Hold &&
        decision.tactical.intent.reason == core::tactical::Reason::Periodic;
    const auto publish = [&]() noexcept {
        runtimeNavigationStatus_ = status;
        if (status.player.isValid() && status.player.slot <= host::kMaxClientSlots)
            runtimeNavigationStatuses_[status.player.slot - 1U] = status;
    };
    const auto reject = [&](RuntimeNavigationApplyReason reason) noexcept {
        status.result = RuntimeNavigationApplyResult::Rejected;
        status.reason = reason;
        publish();
    };
    if (!decision.executable || !decision.hasNavigationGoal) {
        if (!decision.executable) {
            reject(RuntimeNavigationApplyReason::NoExecutableGoal);
        } else {
            const auto* actor = findActor(decision.player);
            if (actor && !actor->explicitRoute_ && actor->session_ &&
                actor->session_->executable()) {
                auto* mutableActor = const_cast<ActorState*>(actor);
                ActorScope scope(current_, mutableActor);
                stopMotion();
                (void)current_->session_->cancel();
                current_->roamArrived_ = true;
                if(current_->execution_.state==nav::runtime::ExecutionState::Running)
                    current_->execution_.state=nav::runtime::ExecutionState::Idle;
            }
            status.result = RuntimeNavigationApplyResult::Unchanged;
            status.reason = RuntimeNavigationApplyReason::None;
            publish();
        }
        return;
    }
    if (!decision.player.isValid() || !decision.agent.isValid() ||
        !decision.navigationGoal.isValid()) {
        reject(RuntimeNavigationApplyReason::InvalidIdentity);
        return;
    }
    if (inRequest_) {
        reject(RuntimeNavigationApplyReason::RequestReentrant);
        return;
    }
    if (!owner.registry().isMapActive()) {
        reject(RuntimeNavigationApplyReason::MapInactive);
        return;
    }
    if (owner.registry().mapGeneration() != decision.team.shared.map ||
        owner.round() != decision.team.shared.round ||
        owner.registry().currentTick() != decision.team.shared.tick ||
        owner.registry().currentTick() == core::TickId{}) {
        reject(RuntimeNavigationApplyReason::StampMismatch);
        return;
    }
    if (!selectActor(decision.player)) {
        reject(RuntimeNavigationApplyReason::ActorUnavailable);
        return;
    }
    const auto s = snapshot(owner);
    if (s.kind != nav::runtime::ActorKind::ManagedBot ||
        s.actor != decision.player || s.agent != decision.agent ||
        s.map != owner.registry().mapGeneration() ||
        s.connected != true || s.alive != true || s.joined != true ||
        !s.position || !s.velocity || !s.view || !s.hull || !s.speedLimit) {
        reject(RuntimeNavigationApplyReason::ActorStateInvalid);
        return;
    }
    if(!navigation_.graph) {
        reject(RuntimeNavigationApplyReason::RouteRejected);
        return;
    }
    const bool sameRunningRoute =
        current_->execution_.state==nav::runtime::ExecutionState::Running &&
        current_->session_ && current_->session_->executable() &&
        current_->session_->trace().actor == s.actor &&
        current_->session_->trace().agent == s.agent &&
        current_->session_->trace().map == s.map &&
        current_->session_->trace().goal == decision.navigationGoal;
    // A periodic runtime decision must not revalidate the current route as a
    // new search.  Execution cooldowns and search budgets describe failed or
    // pending replans; applying them to an already-running route turns a
    // healthy route into GoalReplaced/Unchanged churn.
    if (sameRunningRoute) {
        status.result = RuntimeNavigationApplyResult::Unchanged;
        status.reason = RuntimeNavigationApplyReason::None;
        publish();
        return;
    }
    if(current_->execution_.cooling(decision.navigationGoal,current_->navigationTimeUs_) ||
       !current_->execution_.canSearch(current_->navigationTimeUs_)) {
        reject(RuntimeNavigationApplyReason::RouteRejected); return;
    }
    const auto goalVertex=navigation_.graph->find(decision.navigationGoal);
    if(!goalVertex || !hullFits(navigation_.graph->area(*goalVertex).extent,*s.hull)) {
        if(roamDecision && current_->roamRejectedGoalCount_ < current_->roamRejectedGoals_.size()) {
            bool duplicate=false;
            for(std::size_t i=0;i<current_->roamRejectedGoalCount_;++i)
                duplicate=duplicate || current_->roamRejectedGoals_[i]==decision.navigationGoal;
            if(!duplicate) current_->roamRejectedGoals_[current_->roamRejectedGoalCount_++]=decision.navigationGoal;
        }
        reject(RuntimeNavigationApplyReason::RouteRejected);
        return;
    }
    if (current_->explicitRoute_ && roamDecision) {
        status.result = RuntimeNavigationApplyResult::Unchanged;
        status.reason = RuntimeNavigationApplyReason::None;
        publish();
        return;
    }
    if (!current_->session_ || current_->session_->trace().actor != s.actor ||
        current_->session_->trace().agent != s.agent ||
        current_->session_->trace().map != s.map) {
        current_->session_.emplace(s.agent, s.actor, s.map);
    }
    stopMotion();
    current_->replan_ = {};
    current_->navigationTimeTick_ = s.tick;
    current_->recovery_ = {};
    current_->recoveryReplan_ = false;
    current_->explicitRoute_ = explicitDecision;
    current_->roamArrived_ = false;
    nav::runtime::RouteOptions options;
    options.limits = {100000, 256 * mib};
    options.groundNavTolerance = 18;
    requestRoute(s, decision.navigationGoal, owner, options);
    if (current_->session_ && current_->session_->executable() &&
        current_->execution_.state==nav::runtime::ExecutionState::Running) {
        status.result = RuntimeNavigationApplyResult::Applied;
        status.reason = RuntimeNavigationApplyReason::None;
    } else {
        status.result = RuntimeNavigationApplyResult::Rejected;
        status.reason = RuntimeNavigationApplyReason::RouteRejected;
    }
    publish();
}
void NavConsole::configure(enginefuncs_t* engine,mutil_funcs_t* utility,globalvars_t* globals) noexcept {
    engine_=engine; utility_=utility; globals_=globals;
    if (!engine_ || !engine_->pfnAddServerCommand || !engine_->pfnCmd_Argc || !engine_->pfnCmd_Argv) return;
    static char loadName[]="astrabot_nav_load",gotoName[]="astrabot_goto";
    static char statusName[]="astrabot_nav_status",cancelName[]="astrabot_nav_cancel";
    static char reportName[]="astrabot_report";
    engine_->pfnAddServerCommand(loadName,&loadCommand);
    engine_->pfnAddServerCommand(gotoName,&gotoCommand);
    engine_->pfnAddServerCommand(statusName,&statusCommand);
    engine_->pfnAddServerCommand(cancelName,&cancelCommand);
    engine_->pfnAddServerCommand(reportName,&reportCommand);
}
void NavConsole::sink(void* ctx,const char* text) noexcept { static_cast<NavConsole*>(ctx)->line(text); }
void NavConsole::line(const char* text) noexcept {
    if(!metamod::ConsoleDebug::instance().navEnabled() ||
       !utility_ || !utility_->pfnLogConsole || !text) return;
    utility_->pfnLogConsole(PLID,"%s",text);
}
void NavConsole::printUpdate(const nav::runtime::SessionUpdate& update) noexcept {
    for(std::size_t i=0;i<update.count;++i) debug::printNavTrace(update.events[i],&sink,this);
    if (!update.count && !update.accepted) {
        char text[96]{}; std::snprintf(text,sizeof(text),"nav rejected reason=%u",unsigned(update.reason)); line(text);
    }
}
void NavConsole::invalidateCurrent(nav::runtime::SessionReason reason) noexcept {
    current_->execution_={};
    current_->replan_={};
    current_->recovery_={};
    current_->recoveryReplan_=false;
    current_->lastCurrentArea_.reset();
    current_->lastCurrentAreaActor_={};
    current_->lastCurrentAreaAgent_={};
    current_->lastCurrentAreaMap_={};
    current_->lastCurrentAreaRouteGeneration_=0;
    current_->lastCurrentAreaTick_={};
    current_->explicitRoute_=false;
    current_->roamArrived_=false;
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
    if(world_) world_->clearDistributions();
    for(auto& actor:actors_) if(actor) {
        ActorScope scope(current_,actor.get());
        if(inRequest_) clearPending(); else invalidateCurrent(reason);
    }
    if(inRequest_) { deferredInvalidation_=reason; deferredAll_=true; return; }
    navigation_={}; index_.reset(); distributionTopology_.reset(); mesh_.reset(); ladders_.reset(); queryingEntity_=nullptr; queryingPlayers_=nullptr; queryingOwner_=nullptr;
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
    movement_=nullptr; world_=nullptr;
    for(auto& actor:actors_) if(actor) *actor=ActorState{};
    idle_=ActorState{}; current_=&idle_;
    runtimeNavigationStatus_={};
    runtimeNavigationStatuses_={};
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
    distributionTopology_=nav::query::DistributionTopology::build(map,*graph.value,*index.value);
    if(!distributionTopology_) return {nav::diagnostics::NavErrorKind::AllocationFailure};
    index_=*index.value; mesh_=std::move(mesh); navigation_={map,*graph.value}; return {};
}
bool NavConsole::loadForMap(const char* path,core::MapGeneration map,metamod::LifecycleCoordinator& owner) noexcept {
    if(!path || !*path || inRequest_ || deferredInvalidation_ || !map.isValid() ||
       !owner.registry().isMapActive() || owner.registry().mapGeneration()!=map) return false;
    return load(path,map,owner);
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
nav::runtime::MovementSnapshot NavConsole::snapshot(const metamod::LifecycleCoordinator& owner) const noexcept {
    return snapshotFor(owner,current_->actor);
}
nav::runtime::MovementSnapshot NavConsole::snapshotFor(
    const metamod::LifecycleCoordinator& owner,core::PlayerId player) const noexcept {
    nav::runtime::MovementSnapshot s;
    auto& registry=owner.registry();
    const auto binding=owner.agents().findByPlayer(player);
    s.actor=player; s.agent=binding.agent;
    s.map=registry.mapGeneration(); s.tick=registry.currentTick();
    // MovementCoordinator owns the frame clock used by RunPlayerMove and by
    // pending-command freshness. Do not mix it with globalvars->frametime:
    // the latter can be zero or represent a different callback interval.
    // A zero delta is deliberately preserved for the first frame; callers
    // keep that frame neutral and do not queue movement from it.
    if (movement_)
        s.elapsedUs=movement_->frameDeltaUs();
    s.connected=s.actor.isValid() && registry.currentPlayer(s.actor.slot)==s.actor;
    const auto* join=owner.joinState(s.actor);
    s.joined=join && join->phase()==JoinPhase::Joined && join->player()==s.actor;
    auto* entity=owner.entityFor(s.actor);
    if (!binding.isValid() || binding.player != s.actor ||
        binding.map != s.map || !entity || entity->free || !engine_ ||
        !engine_->pfnIndexOfEdict ||
        engine_->pfnIndexOfEdict(entity)!=s.actor.slot ||
        (engine_->pfnPEntityOfEntIndex &&
         engine_->pfnPEntityOfEntIndex(s.actor.slot)!=entity) ||
        owner.removalPending(s.actor)) return s;
    s.kind=nav::runtime::ActorKind::ManagedBot;
    const auto& v=entity->v;
    s.alive=v.deadflag==DEAD_NO; s.grounded=(v.flags&FL_ONGROUND)!=0; s.ducked=(v.flags&FL_DUCKING)!=0;
    s.position=nav::model::NavVector3{v.origin.x,v.origin.y,v.origin.z};
    s.velocity=nav::model::NavVector3{v.velocity.x,v.velocity.y,v.velocity.z};
    s.view=nav::model::NavVector3{v.v_angle.x,v.v_angle.y,v.v_angle.z};
    s.hull=nav::runtime::HullDimensions{{v.mins.x,v.mins.y,v.mins.z},{v.maxs.x,v.maxs.y,v.maxs.z}};
    if (std::isfinite(v.maxspeed) && v.maxspeed>=0) s.speedLimit=v.maxspeed;
    if (std::isfinite(v.health) && v.health>0) s.health=v.health;
    return s;
}
std::optional<RuntimeNavigationState> NavConsole::runtimeState(
    const metamod::LifecycleCoordinator& owner, core::PlayerId player) const noexcept {
    const auto* actor = findActor(player);
    if (inRequest_ || deferredInvalidation_ ||
        navigation_.map != owner.registry().mapGeneration() || !index_)
        return {};

    RuntimeNavigationState result{};
    result.movement = snapshotFor(owner, player);
    result.roamGeneration =
        static_cast<std::uint64_t>(result.movement.map.value) * 1'000'003ULL +
        static_cast<std::uint64_t>(result.movement.agent.value) * 97ULL +
        static_cast<std::uint64_t>(result.movement.actor.slot) * 53ULL +
        static_cast<std::uint64_t>(result.movement.actor.generation.value) * 7ULL +
        static_cast<std::uint64_t>(owner.round().value);
    if (actor && actor->session_) {
        result.execution=actor->execution_.state;
        result.executionFailure=actor->execution_.failure;
        result.failedEdge=actor->execution_.failedEdge;
        result.retryAtUs=actor->execution_.retryAtUs;
        const auto& trace = actor->session_->trace();
        result.goal = trace.goal.isValid()
            ? std::optional<nav::model::NavAreaId>{trace.goal}
            : std::nullopt;
        result.routeGeneration = trace.routeGeneration;
        result.explicitRoute = actor->explicitRoute_;
        result.roamActive = !actor->explicitRoute_;
        result.roamArrived = actor->roamArrived_ && result.roamActive;
        result.routeExecutable = actor->session_->executable() &&
                                 actor->execution_.state==nav::runtime::ExecutionState::Running &&
                                 !result.roamArrived;
        // A running route is already accepted.  Its goal must remain visible
        // to the planner even while the execution budget contains cooldown
        // information from an earlier bounded recovery.  Only a non-running
        // goal is eligible for rejection/replan signalling here.
        result.roamRejected = false;
        if (result.roamActive && !result.roamArrived &&
            !result.routeExecutable && result.goal) {
            result.roamRejected = actor->execution_.cooling(
                *result.goal, actor->navigationTimeUs_);
            const auto limit = (std::min)(
                actor->roamRejectedGoalCount_, actor->roamRejectedGoals_.size());
            for (std::size_t i = 0; i < limit; ++i) {
                if (actor->roamRejectedGoals_[i] == *result.goal) {
                    result.roamRejected = true;
                    break;
                }
            }
        }
        if (result.routeExecutable && result.goal && navigation_.graph) {
            if (const auto vertex = navigation_.graph->find(*result.goal)) {
                const auto point = navigation_.graph->center(*vertex);
                result.goalPosition =
                    core::perception::Point{point.x, point.y, point.z};
            }
        } else if (result.roamArrived) {
            result.goal.reset();
            result.goalPosition.reset();
        }
    }

    if (result.movement.position) {
        const auto match = index_->containing(*result.movement.position, 72.0);
        if (match && *match.value) {
            result.currentArea = (*match.value)->areaId;
            if (actor && actor->session_) {
                const auto& trace = actor->session_->trace();
                actor->lastCurrentArea_ = result.currentArea;
                actor->lastCurrentAreaActor_ = result.movement.actor;
                actor->lastCurrentAreaAgent_ = result.movement.agent;
                actor->lastCurrentAreaMap_ = result.movement.map;
                actor->lastCurrentAreaRouteGeneration_ = trace.routeGeneration;
                actor->lastCurrentAreaTick_ = result.movement.tick;
            }
        }
    }

    const bool traversal = actor && actor->session_ &&
        actor->session_->executable() &&
        actor->execution_.state==nav::runtime::ExecutionState::Running &&
        result.movement.connected.value_or(false) &&
        result.movement.joined.value_or(false) &&
        result.movement.alive.value_or(false) &&
        result.movement.tick.isValid() && result.routeGeneration != 0 &&
        actor->motionTrace_.decision.accepted &&
        actor->motionTrace_.decision.state == nav::local::WalkState::Running &&
        actor->motionTrace_.decision.binding.actor == result.movement.actor &&
        actor->motionTrace_.decision.binding.agent == result.movement.agent &&
        actor->motionTrace_.decision.binding.map == result.movement.map &&
        actor->motionTrace_.decision.binding.map == navigation_.map &&
        actor->motionTrace_.decision.binding.routeGeneration ==
            result.routeGeneration &&
        (actor->motionTrace_.decision.jumpState.has_value() ||
         actor->motionTrace_.decision.dropState.has_value() ||
         actor->motionTrace_.decision.ladderState.has_value());
    if (!result.currentArea && traversal && actor->lastCurrentArea_ &&
        actor->lastCurrentAreaActor_ == result.movement.actor &&
        actor->lastCurrentAreaAgent_ == result.movement.agent &&
        actor->lastCurrentAreaMap_ == result.movement.map &&
        actor->lastCurrentAreaRouteGeneration_ == result.routeGeneration &&
        actor->lastCurrentAreaTick_.isValid() &&
        !result.movement.tick.isBefore(actor->lastCurrentAreaTick_) &&
        result.movement.tick.value - actor->lastCurrentAreaTick_.value <=
            currentAreaFallbackMaxAgeTicks) {
        result.currentArea = actor->lastCurrentArea_;
        result.currentAreaHeld = true;
    }

    const bool validManagedMovement =
        result.movement.kind == nav::runtime::ActorKind::ManagedBot &&
        result.movement.actor.isValid() && result.movement.agent.isValid() &&
        result.movement.map == navigation_.map;
    if (validManagedMovement && result.currentArea && navigation_.graph) {
        const auto current = *result.currentArea;
        const auto isListed = [](const auto& values, std::size_t count,
                                 nav::model::NavAreaId id) noexcept {
            const auto limit = (std::min)(count, values.size());
            for (std::size_t i = 0; i < limit; ++i)
                if (values[i] == id) return true;
            return false;
        };
        const auto occupiedByOther = [&](nav::model::NavAreaId id) noexcept {
            for (const auto& other : actors_) {
                if (!other || other.get() == actor || !other->session_ ||
                    !other->session_->executable() ||
                    other->execution_.state!=nav::runtime::ExecutionState::Running || other->explicitRoute_ ||
                    other->roamArrived_)
                    continue;
                const auto& trace=other->session_->trace();
                if (trace.goal == id) return true;
                if (trace.route) {
                    const auto cursor=other->walk_ ? other->walk_->step():0;
                    const auto count=(std::min)(trafficLookAhead,
                        trace.route->areas.size()>cursor ? trace.route->areas.size()-cursor:0);
                    for (std::size_t i=0; i<count; ++i)
                        if (trace.route->areas[cursor+i] == id) return true;
                }
            }
            return false;
        };
        const auto addCandidate = [&](nav::model::NavAreaId id,
                                      bool allowRecent) noexcept {
        const bool preserveCurrentGoal = result.roamActive &&
            result.routeExecutable && result.goal && id == *result.goal;
        if (result.roamCandidateCount >= result.roamCandidates.size()) {
            ++result.roamExcludedCapacity; return;
        }
        if (!id.isValid() || id == current) {
            ++result.roamExcludedInvalid; return;
        }
        if (!preserveCurrentGoal && occupiedByOther(id)) {
            ++result.roamExcludedOccupied; return;
        }
        if(!preserveCurrentGoal && actor &&
            (!actor->execution_.canSearch(actor->navigationTimeUs_) ||
             actor->execution_.cooling(id,actor->navigationTimeUs_))) {
            ++result.roamExcludedCooling; return;
        }
        if (!preserveCurrentGoal && actor && isListed(actor->roamRejectedGoals_,
            actor->roamRejectedGoalCount_, id)) {
            ++result.roamExcludedRejected; return;
        }
        if (!preserveCurrentGoal && !allowRecent && actor &&
            isListed(actor->roamRecentGoals_,
            actor->roamRecentGoalCount_, id)) {
            ++result.roamExcludedRecent; return;
        }
        const auto vertex = navigation_.graph->find(id);
        if (!vertex) { ++result.roamExcludedMissing; return; }
        if (!result.movement.hull || !hullFits(navigation_.graph->area(*vertex).extent,*result.movement.hull))
            { ++result.roamExcludedHull; return; }
            const auto point = navigation_.graph->center(*vertex);
            result.roamCandidates[result.roamCandidateCount++] =
                {{id}, {point.x, point.y, point.z}, id.value};
        };

        if (result.roamActive && result.routeExecutable && result.goal)
            addCandidate(*result.goal, true);

        const auto areaCount = navigation_.graph->areaCount();
        const auto seed =
            static_cast<std::size_t>(result.movement.agent.value) * 2654435761U +
            static_cast<std::size_t>(owner.round().value);
        if (areaCount != 0) {
            const auto offset = seed % areaCount;
            for (std::size_t step = 0;
                 step < areaCount &&
                 result.roamCandidateCount < result.roamCandidates.size();
                 ++step) {
                const auto vertex = (offset + step) % areaCount;
                addCandidate(navigation_.graph->area(vertex).id, false);
            }
        }
    }
    return result;
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
    if(command==NavCommand::Report) {
        if(engine_->pfnCmd_Argc()!=3) { line("report error=InvalidArguments expected=reporter_slot:generation target_slot:generation"); return; }
        const auto reporter=parsePlayer(bounded(engine_->pfnCmd_Argv(1),24));
        const auto target=parsePlayer(bounded(engine_->pfnCmd_Argv(2),24));
        if(!reporter || !target) { line("report error=InvalidActorArgument expected=slot:generation"); return; }
        const auto result=owner.report(*reporter,*target); char text[128]{};
        std::snprintf(text,sizeof(text),"report status=%s recipients=%zu",core::world::reportReasonName(result.reason),result.recipients);
        line(text); return;
    }
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
        current_->recovery_={};
        current_->recoveryReplan_=false;
        current_->explicitRoute_=false;
        current_->roamArrived_=false;
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
    if(!navigation_.graph || !s.hull) {
        line("nav error=InvalidGoalArea"); return;
    }
    const auto goalVertex=navigation_.graph->find(*goal);
    if(!goalVertex || !hullFits(navigation_.graph->area(*goalVertex).extent,*s.hull)) {
        line("nav error=InvalidGoalArea"); return;
    }
    if(!current_->session_ || current_->session_->trace().actor!=s.actor || current_->session_->trace().agent!=s.agent || current_->session_->trace().map!=s.map)
        current_->session_.emplace(s.agent,s.actor,s.map);
    stopMotion();
    current_->replan_={}; current_->navigationTimeUs_=0; current_->navigationTimeTick_=s.tick;
    current_->recovery_={};
    current_->recoveryReplan_=false;
    current_->explicitRoute_=true;
    current_->roamArrived_=false;
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
    const auto previousGeneration=current_->session_ ?
        current_->session_->trace().routeGeneration:0;
    const auto shortcutGeneration=previousGeneration==(std::numeric_limits<std::uint64_t>::max)() ?
        std::uint64_t{1}:previousGeneration+1;
    const auto shortcuts=discoverShortcuts(s,shortcutGeneration);
    if (navigation.graph && !shortcuts.links.empty()) {
        const auto augmented=nav::query::NavGraph::augment(navigation.graph,shortcuts,
            {100000,1000000,256U*1024U*1024U},{128,4U*1024U*1024U});
        if (augmented) navigation.graph=*augmented.value;
        char text[160]{};
        std::snprintf(text,sizeof(text),"nav shortcuts actor=%u:%u candidates=%zu accepted=%zu generation=%llu",
            unsigned(s.actor.slot),unsigned(s.actor.generation.value),std::size_t(8),shortcuts.links.size(),
            static_cast<unsigned long long>(shortcutGeneration));
        line(text);
    }
    current_->execution_.begin();
    const nav::runtime::ExecutionPolicy executionPolicy{&current_->execution_,options.policy};
    auto filteredOptions=options;
    filteredOptions.policy=executionPolicy.policy();
    TrafficRoutePolicy traffic;
    traffic.base=filteredOptions.policy;
    for (const auto& other : actors_) {
        if (!other || other.get()==current_ || !other->session_ ||
            !other->session_->executable() ||
            other->execution_.state!=nav::runtime::ExecutionState::Running ||
            other->session_->trace().map!=s.map) continue;
        const auto& route=other->session_->trace().route;
        if (!route || route->steps.empty()) continue;
        const auto cursor=other->walk_ ? other->walk_->step():0;
        const auto count=(std::min)(trafficLookAhead,
            route->steps.size()>cursor ? route->steps.size()-cursor:0);
        for (std::size_t depth=0; depth<count && traffic.count<traffic.occupied.size(); ++depth)
            traffic.occupied[traffic.count++]={route->steps[cursor+depth].edge,depth};
    }
    if (traffic.count) {
        filteredOptions.policy={&traffic,&trafficCost,&trafficHeuristic};
    }
    auto update=current_->session_->request(s,goal,navigation,*this,filteredOptions);
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
    else current_->execution_.fail(goal,nav::runtime::ExecutionFailure::Search,current_->navigationTimeUs_);
}
void NavConsole::printReplan() noexcept {
    char text[512]{};
    const auto fact=current_->replan_.snapshot(navigation_.map,current_->navigationTimeUs_);
    const auto& edge=fact.blocked;
    std::snprintf(text,sizeof(text),"nav replan_actor=%u:%u state=%u attempts=%u max_attempts=%u fact_lifetime_us=%llu reason=%s edge=%u:%u direction=%u link=%llu:%llu:%llu",
        unsigned(current_->actor.slot),unsigned(current_->actor.generation.value),unsigned(current_->replan_.state()),
        current_->replan_.attempts(),nav::runtime::ReplanAttempt::maxAttempts,
        static_cast<unsigned long long>(nav::runtime::ReplanAttempt::factLifetimeUs),
        current_->replan_.state()==nav::runtime::ReplanState::Idle ? "None":(current_->replan_.isRecovery() ? "Stuck":"DynamicObstacle"),
        edge ? edge->source.value:0U,edge ? edge->target.value:0U,edge ? unsigned(edge->direction):0U,
        static_cast<unsigned long long>(edge && edge->external ? edge->external->sourceId:0),
        static_cast<unsigned long long>(edge && edge->external ? edge->external->generation:0),
        static_cast<unsigned long long>(edge && edge->external ? edge->external->linkId:0));
    line(text);
}

nav::enrichment::NavTraversalLinkSet NavConsole::discoverShortcuts(
    const nav::runtime::MovementSnapshot& s,std::uint64_t generation) noexcept {
    nav::enrichment::NavTraversalLinkSet result{};
    if (!navigation_.graph || !index_ || !s.position || !s.hull || !generation ||
        s.kind!=nav::runtime::ActorKind::ManagedBot || s.connected!=true ||
        s.alive!=true || s.joined!=true) return result;
    if (shortcutQueryTick_!=s.tick) {
        shortcutQueryTick_=s.tick;
        shortcutQueriesThisTick_=0;
    }
    const auto current=index_->containing(*s.position,18.0);
    if (!current || !*current.value) return result;
    const auto currentId=(**current.value).areaId;
    const auto currentVertex=navigation_.graph->find(currentId);
    if (!currentVertex) return result;
    struct Candidate { nav::model::NavAreaId id{}; double score{0}; };
    std::array<Candidate,8> candidates{};
    std::size_t candidateCount=0;
    const auto hasNativeEdge=[&](nav::model::NavAreaId id) noexcept {
        for (auto e=navigation_.graph->edgeBegin(*currentVertex);
             e<navigation_.graph->edgeEnd(*currentVertex);++e)
            if (navigation_.graph->edge(e).target==id &&
                !navigation_.graph->edge(e).external) return true;
        return false;
    };
    for (std::size_t i=0;i<navigation_.graph->areaCount();++i) {
        const auto id=navigation_.graph->area(i).id;
        if (id==currentId || hasNativeEdge(id)) continue;
        const auto center=navigation_.graph->center(i);
        const double dx=center.x-s.position->x,dy=center.y-s.position->y;
        const double distance=std::hypot(dx,dy);
        const double height=std::abs(center.z-(double(s.position->z)+s.hull->minimum.z));
        if (!std::isfinite(distance) || !std::isfinite(height) || distance>256 || height>192)
            continue;
        const Candidate candidate{id,distance+height*2};
        std::size_t insert=candidateCount;
        for (std::size_t n=0;n<candidateCount;++n)
            if (candidate.score<candidates[n].score) { insert=n; break; }
        if (candidateCount< candidates.size()) ++candidateCount;
        else if (insert==candidates.size()) continue;
        if (insert==candidateCount-1 && candidateCount<=candidates.size())
            candidates[insert]=candidate;
        else {
            for (std::size_t n=candidateCount-1;n>insert;--n) candidates[n]=candidates[n-1];
            candidates[insert]=candidate;
        }
    }
    for (std::size_t n=0;n<candidateCount && result.links.size()<8;++n) {
        const auto targetVertex=navigation_.graph->find(candidates[n].id);
        if (!targetVertex) continue;
        const auto targetExtent=navigation_.graph->area(*targetVertex).extent;
        const auto targetFloor=nav::query::projectToArea(targetExtent,
            {s.position->x,s.position->y,s.position->z});
        const nav::model::NavVector3 landing{
            static_cast<float>(targetFloor.x),static_cast<float>(targetFloor.y),
            static_cast<float>(targetFloor.z-s.hull->minimum.z)};
        if(shortcutQueriesThisTick_>=8) break;
        nav::runtime::QueryRequest ground{{s.agent,s.actor,s.map,s.tick,generation,
            ++shortcutQueriesThisTick_},
            nav::runtime::QueryKind::GroundedArea,landing,landing,s.hull,18};
        const auto support=query(ground);
        if (support.error!=nav::runtime::QueryError::None || !support.ground ||
            !support.ground->floor || !support.ground->area ||
            *support.ground->area!=candidates[n].id) continue;
        const double sourceFloor=double(s.position->z)+s.hull->minimum.z;
        const double fall=sourceFloor-double(support.ground->floor->height);
        const double rise=-fall;
        nav::model::NavTraversalKind traversal=nav::model::NavTraversalKind::Walk;
        nav::enrichment::NavLinkDirection direction=nav::enrichment::NavLinkDirection::Forward;
        if (rise>18) {
            if (rise>44 || candidates[n].score>160) continue;
            traversal=nav::model::NavTraversalKind::Walk;
            direction=nav::enrichment::NavLinkDirection::Up;
        } else if (fall>18) {
            if (fall>192) continue;
            traversal=nav::model::NavTraversalKind::Walk;
            direction=nav::enrichment::NavLinkDirection::Down;
        }
        if(shortcutQueriesThisTick_>=8) break;
        nav::runtime::QueryRequest clear{{s.agent,s.actor,s.map,s.tick,generation,
            ++shortcutQueriesThisTick_},
            nav::runtime::QueryKind::SweptHull,*s.position,landing,s.hull,18};
        const auto passage=query(clear);
        const bool directClear=passage.error==nav::runtime::QueryError::None &&
            passage.hull && !passage.hull->startSolid && passage.hull->fraction==1;
        if (traversal==nav::model::NavTraversalKind::Walk && !directClear) continue;
        if (traversal==nav::model::NavTraversalKind::Drop && !directClear && fall<=128) continue;
        if (!std::isfinite(landing.x) || !std::isfinite(landing.y) || !std::isfinite(landing.z)) continue;
        const auto linkId=(std::uint64_t(currentId.value)<<32)|candidates[n].id.value;
        result.links.push_back({0x415354524153484FULL,generation,linkId,currentId,candidates[n].id,
            {s.position->x,s.position->y,s.position->z},
            {landing.x,landing.y,landing.z},traversal,direction,0});
    }
    return result;
}
bool NavConsole::runReplan(metamod::LifecycleCoordinator& owner) noexcept {
    if(current_->replan_.state()!=nav::runtime::ReplanState::Pending) return false;
    const auto s=snapshot(owner);
    if(!s.tick.isAfter(current_->motionTrace_.decision.tick)) return true;
    const auto policy=current_->replan_.consume(current_->motionTrace_.decision.binding,s.tick,current_->navigationTimeUs_);
    printReplan();
    if(!policy || !current_->session_ || !current_->session_->executable()) {
        if(current_->recoveryReplan_) {
            current_->motionTrace_.decision.recovery=current_->recovery_.abort(nav::local::StuckCause::Unknown);
            current_->motionTrace_.decision.reason=nav::local::WalkReason::Stuck;
            current_->motionTrace_.decision.terminalEvent=false; // The retiring Walk already emitted its terminal event.
            recordMotion(MotionEvent::Decision);
        }
        failExecution(nav::runtime::ExecutionFailure::Motion);
        return true;
    }
    current_->recovery_.replanned();
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
