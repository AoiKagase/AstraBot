// SPDX-License-Identifier: MPL-2.0
#include "nav/local/crouch.hpp"
#include <cmath>
namespace astrabot::nav::local {
namespace {
bool same(const runtime::HullDimensions& a,const runtime::HullDimensions& b) noexcept {
    return a.minimum==b.minimum && a.maximum==b.maximum;
}
bool valid(const runtime::HullDimensions& h) noexcept {
    return h.minimum.isFinite() && h.maximum.isFinite() && h.minimum.x<h.maximum.x &&
        h.minimum.y<h.maximum.y && h.minimum.z<h.maximum.z;
}
}
CrouchDecision Crouch::result(CrouchReason reason) const noexcept {
    CrouchDecision out; out.state=state_; out.reason=reason;
    if(observedDuck_ || state_==CrouchState::Lowering) out.intent.duck=ActionRequest::Hold;
    return out;
}
CrouchDecision Crouch::fail(CrouchReason reason) noexcept {
    state_=CrouchState::Failed; auto out=result(reason); out.accepted=out.terminalEvent=true; return out;
}
CrouchDecision Crouch::abort() noexcept {
    if(state_==CrouchState::Failed || state_==CrouchState::Aborted) return result();
    state_=CrouchState::Aborted; auto out=result(CrouchReason::Cancelled); out.accepted=out.terminalEvent=true; return out;
}
CrouchDecision Crouch::update(const runtime::MovementSnapshot& s,bool required,std::uint64_t nowUs,
    runtime::IWorldQueries& port,std::uint32_t reserved,std::uint32_t maximum) noexcept {
    if(state_==CrouchState::Failed || state_==CrouchState::Aborted) return result();
    if(!binding_.agent.isValid() || !binding_.actor.isValid() || !binding_.map.isValid() || !binding_.routeGeneration ||
       !valid(limits_.standing) || !valid(limits_.crouched) || !limits_.transitionTimeoutUs ||
       limits_.standing.minimum.x!=limits_.crouched.minimum.x || limits_.standing.maximum.x!=limits_.crouched.maximum.x ||
       limits_.standing.minimum.y!=limits_.crouched.minimum.y || limits_.standing.maximum.y!=limits_.crouched.maximum.y ||
       double(limits_.standing.maximum.z)-limits_.standing.minimum.z<=double(limits_.crouched.maximum.z)-limits_.crouched.minimum.z)
        return fail(CrouchReason::InvalidInput);
    if(s.agent!=binding_.agent || s.actor!=binding_.actor || s.map!=binding_.map ||
       s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true || s.alive!=true || s.joined!=true) {
        state_=CrouchState::Aborted; auto out=result(CrouchReason::InvalidActor); out.accepted=out.terminalEvent=true; return out;
    }
    if(!s.tick.isValid() || (tick_.isValid() && !s.tick.isAfter(tick_))) return result(CrouchReason::StaleTick);
    if(tick_.isValid() && nowUs<=lastUs_) return fail(CrouchReason::InvalidInput);
    tick_=s.tick; lastUs_=nowUs;
    if(s.ducked) observedDuck_=*s.ducked;
    if(!s.ducked || s.grounded!=true || !s.position || !s.position->isFinite() || !s.hull)
        return fail(CrouchReason::MissingObservation);
    const bool confirmed=same(*s.hull,observedDuck_ ? limits_.crouched:limits_.standing);
    if(!confirmed) {
        // SDK posture transition is not evidence of either movement hull yet.
        if(!valid(*s.hull)) return fail(CrouchReason::MissingObservation);
    }
    if(confirmed && observedDuck_==required) {
        waiting_=false; state_=required ? CrouchState::Crouched:CrouchState::Standing;
        auto out=result(); out.accepted=out.movementAllowed=true; return out;
    }
    if(!waiting_) { waiting_=true; startedUs_=nowUs; }
    if(nowUs-startedUs_>=limits_.transitionTimeoutUs) return fail(CrouchReason::TimedOut);
    state_=required ? CrouchState::Lowering:CrouchState::Raising;
    auto out=result(); out.accepted=true;
    if(!confirmed) { out.intent.duck=ActionRequest::Hold; return out; }
    if(reserved>=maximum) { out=fail(CrouchReason::BudgetExceeded); return out; }
    const auto& hull=required ? limits_.crouched:limits_.standing;
    auto center=*s.position;
    center.z=static_cast<float>(double(center.z)+s.hull->minimum.z-hull.minimum.z);
    if(!center.isFinite()) return fail(CrouchReason::InvalidInput);
    const runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,binding_.routeGeneration,reserved+1},
        runtime::QueryKind::Clearance,center,center,hull};
    runtime::WorldQueryResult reply;
    try { reply=port.query(q); } catch(...) { out=fail(CrouchReason::QueryFailed); out.queries=1; return out; }
    if(!(reply.stamp==q.stamp) || reply.kind!=q.kind) out=fail(CrouchReason::StaleQuery);
    else if(reply.error!=runtime::QueryError::None || !reply.clearance) out=fail(CrouchReason::QueryFailed);
    else if(!reply.clearance->clear) {
        if(required) out=fail(CrouchReason::Blocked);
        else { out=result(CrouchReason::Blocked); out.accepted=true; }
    } else out.intent.duck=required ? ActionRequest::Hold:ActionRequest::Release;
    out.queries=1; return out;
}
}
