// SPDX-License-Identifier: MPL-2.0
#include "nav/local/door_wait.hpp"

namespace astrabot::nav::local {
namespace {
bool same(const Binding& a, const Binding& b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
        a.routeGeneration==b.routeGeneration && a.step==b.step;
}
bool terminal(DoorWaitState s) noexcept {
    return s==DoorWaitState::Clear || s==DoorWaitState::Failed || s==DoorWaitState::Aborted;
}
}
DoorWaitDecision DoorWait::finish(DoorWaitState state, DoorWaitReason reason) noexcept {
    state_=state; reason_=reason;
    return {state_,reason_,true,true,{}};
}
DoorWaitDecision DoorWait::abort() noexcept {
    if(terminal(state_)) return {state_,reason_,false,false,{}};
    return finish(DoorWaitState::Aborted,DoorWaitReason::Cancelled);
}
DoorWaitDecision DoorWait::update(const DoorWaitFeedback& f) noexcept {
    if(terminal(state_)) return {state_,reason_,false,false,{}};
    if(!binding_.agent.isValid() || !binding_.actor.isValid() || !binding_.map.isValid() ||
       !binding_.routeGeneration || !timeoutUs_)
        return finish(DoorWaitState::Failed,DoorWaitReason::InvalidInput);
    const auto& q=f.requested;
    if(!same(binding_,f.binding) || q.agent!=binding_.agent || q.actor!=binding_.actor ||
       q.map!=binding_.map || q.routeGeneration!=binding_.routeGeneration)
        return finish(DoorWaitState::Aborted,DoorWaitReason::InvalidBinding);
    if(!q.tick.isValid() || (tick_.isValid() && !q.tick.isAfter(tick_)))
        return {state_,DoorWaitReason::StaleTick,false,false,{}};
    if(state_==DoorWaitState::Waiting && f.nowUs<=lastUs_)
        return finish(DoorWaitState::Failed,DoorWaitReason::InvalidInput);
    tick_=q.tick;
    lastUs_=f.nowUs;
    // Subtraction after monotonicity validation cannot overflow, even near UINT64_MAX.
    // Expiry wins over a newly clear observation at the exact deadline.
    if(state_==DoorWaitState::Waiting && f.nowUs-startedUs_>=timeoutUs_)
        return finish(DoorWaitState::Failed,DoorWaitReason::TimedOut);
    const auto& r=f.observed;
    if(!q.ordinal || !(r.stamp==q) || r.kind!=runtime::QueryKind::Door ||
       r.error!=runtime::QueryError::None || !r.door || !r.door->id)
        return finish(DoorWaitState::Failed,DoorWaitReason::InvalidObservation);
    const auto& door=*r.door;
    if(state_==DoorWaitState::Waiting && door.id!=doorId_)
        return finish(DoorWaitState::Failed,DoorWaitReason::Replaced);
    // 'open' is fresh swept passage clearance, never a toggle-state guess.
    if(door.open) return finish(DoorWaitState::Clear,DoorWaitReason::None);
    if(state_==DoorWaitState::Waiting) return {state_,DoorWaitReason::None,true,false,{}};
    if(f.passive && door.canTouch && !door.canUse) {
        startedUs_=f.nowUs; doorId_=door.id; state_=DoorWaitState::Waiting;
        return {state_,DoorWaitReason::None,true,false,{}};
    }
    if(!door.canUse) return finish(DoorWaitState::Failed,DoorWaitReason::Unusable);
    MovementIntent intent; intent.view=f.useView; intent.use=ActionRequest::Press;
    if(!intent.view || !core::Motor::valid(intent) ||
       intent.view->x<core::kMinPitch || intent.view->x>core::kMaxPitch ||
       intent.view->y<core::kMinYaw || intent.view->y>core::kMaxYaw ||
       intent.view->z<core::kMinRoll || intent.view->z>core::kMaxRoll)
        return finish(DoorWaitState::Failed,DoorWaitReason::InvalidObservation);
    startedUs_=f.nowUs; doorId_=door.id; state_=DoorWaitState::Waiting;
    return {state_,DoorWaitReason::None,true,false,intent};
}
}
