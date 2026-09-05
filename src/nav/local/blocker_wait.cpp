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
std::optional<BlockerDecision> BlockerWait::begin(Binding b, core::TickId tick, std::uint64_t nowUs) noexcept {
    if(terminal_) return BlockerDecision{};
    if(!binding_.agent.isValid() || !binding_.actor.isValid() || !binding_.map.isValid() ||
       !binding_.routeGeneration || !limits_.factLifetimeUs || !limits_.yieldUs ||
       limits_.yieldUs>=limits_.timeoutUs || limits_.factLifetimeUs>limits_.timeoutUs)
        return finish(BlockerAction::Replan,BlockerReason::InvalidInput);
    if(b.agent!=binding_.agent || b.actor!=binding_.actor || b.map!=binding_.map ||
       b.routeGeneration!=binding_.routeGeneration || b.step!=binding_.step)
        return finish(BlockerAction::Aborted,BlockerReason::InvalidBinding);
    if(!tick.isValid() || (tick_.isValid() && !tick.isAfter(tick_)))
        return BlockerDecision{BlockerAction::Neutral,BlockerReason::StaleTick,false,false};
    if(started_ && nowUs<=lastUs_)
        return finish(BlockerAction::Replan,BlockerReason::InvalidInput);
    tick_=tick; lastUs_=nowUs;
    if(started_ && nowUs-startedUs_>=limits_.timeoutUs)
        return finish(BlockerAction::Replan,BlockerReason::TimedOut);
    return {};
}
BlockerDecision BlockerWait::clear(Binding b, core::TickId tick, std::uint64_t nowUs) noexcept {
    if(const auto rejected=begin(b,tick,nowUs)) return *rejected;
    return finish(BlockerAction::ReinspectPassage,BlockerReason::None);
}
BlockerDecision BlockerWait::update(const BlockerFeedback& f) noexcept {
    // Validate query ownership before any state transition or tick acceptance.
    const auto& b=f.binding;
    const auto& q=f.requested;
    if(terminal_) return {};
    if(q.agent!=b.agent || q.actor!=b.actor || q.map!=b.map || q.routeGeneration!=b.routeGeneration)
        return finish(BlockerAction::Aborted,BlockerReason::InvalidBinding);
    if(const auto rejected=begin(b,q.tick,f.nowUs)) return *rejected;
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
        obstacle.kind==runtime::BlockerKind::Enemy || obstacle.kind==runtime::BlockerKind::Player;
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
