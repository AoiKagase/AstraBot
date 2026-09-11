// SPDX-License-Identifier: MPL-2.0
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include "adapter/cstrike/nav/console.hpp"
#include "adapter/metamod/console_debug.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include "adapter/cstrike/nav/world_queries.hpp"
#include "adapter/cstrike/nav/jump_motion.hpp"
#ifdef snprintf
#undef snprintf
#endif
namespace astrabot::adapter::cstrike {
namespace {
constexpr nav::local::WalkLimits walkLimits{{21,4,48,16,18,18,64,4,18,0.7},160,1,1,3,1000000,3000000,12,8,40,25,{120000,400000,3000000},
    {{{-16,-16,-36},{16,16,36}},{{-16,-16,-18},{16,16,18}},1000000}};
std::uint64_t add(std::uint64_t a,std::uint64_t b) noexcept {
    const auto maximum=(std::numeric_limits<std::uint64_t>::max)();
    return b>maximum-a ? maximum:a+b;
}
bool ready(const nav::runtime::MovementSnapshot& s,bool airborne=false) noexcept {
    return s.kind==nav::runtime::ActorKind::ManagedBot && s.connected==true && s.alive==true &&
        s.joined==true && (s.grounded==true || (airborne && s.grounded==false)) && s.position && s.position->isFinite() &&
        s.view && s.view->isFinite() && s.hull && s.hull->minimum.isFinite() && s.hull->maximum.isFinite() &&
        s.speedLimit && std::isfinite(*s.speedLimit) && *s.speedLimit>=0;
}
bool segmentAllows(nav::model::NavVector3 start,nav::model::NavVector3 end,
                   nav::model::NavVector3 position,double travel) noexcept {
    const double dx=double(end.x)-start.x, dy=double(end.y)-start.y;
    const double length=std::hypot(dx,dy);
    if(length<=0) return false;
    const double px=double(position.x)-start.x, py=double(position.y)-start.y;
    const double along=(px*dx+py*dy)/length;
    const double lateral=std::abs(px*dy-py*dx)/length;
    const double expectedZ=start.z+(double(end.z)-start.z)*(along/length);
    return along>=-0.01 && along<=length && lateral<=0.5 &&
        std::abs(double(position.z)-expectedZ)<=walkLimits.probe.supportTolerance &&
        along+travel<=length+0.001;
}
const char* walkState(nav::local::WalkState state) noexcept {
    switch(state) {
    case nav::local::WalkState::Running:return "Running";
    case nav::local::WalkState::Arrived:return "Arrived";
    case nav::local::WalkState::Failed:return "Failed";
    case nav::local::WalkState::Aborted:return "Aborted";
    }
    return "Unknown";
}
}
void NavConsole::recordMotion(MotionEvent event,MotionReason reason) noexcept {
    if(event==MotionEvent::Rejected) current_->recovery_.pause(current_->motionTrace_.decision.binding);
    if(event==MotionEvent::Decision) current_->motionTrace_.selectedEdge.reset();
    if(current_->session_ && current_->session_->trace().route && current_->session_->trace().routeGeneration==current_->motionTrace_.decision.binding.routeGeneration &&
       current_->motionTrace_.decision.binding.step<current_->session_->trace().route->steps.size())
        current_->motionTrace_.selectedEdge=current_->session_->trace().route->steps[current_->motionTrace_.decision.binding.step].edge;
    if(current_->motionTrace_.portalReason==nav::corridor::PortalFailureReason::None)
        current_->motionTrace_.corridorTransition=current_->motionTrace_.decision.binding.step;
    if(current_->walk_) {
        if(const auto* transition=current_->walk_->activeTransition()) {
            current_->motionTrace_.sourceFit=transition->sourceFit;
            current_->motionTrace_.targetFit=transition->targetFit;
            current_->motionTrace_.sourceExtentWidth=double(transition->sourceExtent.southEast.x)-transition->sourceExtent.northWest.x;
            current_->motionTrace_.sourceExtentHeight=double(transition->sourceExtent.southEast.y)-transition->sourceExtent.northWest.y;
            current_->motionTrace_.targetExtentWidth=double(transition->targetExtent.southEast.x)-transition->targetExtent.northWest.x;
            current_->motionTrace_.targetExtentHeight=double(transition->targetExtent.southEast.y)-transition->targetExtent.northWest.y;
        } else if(current_->motionTrace_.portalReason==nav::corridor::PortalFailureReason::None) {
            current_->motionTrace_.sourceFit=nav::corridor::AreaFit::HullSafe;
            current_->motionTrace_.targetFit=nav::corridor::AreaFit::HullSafe;
        }
    }
    current_->motionTrace_.event=event; current_->motionTrace_.reason=reason;
    current_->motionSequence_=add(current_->motionSequence_,1); current_->motionTrace_.sequence=current_->motionSequence_;
    current_->motionHistory_[current_->motionNext_]=current_->motionTrace_; current_->motionNext_=(current_->motionNext_+1)%motionHistoryLimit;
    current_->motionCount_=(std::min)(current_->motionCount_+1,motionHistoryLimit);
    if(event==MotionEvent::Rejected || event==MotionEvent::Cancelled ||
       (event==MotionEvent::Decision && current_->motionTrace_.decision.terminalEvent)) printMotion();
}
void NavConsole::printMotion() noexcept {
    if(!current_->motionTrace_.decision.binding.routeGeneration) return;
    const auto& d=current_->motionTrace_.decision;
    if(d.recovery.cause!=nav::local::StuckCause::None) {
        const auto& r=d.recovery;
        char recovery[512]{};
        std::snprintf(recovery,sizeof(recovery),"recovery state=%u cause=%u symptom=%u attempts=%u commanded_us=%llu deadline_us=%llu displacement=%.6g projected=%.6g travel=%.6g windows_us=(500000,1000000) stage_us=250000",
            unsigned(r.state),unsigned(r.cause),unsigned(r.symptom),r.attempts,
            static_cast<unsigned long long>(r.commandedUs),static_cast<unsigned long long>(r.deadlineUs),r.displacement,r.projected,r.travel);
        line(recovery);
    }
    const auto target=d.target ? d.target->origin:nav::model::NavVector3{};
    char text[1536]{};
    std::snprintf(text,sizeof(text),
        "walk actor=%u:%u map=%u route=%llu step=%zu tick=%llu state=%s reason=%u probe=%u event=%u motion_reason=%u corridor=%u portal_reason=%u transition=%zu micro_area=%u extent=(%.6g,%.6g)->(%.6g,%.6g) hull=(%.6g,%.6g) transport=%u command_tick=%llu dispatch_tick=%llu age_us=%llu speed=%.6g direction=(%.6g,%.6g) target_present=%u target=(%.6g,%.6g,%.6g) support=%u queries=%u samples=%u step_probes=%u queued=%llu dispatched=%llu rejected=%llu missed=%llu history=%zu omitted=%llu edge=%u:%u command_forward=%.6g command_side=%.6g command_msec=%u door=%llu door_state=%u door_reason=%u use_checks=%llu contact_pulse=%u contact_guards=%llu clearance=(%.6g,%.6g) narrow=%u avoiding=%u lateral=%.6g",
        unsigned(d.binding.actor.slot),unsigned(d.binding.actor.generation.value),unsigned(d.binding.map.value),
        static_cast<unsigned long long>(d.binding.routeGeneration),d.binding.step,static_cast<unsigned long long>(d.tick.value),
        walkState(d.state),unsigned(d.reason),unsigned(d.probeReason),unsigned(current_->motionTrace_.event),unsigned(current_->motionTrace_.reason),
        unsigned(current_->motionTrace_.corridorError),unsigned(current_->motionTrace_.portalReason),
        current_->motionTrace_.corridorTransition,
        unsigned(current_->motionTrace_.sourceFit==nav::corridor::AreaFit::MicroTransit ||
                 current_->motionTrace_.targetFit==nav::corridor::AreaFit::MicroTransit),
        current_->motionTrace_.sourceExtentWidth,current_->motionTrace_.sourceExtentHeight,
        current_->motionTrace_.targetExtentWidth,current_->motionTrace_.targetExtentHeight,
        current_->motionTrace_.hullWidth,current_->motionTrace_.hullHeight,
        unsigned(current_->motionTrace_.transportError),
        static_cast<unsigned long long>(current_->motionTrace_.commandTick.value),static_cast<unsigned long long>(current_->motionTrace_.dispatchTick.value),
        static_cast<unsigned long long>(current_->motionTrace_.intentAgeUs),d.intent.speed,d.intent.direction.x,d.intent.direction.y,
        unsigned(d.target.has_value()),double(target.x),double(target.y),double(target.z),
        d.support ? unsigned(d.support->area.value):0U,d.queries,d.samples,d.steps,
        static_cast<unsigned long long>(current_->motionTrace_.queued),static_cast<unsigned long long>(current_->motionTrace_.dispatched),
        static_cast<unsigned long long>(current_->motionTrace_.rejected),static_cast<unsigned long long>(current_->motionTrace_.missedDecisions),
        current_->motionCount_,static_cast<unsigned long long>(current_->motionSequence_-current_->motionCount_),
        current_->motionTrace_.selectedEdge ? unsigned(current_->motionTrace_.selectedEdge->source.value):0U,
        current_->motionTrace_.selectedEdge ? unsigned(current_->motionTrace_.selectedEdge->target.value):0U,
        double(current_->motionTrace_.command.movement.forward),double(current_->motionTrace_.command.movement.side),unsigned(current_->motionTrace_.command.msec),
        static_cast<unsigned long long>(d.doorId),d.doorState ? unsigned(*d.doorState):0U,unsigned(d.doorReason),
        static_cast<unsigned long long>(current_->motionTrace_.useGuardChecks),unsigned(d.contact.has_value()),
        static_cast<unsigned long long>(current_->motionTrace_.contactGuardQueries),d.leftClearance,d.rightClearance,
        unsigned(d.narrow),unsigned(d.avoiding),d.intent.lateralCorrection);
    const auto used=std::strlen(text);
    std::snprintf(text+used,sizeof(text)-used," blocker=%llu blocker_kind=%u blocker_action=%u blocker_reason=%u blocker_player=%u:%u",
        static_cast<unsigned long long>(d.blocker ? d.blocker->id:0),
        d.blocker ? unsigned(d.blocker->kind):0U,unsigned(d.blockerAction),unsigned(d.blockerReason),
        d.blocker && d.blocker->player ? unsigned(d.blocker->player->slot):0U,
        d.blocker && d.blocker->player ? unsigned(d.blocker->player->generation.value):0U);
    line(text);
    if(d.avoiding || d.avoidanceReason!=nav::local::AvoidanceReason::None) {
        char avoidance[512]{};
        const auto& candidate=d.avoidanceCandidate;
        std::snprintf(avoidance,sizeof(avoidance),
            "nav avoidance actor=%u:%u route=%llu step=%zu side=%d reason=%u distance=%.6g candidate_present=%u candidate=(%.6g,%.6g,%.6g) left=%.6g right=%.6g forward_probe=%u queries=%u budget=%u",
            unsigned(d.binding.actor.slot),unsigned(d.binding.actor.generation.value),
            static_cast<unsigned long long>(d.binding.routeGeneration),d.binding.step,
            d.avoidanceSide,unsigned(d.avoidanceReason),d.avoidanceDistance,
            unsigned(candidate.has_value()),candidate ? candidate->x:0.0F,candidate ? candidate->y:0.0F,candidate ? candidate->z:0.0F,
            d.leftClearance,d.rightClearance,unsigned(d.probeReason),d.queries,
            d.queries<walkLimits.probe.maxQueries);
        line(avoidance);
    }
    char timing[384]{};
    std::snprintf(timing,sizeof(timing),
        "motion_timing actor=%u:%u elapsed_us=%llu frame_delta_us=%llu intent_speed=%.6g speed_limit=%.6g pending_remaining_us=%llu stale_reason=%u diagnostic_suppressed=%llu",
        unsigned(d.binding.actor.slot),unsigned(d.binding.actor.generation.value),
        static_cast<unsigned long long>(current_->motionTrace_.elapsedUs),
        static_cast<unsigned long long>(current_->motionTrace_.frameDeltaUs),
        current_->motionTrace_.intentSpeed,current_->motionTrace_.speedLimit,
        static_cast<unsigned long long>(current_->motionTrace_.pendingRemainingUs),
        unsigned(current_->motionTrace_.reason),
        static_cast<unsigned long long>(current_->diagnosticSuppressed));
    line(timing);
    if (current_->motionTrace_.dispatchPhysicalValid) {
        char dispatch[768]{};
        std::snprintf(
            dispatch, sizeof(dispatch),
            "movement_dispatch actor=%u:%u sequence=%llu command_tick=%llu dispatch_tick=%llu physical_valid=%u before_origin=%.3f,%.3f,%.3f after_origin=%.3f,%.3f,%.3f before_velocity=%.3f,%.3f,%.3f after_velocity=%.3f,%.3f,%.3f onground=%u->%u horizontal_displacement=%.3f no_progress=%u",
            unsigned(d.binding.actor.slot), unsigned(d.binding.actor.generation.value),
            static_cast<unsigned long long>(current_->motionTrace_.dispatchSequence),
            static_cast<unsigned long long>(current_->motionTrace_.commandTick.value),
            static_cast<unsigned long long>(current_->motionTrace_.dispatchTick.value),
            unsigned(current_->motionTrace_.dispatchPhysicalValid),
            current_->motionTrace_.dispatchBeforeOriginX,
            current_->motionTrace_.dispatchBeforeOriginY,
            current_->motionTrace_.dispatchBeforeOriginZ,
            current_->motionTrace_.dispatchAfterOriginX,
            current_->motionTrace_.dispatchAfterOriginY,
            current_->motionTrace_.dispatchAfterOriginZ,
            current_->motionTrace_.dispatchBeforeVelocityX,
            current_->motionTrace_.dispatchBeforeVelocityY,
            current_->motionTrace_.dispatchBeforeVelocityZ,
            current_->motionTrace_.dispatchAfterVelocityX,
            current_->motionTrace_.dispatchAfterVelocityY,
            current_->motionTrace_.dispatchAfterVelocityZ,
            unsigned(current_->motionTrace_.dispatchBeforeOnGround),
            unsigned(current_->motionTrace_.dispatchAfterOnGround),
            current_->motionTrace_.dispatchHorizontalDisplacement,
            unsigned(current_->motionTrace_.dispatchNoProgress));
        line(dispatch);
    }
    if(d.posture || d.constraintReason!=nav::local::ConstraintReason::None) {
        char posture[128]{};
        std::snprintf(posture,sizeof(posture),"walk posture_present=%u posture=%u posture_reason=%u constraint_reason=%u duck=%u",
            unsigned(d.posture.has_value()),d.posture ? unsigned(*d.posture):0U,unsigned(d.postureReason),unsigned(d.constraintReason),unsigned(d.intent.duck));
        line(posture);
    }
    if(d.jumpState) {
        char jump[384]{};
        std::snprintf(jump,sizeof(jump),"jump state=%u reason=%u probe=%u geometry=%u press_tick=%llu gravity=%.6g impulse=%.6g guard_queries=%llu profile=standard-cs",
            unsigned(*d.jumpState),unsigned(d.jumpReason),unsigned(d.jumpProbeReason),unsigned(d.jumpGeometryReason),
            static_cast<unsigned long long>(d.jumpPressTick.value),d.jumpPhysics ? d.jumpPhysics->gravity:0,
            d.jumpPhysics ? d.jumpPhysics->verticalImpulse:0,static_cast<unsigned long long>(current_->motionTrace_.jumpGuardQueries));
        line(jump);
    }
    if(d.dropState) {
        char drop[320]{};
        std::snprintf(drop,sizeof(drop),"drop actor=%u:%u route=%llu step=%zu state=%u reason=%u fall=%.6g gap=%.6g predicted_damage=%.6g",
            unsigned(d.binding.actor.slot),unsigned(d.binding.actor.generation.value),
            static_cast<unsigned long long>(d.binding.routeGeneration),d.binding.step,
            unsigned(*d.dropState),unsigned(d.dropReason),d.dropPlan ? d.dropPlan->fall:0,
            d.dropPlan ? d.dropPlan->gap:0,
            d.dropPlan ? d.dropPlan->predictedDamage:0);
        line(drop);
    }
    if(d.ladderState) {
        char ladder[384]{};
        const auto* link=d.ladderPlan ? &d.ladderPlan->link:nullptr;
        std::snprintf(ladder,sizeof(ladder),"ladder state=%u reason=%u source=%llu generation=%llu link=%llu press_tick=%llu bind=%u frame=%u guard_queries=%llu profile=standard-cs",
            unsigned(*d.ladderState),unsigned(d.ladderReason),static_cast<unsigned long long>(link ? link->sourceId:0),
            static_cast<unsigned long long>(link ? link->generation:0),static_cast<unsigned long long>(link ? link->linkId:0),
            static_cast<unsigned long long>(d.ladderPressTick.value),unsigned(current_->motionTrace_.ladderBindingReason),
            unsigned(current_->motionTrace_.ladderFrameReason),static_cast<unsigned long long>(current_->motionTrace_.ladderGuardQueries));
        line(ladder);
    }
}
void NavConsole::clearPending() noexcept {
    if(current_->pendingMotion_ && movement_)
        (void)movement_->cancel(current_->pendingMotion_->binding.actor,current_->pendingMotion_->binding.map,current_->pendingMotion_->tick);
    current_->pendingMotion_.reset();
}
void NavConsole::stopMotion() noexcept {
    current_->neutralDuck_=current_->motionTrace_.decision.intent.duck==core::ActionRequest::Hold;
    if(current_->walk_ || current_->pendingMotion_) current_->neutralBinding_=current_->motionTrace_.decision.binding;
    clearPending();
    if(current_->walk_ && current_->walk_->state()==nav::local::WalkState::Running) {
        current_->motionTrace_.decision=current_->walk_->abort(); recordMotion(MotionEvent::Cancelled,MotionReason::Cancelled);
        current_->neutralDuck_=current_->motionTrace_.decision.intent.duck==core::ActionRequest::Hold;
    }
    current_->walk_.reset(); current_->pump_.reset(); current_->segment_.reset(); current_->intentWallAgeUs_=0;
}
void NavConsole::failExecution(nav::runtime::ExecutionFailure reason,bool structural) noexcept {
    // Local avoidance exhaustion is transient. It must enter the bounded
    // recovery/edge-cooldown path and must never become a NAV-generation
    // permanent exclusion merely because the forward probe was blocked.
    if(current_ && current_->motionTrace_.decision.avoidanceReason!=nav::local::AvoidanceReason::None)
        structural=false;
    const auto goal=current_->session_ ? current_->session_->trace().goal : nav::model::NavAreaId{};
    auto edge=current_->motionTrace_.failedEdge;
    if(!edge) edge=current_->motionTrace_.selectedEdge;
    current_->execution_.fail(goal,reason,current_->navigationTimeUs_,edge,structural);
    current_->motionTrace_.failedEdge=edge;
    if(edge) {
        current_->motionTrace_.edgeCooldownRemainingUs=
            current_->execution_.edgeCooldownRemaining(*edge,current_->navigationTimeUs_);
        current_->motionTrace_.edgeCooling=current_->motionTrace_.edgeCooldownRemainingUs>0;
    }
    current_->motionTrace_.failedEdge=edge;
    const auto& binding=current_->motionTrace_.decision.binding;
    char execution[512]{};
    std::snprintf(execution,sizeof(execution),
        "execution actor=%u:%u agent=%llu map=%u route=%llu tick=%llu state=Failed reason=%u failed_edge=%u:%u failed_transition=%zu structural=%u retry_at_us=%llu",
        unsigned(binding.actor.slot),unsigned(binding.actor.generation.value),
        static_cast<unsigned long long>(binding.agent.value),unsigned(binding.map.value),
        static_cast<unsigned long long>(binding.routeGeneration),
        static_cast<unsigned long long>(current_->motionTrace_.decision.tick.value),unsigned(reason),
        edge ? unsigned(edge->source.value):0U,edge ? unsigned(edge->target.value):0U,
        current_->motionTrace_.corridorTransition,unsigned(structural),
        static_cast<unsigned long long>(current_->execution_.retryAtUs));
    line(execution);
    clearPending();
    current_->neutralBinding_=current_->motionTrace_.decision.binding;
    current_->neutralDuck_=current_->motionTrace_.decision.intent.duck==core::ActionRequest::Hold;
    // Preserve the original terminal trace, rather than replacing it with Cancelled.
    current_->walk_.reset(); current_->pump_.reset(); current_->segment_.reset();
    current_->intentWallAgeUs_=0;
}
void NavConsole::startMotion(const nav::runtime::MovementSnapshot& s) noexcept {
    current_->motionTrace_={}; current_->motionNext_=current_->motionCount_=0; current_->motionSequence_=0; current_->requestTick_=s.tick;
    if(!current_->session_ || !current_->session_->executable()) return;
    const auto& route=current_->session_->trace();
    current_->motionTrace_.decision.binding={s.agent,s.actor,s.map,route.routeGeneration,0};
    if(current_->neutralBinding_) current_->neutralBinding_=current_->motionTrace_.decision.binding;
    current_->motionTrace_.decision.tick=s.tick;
    const auto fail=[&](MotionReason reason) {
        current_->motionTrace_.decision.state=nav::local::WalkState::Failed;
        current_->motionTrace_.decision.terminalEvent=true;
        recordMotion(MotionEvent::Decision,reason);
        failExecution(reason==MotionReason::MissingObservation ?
            nav::runtime::ExecutionFailure::Observation : nav::runtime::ExecutionFailure::Corridor,
            reason==MotionReason::InvalidCorridor &&
            (current_->motionTrace_.portalReason==nav::corridor::PortalFailureReason::BoundaryMismatch ||
             current_->motionTrace_.portalReason==nav::corridor::PortalFailureReason::NoPortalSpan ||
             current_->motionTrace_.portalReason==nav::corridor::PortalFailureReason::UnsupportedTraversal));
    };
    const auto activeGraph=current_->session_ ? current_->session_->graph():navigation_.graph;
    if(!ready(s) || !movement_ || !activeGraph || !index_) { fail(MotionReason::MissingObservation); return; }
    const auto& hull=*s.hull;
    if(hull.minimum.x>=hull.maximum.x || hull.minimum.y>=hull.maximum.y || hull.minimum.z>=hull.maximum.z) {
        fail(MotionReason::MissingObservation); return;
    }
    const nav::corridor::HullClearance clearance{
        (std::max)(std::abs(double(hull.minimum.x)),std::abs(double(hull.maximum.x))),
        (std::max)(std::abs(double(hull.minimum.y)),std::abs(double(hull.maximum.y)))};
    current_->motionTrace_.hullWidth=2*clearance.halfX;
    current_->motionTrace_.hullHeight=2*clearance.halfY;
    const auto corridor=nav::corridor::Corridor::build(*activeGraph,*route.route,clearance,
        {100000,256U*1024U*1024U,1000000},nav::corridor::PortalPolicy::AllowMicroTransit);
    if(!corridor) {
        current_->motionTrace_.corridorError=corridor.error;
        current_->motionTrace_.portalReason=corridor.portalReason;
        current_->motionTrace_.corridorTransition=corridor.transition;
        if(corridor.transition<route.route->steps.size())
            current_->motionTrace_.failedEdge=route.route->steps[corridor.transition].edge;
        if(corridor.transition<route.route->steps.size()) {
            const auto& failedEdge=route.route->steps[corridor.transition].edge;
            const auto from=activeGraph->find(failedEdge.source);
            const auto to=activeGraph->find(failedEdge.target);
            if(from && to) {
            const auto& source=activeGraph->area(*from).extent;
            const auto& target=activeGraph->area(*to).extent;
                current_->motionTrace_.sourceExtentWidth=double(source.southEast.x)-source.northWest.x;
                current_->motionTrace_.sourceExtentHeight=double(source.southEast.y)-source.northWest.y;
                current_->motionTrace_.targetExtentWidth=double(target.southEast.x)-target.northWest.x;
                current_->motionTrace_.targetExtentHeight=double(target.southEast.y)-target.northWest.y;
            }
        }
        fail(corridor.error==nav::corridor::Error::InvalidGoalArea ?
            MotionReason::InvalidGoal : MotionReason::InvalidCorridor); return;
    }
    current_->motionTrace_.portalReason=nav::corridor::PortalFailureReason::None;
    current_->motionTrace_.corridorTransition=0;
    current_->motionTrace_.sourceFit=nav::corridor::AreaFit::HullSafe;
    current_->motionTrace_.targetFit=nav::corridor::AreaFit::HullSafe;
    const auto vertex=activeGraph->find(route.goal);
    if(!vertex) { fail(MotionReason::InvalidGoal); return; }
    const auto& e=activeGraph->area(*vertex).extent;
    const double lowX=double(e.northWest.x)-hull.minimum.x+1, highX=double(e.southEast.x)-hull.maximum.x-1;
    const double lowY=double(e.northWest.y)-hull.minimum.y+1, highY=double(e.southEast.y)-hull.maximum.y-1;
    if(lowX>highX || lowY>highY) { fail(MotionReason::InvalidGoal); return; }
    // An area-id goal chooses a hull-safe XY projection of the current point.
    // Intermediate steering still follows selected portals, never area centers.
    const nav::model::NavVector3 xy{static_cast<float>(std::clamp(double(s.position->x),lowX,highX)),
        static_cast<float>(std::clamp(double(s.position->y),lowY,highY)),0};
    const auto floor=nav::query::projectToArea(e,xy);
    const nav::model::NavVector3 goal{xy.x,xy.y,static_cast<float>(floor.z)};
    auto profile=walkLimits; profile.jump=jumpLimits; profile.drop=nav::local::DropLimits{}; profile.ladder=nav::local::LadderLimits{};
    current_->walk_.emplace(current_->motionTrace_.decision.binding,corridor.value,goal,profile);
    current_->execution_.state=nav::runtime::ExecutionState::Running;
    (void)current_->recovery_.bindRoute(current_->motionTrace_.decision.binding);
    current_->pump_.emplace(current_->motionTrace_.decision.binding); current_->intentWallAgeUs_=0; current_->neutralBinding_.reset();
}
MotionReason NavConsole::guardDrop(metamod::LifecycleCoordinator& owner,
    const nav::runtime::MovementSnapshot& s,const PendingMotion& pending) noexcept {
    using namespace nav::runtime;
    if(!pending.drop || !s.position || !s.velocity || !s.hull || !s.grounded ||
       !index_ || !current_->walk_ || current_->walk_->step()!=pending.binding.step)
        return MotionReason::DropChanged;
    const auto& plan=*pending.drop;
    const auto physics=standardJumpPhysics(engine_,owner.entityFor(s.actor),pending.binding,s.tick);
    if(!physics || physics->gravity!=pending.dropGravity || !s.velocity->isFinite() ||
       plan.fall<=0 || plan.fall>128 || plan.gap<0 || plan.gap>32 ||
       s.velocity->z < -580 || s.ducked!=false ||
       (pending.command.buttons & (static_cast<core::ButtonMask>(core::Button::Jump) |
        static_cast<core::ButtonMask>(core::Button::Duck)))) return MotionReason::DropChanged;
    const auto delta=movement_->frameDeltaUs();
    if(!delta || delta>120000 || current_->guardQueries_>18) return MotionReason::StaleCommand;
    const double dt=double(std::clamp(delta/1000+(delta%1000>=500 ? 1U:0U),
        std::uint64_t{1},std::uint64_t{255}))/1000;
    const double yaw=pending.command.view.yaw*3.14159265358979323846/180;
    const double vx=pending.command.movement.forward*std::cos(yaw)+pending.command.movement.side*std::sin(yaw);
    const double vy=pending.command.movement.forward*std::sin(yaw)-pending.command.movement.side*std::cos(yaw);
    if(std::hypot(vx,vy)>100.001 || pending.command.movement.up!=0 ||
       !plan.landing.isFinite() || !plan.takeoff.isFinite()) return MotionReason::DropChanged;
    const auto ask=[&](QueryKind kind,nav::model::NavVector3 a,nav::model::NavVector3 b,
                       std::optional<HullDimensions> hull)->std::optional<WorldQueryResult> {
        if(current_->guardQueries_>=21) return {};
        const QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,
            ++current_->guardQueries_},kind,a,b,hull};
        WorldQueryResult r;
        try { r=query(q); } catch(...) { return {}; }
        if(!(r.stamp==q.stamp) || r.kind!=q.kind || r.error!=QueryError::None) return {};
        return r;
    };
    // Revalidate the destination floor on every dispatch, not just planning.
    const float floorZ=plan.landing.z+s.hull->minimum.z;
    const nav::model::NavVector3 top{plan.landing.x,plan.landing.y,floorZ+18};
    const nav::model::NavVector3 bottom{plan.landing.x,plan.landing.y,floorZ-18};
    const auto floor=ask(QueryKind::Floor,top,bottom,{});
    if(!floor || !floor->floor || !floor->floor->supported ||
       !std::isfinite(floor->floor->height) || !floor->floor->normal.isFinite() ||
       floor->floor->normal.z<0.7f || std::abs(floor->floor->height-floorZ)>4)
        return MotionReason::DropChanged;
    const auto area=index_->containing({plan.landing.x,plan.landing.y,floor->floor->height},18);
    if(!area || !*area.value || (**area.value).areaId!=plan.target) return MotionReason::DropChanged;
    const double vertical=s.grounded==true ? 0.0 : double(s.velocity->z)*dt-0.5*physics->gravity*dt*dt;
    // Air movement uses observed velocity; ground movement uses the submitted wish.
    const nav::model::NavVector3 end{
        static_cast<float>(s.position->x+(s.grounded==true ? vx:s.velocity->x)*dt),
        static_cast<float>(s.position->y+(s.grounded==true ? vy:s.velocity->y)*dt),
        static_cast<float>(s.position->z+vertical)};
    if(!end.isFinite()) return MotionReason::DropChanged;
    const auto sweep=ask(QueryKind::SweptHull,*s.position,end,s.hull);
    if(!sweep || !sweep->hull || sweep->hull->startSolid || !sweep->hull->end.isFinite() ||
       !sweep->hull->normal.isFinite() || !std::isfinite(sweep->hull->fraction) ||
       sweep->hull->fraction<0 || sweep->hull->fraction>1) return MotionReason::DropChanged;
    const auto& hit=*sweep->hull;
    const auto closeCoordinate=[](double a,double b) { return std::abs(a-b)<=0.01; };
    if(!closeCoordinate(hit.end.x,s.position->x+(end.x-s.position->x)*hit.fraction) ||
       !closeCoordinate(hit.end.y,s.position->y+(end.y-s.position->y)*hit.fraction) ||
       !closeCoordinate(hit.end.z,s.position->z+(end.z-s.position->z)*hit.fraction)) return MotionReason::DropChanged;
    if(hit.fraction<1) {
        if(vertical>=0 || hit.normal.z<0.7f || !navigation_.graph) return MotionReason::DropChanged;
        const auto vertex=navigation_.graph->find(plan.target);
        if(!vertex) return MotionReason::DropChanged;
        const auto& e=navigation_.graph->area(*vertex).extent;
        if(hit.end.x+s.hull->minimum.x<e.northWest.x || hit.end.x+s.hull->maximum.x>e.southEast.x ||
           hit.end.y+s.hull->minimum.y<e.northWest.y || hit.end.y+s.hull->maximum.y>e.southEast.y)
            return MotionReason::DropChanged;
        const auto contact=ask(QueryKind::GroundedArea,hit.end,hit.end,s.hull);
        if(!contact || !contact->ground || contact->ground->area!=plan.target || !contact->ground->floor ||
           !contact->ground->floor->supported || !contact->ground->floor->normal.isFinite() ||
           contact->ground->floor->normal.z<0.7f || !std::isfinite(contact->ground->floor->height) ||
           std::abs(hit.end.z+s.hull->minimum.z-contact->ground->floor->height)>4)
            return MotionReason::DropChanged;
    }
    return MotionReason::None;
}
void NavConsole::beforeDispatch(metamod::LifecycleCoordinator& owner) noexcept {
    observe(owner);
    if(current_->guardTick_!=owner.registry().currentTick()) { current_->guardTick_=owner.registry().currentTick(); current_->guardQueries_=0; }
    if(!current_->pendingMotion_ || !movement_) return;
    const auto s=snapshot(owner);
    const auto& pending=*current_->pendingMotion_;
    current_->motionTrace_.dispatchOrigin=s.position;
    const auto duration=movement_->frameDeltaUs();
    current_->motionTrace_.dispatchDurationUs=1000*std::clamp(duration/1000+(duration%1000>=500 ? 1U:0U),std::uint64_t{1},std::uint64_t{255});
    current_->motionTrace_.elapsedUs=s.elapsedUs;
    current_->motionTrace_.frameDeltaUs=duration;
    current_->motionTrace_.pendingRemainingUs=pending.remainingFreshUs;
    current_->motionTrace_.intentSpeed=std::hypot(
        double(current_->motionTrace_.decision.intent.direction.x),
        double(current_->motionTrace_.decision.intent.direction.y)) *
        current_->motionTrace_.decision.intent.speed;
    current_->motionTrace_.speedLimit=s.speedLimit ? double(*s.speedLimit) : 0.0;
    MotionReason reason=MotionReason::None;
    const auto elapsed=s.elapsedUs;
    const bool stationary=pending.command.movement==core::Movement{} && pending.command.buttons==0;
    if(!ready(s,pending.jump.has_value() || pending.drop.has_value() || pending.ladder.has_value() || stationary) || s.actor!=pending.binding.actor || s.agent!=pending.binding.agent || s.map!=pending.binding.map ||
       !s.tick.isAfter(pending.tick) || s.hull->minimum!=pending.observation.hull->minimum ||
       s.hull->maximum!=pending.observation.hull->maximum) reason=MotionReason::MissingObservation;
    else if(!s.elapsedUs || !movement_->frameDeltaUs() || elapsed>pending.remainingFreshUs)
        reason=MotionReason::StaleCommand;
    else if(current_->motionTrace_.decision.recovery.deadlineUs &&
        add(add(current_->navigationTimeUs_,elapsed),current_->motionTrace_.dispatchDurationUs)>current_->motionTrace_.decision.recovery.deadlineUs)
        reason=MotionReason::StaleCommand;
    else {
        const auto& m=pending.command.movement;
        const double speed=std::hypot(double(m.forward),double(m.side));
        const auto delta=movement_->frameDeltaUs();
        const auto msec=std::clamp(delta/1000+(delta%1000>=500 ? 1U:0U),std::uint64_t{1},std::uint64_t{255});
        if(speed>double(*s.speedLimit)+0.001 || (speed>0 && !pending.jump && !pending.drop && !pending.ladder && !pending.contact && (!pending.segment ||
           !segmentAllows(pending.segment->start,pending.segment->end,*s.position,speed*double(msec)/1000))))
            reason=MotionReason::Deviation;
    }
    if(reason!=MotionReason::None) {
        const auto binding=pending.binding;
        const auto edge=current_->motionTrace_.selectedEdge;
        if(pending.ladder) reportLadderTransport(current_->motionTrace_.decision,pending.tick,s.tick,false);
        if(current_->walk_ && (pending.command.buttons&static_cast<core::ButtonMask>(core::Button::Jump)))
            (void)current_->walk_->reportJumpDispatch({pending.binding,pending.tick,s.tick,false});
        current_->motionTrace_.commandTick=pending.tick; current_->motionTrace_.dispatchTick=s.tick;
        current_->motionTrace_.failedEdge=edge;
        clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
        current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,reason);
        if (reason==MotionReason::StaleCommand && current_->walk_ &&
            current_->session_ && current_->session_->executable() &&
            binding.actor==current_->actor && binding.agent.isValid() &&
            binding.map==navigation_.map) {
            // A stale ticket is not evidence that the old command can still
            // be executed. Retire the primitive, including Jump/Drop/Ladder
            // state, and hand the active edge to bounded replan.
            current_->motionTrace_.decision=current_->walk_->abort();
            current_->motionTrace_.failedEdge=edge;
            current_->replan_={};
            const bool scheduled=edge
                ? current_->replan_.schedule(binding,*edge,s.tick,current_->navigationTimeUs_)
                : current_->replan_.scheduleRecovery(binding,s.tick,current_->navigationTimeUs_);
            current_->recoveryReplan_=scheduled;
            if (scheduled) {
                current_->execution_.state=nav::runtime::ExecutionState::Recovering;
                current_->motionTrace_.decision.terminalEvent=true;
                current_->motionTrace_.decision.reason=nav::local::WalkReason::RecoveryReplan;
                recordMotion(MotionEvent::Decision,reason);
            } else {
                stopMotion();
                failExecution(nav::runtime::ExecutionFailure::Transport);
            }
        } else {
            failExecution(reason==MotionReason::MissingObservation ?
                nav::runtime::ExecutionFailure::Observation : nav::runtime::ExecutionFailure::Transport);
        }
        return;
    }
    if(pending.ladder) {
        const auto queued=pending.tick;
        inRequest_=true;
        const auto guarded=guardLadder(owner,s,pending);
        inRequest_=false;
        if(deferredInvalidation_) { (void)applyDeferredInvalidation(); return; }
        if(guarded!=MotionReason::None) {
            reportLadderTransport(current_->motionTrace_.decision,queued,s.tick,false);
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,guarded);
        }
        return;
    }
    if(pending.drop) {
        const auto queued=pending.tick;
        inRequest_=true; queryingEntity_=owner.entityFor(current_->actor); queryingPlayers_=&owner.registry(); queryingOwner_=&owner;
        const auto guarded=guardDrop(owner,s,pending);
        inRequest_=false; queryingEntity_=nullptr; queryingPlayers_=nullptr; queryingOwner_=nullptr;
        if(deferredInvalidation_) { (void)applyDeferredInvalidation(); return; }
        if(guarded!=MotionReason::None) {
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1);
            recordMotion(MotionEvent::Rejected,guarded);
            failExecution(nav::runtime::ExecutionFailure::Transport);
        }
        return;
    }
    if(pending.jump) {
        const auto queued=pending.tick; const auto binding=pending.binding;
        const bool press=(pending.command.buttons&static_cast<core::ButtonMask>(core::Button::Jump))!=0;
        inRequest_=true; queryingEntity_=owner.entityFor(current_->actor); queryingPlayers_=&owner.registry(); queryingOwner_=&owner;
        const auto guarded=guardJump(owner,s,pending);
        inRequest_=false; queryingEntity_=nullptr; queryingPlayers_=nullptr; queryingOwner_=nullptr;
        if(deferredInvalidation_) { (void)applyDeferredInvalidation(); return; }
        if(guarded!=MotionReason::None) {
            if(press && current_->walk_) (void)current_->walk_->reportJumpDispatch({binding,queued,s.tick,false});
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,guarded);
        }
        return;
    }
    if(s.ducked==true && (pending.command.buttons&static_cast<core::ButtonMask>(core::Button::Duck))==0) {
        const auto queued=pending.tick;
        auto center=*s.position;
        center.z+=s.hull->minimum.z-walkLimits.crouch.standing.minimum.z;
        const nav::runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,++current_->guardQueries_},
            nav::runtime::QueryKind::Clearance,center,center,walkLimits.crouch.standing};
        inRequest_=true;
        const auto result=queryNavWorld(engine_,owner.entityFor(current_->actor),index_.get(),q,globals_ ? globals_->maxEntities:0);
        inRequest_=false;
        if(deferredInvalidation_) { (void)applyDeferredInvalidation(); return; }
        if(result.error!=nav::runtime::QueryError::None || !result.clearance || !result.clearance->clear) {
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1);
            recordMotion(MotionEvent::Rejected,MotionReason::PostureChanged); return;
        }
    }
    if(pending.contact) {
        const auto contact=*pending.contact; const auto queued=pending.tick;
        const auto command=pending.command;
        bool valid=false;
        if(!current_->guardQueries_) {
            ++current_->guardQueries_; current_->motionTrace_.contactGuardQueries=add(current_->motionTrace_.contactGuardQueries,1);
            nav::runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,1},
                nav::runtime::QueryKind::Door,*s.position,contact.end,s.hull,walkLimits.probe.navTolerance,contact.id};
            inRequest_=true;
            const auto result=queryNavWorld(engine_,owner.entityFor(current_->actor),index_.get(),q,
                globals_ ? globals_->maxEntities:0);
            inRequest_=false;
            if(deferredInvalidation_) {
                (void)applyDeferredInvalidation(); return;
            }
            if(result.error==nav::runtime::QueryError::None && result.door && result.door->id==contact.id &&
               result.door->canTouch && !result.door->open && result.hull && !result.hull->startSolid) {
                const double dx=double(contact.end.x)-s.position->x,dy=double(contact.end.y)-s.position->y;
                const auto& h=*result.hull;
                const double length=std::hypot(dx,dy),distance=std::hypot(double(h.end.x)-s.position->x,double(h.end.y)-s.position->y);
                const auto delta=movement_->frameDeltaUs();
                const auto msec=std::clamp(delta/1000+(delta%1000>=500 ? 1U:0U),std::uint64_t{1},std::uint64_t{255});
                const double speed=std::hypot(command.movement.forward,command.movement.side);
                const double travel=speed*double(msec)/1000;
                constexpr double radians=3.14159265358979323846/180;
                const double yaw=command.view.yaw*radians;
                const double vx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
                const double vy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw);
                valid=length>0 && length<=0.751 && distance<=0.125 && h.fraction>=0 && h.fraction<1 &&
                    travel>=distance+0.001 && travel<=0.75 && command.buttons==0 && command.movement.up==0 &&
                    std::abs(double(h.end.z)-s.position->z)<=0.1 && std::abs(double(contact.end.z)-s.position->z)<=0.1 &&
                    std::abs((double(h.end.x)-s.position->x)*dy-(double(h.end.y)-s.position->y)*dx)/length<=0.01 &&
                    std::abs(distance-length*h.fraction)<=0.02 && h.normal.x*dx/length+h.normal.y*dy/length<= -0.7 &&
                    std::abs(h.normal.z)<=0.2 && speed>0 && (vx*dx+vy*dy)/(speed*length)>0.999;
            }
        }
        if(!valid) {
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::DoorChanged);
        }
        return;
    }
    if((pending.command.buttons&static_cast<core::ButtonMask>(core::Button::Use))!=0) {
        // A queued press must still select the same generation at dispatch.
        // No NAV query ordinal is consumed: this bounded, trace-free selection
        // guard is counted separately and executes at most once per frame.
        const auto expected=pending.command.view;
        const auto queued=pending.tick;
        inRequest_=true;
        const auto view=doorUseView(engine_,owner.entityFor(current_->actor),
            current_->motionTrace_.decision.doorId,globals_ ? globals_->maxEntities:0);
        inRequest_=false;
        if(deferredInvalidation_) {
            (void)applyDeferredInvalidation(); return;
        }
        current_->motionTrace_.useGuardChecks=add(current_->motionTrace_.useGuardChecks,1);
        if(!view || std::abs(double(view->x)-expected.pitch)>0.01 ||
           std::abs(double(view->y)-expected.yaw)>0.01 || std::abs(double(view->z)-expected.roll)>0.01) {
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::DoorChanged);
        }
    }
}
void NavConsole::afterDispatch(const metamod::MovementResult& result,core::TickId tick,
    const std::optional<MotionTrace>& ticket) noexcept {
    if(!ticket) return;
    const auto& b=ticket->decision.binding;
    const auto& current=current_->motionTrace_.decision.binding;
    const bool sameRoute=b.agent==current.agent && b.actor==current.actor && b.map==current.map &&
        b.routeGeneration==current.routeGeneration;
    const auto saved=current_->motionTrace_;
    if(!sameRoute) current_->motionTrace_=*ticket; // reentrant goto must not receive old-route feedback
    if (movement_ != nullptr) {
        const auto& transport=movement_->frameDispatchTrace(b.actor);
        if (transport.physical.valid) {
            auto& trace=current_->motionTrace_;
            trace.dispatchPhysicalValid=true;
            trace.dispatchSequence=transport.callCount;
            trace.dispatchBeforeOriginX=transport.physical.beforeOriginX;
            trace.dispatchBeforeOriginY=transport.physical.beforeOriginY;
            trace.dispatchBeforeOriginZ=transport.physical.beforeOriginZ;
            trace.dispatchAfterOriginX=transport.physical.afterOriginX;
            trace.dispatchAfterOriginY=transport.physical.afterOriginY;
            trace.dispatchAfterOriginZ=transport.physical.afterOriginZ;
            trace.dispatchBeforeVelocityX=transport.physical.beforeVelocityX;
            trace.dispatchBeforeVelocityY=transport.physical.beforeVelocityY;
            trace.dispatchBeforeVelocityZ=transport.physical.beforeVelocityZ;
            trace.dispatchAfterVelocityX=transport.physical.afterVelocityX;
            trace.dispatchAfterVelocityY=transport.physical.afterVelocityY;
            trace.dispatchAfterVelocityZ=transport.physical.afterVelocityZ;
            trace.dispatchBeforeOnGround=transport.physical.beforeOnGround;
            trace.dispatchAfterOnGround=transport.physical.afterOnGround;
            const auto dx=static_cast<double>(trace.dispatchAfterOriginX)-trace.dispatchBeforeOriginX;
            const auto dy=static_cast<double>(trace.dispatchAfterOriginY)-trace.dispatchBeforeOriginY;
            trace.dispatchHorizontalDisplacement=std::sqrt(dx*dx+dy*dy);
            trace.dispatchNoProgress=(transport.forward!=0.0F ||
                transport.side!=0.0F || transport.up!=0.0F || transport.buttons!=0U) &&
                trace.dispatchHorizontalDisplacement<1.0;
                const double corridorProgress=dx*trace.decision.progressDirection.x+
                    dy*trace.decision.progressDirection.y;
                if(sameRoute && corridorProgress>=4.0 && trace.selectedEdge)
                    current_->execution_.clearEdgeCooldown(*trace.selectedEdge);
        }
    }
    if(sameRoute) reportLadderTransport(ticket->decision,ticket->commandTick,tick,result.dispatched());
    if(sameRoute && ticket->dispatchOrigin) {
        const auto& d=ticket->decision;
        auto expected=nav::local::ExpectedProgress::Pause;
    if(d.state==nav::local::WalkState::Running && !d.jumpState && !d.dropState && !d.ladderState && !d.doorState && !d.contact &&
           d.blockerAction!=nav::local::BlockerAction::Yield && d.intent.speed>0 &&
           (ticket->command.movement.forward!=0 || ticket->command.movement.side!=0))
            expected=(ticket->command.buttons&static_cast<core::ButtonMask>(core::Button::Duck)) ?
                nav::local::ExpectedProgress::Crouch:nav::local::ExpectedProgress::Walk;
        (void)current_->recovery_.report({b,ticket->commandTick,tick,*ticket->dispatchOrigin,d.progressDirection,
            ticket->dispatchDurationUs,expected,result.dispatched()});
    }
    if(sameRoute && current_->walk_ && (ticket->command.buttons&static_cast<core::ButtonMask>(core::Button::Jump)))
        (void)current_->walk_->reportJumpDispatch({b,ticket->commandTick,tick,result.dispatched()});
    if(current_->pendingMotion_ && current_->pendingMotion_->tick==ticket->commandTick && current_->pendingMotion_->binding.actor==b.actor &&
       current_->pendingMotion_->binding.map==b.map && current_->pendingMotion_->binding.routeGeneration==b.routeGeneration)
        current_->pendingMotion_.reset();
    current_->motionTrace_.commandTick=ticket->commandTick; current_->motionTrace_.dispatchTick=tick;
    current_->motionTrace_.transportError=result.error;
    if(result.dispatched()) {
        current_->motionTrace_.dispatched=add(current_->motionTrace_.dispatched,1); recordMotion(MotionEvent::Dispatched);
    } else {
        if(sameRoute && current_->pump_) current_->pump_->submissionRejected();
        current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::TransportRejected);
        if(sameRoute) failExecution(nav::runtime::ExecutionFailure::Transport);
    }
    if(!sameRoute) current_->motionTrace_=saved;
}
void NavConsole::moveFrame(metamod::LifecycleCoordinator& owner) noexcept {
    observe(owner);
    if(inRequest_ || !movement_) return;
    if(owner.registry().currentTick().isAfter(current_->navigationTimeTick_)) {
        current_->navigationTimeTick_=owner.registry().currentTick();
        current_->navigationTimeUs_=add(current_->navigationTimeUs_,movement_->frameDeltaUs());
    }
    if (metamod::ConsoleDebug::instance().navEnabled()) {
        if (current_->navigationTimeUs_ >= current_->diagnosticNextUs) {
            if (current_->motionTrace_.decision.binding.routeGeneration)
                printMotion();
            current_->diagnosticSuppressed=0;
            current_->diagnosticNextUs=add(current_->navigationTimeUs_,1'000'000U);
        } else {
            ++current_->diagnosticSuppressed;
        }
    }
    if(runReplan(owner)) return;
    if(current_->neutralBinding_) {
        const auto s=snapshot(owner);
        if(!ready(s,true) || s.actor!=current_->neutralBinding_->actor || s.agent!=current_->neutralBinding_->agent || s.map!=current_->neutralBinding_->map) {
            current_->neutralBinding_.reset(); return;
        }
        if(!s.elapsedUs || !s.tick.isAfter(current_->motionTrace_.commandTick)) return;
        current_->motionTrace_.decision.binding=*current_->neutralBinding_; current_->neutralBinding_.reset();
        core::MovementIntent neutral;
        if(current_->neutralDuck_ || s.ducked==true) neutral.duck=core::ActionRequest::Hold;
        submitMotion(s,owner,neutral,true,0); return;
    }
    if(inRequest_ || !current_->walk_ || !current_->pump_ || !movement_ || !current_->session_ || !current_->session_->executable() ||
       current_->walk_->state()!=nav::local::WalkState::Running) return;
    const auto s=snapshot(owner);
    // There is no valid simulation interval on the first armed frame. The
    // movement coordinator emits the neutral heartbeat; NAV waits for the
    // next frame before creating an aged action command.
    if (!s.elapsedUs || !movement_->frameDeltaUs()) return;
    if(!s.tick.isAfter(current_->requestTick_)) return; // Route request owns ordinal 1 on its tick.
    const auto schedule=current_->pump_->beginFrame(s);
    if(!schedule.accepted) { stopMotion(); return; }
    current_->intentWallAgeUs_=add(current_->intentWallAgeUs_,movement_->frameDeltaUs());
    if(schedule.decisionDue) {
        queryingEntity_=owner.entityFor(current_->actor); queryingPlayers_=&owner.registry(); queryingOwner_=&owner; inRequest_=true;
        const auto index=index_; // pins navigation across synchronous host reentry
        const auto reserved=current_->guardTick_==s.tick ? current_->guardQueries_:0;
        auto binding=current_->motionTrace_.decision.binding; binding.step=current_->walk_->step();
        const auto recovery=s.position ? current_->recovery_.observe(binding,s.tick,current_->navigationTimeUs_,*s.position):current_->recovery_.decision();
        nav::local::WalkDecision decision;
        if(recovery.state==nav::local::RecoveryState::Monitoring) {
            const auto physics=standardJumpPhysics(engine_,queryingEntity_,binding,s.tick);
            const auto ladder=observeLadder(owner,s,binding,reserved);
            decision=current_->walk_->update(s,*index,navigation_.map,*this,current_->pump_->timeUs(),reserved,physics,ladder);
            decision.recovery=recovery;
        } else decision=current_->walk_->recover(s,*index,navigation_.map,*this,recovery,reserved);
        inRequest_=false; queryingEntity_=nullptr; queryingPlayers_=nullptr; queryingOwner_=nullptr;
        if(deferredInvalidation_) {
            (void)applyDeferredInvalidation(); return;
        }
        if(decision.terminalEvent && decision.state==nav::local::WalkState::Failed &&
           recovery.state==nav::local::RecoveryState::Monitoring) {
            decision.recovery.state=nav::local::RecoveryState::Aborted;
            decision.recovery.cause=nav::local::observedStuckCause(decision);
            decision.recovery.terminalEvent=true;
        }
        current_->motionTrace_.decision=decision; current_->motionTrace_.missedDecisions=add(current_->motionTrace_.missedDecisions,schedule.missedDeadlines);
        if (decision.state == nav::local::WalkState::Arrived) {
            const auto wasRoam = current_->roamActive_;
            const auto goal = current_->session_
                ? current_->session_->trace().goal
                : nav::model::NavAreaId{};
            if (wasRoam) {
                current_->roamArrived_ = true;
                if (goal.isValid()) {
                    for (std::size_t i = (std::min)(current_->roamRecentGoalCount_,
                                                    current_->roamRecentGoals_.size());
                         i > 0; --i) {
                        if (i < current_->roamRecentGoals_.size())
                            current_->roamRecentGoals_[i] =
                                current_->roamRecentGoals_[i - 1U];
                    }
                    current_->roamRecentGoals_[0] = goal;
                    if (current_->roamRecentGoalCount_ <
                        current_->roamRecentGoals_.size())
                        ++current_->roamRecentGoalCount_;
                }
            }
            stopMotion();
            if (current_->session_) printUpdate(current_->session_->cancel());
            return;
        }
        current_->segment_=decision.target && s.position ? std::optional<Segment>{{*s.position,decision.target->origin}}:std::nullopt;
        current_->intentWallAgeUs_=0;
        if(recovery.measuredProgress && current_->recoveryReplan_) {
            current_->replan_={}; current_->recoveryReplan_=false;
        }
        if(decision.terminalEvent && decision.reason==nav::local::WalkReason::RecoveryReplan) {
            current_->recoveryReplan_=current_->replan_.scheduleRecovery(decision.binding,s.tick,current_->navigationTimeUs_);
            if(!current_->recoveryReplan_) {
                decision.recovery=current_->recovery_.abort(decision.recovery.cause);
                decision.reason=nav::local::WalkReason::Stuck;
                current_->motionTrace_.decision=decision;
            }
            printReplan();
        }
        recordMotion(MotionEvent::Decision);
        if(decision.terminalEvent && decision.reason==nav::local::WalkReason::DynamicBlocked &&
           decision.blockerAction==nav::local::BlockerAction::Replan &&
           decision.blockerReason==nav::local::BlockerReason::TimedOut && decision.blocker &&
           decision.blocker->id && current_->motionTrace_.selectedEdge) {
            const auto& blocker=*decision.blocker;
            const bool player=blocker.kind==nav::runtime::BlockerKind::Player ||
                blocker.kind==nav::runtime::BlockerKind::Teammate || blocker.kind==nav::runtime::BlockerKind::Enemy;
            if((player && blocker.player && blocker.player->isValid() && !blocker.player->sameSlot(s.actor)) ||
               (blocker.kind==nav::runtime::BlockerKind::Other && !blocker.player)) {
                (void)current_->replan_.schedule(decision.binding,*current_->motionTrace_.selectedEdge,s.tick,current_->navigationTimeUs_);
                printReplan();
            }
        }
        if(decision.terminalEvent && decision.state==nav::local::WalkState::Failed) {
            if(current_->replan_.state()==nav::runtime::ReplanState::Pending) {
                current_->execution_.state=nav::runtime::ExecutionState::Recovering;
                clearPending();
            } else {
                // A jump failure, or an unoccupied ground probe that reports
                // a hard hull block, is a repeatable physical failure of the
                // directed NAV edge. Exclude that edge for this NAV
                // generation; dynamic/player blockers remain on bounded
                // recovery instead of being misclassified here.
                const bool jumpStructural = decision.reason == nav::local::WalkReason::JumpFailed &&
                    (decision.jumpGeometryReason==nav::local::JumpGeometryReason::UnsupportedTransition ||
                     decision.jumpGeometryReason==nav::local::JumpGeometryReason::NoRoom ||
                     decision.jumpGeometryReason==nav::local::JumpGeometryReason::InvalidGeometry ||
                     decision.jumpGeometryReason==nav::local::JumpGeometryReason::HeightUnsupported ||
                     decision.jumpProbeReason==nav::local::JumpProbeReason::UnsupportedConstraints ||
                     decision.jumpProbeReason==nav::local::JumpProbeReason::CannotLand ||
                     decision.jumpProbeReason==nav::local::JumpProbeReason::WrongArea ||
                     decision.jumpReason==nav::local::JumpReason::WrongLanding);
                const bool structural =
                    decision.reason == nav::local::WalkReason::UnsupportedTraversal ||
                    (jumpStructural && !decision.blocker) ||
                    (decision.reason == nav::local::WalkReason::ProbeFailed &&
                     decision.probeReason == nav::local::ProbeReason::Blocked &&
                     !decision.blocker);
                failExecution(nav::runtime::ExecutionFailure::Motion,
                    structural);
            }
            return;
        }
        if(decision.state==nav::local::WalkState::Arrived)
            current_->execution_.state=nav::runtime::ExecutionState::Arrived;
        if(!current_->pump_->publish(decision.binding,s.tick,decision.intent)) return;
    }
    const auto output=current_->pump_->take();
    if(!output.emit || !ready(s,current_->motionTrace_.decision.jumpState.has_value() || current_->motionTrace_.decision.dropState.has_value() || current_->motionTrace_.decision.ladderState.has_value())) return;
    const auto age=(std::max)(output.intentAgeUs,current_->intentWallAgeUs_);
    if(age>nav::local::IntentPump::maxIntentAgeUs) { current_->pump_->stop(nav::local::PumpReason::StaleIntent); return; }
    submitMotion(s,owner,output.intent,output.firstFrame,age);
}
void NavConsole::submitMotion(const nav::runtime::MovementSnapshot& s,metamod::LifecycleCoordinator& owner,
    const core::MovementIntent& intent,bool firstFrame,std::uint64_t age) noexcept {
    const auto contact=firstFrame ? current_->motionTrace_.decision.contact:std::nullopt;
    auto effective=current_->motionTrace_.decision.contact && !firstFrame ? core::MovementIntent{}:intent;
    const auto& decision=current_->motionTrace_.decision;
    if(s.grounded==true && (decision.jumpState==nav::local::JumpState::Airborne || decision.jumpState==nav::local::JumpState::Recover)) {
        effective={}; effective.jump=core::ActionRequest::Release;
    }
    if(!firstFrame && effective.duck==core::ActionRequest::Release)
        effective.duck=s.ducked==true ? core::ActionRequest::Hold:core::ActionRequest::None;
    if(s.ducked==true && effective.duck==core::ActionRequest::None) effective.duck=core::ActionRequest::Hold;
    const auto command=core::Motor::command(effective,{s.view->x,s.view->y,s.view->z},*s.speedLimit,s.elapsedUs,firstFrame);
    if(!command) {
        if(current_->pump_) current_->pump_->submissionRejected();
        current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::MotorRejected);
        failExecution(nav::runtime::ExecutionFailure::Motion); return;
    }
    core::BotCommand submittedCommand=*command.command;
    metamod::MovementResult result{};
    // Runtime combat is a one-shot value owned by the orchestrator. Consume
    // it only for this exact navigation stamp so a stale attack cannot be
    // attached to a later movement command. The fallback preserves manual NAV
    // operation when the high-level runtime has no executable decision.
    if (const auto combat = owner.takeRuntimeCombatDecision(
            s.actor, s.agent, s.map, owner.round(), s.tick)) {
        const auto composed = owner.submitCombatDecision(
            s.actor, s.map, s.tick, *combat, submittedCommand);
        if (composed.composition) submittedCommand = composed.composition.command;
        result = composed.transport;
        if (!composed.accepted && result.error == metamod::MovementError::None)
            result = metamod::MovementResult::rejectedResult(
                metamod::MovementError::RegistryRejected);
    } else {
        result=owner.submitCommand(s.actor,s.map,s.tick,submittedCommand);
    }
    current_->motionTrace_.command=submittedCommand;
    current_->motionTrace_.commandTick=s.tick; current_->motionTrace_.dispatchTick={}; current_->motionTrace_.intentAgeUs=age;
    current_->motionTrace_.transportError=result.error;
    if(result.queued()) {
        // A command is queued for the next server tick. Under a loaded HLDS
        // that tick can legitimately take longer than the nominal intent
        // lifetime; retain one measured frame of grace so a one-tick command
        // is not rejected solely because wall time exceeded 120 ms. A second
        // missed dispatch still expires through the normal bounded stale path.
        const auto frameDelta=movement_->frameDeltaUs();
        const auto remaining=nav::local::IntentPump::maxIntentAgeUs>age
            ? nav::local::IntentPump::maxIntentAgeUs-age : 0U;
        const auto pendingFreshness=add(remaining,frameDelta);
        current_->motionTrace_.elapsedUs=s.elapsedUs;
        current_->motionTrace_.frameDeltaUs=frameDelta;
        current_->motionTrace_.pendingRemainingUs=pendingFreshness;
        current_->motionTrace_.intentSpeed=double(intent.speed);
        current_->motionTrace_.speedLimit=double(*s.speedLimit);
        current_->pendingMotion_=PendingMotion{current_->motionTrace_.decision.binding,s.tick,pendingFreshness,
            s,submittedCommand,current_->segment_,contact};
        if(decision.jumpState && decision.jumpPlan && decision.jumpPhysics)
            current_->pendingMotion_->jump=JumpTicket{*decision.jumpPlan,*decision.jumpPhysics,*decision.jumpState,decision.jumpPressTick};
        if(decision.dropState && *decision.dropState!=nav::local::DropState::Landed &&
           *decision.dropState!=nav::local::DropState::Failed && decision.dropPlan && decision.jumpPhysics) {
            current_->pendingMotion_->drop=decision.dropPlan;
            current_->pendingMotion_->dropState=*decision.dropState;
            current_->pendingMotion_->dropGravity=decision.jumpPhysics->gravity;
        }
        if(decision.ladderState && decision.ladderPlan) {
            auto& pending=*current_->pendingMotion_; pending.ladder=decision.ladderPlan; pending.ladderState=*decision.ladderState;
            pending.ladderPressTick=decision.ladderPressTick;
            // Guard the target associated with the issued state/intent. A state
            // transition's neutral command can use the newly active target.
            pending.ladderTarget=current_->walk_ ? current_->walk_->ladderTarget(*decision.ladderPlan,*s.position):decision.ladderPlan->end;
        }
        current_->motionTrace_.queued=add(current_->motionTrace_.queued,1); recordMotion(MotionEvent::Queued);
    } else {
        reportLadderTransport(decision,s.tick,s.tick,false);
        if(current_->walk_ && (submittedCommand.buttons&static_cast<core::ButtonMask>(core::Button::Jump)))
            (void)current_->walk_->reportJumpDispatch({decision.binding,s.tick,s.tick,false});
        if(current_->pump_) current_->pump_->submissionRejected();
        current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1);
        recordMotion(MotionEvent::Rejected,MotionReason::TransportRejected);
        failExecution(nav::runtime::ExecutionFailure::Transport);
    }
}
}
