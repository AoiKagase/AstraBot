// SPDX-License-Identifier: MPL-2.0
#include "nav/local/locomotion_controller.hpp"
#include <algorithm>
#include <cmath>

namespace astrabot::nav::local {
namespace {
using Vec=model::NavVector3;
constexpr double pi=3.14159265358979323846;
double range(Vec a,Vec b) noexcept { return std::hypot(double(a.x)-b.x,double(a.y)-b.y); }
bool ordinary(const corridor::Transition& t) noexcept {
    return !t.edge.external && (t.effectiveTraversal==model::NavTraversalKind::Walk ||
        t.effectiveTraversal==model::NavTraversalKind::Crouch) &&
        ((t.sourceAttributes|t.targetAttributes)&2U)==0;
}
bool validActor(const runtime::MovementSnapshot& s,Binding b) noexcept {
    return s.agent==b.agent && s.actor==b.actor && s.map==b.map &&
        s.kind==runtime::ActorKind::ManagedBot && s.connected==true && s.alive==true && s.joined==true;
}
struct Queries final : runtime::IWorldQueries {
    runtime::IWorldQueries& port; std::uint32_t used,maximum;
    Queries(runtime::IWorldQueries& p,std::uint32_t reserved,std::uint32_t limit)
        :port(p),used(reserved),maximum(limit) {}
    runtime::WorldQueryResult query(const runtime::QueryRequest& original) override {
        runtime::WorldQueryResult out; out.stamp=original.stamp; out.kind=original.kind;
        if(used>=maximum) { out.error=runtime::QueryError::BudgetExceeded; return out; }
        auto request=original; request.stamp.ordinal=++used;
        try { out=port.query(request); } catch(...) { out.error=runtime::QueryError::Unavailable; return out; }
        if(sameQueryContext(out.stamp,request.stamp)) out.stamp=original.stamp;
        else { out.stamp={}; out.error=runtime::QueryError::InvalidResult; }
        return out;
    }
};
runtime::WorldQueryResult ask(Queries& q,const runtime::MovementSnapshot& s,Binding b,
    runtime::QueryKind kind,Vec from,Vec to) {
    runtime::QueryRequest request{{s.agent,s.actor,s.map,s.tick,b.routeGeneration,1},kind,from,to,s.hull};
    auto result=q.query(request);
    if(!(result.stamp==request.stamp) || result.kind!=kind) result.error=runtime::QueryError::InvalidResult;
    return result;
}
bool clear(const runtime::WorldQueryResult& r) noexcept {
    return r.error==runtime::QueryError::None && r.hull && !r.hull->startSolid &&
        !r.hull->allSolid && std::isfinite(r.hull->fraction) && r.hull->fraction==1;
}
bool unknown(ProbeReason r) noexcept {
    return r==ProbeReason::BudgetExceeded || r==ProbeReason::QueryUnavailable ||
        r==ProbeReason::QueryFailed || r==ProbeReason::StaleQuery || r==ProbeReason::InvalidResult;
}
}
LocomotionController::LocomotionController(Binding b,std::shared_ptr<const corridor::Corridor> corridor,
    Vec goal,WalkLimits limits,JumpAttemptRegistry* attempts) noexcept
    :binding_(b),corridor_(corridor),follower_(corridor,{goal.x,goal.y,goal.z}),
     legacy_(b,corridor,goal,limits,attempts),limits_(limits) {
#ifdef ASTRABOT_LEGACY_LOCOMOTION
    comparison_=true;
#endif
    limits_.probe.maxQueries=queryLimit-dispatchReserve;
    limits_.probe.maxSamples=8;
    limits_.probe.maxDistance=64;
}
WalkState LocomotionController::state() const noexcept { return comparison_ ? legacy_.state():state_; }
std::size_t LocomotionController::step() const noexcept { return comparison_ ? legacy_.step():follower_.step(); }
const corridor::Transition* LocomotionController::activeTransition() const noexcept {
    return comparison_ ? legacy_.activeTransition():follower_.activeTransition();
}
bool LocomotionController::needsJumpProofBudget() const noexcept {
    return comparison_ ? legacy_.needsJumpProofBudget():false;
}
bool LocomotionController::reportJumpDispatch(const JumpDispatch& d) noexcept {
    if(comparison_ || ladderOwned_) return legacy_.reportJumpDispatch(d);
    if(d.binding.agent!=binding_.agent || d.binding.actor!=binding_.actor || d.binding.map!=binding_.map ||
       d.binding.routeGeneration!=binding_.routeGeneration || d.commandTick!=pressTick_ ||
       (d.dispatched && !d.dispatchTick.isAfter(d.commandTick))) return false;
    pressDispatched_=d.dispatched; dispatchTick_=d.dispatchTick; return true;
}
bool LocomotionController::reportLadderDispatch(const LadderDispatch& d) noexcept { return legacy_.reportLadderDispatch(d); }
std::optional<enrichment::NavTraversalLink> LocomotionController::selectedLadderLink() const noexcept {
    return (comparison_ || ladderOwned_) ? legacy_.selectedLadderLink():std::nullopt;
}
Vec LocomotionController::ladderTarget(const LadderPlan& p,Vec origin) const noexcept { return legacy_.ladderTarget(p,origin); }
WalkDecision LocomotionController::finish(WalkDecision out,WalkState state,WalkReason reason) noexcept {
    state_=state; out.state=state; out.reason=reason; out.terminalEvent=true;
    out.intent={}; envelope_.reset(); blockerWait_.reset(); return out;
}
WalkDecision LocomotionController::abort() noexcept {
    if(comparison_) return legacy_.abort();
    if(ladderOwned_) (void)legacy_.abort();
    WalkDecision out; out.binding=binding_; out.binding.step=step(); out.tick=tick_; out.accepted=true;
    return finish(out,WalkState::Aborted,WalkReason::Cancelled);
}
WalkDecision LocomotionController::hold(WalkDecision out,ProbeReason reason) noexcept {
    out.intent={}; out.probeReason=reason; out.disposition=MotionDisposition::Hold; envelope_.reset();
    // Lack of observations is never structural edge evidence. The bounded
    // path timeout can request a fresh route without poisoning the old edge.
    if(progressInitialized_ && nowUs_-progressUs_>=5000000)
        return finish(out,WalkState::Failed,WalkReason::RecoveryReplan);
    return out;
}
bool LocomotionController::beginJump(WalkDecision& out,const runtime::MovementSnapshot& s,
    const GroundedTarget& support,core::IntentVector direction,const query::NavSpatialIndex& index,
    core::MapGeneration map,runtime::IWorldQueries& port,std::uint32_t& used,JumpPhysics physics,bool drop,
    const runtime::WorldQueryResult* obstacle) {
    const auto* t=follower_.activeTransition();
    const auto attributes=t ? (t->sourceAttributes|t->targetAttributes):corridor_->startAttributes();
    if(!drop && (attributes&8U)) return false;
    const bool plannedJump=!drop && t &&
        t->effectiveTraversal==model::NavTraversalKind::Jump;
    if(!s.grounded.value_or(false) || !s.velocity || !s.speedLimit || !s.hull ||
       !std::isfinite(physics.gravity) || physics.gravity<=0 ||
       !std::isfinite(physics.verticalImpulse) || physics.verticalImpulse<=0) return false;
    // NAV traversal and area attributes are advisory. A ground jump needs a
    // current physical face with takeoff room; an uphill support plane is not
    // an obstacle and must remain ordinary ground locomotion.
    if(!drop && !plannedJump) {
        // A rising support plane is ordinary ground locomotion. Treating its
        // forward hull contact as a jumpable wall produces repeated jumps up
        // long ramps. The floor normal is measured by TerrainSampler, so use
        // the same physical observation here.
        const auto& normal=support.floor.normal;
        const double supportNormalLength=std::sqrt(double(normal.x)*normal.x+
            double(normal.y)*normal.y+double(normal.z)*normal.z);
        const double slopeRise=normal.z>0.001 ?
            -(double(normal.x)*direction.x+double(normal.y)*direction.y)/normal.z:0.0;
        if(!normal.isFinite() || supportNormalLength<0.99 || supportNormalLength>1.01 ||
           normal.z<0.7f || slopeRise>0.05) return false;
        if(obstacle==nullptr || obstacle->error!=runtime::QueryError::None ||
           !obstacle->blocker || obstacle->blocker->kind!=runtime::BlockerKind::Geometry ||
           !obstacle->hull) return false;
        const auto& hit=*obstacle->hull;
        const double normalLength=std::sqrt(double(hit.normal.x)*hit.normal.x+
            double(hit.normal.y)*hit.normal.y+double(hit.normal.z)*hit.normal.z);
        const double facing=double(hit.normal.x)*direction.x+double(hit.normal.y)*direction.y;
        const double takeoff=range(*s.position,hit.end);
        if(hit.startSolid || hit.allSolid || !hit.end.isFinite() || !hit.normal.isFinite() ||
           !std::isfinite(hit.fraction) || hit.fraction<0 || hit.fraction>=1 ||
           normalLength<0.99 || normalLength>1.01 || facing>=-0.25 ||
           std::abs(double(hit.normal.z))>0.25 || takeoff>32.5) return false;
    }
    const double horizontal=std::hypot(double(s.velocity->x),double(s.velocity->y));
    const double forward=s.velocity->x*direction.x+s.velocity->y*direction.y;
    if(horizontal<30 || forward<20) return false;
    // Use measured momentum, not the requested run speed. Landing is a
    // physically supported area, not a radius around an arbitrary NAV point.
    const double impulse=drop ? 0:physics.verticalImpulse;
    double seconds=drop ? std::sqrt(2*limits_.probe.maxDrop/physics.gravity):2*impulse/physics.gravity;
    if(!std::isfinite(seconds) || seconds<=0 || seconds>1.5 || horizontal*seconds>256) return false;
    Queries queries(port,used,queryLimit-dispatchReserve);
    const auto save=[&] { used=queries.used; out.queries=used; };
    const auto reject=[&]() noexcept {
        if(!drop) {
            jumpSuppressedStep_=follower_.step();
            jumpSuppressedOrigin_=*s.position;
            jumpRetryUntilUs_=nowUs_+750000;
        }
        save();
        return false;
    };
    const double feet=double(s.position->z)+s.hull->minimum.z;
    Vec target{static_cast<float>(s.position->x+s.velocity->x*seconds),
        static_cast<float>(s.position->y+s.velocity->y*seconds),s.position->z};
    const double apex=drop ? limits_.probe.maxStepUp:impulse*impulse/(2*physics.gravity);
    if(!drop) {
        // Prove both headroom over the takeoff and a hull-wide passage over
        // the observed upper edge before considering the predicted landing.
        const Vec raised{s.position->x,s.position->y,static_cast<float>(s.position->z+apex)};
        const Vec beyond{static_cast<float>(raised.x+direction.x*32),
            static_cast<float>(raised.y+direction.y*32),raised.z};
        const auto overhead=ask(queries,s,binding_,runtime::QueryKind::SweptHull,*s.position,raised);
        if(!clear(overhead)) { out.probeReason=overhead.error==runtime::QueryError::BudgetExceeded ?
            ProbeReason::BudgetExceeded:ProbeReason::Blocked; return reject(); }
        const auto upperEdge=ask(queries,s,binding_,runtime::QueryKind::SweptHull,raised,beyond);
        if(!clear(upperEdge)) { out.probeReason=upperEdge.error==runtime::QueryError::BudgetExceeded ?
            ProbeReason::BudgetExceeded:ProbeReason::Blocked; return reject(); }
    }
    const auto candidate=ask(queries,s,binding_,runtime::QueryKind::FloorCandidate,
        {target.x,target.y,static_cast<float>(feet+apex+1)},
        {target.x,target.y,static_cast<float>(feet-64)});
    if(candidate.error!=runtime::QueryError::None || !candidate.floor || !candidate.floor->supported) {
        out.probeReason=candidate.error==runtime::QueryError::BudgetExceeded ? ProbeReason::BudgetExceeded:ProbeReason::QueryUnavailable;
        return reject();
    }
    const double height=candidate.floor->height;
    const double fall=feet-height;
    if(fall>64 || (drop && fall<0) || (!drop && height-feet>apex)) return reject();
    const double discriminant=impulse*impulse+2*physics.gravity*fall;
    if(discriminant<0) return reject();
    seconds=(impulse+std::sqrt(discriminant))/physics.gravity;
    if(seconds<=0 || seconds>1.5 || horizontal*seconds>256) return reject();
    target={static_cast<float>(s.position->x+s.velocity->x*seconds),
        static_cast<float>(s.position->y+s.velocity->y*seconds),static_cast<float>(height-s.hull->minimum.z)};
    auto landing=s; landing.position=target; landing.grounded=true;
    auto allowance=limits_.probe; allowance.maxQueries=queries.maximum-queries.used;
    const auto supported=TerrainSampler::locate(landing,binding_.routeGeneration,index,map,queries,allowance);
    if(!supported || !reachableArea(supported.target->area)) {
        out.probeReason=supported.reason; return reject();
    }
    const unsigned segments=static_cast<unsigned>(std::ceil(seconds/0.05));
    if(queries.maximum-queries.used<segments) { out.probeReason=ProbeReason::BudgetExceeded; return reject(); }
    Vec previous=*s.position;
    for(unsigned i=1;i<=segments;++i) {
        const double elapsed=seconds*i/segments;
        Vec next{static_cast<float>(s.position->x+s.velocity->x*elapsed),
            static_cast<float>(s.position->y+s.velocity->y*elapsed),
            static_cast<float>(s.position->z+impulse*elapsed-0.5*physics.gravity*elapsed*elapsed)};
        const auto sweep=ask(queries,s,binding_,runtime::QueryKind::SweptHull,previous,next);
        if(!clear(sweep)) { out.probeReason=sweep.error==runtime::QueryError::BudgetExceeded ?
            ProbeReason::BudgetExceeded:ProbeReason::Blocked; return reject(); }
        previous=next;
    }
    airborneTarget_=supported.target->origin; airborneArea_=supported.target->area;
    flightPhysics_=physics; phase_=drop ? LocomotionPhase::Drop:LocomotionPhase::Takeoff;
    phaseStartedUs_=nowUs_; pressDispatched_=false; airborneSeen_=false;
    // Dispatch acknowledgement belongs to this jump attempt. Retaining a
    // prior attempt's dispatch tick makes the next Takeoff look acknowledged
    // before its Press has even reached the engine.
    dispatchTick_={}; jumpWasDrop_=drop; jumpSuppressedStep_.reset();
    pressTick_=drop ? core::TickId{}:s.tick;
    out.support=support; issue(out,s,*supported.target,direction,phase_);
    if(!envelope_) { phase_=LocomotionPhase::Ground; return reject(); }
    out.intent.jump=drop ? core::ActionRequest::Release:core::ActionRequest::Press;
    if(s.ducked==true) out.intent.duck=core::ActionRequest::Hold;
    out.jumpState=drop ? JumpState::Airborne:JumpState::Takeoff;
    out.jumpPhysics=physics; out.jumpPressTick=pressTick_;
    out.jumpTrajectoryReady=true; out.jumpPredictedLanding=airborneTarget_;
    if(!drop) jumpCooldownUntilUs_=nowUs_+750000;
    out.obstacleClass=ObstacleClass::Jumpable; save(); return true;
}
WalkDecision LocomotionController::airborne(WalkDecision out,const runtime::MovementSnapshot& s,
    const query::NavSpatialIndex& index,core::MapGeneration map,runtime::IWorldQueries& port,std::uint32_t reserved) {
    out.jumpState=phase_==LocomotionPhase::Takeoff ? JumpState::Takeoff:JumpState::Airborne;
    out.jumpPhysics=flightPhysics_; out.jumpPressTick=pressTick_;
    if(nowUs_-phaseStartedUs_>2000000) return finish(out,WalkState::Failed,WalkReason::RecoveryReplan);
    if(phase_==LocomotionPhase::Takeoff) {
        if(!pressDispatched_) {
            if(dispatchTick_.isValid() || nowUs_-phaseStartedUs_>200000) {
                if(!jumpWasDrop_ && s.position) {
                    jumpSuppressedStep_=follower_.step();
                    jumpSuppressedOrigin_=*s.position;
                }
                phase_=LocomotionPhase::Ground; return hold(out,ProbeReason::QueryUnavailable);
            }
            return hold(out,ProbeReason::None); // Never repeat Press while acknowledgment is pending.
        }
        if(s.grounded==false) { phase_=LocomotionPhase::Airborne; airborneSeen_=true; }
        else if(nowUs_-phaseStartedUs_>200000) {
            if(!jumpWasDrop_) {
                jumpSuppressedStep_=follower_.step();
                jumpSuppressedOrigin_=*s.position;
            }
            phase_=LocomotionPhase::Ground; return hold(out,ProbeReason::Blocked);
        }
    } else if(s.grounded==true && airborneSeen_) {
        Queries queries(port,reserved,queryLimit-dispatchReserve);
        auto allowance=limits_.probe; allowance.maxQueries=queries.maximum-queries.used;
        const auto support=TerrainSampler::locate(s,binding_.routeGeneration,index,map,queries,allowance);
        out.queries=queries.used;
        if(!support) return hold(out,support.reason);
        if(!reachableArea(support.target->area)) return finish(out,WalkState::Failed,WalkReason::RecoveryReplan);
        const auto* t=follower_.activeTransition();
        if(t && !ordinary(*t) && support.target->area==t->edge.target)
            (void)follower_.completeSpecial(follower_.step(),support.target->area,true);
        if(!jumpWasDrop_) {
            jumpSuppressedStep_=follower_.step();
            jumpSuppressedOrigin_=*s.position;
        }
        phase_=LocomotionPhase::Ground;
        out.jumpState=JumpState::Complete; out.support=support.target;
        return hold(out,ProbeReason::None);
    }
    if(s.grounded==false) airborneSeen_=true;
    const double distance=range(*s.position,airborneTarget_);
    if(distance<1) return hold(out,ProbeReason::None);
    core::IntentVector direction{(airborneTarget_.x-s.position->x)/distance,
        (airborneTarget_.y-s.position->y)/distance,0};
    GroundedTarget target{airborneTarget_,airborneArea_,{}};
    issue(out,s,target,direction,phase_);
    out.intent.jump=core::ActionRequest::Release;
    if(s.ducked==true) out.intent.duck=core::ActionRequest::Hold;
    return out;
}
ProbeResult LocomotionController::guard(const runtime::MovementSnapshot& s,const MotionEnvelope& envelope,
    Vec destination,const query::NavSpatialIndex& index,core::MapGeneration map,
    runtime::IWorldQueries& port,std::uint32_t reserved,std::optional<JumpPhysics> physics) noexcept {
    ProbeResult out; out.reason=ProbeReason::StaleNavigation;
    if(!validActor(s,binding_) || s.map!=map || !s.position || !s.hull ||
       envelope.binding.routeGeneration!=binding_.routeGeneration || envelope.binding.step!=step() ||
       !destination.isFinite()) return out;
    if(!std::isfinite(limits_.probe.maxStepUp) || limits_.probe.maxStepUp<=0) {
        out.reason=ProbeReason::QueryUnavailable; return out;
    }
    if(reserved>=queryLimit) { out.reason=ProbeReason::BudgetExceeded; return out; }
    Queries queries(port,reserved,queryLimit);
    if(envelope.phase==LocomotionPhase::Ground) {
        auto limits=limits_.probe; limits.maxQueries=queryLimit-queries.used;
        const auto support=TerrainSampler::locate(s,binding_.routeGeneration,index,map,queries,limits);
        if(!support) { out=support; out.queries=queries.used; return out; }
        limits.maxQueries=queryLimit-queries.used;
        out=TerrainSampler::inspect(s,binding_.routeGeneration,support.target->area,destination.x,destination.y,index,map,queries,limits);
        if(out && !reachableArea(out.target->area)) { out.reason=ProbeReason::NoArea; out.target.reset(); }
    } else {
        if(!physics || !flightPhysics_ || !s.velocity || physics->gravity!=flightPhysics_->gravity ||
           physics->verticalImpulse!=flightPhysics_->verticalImpulse) { out.reason=ProbeReason::QueryUnavailable; return out; }
        const double dt=double(s.elapsedUs)/1000000;
        if(dt<=0 || dt>0.12) { out.reason=ProbeReason::InvalidInput; return out; }
        const bool awaitingLiftoff=envelope.phase==LocomotionPhase::Takeoff && pressDispatched_;
        const bool leavingEdge=(envelope.phase==LocomotionPhase::Drop || awaitingLiftoff) && s.grounded==true;
        const double vertical=envelope.phase==LocomotionPhase::Takeoff && !pressDispatched_ ? physics->verticalImpulse:s.velocity->z;
        destination.z=leavingEdge ? s.position->z:
            static_cast<float>(s.position->z+vertical*dt-0.5*physics->gravity*dt*dt);
        const auto sweep=ask(queries,s,binding_,runtime::QueryKind::SweptHull,*s.position,destination);
        out.reason=clear(sweep) ? ProbeReason::None:
            (sweep.error==runtime::QueryError::BudgetExceeded ? ProbeReason::BudgetExceeded:ProbeReason::Blocked);
        if(out.reason==ProbeReason::None) out.target=GroundedTarget{destination,airborneArea_,{}};
    }
    out.queries=queries.used; return out;
}
bool LocomotionController::reachableArea(model::NavAreaId area) const noexcept {
    if(!corridor_ || !area.isValid()) return false;
    if(!follower_.activeTransition()) return area==corridor_->goal();
    auto i=follower_.step();
    const auto& transitions=corridor_->transitions();
    if(area==transitions[i].edge.source) return true;
    // A loaded 20-BOT frame can advance through several compact NAV patches
    // between observations. Keep the ZBot-style forward window wide enough
    // to recognize a later ordinary area without declaring it off-corridor.
    for(std::size_t n=0;i<transitions.size() && n<64;++i,++n) {
        if(area==transitions[i].edge.target) return true;
        if(!ordinary(transitions[i])) break;
    }
    return false;
}
void LocomotionController::issue(WalkDecision& out,const runtime::MovementSnapshot& s,
    const GroundedTarget& target,core::IntentVector direction,LocomotionPhase phase) noexcept {
    out.target=target; out.intent.direction=direction;
    const auto distance=range(*s.position,target.origin);
    if(!s.speedLimit || *s.speedLimit<=0 || distance<=0 ||
       !core::Motor::bindLocomotion(out.intent,core::LocomotionMode::Run,*s.speedLimit,distance)) {
        out.intent={}; out.disposition=MotionDisposition::Hold; envelope_.reset(); return;
    }
    out.intent.view=core::IntentVector{0,std::atan2(direction.y,direction.x)*180/pi,0};
    out.disposition=MotionDisposition::Execute;
    envelope_=MotionEnvelope{out.binding,s.tick,*s.hull,*s.position,target.origin,
        out.support ? out.support->area:target.area,phase,out.intent.validForUs,64};
}
void LocomotionController::feedback(const MovementFeedback& f) noexcept {
    if(!native() || f.binding.agent!=binding_.agent || f.binding.actor!=binding_.actor ||
       f.binding.map!=binding_.map || f.binding.routeGeneration!=binding_.routeGeneration ||
       !f.dispatchTick.isAfter(f.commandTick)) return;
    if(f.commandTick==pressTick_) {
        pressDispatched_=f.dispatched; dispatchTick_=f.dispatchTick;
    }
    // Actual feedback is diagnostic/ownership evidence. Progress is credited
    // only on the next supported observation, never for a requested command.
}
WalkDecision LocomotionController::recover(const runtime::MovementSnapshot& s,const query::NavSpatialIndex& index,
    core::MapGeneration map,runtime::IWorldQueries& port,const RecoveryDecision& recovery,std::uint32_t reserved) noexcept {
    if(comparison_) return legacy_.recover(s,index,map,port,recovery,reserved);
    return update(s,index,map,port,nowUs_+s.elapsedUs,reserved,flightPhysics_);
}
WalkDecision LocomotionController::update(const runtime::MovementSnapshot& s,const query::NavSpatialIndex& index,
    core::MapGeneration map,runtime::IWorldQueries& port,std::uint64_t now,std::uint32_t reserved,
    std::optional<JumpPhysics> physics,std::optional<LadderObservation> ladder) noexcept {
    if(comparison_) return legacy_.update(s,index,map,port,now,reserved,physics,ladder);
    WalkDecision out; out.binding=binding_; out.binding.step=step(); out.tick=s.tick; out.state=state_;
    if(state_!=WalkState::Running) return out;
    if(!validActor(s,binding_) || s.map!=map) return finish(out,WalkState::Aborted,WalkReason::InvalidActor);
    if(!s.tick.isAfter(tick_) || now<nowUs_) { out.reason=WalkReason::StaleTick; return out; }
    const auto previousEnvelope=envelope_;
    tick_=s.tick; nowUs_=now; out.accepted=true; envelope_.reset();
    if(!std::isfinite(limits_.probe.maxStepUp) || limits_.probe.maxStepUp<=0)
        return hold(out,ProbeReason::QueryUnavailable);
    if(!s.position || !s.position->isFinite() || !s.hull || !s.speedLimit || !s.velocity ||
       !s.velocity->isFinite() || !s.elapsedUs || !s.grounded.has_value()) return hold(out,ProbeReason::QueryUnavailable);
    if(!progressInitialized_) { progressInitialized_=true; progressAnchor_=*s.position; progressUs_=now; }
    if(ladderOwned_) {
        out=legacy_.update(s,index,map,port,now,reserved,physics,ladder);
        if(out.state!=WalkState::Running) { state_=out.state; return out; }
        if(legacy_.step()>follower_.step() && out.support) {
            if(!follower_.completeSpecial(follower_.step(),out.support->area,true))
                return finish(out,WalkState::Failed,WalkReason::RecoveryReplan);
            ladderOwned_=false; progressUs_=now;
        } else return out;
    }
    Queries queries(port,reserved,queryLimit-dispatchReserve);
    // Refresh contact identity every native frame, before source occupancy can
    // fail on a touching player. The adapter alone supplies registry identities.
    Vec contactTarget=*s.position;
    double cx=progressAxis_.x, cy=progressAxis_.y;
    if(previousEnvelope) {
        cx=double(previousEnvelope->target.x)-s.position->x;
        cy=double(previousEnvelope->target.y)-s.position->y;
    } else if(std::hypot(cx,cy)<0.001 && s.view && s.view->isFinite()) {
        cx=std::cos(s.view->y*pi/180.0); cy=std::sin(s.view->y*pi/180.0);
    }
    const double contactLength=std::hypot(cx,cy);
    if(contactLength>0.001) {
        contactTarget.x+=static_cast<float>(cx/contactLength*32);
        contactTarget.y+=static_cast<float>(cy/contactLength*32);
    }
    const auto contact=ask(queries,s,binding_,runtime::QueryKind::Blocker,*s.position,contactTarget);
    out.queries=queries.used;
    if(contact.error!=runtime::QueryError::None) {
        const auto reason=contact.error==runtime::QueryError::BudgetExceeded ?
            ProbeReason::BudgetExceeded:ProbeReason::QueryUnavailable;
        // A blocker query is advisory. Invalid/unavailable contact metadata
        // must not suppress the terrain sampler or freeze the actor; the
        // following support/sweep pass can still produce a safe movement
        // intent. Only an exhausted frame budget is a hard hold because no
        // additional physical query can be issued this tick.
        if(phase_==LocomotionPhase::Ground &&
           contact.error==runtime::QueryError::BudgetExceeded)
            return hold(out,reason);
    } else if(contact.blocker) {
        out.blocker=contact.blocker; out.obstacleHull=contact.hull;
        const auto& observed=*contact.blocker;
        const bool player=observed.kind==runtime::BlockerKind::Player ||
            observed.kind==runtime::BlockerKind::Teammate || observed.kind==runtime::BlockerKind::Enemy;
        if(player && (!observed.id || !observed.player || !observed.player->isValid() ||
                      observed.player->sameSlot(binding_.actor))) {
            if(phase_==LocomotionPhase::Ground) return hold(out,ProbeReason::InvalidResult);
        } else if(player) {
            out.obstacleClass=ObstacleClass::Dynamic;
            if(!blockerWait_ || blockerStep_!=step()) {
                const auto policy=limits_.blocker.timeoutUs ? limits_.blocker:BlockerLimits{120000,200000,1000000};
                blockerWait_.emplace(binding_,policy); blockerStep_=step();
            }
            const runtime::QueryStamp requested{s.agent,s.actor,s.map,s.tick,binding_.routeGeneration,1};
            const auto decision=blockerWait_->update({out.binding,requested,contact,now});
            out.blockerAction=decision.action; out.blockerReason=decision.reason;
            if(phase_==LocomotionPhase::Ground) {
                if(decision.action==BlockerAction::Yield) return hold(out,ProbeReason::Blocked);
                if(decision.action==BlockerAction::Aborted)
                    return finish(out,WalkState::Aborted,WalkReason::InvalidActor);
                if(decision.action==BlockerAction::Replan)
                    return finish(out,WalkState::Failed,WalkReason::DynamicBlocked);
            }
        } else {
            blockerWait_.reset();
        }
    } else {
        blockerWait_.reset();
    }
    // Airborne motion retains its flight controller; yielding cannot freeze
    // gravity or revoke an already-dispatched jump.
    if(phase_!=LocomotionPhase::Ground) return airborne(out,s,index,map,port,queries.used);
    if(queries.used>=queries.maximum) return hold(out,ProbeReason::BudgetExceeded);
    auto budget=limits_.probe; budget.maxQueries=queries.maximum-queries.used;
    const auto ground=TerrainSampler::locate(s,binding_.routeGeneration,index,map,queries,budget);
    out.queries=queries.used; out.support=ground.target; out.probeReason=ground.reason;
    if(!ground) {
        // A hull that is already touching a wall or another actor can make
        // the support probe report StartSolid/AllSolid before the normal
        // detour branch is reached. ZBot still performs a short two-sided
        // reflex sweep in this state. Reuse the measured actor feet as a
        // transient support and issue only a bounded local move; the next
        // frame must re-establish full TerrainSampler support before normal
        // route progress is credited.
        const auto* transition=follower_.activeTransition();
        const bool ordinaryGround=transition && ordinary(*transition) &&
            ground.reason!=ProbeReason::BudgetExceeded &&
            ground.reason!=ProbeReason::StaleQuery &&
            ground.reason!=ProbeReason::QueryUnavailable &&
            ground.reason!=ProbeReason::QueryFailed &&
            ground.reason!=ProbeReason::InvalidResult;
        if(ordinaryGround && s.position && s.hull && s.grounded==true &&
           transition->edge.source.isValid() && queries.maximum-queries.used>=2) {
            double ax=progressAxis_.x, ay=progressAxis_.y;
            if(std::hypot(ax,ay)<0.001 && s.view && s.view->isFinite()) {
                ax=std::cos(s.view->y*pi/180.0);
                ay=std::sin(s.view->y*pi/180.0);
            }
            const double axisLength=std::hypot(ax,ay);
            if(axisLength>0.001) {
                ax/=axisLength; ay/=axisLength;
                const double feet=double(s.position->z)+s.hull->minimum.z;
                GroundedTarget fallback{*s.position,transition->edge.source,
                    runtime::FloorObservation{static_cast<float>(feet),{0,0,1},true,
                        runtime::FloorObservationStatus::Supported,{}}};
                for(int side : {-1,1}) {
                    const Vec candidateEnd{
                        static_cast<float>(s.position->x-ay*side*16.0),
                        static_cast<float>(s.position->y+ax*side*16.0),
                        s.position->z};
                    const auto detour=ask(queries,s,binding_,runtime::QueryKind::SweptHull,
                        *s.position,candidateEnd);
                    const bool clearDetour=clear(detour);
                    if(side==-1) {
                        out.detourFirstAttempted=true;
                        out.detourFirstSide=-1;
                        out.detourFirstReason=clearDetour ? ProbeReason::None : ProbeReason::Blocked;
                    } else {
                        out.detourSecondAttempted=true;
                        out.detourSecondSide=1;
                        out.detourSecondReason=clearDetour ? ProbeReason::None : ProbeReason::Blocked;
                    }
                    if(clearDetour) {
                        out.support=fallback;
                        out.obstacleClass=ObstacleClass::Dynamic;
                        out.avoiding=true;
                        out.avoidanceSide=side;
                        out.avoidanceDistance=16.0;
                        out.avoidanceCandidate=candidateEnd;
                        out.probeReason=ProbeReason::None;
                        issue(out,s,GroundedTarget{candidateEnd,fallback.area,fallback.floor},
                            core::IntentVector{-ay*side,ax*side,0},LocomotionPhase::Ground);
                        out.queries=queries.used;
                        return out;
                    }
                }
                if(queries.maximum-queries.used>=1) {
                    const Vec end{static_cast<float>(s.position->x+ax*16.0),
                        static_cast<float>(s.position->y+ay*16.0),s.position->z};
                    const auto forwardSweep=ask(queries,s,binding_,runtime::QueryKind::SweptHull,
                        *s.position,end);
                    if(clear(forwardSweep)) {
                        out.support=fallback;
                        out.avoiding=true;
                        out.avoidanceSide=0;
                        out.avoidanceDistance=16.0;
                        out.avoidanceCandidate=end;
                        out.probeReason=ProbeReason::None;
                        issue(out,s,GroundedTarget{end,fallback.area,fallback.floor},
                            core::IntentVector{ax,ay,0},LocomotionPhase::Ground);
                        out.queries=queries.used;
                        return out;
                    }
                }
            }
        }
        const bool transientGap =
            ground.reason==ProbeReason::BudgetExceeded ||
            ground.reason==ProbeReason::StaleQuery ||
            ground.reason==ProbeReason::QueryUnavailable ||
            ground.reason==ProbeReason::QueryFailed ||
            ground.reason==ProbeReason::InvalidResult ||
            ground.reason==ProbeReason::NavContainmentMissing ||
            ground.reason==ProbeReason::NoArea;
        if(transientGap && transition && ordinary(*transition) && s.position &&
           s.grounded==true && follower_.confirmedTarget()) {
            const auto saved=*follower_.confirmedTarget();
            const double dx=double(saved.x)-s.position->x, dy=double(saved.y)-s.position->y;
            const double length=std::hypot(dx,dy);
            if(length>1e-3 && std::isfinite(length)) {
                const double stepLength=std::min(length,32.0);
                const Vec end{static_cast<float>(s.position->x+dx/length*stepLength),
                    static_cast<float>(s.position->y+dy/length*stepLength),s.position->z};
                const auto sweep=ask(queries,s,binding_,runtime::QueryKind::SweptHull,
                    *s.position,end);
                if(clear(sweep)) {
                    out.retainedTarget=true;
                    out.avoidanceDistance=stepLength;
                    out.avoidanceCandidate=end;
                    out.probeReason=ProbeReason::None;
                    issue(out,s,GroundedTarget{end,transition->edge.source,
                        runtime::FloorObservation{static_cast<float>(double(s.position->z)+s.hull->minimum.z),
                            {0,0,1},true,runtime::FloorObservationStatus::Supported,{}}},
                        core::IntentVector{dx/length,dy/length,0},LocomotionPhase::Ground);
                    out.queries=queries.used;
                    return out;
                }
            }
        }
        return hold(out,ground.reason);
    }
    const Vec feet{s.position->x,s.position->y,ground.target->floor.height};
    const auto following=follower_.update({feet.x,feet.y,feet.z},ground.target->area,true,
        s.ducked==true ? 30:50,limits_.arrivalTolerance);
    out.routeForward=following.routeForward;
    out.routeProjection=following.routeProjection;
    out.targetProjection=following.targetProjection;
    out.routeProgress=following.routeProgress;
    out.retainedTarget=following.retainedTarget;
    out.stuckElapsedUs = progressInitialized_ && now >= progressUs_
        ? now - progressUs_ : 0U;
    out.binding.step=follower_.step(); binding_.step=out.binding.step;
    if(jumpSuppressedStep_ &&
       (follower_.step()!=*jumpSuppressedStep_ ||
        (range(*s.position,jumpSuppressedOrigin_)>=24.0 && now>=jumpRetryUntilUs_)))
        jumpSuppressedStep_.reset();
    if(following.changed) { progressUs_=now; progressAnchor_=*s.position; progressAxis_={}; progressBest_=0; }
    if(following.arrived) return finish(out,WalkState::Arrived,WalkReason::None);
    if(!following.target) return finish(out,WalkState::Failed,WalkReason::RecoveryReplan);
    const auto* transition=follower_.activeTransition();
    if(transition && transition->effectiveTraversal==model::NavTraversalKind::Ladder) {
        if(!legacy_.resumeSpecial(follower_.step(),ground.target->area))
            return finish(out,WalkState::Failed,WalkReason::RecoveryReplan);
        ladderOwned_=true; return legacy_.update(s,index,map,port,now,reserved,physics,ladder);
    }
    const auto sourceAttributes=transition ? transition->sourceAttributes:corridor_->startAttributes();
    const auto targetAttributes=transition ? transition->targetAttributes:sourceAttributes;
    const bool crouch=((sourceAttributes|targetAttributes)&1U)!=0;
    if(s.grounded!=true && crouch!=(s.ducked==true)) return hold(out,ProbeReason::QueryUnavailable);
    if(limits_.crouch.transitionTimeoutUs && s.grounded==true) {
        if(!posture_) posture_.emplace(binding_,limits_.crouch);
        const auto pose=posture_->update(s,crouch,now,queries,queries.used,queries.maximum);
        out.posture=pose.state; out.postureReason=pose.reason;
        if(!pose.movementAllowed) {
            out=hold(out,ProbeReason::QueryUnavailable); out.intent=pose.intent; out.queries=queries.used; return out;
        }
    }
    std::optional<GroundedTarget> contactDetour;
    const bool playerBlocker = out.blocker && out.blocker->player &&
        out.blocker->player->isValid() &&
        (out.blocker->kind==runtime::BlockerKind::Player ||
         out.blocker->kind==runtime::BlockerKind::Teammate ||
         out.blocker->kind==runtime::BlockerKind::Enemy) &&
        out.blockerAction==BlockerAction::InspectAvoidance;
    Vec target{static_cast<float>(following.target->x),static_cast<float>(following.target->y),s.position->z};
    if(transition && !ordinary(*transition) && range(*s.position,target)<5) {
        target.x=(transition->targetExtent.northWest.x+transition->targetExtent.southEast.x)*0.5F;
        target.y=(transition->targetExtent.northWest.y+transition->targetExtent.southEast.y)*0.5F;
    }
    const auto distance=range(*s.position,target);
    if(distance<0.01) return hold(out,ProbeReason::None);
    core::IntentVector direction{(target.x-s.position->x)/distance,(target.y-s.position->y)/distance,0};
    if(playerBlocker && queries.maximum-queries.used>=2) {
        if(side_==0) side_=(*out.blocker->player < binding_.actor) ? 1 : -1;
        const int first=side_, second=-side_;
        for(int candidateSide : {first,second}) {
            const Vec lateral{
                static_cast<float>(-direction.y*static_cast<double>(candidateSide)),
                static_cast<float>(direction.x*static_cast<double>(candidateSide)),0.0F};
            const Vec end{static_cast<float>(s.position->x+lateral.x*16.0F),
                static_cast<float>(s.position->y+lateral.y*16.0F),s.position->z};
            const auto sweep=ask(queries,s,binding_,runtime::QueryKind::SweptHull,
                *s.position,end);
            const bool pass=clear(sweep);
            if(candidateSide==first) {
                out.detourFirstAttempted=true;
                out.detourFirstSide=static_cast<std::int8_t>(candidateSide);
                out.detourFirstReason=pass ? ProbeReason::None:ProbeReason::Blocked;
            } else {
                out.detourSecondAttempted=true;
                out.detourSecondSide=static_cast<std::int8_t>(candidateSide);
                out.detourSecondReason=pass ? ProbeReason::None:ProbeReason::Blocked;
            }
            if(pass) {
                side_=candidateSide;
                out.avoiding=true;
                out.avoidanceSide=candidateSide;
                out.avoidanceDistance=16.0;
                out.avoidanceCandidate=end;
                direction.x=lateral.x;
                direction.y=lateral.y;
                direction.z=0;
                contactDetour=GroundedTarget{end,ground.target->area,ground.target->floor};
                break;
            }
        }
    }
    if(contactDetour) {
        out.progressDirection=direction;
        if(progressAxis_.x==0 && progressAxis_.y==0) progressAxis_=direction;
        out.probeReason=ProbeReason::None;
        issue(out,s,*contactDetour,direction,LocomotionPhase::Ground);
        out.queries=queries.used;
        return out;
    }
    out.progressDirection=direction;
    if(progressAxis_.x==0 && progressAxis_.y==0) progressAxis_=direction;
    const double projected=(s.position->x-progressAnchor_.x)*progressAxis_.x+(s.position->y-progressAnchor_.y)*progressAxis_.y;
    if(projected>=progressBest_+4) { progressBest_=projected; progressUs_=now; blockedUs_=0; }
    if(now-progressUs_>=5000000) return finish(out,WalkState::Failed,WalkReason::RecoveryReplan);
    const bool isDrop=transition && transition->effectiveTraversal==model::NavTraversalKind::Drop;
    const bool isJump=transition &&
        transition->effectiveTraversal==model::NavTraversalKind::Jump;
    if(isJump && physics) {
        auto used=queries.used;
        if(beginJump(out,s,*ground.target,direction,index,map,port,used,*physics,false,nullptr))
            return out;
        queries.used=used; out.queries=used;
        if(unknown(out.probeReason)) return hold(out,out.probeReason);
    }
    if(isDrop && physics) {
        auto used=queries.used;
        if(beginJump(out,s,*ground.target,direction,index,map,port,used,*physics,true,nullptr)) return out;
        queries.used=used; out.queries=used;
        if(unknown(out.probeReason)) return hold(out,out.probeReason);
    }
    // Feelers follow the support plane and start just above the step height.
    // Side selection persists over a short interval instead of toggling every tick.
    double fractions[2]{1,1};
    const bool precise=((sourceAttributes|targetAttributes)&4U)!=0;
    if(!precise && queries.maximum-queries.used>=2) {
        const double length=s.ducked==true ? 20.0:50.0;
        const auto normal=ground.target->floor.normal;
        const double dz=normal.z>0.001 ? -(normal.x*direction.x+normal.y*direction.y)/normal.z:0;
        for(int side=0;side<2;++side) {
            const double offset=side==0 ? -16.0:16.0;
            Vec from{static_cast<float>(s.position->x-direction.y*offset),
                static_cast<float>(s.position->y+direction.x*offset),
                static_cast<float>(ground.target->floor.height+limits_.probe.maxStepUp+0.1)};
            Vec to{static_cast<float>(from.x+direction.x*length),static_cast<float>(from.y+direction.y*length),
                static_cast<float>(from.z+dz*length)};
            const auto feeler=ask(queries,s,binding_,runtime::QueryKind::Feeler,from,to);
            if(feeler.error==runtime::QueryError::None && feeler.hull && std::isfinite(feeler.hull->fraction))
                fractions[side]=std::clamp(double(feeler.hull->fraction),0.0,1.0);
        }
        out.leftClearance=fractions[0]*length; out.rightClearance=fractions[1]*length;
        const double bias=(fractions[1]-fractions[0])*0.5;
        const double x=direction.x-direction.y*bias,y=direction.y+direction.x*bias;
        const auto norm=std::hypot(x,y); direction={x/norm,y/norm,0};
    }
    const double travel=(std::min)(distance,limits_.probe.maxDistance);
    const auto inspect=[&](core::IntentVector axis) {
        auto allowance=limits_.probe; allowance.maxQueries=queries.maximum-queries.used;
        return TerrainSampler::inspect(s,binding_.routeGeneration,ground.target->area,
            static_cast<float>(s.position->x+axis.x*travel),static_cast<float>(s.position->y+axis.y*travel),
            index,map,queries,allowance);
    };
    auto proof=inspect(direction);
    if(proof && !reachableArea(proof.target->area)) proof.reason=ProbeReason::NoArea;
    const bool geometryContact=out.blocker && out.obstacleHull &&
        out.blocker->kind==runtime::BlockerKind::Geometry;
    // A Blocker trace is authoritative for the current segment even when the
    // floor-only inspect succeeds. Force the same local-block path so both
    // detours (and a proven jump) are attempted before holding the route.
    if (geometryContact && proof) {
        proof.target.reset();
        proof.reason=ProbeReason::Blocked;
    }
    if(!proof && !unknown(proof.reason)) {
        // A geometry contact is the trigger for local reflex movement.  ZBot
        // attempts both sides (and a jump when physically proven) before
        // invalidating the route; an early RecoveryReplan here made a wall
        // contact bypass both detours and caused repeated reversals.
        auto used=queries.used;
        const auto door=door_.update(s,out.binding,target,index,map,limits_.probe,port,now,used,queries.maximum);
        queries.used=used;
        if(door) {
            if(door->target && !door->contact && door->intent.speed>0)
                envelope_=MotionEnvelope{door->binding,s.tick,*s.hull,*s.position,door->target->origin,
                    ground.target->area,LocomotionPhase::Ground,door->intent.validForUs,64};
            return *door;
        }
        if(geometryContact && physics && !jumpSuppressedStep_ &&
           nowUs_>=jumpCooldownUntilUs_ &&
           beginJump(out,s,*ground.target,direction,index,map,port,used,*physics,false,&contact)) return out;
        queries.used=used;
        if(!side_ || now>=sideUntilUs_) { side_=fractions[1]>=fractions[0] ? 1:-1; sideUntilUs_=now+500000; }
        // A diagonal detour remains under the same physical corridor proof.
        // Try the preferred side first, then the opposite side when the first
        // sweep is physically blocked. This keeps a local obstacle from
        // immediately escalating into a route replacement.
        if(!precise || geometryContact) {
            const auto detour = [](core::IntentVector axis, int side) {
                return core::IntentVector{axis.x*0.5-axis.y*side*0.8660254,
                    axis.y*0.5+axis.x*side*0.8660254,0};
            };
            unsigned detourAttempt=0;
            const auto trySide = [&](int side) {
                const auto candidate=detour(direction,side);
                auto candidateProof=inspect(candidate);
                if(detourAttempt++ == 0U) {
                    out.detourFirstAttempted=true;
                    out.detourFirstSide=static_cast<std::int8_t>(side);
                    out.detourFirstReason=candidateProof.reason;
                } else {
                    out.detourSecondAttempted=true;
                    out.detourSecondSide=static_cast<std::int8_t>(side);
                    out.detourSecondReason=candidateProof.reason;
                }
                if(candidateProof && reachableArea(candidateProof.target->area)) {
                    proof=candidateProof;
                    direction=candidate;
                    out.avoiding=true;
                    out.avoidanceSide=side;
                    return true;
                }
                return false;
            };
            const bool localBlock=proof.reason==ProbeReason::Blocked ||
                proof.reason==ProbeReason::NoArea ||
                proof.reason==ProbeReason::AllSolid ||
                proof.reason==ProbeReason::StartSolid;
            if(!trySide(side_) && localBlock) {
                const int opposite=side_==0 ? 1:-side_;
                (void)trySide(opposite);
            }
        }
    }
    out.queries=queries.used; out.probeReason=proof.reason; out.samples=proof.samples; out.steps=proof.steps;
    if(!proof) return hold(out,proof.reason);
    door_.reset();
    issue(out,s,contactDetour ? *contactDetour : *proof.target,direction,LocomotionPhase::Ground);
    if(crouch || s.ducked==true) out.intent.duck=core::ActionRequest::Hold;
    return out;
}
}
