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
        if(!out.support || !s.position || !s.hull || !inside(t.targetExtent,*s.position,*s.hull))
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
