// SPDX-License-Identifier: MPL-2.0
#include "nav/local/blocker_wait.hpp"

namespace astrabot::nav::local {
BlockerDecision BlockerWait::finish(BlockerAction action, BlockerReason reason) noexcept {
    terminal_=true; fact_.reset();
    return {action,reason,true,true};
}
BlockerDecision BlockerWait::abort() noexcept {
    if(terminal_) return {};
    return finish(BlockerAction::Aborted,BlockerReason::Cancelled);
}
std::optional<runtime::BlockerObservation> BlockerWait::fact(std::uint64_t nowUs) const noexcept {
    if(terminal_ || !fact_ || nowUs<lastUs_ || nowUs<observedUs_ ||
       nowUs-observedUs_>=limits_.factLifetimeUs || nowUs-startedUs_>=limits_.timeoutUs)
        return {};
    return fact_;
}
BlockerDecision BlockerWait::update(const BlockerFeedback& f) noexcept {
    if(terminal_) return {};
    if(!binding_.agent.isValid() || !binding_.actor.isValid() || !binding_.map.isValid() ||
       !binding_.routeGeneration || !limits_.factLifetimeUs || !limits_.yieldUs ||
       limits_.yieldUs>=limits_.timeoutUs || limits_.factLifetimeUs>limits_.timeoutUs)
        return finish(BlockerAction::Replan,BlockerReason::InvalidInput);
    const auto& b=f.binding;
    const auto& q=f.requested;
    if(b.agent!=binding_.agent || b.actor!=binding_.actor || b.map!=binding_.map ||
       b.routeGeneration!=binding_.routeGeneration || b.step!=binding_.step ||
       q.agent!=b.agent || q.actor!=b.actor || q.map!=b.map || q.routeGeneration!=b.routeGeneration)
        return finish(BlockerAction::Aborted,BlockerReason::InvalidBinding);
    if(!q.tick.isValid() || (tick_.isValid() && !q.tick.isAfter(tick_)))
        return {BlockerAction::Neutral,BlockerReason::StaleTick,false,false};
    if(started_ && f.nowUs<=lastUs_)
        return finish(BlockerAction::Replan,BlockerReason::InvalidInput);
    tick_=q.tick; lastUs_=f.nowUs;
    if(started_ && f.nowUs-startedUs_>=limits_.timeoutUs)
        return finish(BlockerAction::Replan,BlockerReason::TimedOut);
    const auto& r=f.observed;
    if(!q.ordinal || !(r.stamp==q))
        return finish(BlockerAction::Replan,BlockerReason::InvalidObservation);
    if(r.error==runtime::QueryError::Unavailable)
        return finish(BlockerAction::Replan,BlockerReason::Unavailable);
    if(r.error!=runtime::QueryError::None)
        return finish(BlockerAction::Replan,BlockerReason::InvalidObservation);
    if(r.kind==runtime::QueryKind::Clearance && r.clearance && r.clearance->clear)
        return finish(BlockerAction::ReinspectPassage,BlockerReason::None);
    if(r.kind!=runtime::QueryKind::Blocker || !r.blocker)
        return finish(BlockerAction::Replan,BlockerReason::InvalidObservation);
    const auto& obstacle=*r.blocker;
    const bool player=obstacle.kind==runtime::BlockerKind::Teammate ||
        obstacle.kind==runtime::BlockerKind::Enemy;
    if((obstacle.kind!=runtime::BlockerKind::Geometry && !player &&
        obstacle.kind!=runtime::BlockerKind::Other) ||
       (!obstacle.id && obstacle.kind!=runtime::BlockerKind::Geometry) ||
       (player && (!obstacle.player || !obstacle.player->isValid() || obstacle.player->sameSlot(b.actor))) ||
       (!player && obstacle.player))
        return finish(BlockerAction::Replan,BlockerReason::InvalidObservation);
    if(!started_) { started_=true; startedUs_=f.nowUs; }
    observedUs_=f.nowUs; fact_=obstacle;
    // Lower stable PlayerId has priority to inspect an avoidance route. Neither
    // actor gets collision permission. The other yields only for a finite time.
    const bool yield=player && *obstacle.player<b.actor && f.nowUs-startedUs_<limits_.yieldUs;
    return {yield ? BlockerAction::Yield : BlockerAction::InspectAvoidance,
        BlockerReason::None,true,false};
}
}
