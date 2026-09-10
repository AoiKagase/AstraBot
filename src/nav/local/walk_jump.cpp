// SPDX-License-Identifier: MPL-2.0
#include "nav/local/walk.hpp"
#include <algorithm>
#include <cmath>

namespace astrabot::nav::local {
namespace {
bool same(Binding a,Binding b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
        a.routeGeneration==b.routeGeneration && a.step==b.step;
}
bool inside(const model::NavExtent& e,model::NavVector3 p,runtime::HullDimensions h) noexcept {
    return double(p.x)+h.minimum.x>=e.northWest.x && double(p.x)+h.maximum.x<=e.southEast.x &&
        double(p.y)+h.minimum.y>=e.northWest.y && double(p.y)+h.maximum.y<=e.southEast.y;
}
class JumpQueries final : public runtime::IWorldQueries {
public:
    JumpQueries(runtime::IWorldQueries& port,std::uint32_t reserved,std::uint32_t maximum) noexcept
        : issued(reserved),port_(port),maximum_(maximum) {}
    std::uint32_t issued;
    runtime::WorldQueryResult query(const runtime::QueryRequest& request) override {
        if(issued==maximum_) {
            runtime::WorldQueryResult r; r.stamp=request.stamp; r.kind=request.kind;
            r.error=runtime::QueryError::BudgetExceeded; return r;
        }
        auto wire=request; wire.stamp.ordinal=++issued;
        auto reply=port_.query(wire);
        if(reply.stamp==wire.stamp) reply.stamp=request.stamp;
        else reply.stamp={};
        return reply;
    }
private:
    runtime::IWorldQueries& port_;
    std::uint32_t maximum_;
};
bool launchReady(const runtime::MovementSnapshot& s,JumpPlan plan,JumpLimits limits) noexcept {
    if(!s.velocity || !s.velocity->isFinite() || !s.view || !s.view->isFinite() || !s.speedLimit) return false;
    const double length=std::hypot(double(plan.landing.x)-plan.takeoff.x,double(plan.landing.y)-plan.takeoff.y);
    if(length<=0) return false;
    const double ux=(double(plan.landing.x)-plan.takeoff.x)/length,uy=(double(plan.landing.y)-plan.takeoff.y)/length;
    const double speed=s.velocity->x*ux+s.velocity->y*uy,lateral=std::abs(s.velocity->x*uy-s.velocity->y*ux);
    const double yaw=std::atan2(uy,ux)*180/3.14159265358979323846;
    return speed>=limits.minimumSpeed && speed<=limits.maximumSpeed && speed<=*s.speedLimit &&
        lateral<=limits.minimumSpeed*0.1 && std::abs(std::remainder(double(s.view->y)-yaw,360.0))<=limits.facingDegrees;
}
}
WalkDecision Walk::updateDrop(WalkDecision out,const runtime::MovementSnapshot& s,
    const query::NavSpatialIndex& index,core::MapGeneration indexMap,runtime::IWorldQueries& port,
    std::uint64_t now,std::uint32_t reserved,std::optional<JumpPhysics> physics) noexcept {
    out.queries=reserved; out.dropState=dropState_; out.dropPlan=dropPlan_; out.jumpPhysics=physics;
    const auto fail=[&](DropReason reason) {
        out.dropReason=reason; out.dropState=DropState::Failed; out.intent={};
        return finish(out,WalkState::Failed,WalkReason::ProbeFailed);
    };
    if(!limits_.drop) return fail(DropReason::Disabled);
    const auto& l=*limits_.drop;
    const auto positive=[](double v) { return std::isfinite(v) && v>0; };
    if(cursor_.exhausted() || !positive(l.maximumFall) || l.maximumFall>128 ||
       !std::isfinite(l.maximumGap) || l.maximumGap<0 || l.maximumGap>32 || !positive(l.speed) ||
       l.speed>400 || !positive(l.arrivalTolerance) || l.arrivalTolerance>8 ||
       !l.approachTimeoutUs || !l.airborneTimeoutUs || !l.maxSegments || l.maxSegments>12 ||
       !l.maxQueries || l.maxQueries>21 || reserved>=l.maxQueries || reserved>=limits_.probe.maxQueries)
        return fail(DropReason::InvalidInput);
    if(!s.position || !s.position->isFinite() || !s.velocity || !s.velocity->isFinite() ||
       !s.hull || !s.hull->minimum.isFinite() || !s.hull->maximum.isFinite() ||
       s.hull->minimum.x>=s.hull->maximum.x || s.hull->minimum.y>=s.hull->maximum.y ||
       s.hull->minimum.z>=s.hull->maximum.z || !s.grounded || s.ducked!=false ||
       !s.speedLimit || !positive(*s.speedLimit) || !s.elapsedUs || s.elapsedUs>250000)
        return fail(DropReason::MissingObservation);
    if(!physics || !same(physics->binding,out.binding) || physics->tick!=s.tick ||
       !positive(physics->gravity) || (dropPlan_ && dropGravity_!=physics->gravity))
        return fail(DropReason::StalePhysics);
    const auto& t=corridor_->transitions()[cursor_.index()];
    const auto hints=constraints(t.edge.traversal,t.sourceAttributes,t.targetAttributes);
    if(t.effectiveTraversal!=model::NavTraversalKind::Drop || t.edge.external ||
       !hints || hints.kind!=model::NavTraversalKind::Walk)
        return fail(DropReason::UnsafeGeometry);
    const auto goal=index.containing(goal_,limits_.probe.navTolerance);
    if(!goal || !*goal.value || (**goal.value).areaId!=corridor_->goal())
        return fail(DropReason::UnsafeGeometry);
    const auto maximum=(std::min)(l.maxQueries,limits_.probe.maxQueries);
    JumpQueries queries(port,reserved,maximum);
    auto groundLimits=limits_.probe; groundLimits.maxQueries=maximum-reserved;
    const auto doneFail=[&](DropReason reason) { out.queries=queries.issued; return fail(reason); };
    const auto& p=*s.position;
    const bool vertical=t.edge.direction==1 || t.edge.direction==3;
    const double sign=(t.edge.direction==1 || t.edge.direction==2) ? 1.0:-1.0;
    const double ux=vertical ? sign:0, uy=vertical ? 0:sign;
    if(!dropPlan_) {
        if(s.grounded!=true) return doneFail(DropReason::MissingSupport);
        const auto projected=corridor_->target(cursor_.index(),{p.x,p.y,p.z},1);
        if(!projected) return doneFail(DropReason::UnsafeGeometry);
        const auto q=*projected.value;
        const double tangent=vertical ? q.y:q.x;
        const double boundary=vertical ? q.x:q.y;
        const double targetBoundary=vertical ? t.targetLow.x:t.targetLow.y;
        const double targetInset=(vertical ? (sign>0 ? -s.hull->minimum.x:s.hull->maximum.x)
                                                         : (sign>0 ? -s.hull->minimum.y:s.hull->maximum.y))+l.arrivalTolerance;
        const double landingNormal=targetBoundary+sign*targetInset;
        model::NavVector3 takeoff{static_cast<float>(vertical ? boundary-sign:tangent),
            static_cast<float>(vertical ? tangent:boundary-sign),0};
        model::NavVector3 landing{static_cast<float>(vertical ? landingNormal:tangent),
            static_cast<float>(vertical ? tangent:landingNormal),0};
        takeoff.z=static_cast<float>(query::projectToArea(t.sourceExtent,takeoff).z-s.hull->minimum.z);
        landing.z=static_cast<float>(query::projectToArea(t.targetExtent,landing).z-s.hull->minimum.z);
        const double fall=double(takeoff.z)-landing.z, gap=(targetBoundary-boundary)*sign;
        if(!takeoff.isFinite() || !landing.isFinite() || fall<=limits_.probe.maxStepUp ||
           fall>l.maximumFall || gap<0 || gap>l.maximumGap || !inside(t.targetExtent,landing,*s.hull))
            return doneFail(DropReason::UnsafeGeometry);
        dropPlan_=DropPlan{t.edge.source,t.edge.target,takeoff,landing,fall,gap};
        dropGravity_=physics->gravity; dropStartedUs_=now; dropLastUs_=now;
    }
    out.dropPlan=dropPlan_;
    if(now<dropLastUs_) return doneFail(DropReason::Timeout);
    dropLastUs_=now;
    const auto plan=*dropPlan_;
    if(dropState_!=DropState::Airborne && now-dropStartedUs_>=l.approachTimeoutUs)
        return doneFail(DropReason::Timeout);
    if(s.grounded==false) {
        if(dropState_==DropState::Approach) return doneFail(DropReason::MissingSupport);
        if(dropState_!=DropState::Airborne) { dropState_=DropState::Airborne; dropAirborneUs_=now; }
        if(now-dropAirborneUs_>=l.airborneTimeoutUs) return doneFail(DropReason::Timeout);
    } else {
        const auto supported=GroundProbe::locate(s,binding_.routeGeneration,index,indexMap,queries,groundLimits);
        out.queries=queries.issued; out.probeReason=supported.reason;
        if(!supported) return doneFail(DropReason::MissingSupport);
        out.support=supported.target;
        if(dropState_==DropState::Airborne) {
            if(supported.target->area!=plan.target || !inside(t.targetExtent,p,*s.hull) ||
               std::abs(double(p.z)-plan.landing.z)>limits_.probe.supportTolerance ||
               !cursor_.advance(out.binding.step,supported.target->area,true))
                return doneFail(DropReason::WrongLanding);
            out.dropState=DropState::Landed; out.primitiveEvent=PrimitiveEvent::Complete;
            dropPlan_.reset(); dropState_=DropState::Approach; primitive_=Primitive{};
            return out;
        }
        if(supported.target->area!=plan.source) return doneFail(DropReason::WrongLanding);
    }
    const double distance=std::hypot(double(plan.takeoff.x)-p.x,double(plan.takeoff.y)-p.y);
    if(dropState_==DropState::Approach && distance>l.arrivalTolerance) {
        groundLimits.maxQueries=maximum-queries.issued;
        const auto proof=GroundProbe::inspect(s,binding_.routeGeneration,plan.source,
            plan.takeoff.x,plan.takeoff.y,index,indexMap,queries,groundLimits);
        out.queries=queries.issued; out.probeReason=proof.reason;
        if(!proof || proof.target->area!=plan.source) return doneFail(DropReason::MissingSupport);
        out.target=proof.target;
        const double dx=double(proof.target->origin.x)-p.x,dy=double(proof.target->origin.y)-p.y;
        const double range=std::hypot(dx,dy);
        if(range<=0) return doneFail(DropReason::Blocked);
        out.intent.direction={dx/range,dy/range,0};
        out.intent.speed=(std::min)({l.speed,double(*s.speedLimit),range/0.12});
        out.progressDirection={ux,uy,0}; return out;
    }
    DropReason queryFailure=DropReason::None;
    auto sweptHull=*s.hull;
    const auto fetch=[&](runtime::QueryKind kind,model::NavVector3 a,model::NavVector3 b)
        ->std::optional<runtime::WorldQueryResult> {
        if(queries.issued>=maximum) { queryFailure=DropReason::BudgetExceeded; return {}; }
        runtime::QueryRequest request{{binding_.agent,binding_.actor,binding_.map,s.tick,
            binding_.routeGeneration,1},kind,a,b,s.hull,limits_.probe.navTolerance};
        if(kind==runtime::QueryKind::SweptHull) request.hull=sweptHull;
        try {
            const auto r=queries.query(request); out.queries=queries.issued;
            if(!(r.stamp==request.stamp) || r.kind!=kind) queryFailure=DropReason::StaleQuery;
            else if(r.error!=runtime::QueryError::None) queryFailure=DropReason::QueryFailed;
            else return r;
        } catch(...) { queryFailure=DropReason::QueryFailed; }
        return {};
    };
    const auto sweep=[&](model::NavVector3 a,model::NavVector3 b) {
        const auto r=fetch(runtime::QueryKind::SweptHull,a,b);
        if(!r) return false;
        if(!r->hull || r->hull->startSolid || r->hull->fraction!=1 ||
           !r->hull->end.isFinite() || !r->hull->normal.isFinite() ||
           std::hypot(double(r->hull->end.x)-b.x,double(r->hull->end.y)-b.y)>0.001 ||
           std::abs(double(r->hull->end.z)-b.z)>0.001) { queryFailure=DropReason::Blocked; return false; }
        return true;
    };
    // Release starts after the complete hull has cleared the source ledge.
    const double leading=vertical ? (sign>0 ? -s.hull->minimum.x:s.hull->maximum.x)
                                 : (sign>0 ? -s.hull->minimum.y:s.hull->maximum.y);
    model::NavVector3 release=plan.takeoff;
    release.x=static_cast<float>(release.x+ux*(leading+2));
    release.y=static_cast<float>(release.y+uy*(leading+2));
    const bool airborne=dropState_==DropState::Airborne;
    const auto origin=airborne ? p:release;
    const double height=double(origin.z)-plan.landing.z;
    const double vz=airborne ? s.velocity->z:0;
    if(height<0 || height>l.maximumFall || vz>1) return doneFail(DropReason::UnsafeGeometry);
    const double flight=(vz+std::sqrt(vz*vz+2*physics->gravity*height))/physics->gravity;
    if(!positive(flight) || flight>double(l.airborneTimeoutUs)/1000000)
        return doneFail(DropReason::UnsafeGeometry);
    const double desired=std::hypot(double(plan.landing.x)-release.x,double(plan.landing.y)-release.y)/flight;
    const double forward=s.velocity->x*ux+s.velocity->y*uy;
    const double lateral=std::abs(s.velocity->x*uy-s.velocity->y*ux);
    if(forward<0 || forward>l.speed || forward>*s.speedLimit ||
       (airborne && lateral>4)) return doneFail(DropReason::UnsafeVelocity);
    if(!airborne && (desired<=0 || desired>l.speed || desired>*s.speedLimit))
        return doneFail(DropReason::UnsafeVelocity);
    if(!airborne && (forward<desired*0.8 || forward>desired*1.2 || lateral>4)) {
        // Accelerating is allowed only while the entire issued segment remains
        // physically supported on the source. Never push over the lip blind.
        const double travel=desired*double(s.elapsedUs)/1000000;
        const float x=static_cast<float>(p.x+ux*travel),y=static_cast<float>(p.y+uy*travel);
        if(!query::containsXY(t.sourceExtent,{x,y,p.z})) return doneFail(DropReason::UnsafeVelocity);
        groundLimits.maxQueries=maximum-queries.issued;
        const auto proof=GroundProbe::inspect(s,binding_.routeGeneration,plan.source,x,y,index,indexMap,queries,groundLimits);
        if(!proof || proof.target->area!=plan.source) return doneFail(DropReason::MissingSupport);
        if(std::hypot(double(proof.target->origin.x)-x,double(proof.target->origin.y)-y)>0.01)
            return doneFail(DropReason::MissingSupport);
        out.queries=queries.issued; out.target=proof.target; out.intent.direction={ux,uy,0};
        out.intent.speed=desired; out.progressDirection={ux,uy,0}; return out;
    }
    model::NavVector3 landing{static_cast<float>(origin.x+s.velocity->x*flight),
        static_cast<float>(origin.y+s.velocity->y*flight),plan.landing.z};
    if(!landing.isFinite() || !inside(t.targetExtent,landing,*s.hull)) return doneFail(DropReason::UnsafeVelocity);
    const auto support=fetch(runtime::QueryKind::GroundedArea,landing,landing);
    if(!support) return doneFail(queryFailure);
    if(!support->ground || support->ground->area!=plan.target || !support->ground->floor)
        return doneFail(DropReason::MissingSupport);
    const auto floor=*support->ground->floor;
    const double normal=double(floor.normal.x)*floor.normal.x+double(floor.normal.y)*floor.normal.y+double(floor.normal.z)*floor.normal.z;
    const auto area=index.containing({landing.x,landing.y,floor.height},limits_.probe.navTolerance);
    if(!floor.supported || !floor.normal.isFinite() || normal<0.99 || normal>1.01 ||
       floor.normal.z<limits_.probe.minNormalZ || !std::isfinite(floor.height) ||
       std::abs(double(landing.z)+s.hull->minimum.z-floor.height)>limits_.probe.supportTolerance ||
       !area || !*area.value || (**area.value).areaId!=plan.target)
        return doneFail(DropReason::MissingSupport);
    const auto clear=fetch(runtime::QueryKind::Clearance,landing,landing);
    if(!clear) return doneFail(queryFailure);
    if(!clear->clearance || !clear->clearance->clear) return doneFail(DropReason::Blocked);
    if(!airborne && !sweep(p,release)) return doneFail(queryFailure);
    const double count=std::ceil(flight/0.05);
    if(count<1 || count>l.maxSegments) return doneFail(DropReason::BudgetExceeded);
    const auto segments=static_cast<std::uint32_t>(count);
    // A falling parabola is above its chord. Sweep the complete upper envelope
    // so discrete chords cannot certify a ceiling the true hull would hit.
    const double chordSeconds=flight/segments;
    sweptHull.maximum.z=static_cast<float>(sweptHull.maximum.z+physics->gravity*chordSeconds*chordSeconds/8+0.01);
    if(!sweptHull.maximum.isFinite()) return doneFail(DropReason::UnsafeGeometry);
    auto previous=origin;
    for(std::uint32_t i=1;i<=segments;++i) {
        const double time=flight*i/segments;
        model::NavVector3 next{static_cast<float>(origin.x+s.velocity->x*time),
            static_cast<float>(origin.y+s.velocity->y*time),
            static_cast<float>(origin.z+vz*time-0.5*physics->gravity*time*time)};
        if(!next.isFinite() || !sweep(previous,next)) return doneFail(queryFailure);
        previous=next;
    }
    if(!airborne) {
        dropState_=DropState::StepOff;
        out.intent.direction={ux,uy,0}; out.intent.speed=forward;
        out.intent.view=core::IntentVector{0,std::atan2(uy,ux)*180/3.14159265358979323846,0};
    }
    out.dropState=dropState_; out.progressDirection={ux,uy,0}; return out;
}

bool Walk::reportJumpDispatch(const JumpDispatch& dispatch) noexcept {
    auto expected=binding_; expected.step=cursor_.index();
    if(!jump_ || jump_->state()!=JumpState::Takeoff || !jumpPressTick_.isValid() || jumpDispatchSeen_ ||
       !same(dispatch.binding,expected) || dispatch.commandTick!=jumpPressTick_) return false;
    jumpDispatch_=dispatch; jumpDispatchSeen_=true; return true;
}
WalkDecision Walk::updateJump(WalkDecision out,const runtime::MovementSnapshot& s,const query::NavSpatialIndex& index,
    core::MapGeneration indexMap,runtime::IWorldQueries& port,std::uint64_t nowUs,std::uint32_t reserved,
    std::optional<JumpPhysics> physics) noexcept {
    out.queries=reserved; out.jumpState=jump_ ? jump_->state():JumpState::Approach;
    out.jumpPlan=jumpPlan_; out.jumpPhysics=physics; out.jumpPressTick=jumpPressTick_;
    const auto fail=[&](JumpReason reason) {
        out.jumpReason=reason; out.jumpState=JumpState::Failed;
        return finish(out,WalkState::Failed,WalkReason::JumpFailed);
    };
    if(!limits_.jump || cursor_.exhausted()) return finish(out,WalkState::Failed,WalkReason::UnsupportedTraversal);
    const auto& t=corridor_->transitions()[cursor_.index()];
    const auto hints=constraints(t.edge.traversal,t.sourceAttributes,t.targetAttributes);
    out.constraintReason=hints.reason;
    if(!hints || hints.kind!=model::NavTraversalKind::Jump || t.edge.external)
        return finish(out,WalkState::Failed,WalkReason::UnsupportedTraversal);
    const auto goal=index.containing(goal_,limits_.probe.navTolerance);
    if(!goal || !*goal.value || (**goal.value).areaId!=corridor_->goal())
        return finish(out,WalkState::Failed,WalkReason::InvalidGoal);
    if(!physics) return fail(JumpReason::MissingObservation);
    if(!same(physics->binding,out.binding) || physics->tick!=s.tick || !std::isfinite(physics->gravity) ||
       physics->gravity<=0 || !std::isfinite(physics->verticalImpulse) || physics->verticalImpulse<=0 ||
       (jumpPhysics_ && (jumpPhysics_->gravity!=physics->gravity || jumpPhysics_->verticalImpulse!=physics->verticalImpulse)))
        return fail(JumpReason::StaleInspection);
    const auto& profile=*limits_.jump;
    const auto maximum=(std::min)({limits_.probe.maxQueries,profile.motion.maxQueries,profile.flight.maxQueries,21U});
    if(!maximum || reserved>maximum) return fail(JumpReason::InvalidInput);
    jumpPhysics_=physics;
    if(!jump_) {
        if(limits_.crouch.transitionTimeoutUs) {
            if(!crouch_) crouch_.emplace(binding_,limits_.crouch);
            const auto pose=crouch_->update(s,false,nowUs,port,reserved,maximum);
            out.queries+=pose.queries; posture_=pose.state; postureReason_=pose.reason; postureAction_=pose.intent.duck;
            if(pose.terminalEvent) return finish(out,WalkState::Failed,WalkReason::PostureFailed);
            if(!pose.movementAllowed) return out;
        }
        const auto candidate=JumpGeometry::derive(*corridor_,out.binding,s,profile.motion,profile.geometry);
        out.jumpGeometryReason=candidate.reason;
        if(!candidate) return fail(JumpReason::InvalidInput);
        const auto entered=primitive_.enter(out.binding,t,s.tick);
        out.primitiveEvent=entered.event;
        if(!entered.accepted || entered.state!=PrimitiveState::Running) return fail(JumpReason::InvalidInput);
        jumpPlan_=candidate.plan; jump_.emplace(out.binding,*jumpPlan_,profile.motion);
        jumpDispatch_.reset(); jumpDispatchSeen_=false; jumpPressTick_={};
        out.jumpPlan=jumpPlan_; out.jumpPressTick={}; return out; // Primitive entry owns its own tick.
    }
    JumpFeedback feedback{out.binding,s,nowUs,{},jumpDispatch_}; jumpDispatch_.reset();
    JumpQueries queries(port,reserved,maximum);
    const auto state=jump_->state();
    // Takeoff waits for dispatch/observed airborne without issuing ground motion.
    // Airborne decisions need no fictitious support. Landing always does.
    if(s.grounded==true && state!=JumpState::Takeoff) {
        JumpProbeResult proof;
        if(reserved==maximum) proof.reason=JumpProbeReason::BudgetExceeded;
        else {
            auto ground=limits_.probe; ground.maxQueries=maximum-reserved;
            if(state==JumpState::Airborne || state==JumpState::Recover)
                proof=JumpProbe::land(s,out.binding,*jumpPlan_,profile.motion,ground,index,indexMap,queries);
            else if(state==JumpState::Accelerate && launchReady(s,*jumpPlan_,profile.motion)) {
                auto flight=profile.flight; flight.maxQueries=maximum-reserved;
                proof=JumpProbe::launch(s,out.binding,*jumpPlan_,profile.motion,*physics,flight,index,indexMap,queries);
            } else proof=JumpProbe::prepare(s,out.binding,*jumpPlan_,profile.motion,ground,index,indexMap,queries);
        }
        out.jumpProbeReason=proof.reason;
        if(proof) {
            feedback.inspection=proof.inspection;
            feedback.inspection->queries=queries.issued;
            out.support=proof.inspection->support; out.target=proof.inspection->approach;
        }
    }
    out.queries=queries.issued;
    const auto decision=jump_->update(feedback);
    out.jumpState=decision.state; out.jumpReason=decision.reason; out.jumpPressTick=decision.pressTick;
    out.intent=decision.intent;
    if(decision.intent.jump==ActionRequest::Press) jumpPressTick_=decision.pressTick;
    if(decision.state==JumpState::Failed || decision.state==JumpState::Aborted)
        return finish(out,decision.state==JumpState::Aborted ? WalkState::Aborted:WalkState::Failed,WalkReason::JumpFailed);
    if(decision.state==JumpState::Complete) {
        if(!out.support || !s.position || !s.hull ||
           !(t.targetFit==corridor::AreaFit::MicroTransit ? query::containsXY(t.targetExtent,*s.position)
                                                       : inside(t.targetExtent,*s.position,*s.hull)))
            return fail(JumpReason::WrongLanding);
        const auto completed=primitive_.update({out.binding,s.tick,Progress::Complete,{},out.support->area,true});
        if(!completed.accepted || completed.state!=PrimitiveState::Complete || !cursor_.advance(out.binding.step,out.support->area,true))
            return fail(JumpReason::WrongLanding);
        out.primitiveEvent=completed.event; completedJumpStep_=out.binding.step;
        primitive_=Primitive{}; jump_.reset(); jumpPlan_.reset(); jumpPhysics_.reset(); jumpDispatch_.reset();
        jumpPressTick_={};
        out.intent={}; out.intent.jump=ActionRequest::Release;
    }
    return out;
}
}
