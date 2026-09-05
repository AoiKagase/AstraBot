// SPDX-License-Identifier: MPL-2.0
#include "nav/local/simple_jump.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot::nav::local {
namespace {
bool same(Binding a,Binding b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map && a.routeGeneration==b.routeGeneration && a.step==b.step;
}
bool terminal(JumpState s) noexcept { return s==JumpState::Complete || s==JumpState::Failed || s==JumpState::Aborted; }
double distance(model::NavVector3 a,model::NavVector3 b) noexcept { return std::hypot(double(a.x)-b.x,double(a.y)-b.y); }
bool support(const GroundedTarget& p,const runtime::MovementSnapshot& s,model::NavAreaId area,double tolerance) noexcept {
    return p.area==area && p.origin.isFinite() && p.floor.supported && std::isfinite(p.floor.height) &&
        p.floor.normal.isFinite() && p.floor.normal.z>=0.7f && distance(p.origin,*s.position)<=0.01 &&
        std::abs(double(p.origin.z)-s.position->z)<=tolerance &&
        std::abs(double(s.position->z)+s.hull->minimum.z-p.floor.height)<=tolerance;
}
}
JumpDecision SimpleJump::result(JumpReason reason) const noexcept {
    JumpDecision out; out.state=state_; out.reason=reason; out.pressTick=pressTick_; out.intent.jump=ActionRequest::Release; return out;
}
JumpDecision SimpleJump::finish(JumpState state,JumpReason reason) noexcept {
    const bool first=!terminal(state_); state_=state; auto out=result(reason); out.accepted=out.terminalEvent=first; return out;
}
JumpDecision SimpleJump::abort() noexcept {
    return terminal(state_) ? result():finish(JumpState::Aborted,JumpReason::Cancelled);
}
JumpDecision SimpleJump::update(const JumpFeedback& f) noexcept {
    if(terminal(state_)) return result();
    const auto& s=f.movement;
    const auto fail=[&](JumpReason reason) { return finish(JumpState::Failed,reason); };
    const auto positive=[](double n) { return std::isfinite(n) && n>0; };
    const auto hints=constraints(model::NavTraversalKind::Jump,plan_.sourceAttributes,plan_.targetAttributes);
    if(!binding_.agent.isValid() || !binding_.actor.isValid() || !binding_.map.isValid() || !binding_.routeGeneration ||
       !plan_.source.isValid() || !plan_.target.isValid() || plan_.source==plan_.target || !hints ||
       !plan_.takeoff.isFinite() || !plan_.landing.isFinite() ||
       !positive(limits_.approachSpeed) || !positive(limits_.minimumSpeed) || !positive(limits_.maximumSpeed) ||
       limits_.minimumSpeed>limits_.maximumSpeed || limits_.maximumSpeed>400 || limits_.approachSpeed>limits_.maximumSpeed ||
       !positive(limits_.takeoffRadius) || !positive(limits_.landingRadius) || !positive(limits_.facingDegrees) || limits_.facingDegrees>45 ||
       !positive(limits_.maximumDistance) || !positive(limits_.maximumRise) || !positive(limits_.supportTolerance) ||
       !limits_.maxQueries || !limits_.approachTimeoutUs || !limits_.takeoffTimeoutUs || !limits_.airborneTimeoutUs || !limits_.cooldownUs)
        return fail(JumpReason::InvalidInput);
    const auto length=distance(plan_.takeoff,plan_.landing);
    if(length<=0 || length>limits_.maximumDistance || plan_.landing.z<plan_.takeoff.z ||
       double(plan_.landing.z)-plan_.takeoff.z>limits_.maximumRise) return fail(JumpReason::InvalidInput);
    if(!same(f.binding,binding_) || s.agent!=binding_.agent || s.actor!=binding_.actor || s.map!=binding_.map ||
       s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true || s.alive!=true || s.joined!=true)
        return finish(JumpState::Aborted,JumpReason::InvalidActor);
    if(!s.tick.isValid() || (started_ && !s.tick.isAfter(tick_))) return result(JumpReason::StaleTick);
    if(started_ && f.nowUs<=lastUs_) return fail(JumpReason::InvalidInput);
    tick_=s.tick; lastUs_=f.nowUs;
    if(!started_) { started_=true; startedUs_=phaseUs_=f.nowUs; }
    if(!s.position || !s.position->isFinite() || !s.velocity || !s.velocity->isFinite() ||
       !s.view || !s.view->isFinite() || !s.hull || !s.hull->minimum.isFinite() || !s.hull->maximum.isFinite() ||
       s.hull->minimum.x>=s.hull->maximum.x || s.hull->minimum.y>=s.hull->maximum.y || s.hull->minimum.z>=s.hull->maximum.z ||
       !s.grounded || s.ducked!=false || !s.speedLimit || !std::isfinite(*s.speedLimit) || *s.speedLimit<limits_.minimumSpeed)
        return fail(JumpReason::MissingObservation);
    const auto* proof=f.inspection ? &*f.inspection:nullptr;
    if(proof && (proof->stamp.agent!=s.agent || proof->stamp.actor!=s.actor || proof->stamp.map!=s.map ||
       proof->stamp.tick!=s.tick || proof->stamp.routeGeneration!=binding_.routeGeneration || proof->stamp.ordinal ||
       proof->step!=binding_.step || !proof->queries || proof->queries>limits_.maxQueries || proof->origin!=*s.position ||
       (proof->velocity && *proof->velocity!=*s.velocity) ||
       proof->hull.minimum!=s.hull->minimum || proof->hull.maximum!=s.hull->maximum ||
       proof->takeoff!=plan_.takeoff || proof->landing!=plan_.landing))
        return fail(JumpReason::StaleInspection);
    const double ux=(double(plan_.landing.x)-plan_.takeoff.x)/length,uy=(double(plan_.landing.y)-plan_.takeoff.y)/length;
    const double yaw=std::atan2(uy,ux)*180/3.14159265358979323846;
    auto out=result(); out.accepted=true;
    const auto moving=[&](double speed) {
        out.intent.direction={ux,uy,0}; out.intent.speed=(std::min)(speed,double(*s.speedLimit));
        out.intent.view=core::IntentVector{0,yaw,0}; return out;
    };
    if(state_==JumpState::Takeoff && f.dispatch) {
        const auto& dispatch=*f.dispatch;
        if(!same(dispatch.binding,binding_) || dispatch.commandTick!=pressTick_ ||
           !dispatch.dispatchTick.isAfter(pressTick_) || s.tick<dispatch.dispatchTick) return fail(JumpReason::StaleDispatch);
        if(!dispatch.dispatched) return fail(JumpReason::DispatchRejected);
        dispatched_=true;
    }
    if(state_==JumpState::Takeoff) {
        if(!*s.grounded) {
            if(!dispatched_) return fail(JumpReason::MissingDispatch);
            state_=JumpState::Airborne; phaseUs_=f.nowUs; out.state=state_; return moving(flightSpeed_);
        }
        if(f.nowUs-phaseUs_>=limits_.takeoffTimeoutUs) return fail(JumpReason::TakeoffTimeout);
        return out; // No additional ground translation while waiting for actual takeoff.
    }
    if(state_==JumpState::Airborne || state_==JumpState::Recover) {
        if(state_==JumpState::Airborne && f.nowUs-phaseUs_>=limits_.airborneTimeoutUs) return fail(JumpReason::AirborneTimeout);
        if(!*s.grounded) return state_==JumpState::Recover ? fail(JumpReason::LostSupport):moving(flightSpeed_);
        if(!proof || !proof->support || !support(*proof->support,s,plan_.target,limits_.supportTolerance) ||
           distance(*s.position,plan_.landing)>limits_.landingRadius ||
           std::abs(double(s.position->z)-plan_.landing.z)>limits_.supportTolerance) return fail(JumpReason::WrongLanding);
        if(state_==JumpState::Airborne) { state_=JumpState::Recover; phaseUs_=f.nowUs; out.state=state_; return out; }
        if(f.nowUs-phaseUs_>=limits_.cooldownUs) return finish(JumpState::Complete,JumpReason::None);
        return out;
    }
    if(f.nowUs-startedUs_>=limits_.approachTimeoutUs) return fail(JumpReason::ApproachTimeout);
    if(!*s.grounded || !proof || !proof->support || !support(*proof->support,s,plan_.source,limits_.supportTolerance))
        return fail(JumpReason::MissingSupport);
    const auto fromTakeoff=distance(*s.position,plan_.takeoff);
    if(state_==JumpState::Approach) {
        if(fromTakeoff<=limits_.takeoffRadius) { state_=JumpState::Align; out.state=state_; return out; }
        if(proof->approachClear!=true || !proof->approach || !proof->approach->origin.isFinite() ||
           proof->approach->area!=plan_.source || !proof->approach->floor.supported ||
           !std::isfinite(proof->approach->floor.height) || !proof->approach->floor.normal.isFinite() ||
           proof->approach->floor.normal.z<0.7f || std::abs(double(proof->approach->origin.z)+s.hull->minimum.z-
               proof->approach->floor.height)>limits_.supportTolerance) return fail(JumpReason::Blocked);
        const auto endpoint=proof->approach->origin; const auto range=distance(*s.position,endpoint);
        if(range<=0 || range>limits_.maximumDistance || distance(endpoint,plan_.takeoff)>=fromTakeoff) return fail(JumpReason::InvalidInput);
        out.intent.direction={(double(endpoint.x)-s.position->x)/range,(double(endpoint.y)-s.position->y)/range,0};
        out.intent.speed=(std::min)({limits_.approachSpeed,double(*s.speedLimit),range/0.120}); return out;
    }
    if(fromTakeoff>limits_.takeoffRadius) return fail(JumpReason::OutsideTakeoff);
    out.intent.view=core::IntentVector{0,yaw,0};
    const bool aligned=std::abs(std::remainder(double(s.view->y)-yaw,360.0))<=limits_.facingDegrees;
    if(state_==JumpState::Align) {
        if(aligned) { state_=JumpState::Accelerate; out.state=state_; }
        return out;
    }
    if(!aligned) { state_=JumpState::Align; out.state=state_; return out; }
    if(proof->takeoffClear!=true) return fail(JumpReason::Blocked);
    const double speed=s.velocity->x*ux+s.velocity->y*uy;
    const double lateral=std::abs(s.velocity->x*uy-s.velocity->y*ux);
    if(speed>=limits_.minimumSpeed && speed<=limits_.maximumSpeed && speed<=*s.speedLimit && lateral<=limits_.minimumSpeed*0.1) {
        if(!proof->velocity || proof->flightClear!=true || proof->landingClear!=true) return fail(JumpReason::Blocked);
        flightSpeed_=speed; pressTick_=s.tick; phaseUs_=f.nowUs; state_=JumpState::Takeoff;
        out.state=state_; out.pressTick=pressTick_; out.intent.jump=ActionRequest::Press; return moving(flightSpeed_);
    }
    // The host must validate every approach/acceleration segment and must not
    // let a cached command outrun this finite takeoff region.
    const double px=double(s.position->x)-plan_.takeoff.x,py=double(s.position->y)-plan_.takeoff.y;
    const double along=px*ux+py*uy,side=px*uy-py*ux;
    const double remaining=std::sqrt((std::max)(0.0,limits_.takeoffRadius*limits_.takeoffRadius-side*side))-along;
    return moving((std::min)(limits_.approachSpeed,(std::max)(0.0,remaining)/0.120));
}
}
