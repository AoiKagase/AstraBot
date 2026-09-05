// SPDX-License-Identifier: MPL-2.0
#include "nav/local/primitive.hpp"
#include "core/command.hpp"
#include <cmath>

namespace astrabot::nav::local {
namespace {
bool finite(query::NavQueryPoint p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool action(ActionRequest a) noexcept {
    switch(a) {
    case ActionRequest::None: case ActionRequest::Press: case ActionRequest::Hold: case ActionRequest::Release:
        return true;
    default: return false;
    }
}
bool valid(const MovementIntent& i) noexcept {
    if(!finite(i.direction) || !std::isfinite(i.speed) || i.speed<0 || i.speed>core::kMaxMovement ||
       !std::isfinite(i.lateralCorrection) || std::abs(i.lateralCorrection)>1 ||
       (i.view && !finite(*i.view)) || !action(i.duck) || !action(i.jump) || !action(i.use)) return false;
    const double norm=i.direction.x*i.direction.x+i.direction.y*i.direction.y+i.direction.z*i.direction.z;
    return norm<=1.000001 && (i.speed==0 || norm>0);
}
bool same(const Binding& a, const Binding& b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
           a.routeGeneration==b.routeGeneration && a.step==b.step;
}
}
bool Primitive::supported(model::NavTraversalKind kind) noexcept {
    switch(kind) {
    case model::NavTraversalKind::Walk: case model::NavTraversalKind::Crouch:
    case model::NavTraversalKind::Jump: case model::NavTraversalKind::Ladder: return true;
    case model::NavTraversalKind::Drop: default: return false;
    }
}
PrimitiveUpdate Primitive::finish(PrimitiveState state, PrimitiveEvent event, PrimitiveReason reason) noexcept {
    state_=state;
    return {true,event,state_,reason,{}};
}
PrimitiveUpdate Primitive::enter(Binding binding, const corridor::Transition& transition, core::TickId tick) noexcept {
    if(state_!=PrimitiveState::Idle) return {false,PrimitiveEvent::None,state_,PrimitiveReason::AlreadyEntered,{}};
    if(!binding.agent.isValid() || !binding.actor.isValid() || !binding.map.isValid() ||
       binding.routeGeneration==0 || !tick.isValid())
        return {false,PrimitiveEvent::None,state_,PrimitiveReason::InvalidBinding,{}};
    if(!transition.edge.source.isValid() || !transition.edge.target.isValid() ||
       transition.edge.source==transition.edge.target || !finite(transition.sourceLow) ||
       !finite(transition.sourceHigh) || !finite(transition.targetLow) || !finite(transition.targetHigh))
        return {false,PrimitiveEvent::None,state_,PrimitiveReason::InvalidTransition,{}};
    binding_=binding; transition_=transition; tick_=tick;
    if(!supported(transition.edge.traversal))
        return finish(PrimitiveState::Failed,PrimitiveEvent::Failed,PrimitiveReason::UnsupportedTraversal);
    return finish(PrimitiveState::Running,PrimitiveEvent::Entered,PrimitiveReason::None);
}
PrimitiveUpdate Primitive::update(const Feedback& feedback) noexcept {
    if(state_!=PrimitiveState::Running || !same(binding_,feedback.binding) || !feedback.tick.isAfter(tick_))
        return {false,PrimitiveEvent::None,state_,PrimitiveReason::StaleUpdate,{}};
    tick_=feedback.tick;
    switch(feedback.progress) {
    case Progress::Failed:
        return finish(PrimitiveState::Failed,PrimitiveEvent::Failed,PrimitiveReason::ControllerFailure);
    case Progress::Complete:
        if(!feedback.supportVerified || !feedback.supportedArea || *feedback.supportedArea!=transition_->edge.target)
            return finish(PrimitiveState::Failed,PrimitiveEvent::Failed,PrimitiveReason::MissingSupport);
        return finish(PrimitiveState::Complete,PrimitiveEvent::Complete,PrimitiveReason::None);
    case Progress::Running:
        if(!valid(feedback.intent))
            return finish(PrimitiveState::Failed,PrimitiveEvent::Failed,PrimitiveReason::InvalidIntent);
        return {true,PrimitiveEvent::None,state_,PrimitiveReason::None,feedback.intent};
    default:
        return finish(PrimitiveState::Failed,PrimitiveEvent::Failed,PrimitiveReason::InvalidFeedback);
    }
}
PrimitiveUpdate Primitive::abort() noexcept {
    if(state_!=PrimitiveState::Running) return {false,PrimitiveEvent::None,state_,PrimitiveReason::None,{}};
    return finish(PrimitiveState::Aborted,PrimitiveEvent::Aborted,PrimitiveReason::Cancelled);
}
} // namespace astrabot::nav::local
