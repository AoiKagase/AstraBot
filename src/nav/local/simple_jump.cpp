// SPDX-License-Identifier: MPL-2.0
#include "nav/local/simple_jump.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::nav::local {
namespace {
bool same(Binding a,Binding b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
        a.routeGeneration==b.routeGeneration && a.step==b.step;
}
bool terminal(JumpState s) noexcept {
    return s==JumpState::Complete || s==JumpState::Failed || s==JumpState::Aborted;
}
double distance(model::NavVector3 a,model::NavVector3 b) noexcept {
    return std::hypot(double(a.x)-b.x,double(a.y)-b.y);
}
bool support(const GroundedTarget& p,const runtime::MovementSnapshot& s,
             model::NavAreaId area,double tolerance) noexcept {
    return p.area==area && p.origin.isFinite() && p.floor.supported &&
        std::isfinite(p.floor.height) && p.floor.normal.isFinite() && p.floor.normal.z>=0.7f &&
        distance(p.origin,*s.position)<=0.01 && std::abs(double(p.origin.z)-s.position->z)<=tolerance &&
        std::abs(double(s.position->z)+s.hull->minimum.z-p.floor.height)<=tolerance;
}
bool validKey(const JumpAttemptKey& key) noexcept {
    return key.agent.isValid() && key.actor.isValid() && key.map.isValid() &&
        key.source.isValid() && key.target.isValid() && key.source!=key.target &&
        key.traversal==model::NavTraversalKind::Jump;
}
std::uint64_t add(std::uint64_t value,std::uint64_t amount) noexcept {
    return value>(std::numeric_limits<std::uint64_t>::max)()-amount
        ? (std::numeric_limits<std::uint64_t>::max)():value+amount;
}
}

JumpAttemptContext& JumpAttemptRegistry::acquire(JumpAttemptKey key,std::uint64_t nowUs) noexcept {
    for(auto& item:attempts_) if(validKey(item.key) && item.key==key) return item;
    JumpAttemptContext* slot=nullptr;
    for(auto& item:attempts_) if(!validKey(item.key)) { slot=&item; break; }
    if(!slot) slot=&*std::min_element(attempts_.begin(),attempts_.end(),
        [](const auto& a,const auto& b) { return a.startedUs<b.startedUs; });
    nextAttemptId_=add(nextAttemptId_,1);
    if(!nextAttemptId_) nextAttemptId_=1;
    *slot={key,nextAttemptId_,nowUs,{},false};
    return *slot;
}

void JumpAttemptRegistry::erase(JumpAttemptKey key) noexcept {
    for(auto& item:attempts_) if(validKey(item.key) && item.key==key) { item={}; return; }
}

void JumpAttemptRegistry::clear() noexcept { attempts_={}; }

JumpDecision SimpleJump::result(JumpReason reason) const noexcept {
    JumpDecision out;
    out.state=state_; out.reason=reason; out.pressTick=pressTick_;
    const auto* attempt=attempt_ ? attempt_:&localAttempt_;
    out.attemptId=attempt->attemptId; out.attemptStartedUs=attempt->startedUs;
    out.minimumSpeed=limits_.minimumSpeed; out.maximumSpeed=limits_.maximumSpeed;
    out.intent.jump=ActionRequest::Release;
    const bool duck=((plan_.sourceAttributes&1U)!=0) ||
        (plan_.flightHull && (state_==JumpState::Airborne || state_==JumpState::Recover || state_==JumpState::Complete));
    out.intent.duck=duck ? ActionRequest::Hold:ActionRequest::Release;
    return out;
}

JumpDecision SimpleJump::finish(JumpState state,JumpReason reason) noexcept {
    const bool first=!terminal(state_);
    state_=state;
    auto out=result(reason);
    out.accepted=out.terminalEvent=first;
    return out;
}

JumpDecision SimpleJump::abort() noexcept {
    return terminal(state_) ? result():finish(JumpState::Aborted,JumpReason::Cancelled);
}

JumpDecision SimpleJump::update(const JumpFeedback& f) noexcept {
    if(terminal(state_)) return result();
    const auto& s=f.movement;
    const auto fail=[&](JumpReason reason,RecoveryDisposition disposition=RecoveryDisposition::EdgeCooldown) {
        auto out=finish(JumpState::Failed,reason); out.recoveryDisposition=disposition; return out;
    };
    const auto positive=[](double n) { return std::isfinite(n) && n>0; };
    const auto hints=constraints(model::NavTraversalKind::Jump,plan_.sourceAttributes,plan_.targetAttributes);
    if(!binding_.agent.isValid() || !binding_.actor.isValid() || !binding_.map.isValid() ||
       !binding_.routeGeneration || !plan_.source.isValid() || !plan_.target.isValid() ||
       plan_.source==plan_.target || !plan_.takeoff.isFinite() || !plan_.landing.isFinite() ||
       !positive(limits_.approachSpeed) || !positive(limits_.minimumSpeed) ||
       !positive(limits_.maximumSpeed) || limits_.minimumSpeed>limits_.maximumSpeed ||
       limits_.maximumSpeed>400 || limits_.approachSpeed>limits_.maximumSpeed ||
       !positive(limits_.takeoffRadius) || !positive(limits_.landingRadius) ||
       !positive(limits_.facingDegrees) || limits_.facingDegrees>45 ||
       !positive(limits_.maximumDistance) || !positive(limits_.maximumRise) ||
       !positive(limits_.supportTolerance) || !limits_.maxQueries ||
       !limits_.approachTimeoutUs || !limits_.takeoffTimeoutUs ||
       !limits_.airborneTimeoutUs || !limits_.cooldownUs)
        return fail(JumpReason::InvalidInput,RecoveryDisposition::Hold);
    const auto length=distance(plan_.takeoff,plan_.landing);
    if(length<=0 || length>limits_.maximumDistance ||
       (plan_.landing.z<plan_.takeoff.z && !plan_.flightHull) ||
       double(plan_.landing.z)-plan_.takeoff.z>limits_.maximumRise)
        return fail(JumpReason::InvalidInput,RecoveryDisposition::Hold);
    if(!same(f.binding,binding_) || s.agent!=binding_.agent || s.actor!=binding_.actor ||
       s.map!=binding_.map || s.kind!=runtime::ActorKind::ManagedBot ||
       s.connected!=true || s.alive!=true || s.joined!=true)
        return finish(JumpState::Aborted,JumpReason::InvalidActor);
    if(!s.tick.isValid() || (started_ && !s.tick.isAfter(tick_))) return result(JumpReason::StaleTick);
    if(started_ && f.nowUs<=lastUs_) return fail(JumpReason::InvalidInput,RecoveryDisposition::Hold);
    tick_=s.tick; lastUs_=f.nowUs;
    if(!attempt_) {
        localAttempt_.key={binding_.agent,binding_.actor,binding_.map,plan_.source,plan_.target,
                           model::NavTraversalKind::Jump};
        localAttempt_.attemptId=1;
        attempt_=&localAttempt_;
    }
    if(!started_) {
        started_=true;
        if(!attempt_->startedUs) attempt_->startedUs=f.nowUs;
        startedUs_=attempt_->startedUs;
        phaseUs_=f.nowUs;
        if(attempt_->pressed) { pressTick_=attempt_->pressTick; state_=JumpState::Takeoff; }
    }
    if(!s.position || !s.position->isFinite() || !s.velocity || !s.velocity->isFinite() ||
       !s.view || !s.view->isFinite() || !s.hull || !s.hull->minimum.isFinite() ||
       !s.hull->maximum.isFinite() || s.hull->minimum.x>=s.hull->maximum.x ||
       s.hull->minimum.y>=s.hull->maximum.y || s.hull->minimum.z>=s.hull->maximum.z ||
       !s.grounded || !s.ducked || !s.speedLimit || !positive(*s.speedLimit))
        return fail(JumpReason::MissingObservation,RecoveryDisposition::Hold);

    const double ux=(double(plan_.landing.x)-plan_.takeoff.x)/length;
    const double uy=(double(plan_.landing.y)-plan_.takeoff.y)/length;
    const double yaw=std::atan2(uy,ux)*180/3.14159265358979323846;
    auto out=result(); out.accepted=true;
    out.jumpAxis={static_cast<float>(ux),static_cast<float>(uy),0}; out.speedLimit=*s.speedLimit;
    out.takeoffProof=f.takeoffProof; out.flightProof=f.flightProof; out.landingProof=f.landingProof;
    out.proofProvenance=f.proofProvenance;
    const bool flight=state_==JumpState::Airborne || state_==JumpState::Recover ||
        (state_==JumpState::Takeoff && !*s.grounded);
    const bool requiredDuck=flight ? plan_.flightHull.has_value():hints.sourceDuck;
    out.intent.duck=requiredDuck ? ActionRequest::Hold:ActionRequest::Release;
    if(!flight && *s.ducked!=hints.sourceDuck)
        return fail(JumpReason::MissingObservation,RecoveryDisposition::Hold);
    if(flight && *s.grounded && *s.ducked!=requiredDuck)
        return fail(JumpReason::WrongLanding,RecoveryDisposition::EdgeCooldown);
    const auto moving=[&](double speed) {
        out.intent.direction={ux,uy,0}; out.intent.speed=(std::min)(speed,double(*s.speedLimit));
        out.intent.view=core::IntentVector{0,yaw,0}; return out;
    };

    if(state_==JumpState::Takeoff && f.dispatch) {
        const auto& dispatch=*f.dispatch;
        if(!same(dispatch.binding,binding_) || dispatch.commandTick!=pressTick_ ||
           dispatch.dispatchTick<pressTick_ || (dispatch.dispatched && !dispatch.dispatchTick.isAfter(pressTick_)) ||
           s.tick<dispatch.dispatchTick)
            return fail(JumpReason::StaleDispatch,RecoveryDisposition::EdgeCooldown);
        if(!dispatch.dispatched) return fail(JumpReason::DispatchRejected,RecoveryDisposition::Retry);
        dispatched_=true;
    }
    if(state_==JumpState::Takeoff) {
        if(!*s.grounded) {
            if(!dispatched_) return fail(JumpReason::MissingDispatch,RecoveryDisposition::Retry);
            state_=JumpState::Airborne; phaseUs_=f.nowUs; out.state=state_; return moving(flightSpeed_);
        }
        if(f.nowUs-phaseUs_>=limits_.takeoffTimeoutUs)
            return fail(JumpReason::TakeoffTimeout,RecoveryDisposition::EdgeCooldown);
        return out;
    }
    const auto* proof=f.inspection ? &*f.inspection:nullptr;
    if(state_==JumpState::Airborne || state_==JumpState::Recover) {
        if(state_==JumpState::Airborne && f.nowUs-phaseUs_>=limits_.airborneTimeoutUs)
            return fail(JumpReason::AirborneTimeout,RecoveryDisposition::EdgeCooldown);
        if(!*s.grounded)
            return state_==JumpState::Recover
                ? fail(JumpReason::LostSupport,RecoveryDisposition::EdgeCooldown):moving(flightSpeed_);
        if(!proof || !proof->support || !support(*proof->support,s,jumpLandingArea(plan_),limits_.supportTolerance) ||
           distance(*s.position,plan_.landing)>limits_.landingRadius ||
           std::abs(double(s.position->z)-plan_.landing.z)>limits_.supportTolerance)
            return fail(JumpReason::WrongLanding,RecoveryDisposition::EdgeCooldown);
        if(state_==JumpState::Airborne) { state_=JumpState::Recover; phaseUs_=f.nowUs; out.state=state_; return out; }
        if(f.nowUs-phaseUs_>=limits_.cooldownUs) return finish(JumpState::Complete,JumpReason::None);
        return out;
    }

    const bool expired=f.nowUs>=attempt_->startedUs &&
        f.nowUs-attempt_->startedUs>=limits_.approachTimeoutUs;
    if(state_==JumpState::Accelerate &&
       (std::min)(limits_.maximumSpeed,double(*s.speedLimit))<limits_.minimumSpeed) {
        out.readiness=JumpReadiness::SpeedLimitInsufficient;
        out.recoveryDisposition=RecoveryDisposition::Hold;
        return out;
    }
    if(!proof) {
        if(f.takeoffProof==JumpProof::StaticBlocked || f.flightProof==JumpProof::StaticBlocked ||
           f.landingProof==JumpProof::StaticBlocked)
            return fail(JumpReason::Blocked,RecoveryDisposition::StructuralEdgeExclusion);
        out.reason=JumpReason::StaleInspection; out.recoveryDisposition=RecoveryDisposition::Retry;
        if(expired) return fail(JumpReason::ProofTimeout,RecoveryDisposition::EdgeCooldown);
        return out;
    }
    if(proof->stamp.agent!=s.agent || proof->stamp.actor!=s.actor || proof->stamp.map!=s.map ||
       proof->stamp.tick!=s.tick || proof->stamp.routeGeneration!=binding_.routeGeneration ||
       proof->stamp.ordinal || proof->step!=binding_.step ||
       (attempt_!=&localAttempt_ && proof->attemptId!=attempt_->attemptId) ||
       proof->queries>limits_.maxQueries || proof->origin!=*s.position ||
       (proof->velocity && *proof->velocity!=*s.velocity) ||
       proof->hull.minimum!=s.hull->minimum || proof->hull.maximum!=s.hull->maximum ||
       proof->takeoff!=plan_.takeoff || proof->landing!=plan_.landing) {
        out.reason=JumpReason::StaleInspection; out.recoveryDisposition=RecoveryDisposition::Retry;
        out.proofProvenance=JumpProofProvenance::Stale;
        if(expired) return fail(JumpReason::ProofTimeout,RecoveryDisposition::EdgeCooldown);
        return out;
    }
    out.takeoffProof=proof->takeoffProof; out.flightProof=proof->flightProof;
    out.landingProof=proof->landingProof; out.proofProvenance=proof->provenance;
    out.validatedDistance=proof->validatedDistance; out.validForUs=proof->validForUs;
    if(proof->takeoffProof==JumpProof::StaticBlocked || proof->flightProof==JumpProof::StaticBlocked ||
       proof->landingProof==JumpProof::StaticBlocked)
        return fail(JumpReason::Blocked,RecoveryDisposition::StructuralEdgeExclusion);
    if(!*s.grounded || !proof->support || !support(*proof->support,s,plan_.source,limits_.supportTolerance))
        return fail(JumpReason::MissingSupport,RecoveryDisposition::Retry);
    out.fromTakeoff=distance(*s.position,plan_.takeoff);
    if(state_==JumpState::Approach) {
        if(out.fromTakeoff<=limits_.takeoffRadius) { state_=JumpState::Align; out.state=state_; }
        else {
            if(!proof->approach || proof->approachProof!=JumpProof::Passed) {
                if(expired) return fail(JumpReason::ProofTimeout,RecoveryDisposition::EdgeCooldown);
                out.recoveryDisposition=RecoveryDisposition::Retry; return out;
            }
            const auto endpoint=proof->approach->origin;
            const auto range=distance(*s.position,endpoint);
            if(range<=0 || range>limits_.maximumDistance || distance(endpoint,plan_.takeoff)>=out.fromTakeoff)
                return fail(JumpReason::InvalidInput,RecoveryDisposition::Hold);
            out.intent.direction={(double(endpoint.x)-s.position->x)/range,
                                  (double(endpoint.y)-s.position->y)/range,0};
            out.intent.view=core::IntentVector{0,yaw,0};
            if(!core::Motor::bindLocomotion(out.intent,core::LocomotionMode::Run,*s.speedLimit,range) ||
               out.intent.validForUs<s.elapsedUs)
                return fail(JumpReason::Blocked,RecoveryDisposition::Retry);
            out.commandDirection=out.intent.direction; out.validatedDistance=range;
            out.validForUs=out.intent.validForUs; return out;
        }
    }
    if(out.fromTakeoff>limits_.takeoffRadius)
        return fail(JumpReason::OutsideTakeoff,RecoveryDisposition::EdgeCooldown);
    out.intent.view=core::IntentVector{0,yaw,0};
    const bool aligned=std::abs(std::remainder(double(s.view->y)-yaw,360.0))<=limits_.facingDegrees;
    if(state_==JumpState::Align) {
        if(aligned) { state_=JumpState::Accelerate; out.state=state_; }
        if(expired) return fail(JumpReason::VelocityTimeout,RecoveryDisposition::EdgeCooldown);
        return out;
    }
    if(!aligned) { state_=JumpState::Align; out.state=state_; return out; }
    if(proof->trajectoryReady && (proof->takeoffProof!=JumpProof::Passed || proof->flightProof!=JumpProof::Passed ||
       proof->landingProof!=JumpProof::Passed)) {
        if(expired) return fail(JumpReason::ProofTimeout,RecoveryDisposition::EdgeCooldown);
        out.recoveryDisposition=RecoveryDisposition::Retry; return out;
    }

    const double cap=(std::min)(limits_.maximumSpeed,double(*s.speedLimit));
    out.speedLimit=cap;
    if(cap<limits_.minimumSpeed) {
        out.readiness=JumpReadiness::SpeedLimitInsufficient;
        out.recoveryDisposition=RecoveryDisposition::Hold;
        return out;
    }
    const double desired=(std::min)((std::max)(limits_.approachSpeed,limits_.minimumSpeed),cap);
    const double along=s.velocity->x*ux+s.velocity->y*uy;
    const double lateral=s.velocity->x*(-uy)+s.velocity->y*ux;
    out.along=along; out.lateral=lateral; out.desiredSpeed=desired;
    if(along<limits_.minimumSpeed) out.readiness=JumpReadiness::UnderSpeed;
    else if(along>cap) out.readiness=JumpReadiness::OverSpeed;
    else if(std::abs(lateral)>limits_.minimumSpeed*0.1) out.readiness=JumpReadiness::LateralError;
    else out.readiness=JumpReadiness::Ready;

    const double ex=ux*desired-s.velocity->x,ey=uy*desired-s.velocity->y;
    const double error=std::hypot(ex,ey);
    if(error>1e-3) out.commandDirection={static_cast<float>(ex/error),static_cast<float>(ey/error),0};
    else out.commandDirection={static_cast<float>(ux),static_cast<float>(uy),0};
    if(!s.elapsedUs || s.elapsedUs>120'000) return fail(JumpReason::Blocked,RecoveryDisposition::Retry);
    const double dt=double(s.elapsedUs)/1'000'000.0;
    const double horizontalSpeed=std::hypot(double(s.velocity->x),double(s.velocity->y));
    const double requiredDistance=(horizontalSpeed+*s.speedLimit)*dt;
    const model::NavVector3 projected{
        static_cast<float>(s.position->x+s.velocity->x*dt+out.commandDirection.x*(*s.speedLimit)*dt),
        static_cast<float>(s.position->y+s.velocity->y*dt+out.commandDirection.y*(*s.speedLimit)*dt),
        s.position->z};
    if(!projected.isFinite() || distance(*s.position,plan_.takeoff)>limits_.takeoffRadius ||
       distance(projected,plan_.takeoff)>limits_.takeoffRadius)
        return fail(JumpReason::OutsideTakeoff,RecoveryDisposition::EdgeCooldown);
    if(proof->validatedDistance+0.001<requiredDistance || proof->validForUs<s.elapsedUs)
        return fail(JumpReason::Blocked,RecoveryDisposition::Retry);
    out.intent.direction=out.commandDirection;
    if(!core::Motor::bindLocomotion(out.intent,core::LocomotionMode::Run,*s.speedLimit,
                                    proof->validatedDistance) || out.intent.validForUs<s.elapsedUs)
        return fail(JumpReason::Blocked,RecoveryDisposition::Retry);
    out.validForUs=out.intent.validForUs;
    if(!proof->trajectoryReady) {
        out.recoveryDisposition=RecoveryDisposition::Retry;
        return out;
    }
    if(out.readiness!=JumpReadiness::Ready) {
        if(expired) return fail(JumpReason::VelocityTimeout,RecoveryDisposition::EdgeCooldown);
        out.recoveryDisposition=RecoveryDisposition::Retry; return out;
    }
    flightSpeed_=along;
    if(!attempt_->pressed) {
        attempt_->pressed=true; attempt_->pressTick=s.tick;
        pressTick_=s.tick; phaseUs_=f.nowUs; state_=JumpState::Takeoff;
        out.state=state_; out.pressTick=pressTick_; out.intent.jump=ActionRequest::Press;
    } else {
        pressTick_=attempt_->pressTick; state_=JumpState::Takeoff; out.state=state_; out.pressTick=pressTick_;
    }
    return out;
}
}
