// SPDX-License-Identifier: MPL-2.0
#include "nav/local/walk.hpp"
#include <algorithm>
namespace astrabot::nav::local {
namespace {
bool same(const enrichment::NavTraversalLink& a,const enrichment::NavTraversalLink& b) noexcept {
    return a.sourceId==b.sourceId && a.generation==b.generation && a.linkId==b.linkId &&
        a.from==b.from && a.to==b.to && a.entry.x==b.entry.x && a.entry.y==b.entry.y && a.entry.z==b.entry.z &&
        a.exit.x==b.exit.x && a.exit.y==b.exit.y && a.exit.z==b.exit.z && a.traversal==b.traversal &&
        a.direction==b.direction && a.additionalCost==b.additionalCost;
}
bool same(const LadderPlan& a,const LadderPlan& b) noexcept {
    return same(a.link,b.link) && a.start==b.start && a.end==b.end && a.mount==b.mount &&
        a.dismount==b.dismount && a.normal==b.normal;
}
}
std::optional<enrichment::NavTraversalLink> Walk::selectedLadderLink() const noexcept {
    if(state_!=WalkState::Running || !corridor_ || cursor_.exhausted()) return {};
    const auto& edge=corridor_->transitions()[cursor_.index()].edge;
    return edge.traversal==model::NavTraversalKind::Ladder ? edge.external:std::nullopt;
}
model::NavVector3 Walk::ladderTarget(const LadderPlan& plan,model::NavVector3 origin) const noexcept {
    return ladder_ ? ladder_->target(origin):plan.start;
}
bool Walk::reportLadderDispatch(const LadderDispatch& dispatch) noexcept {
    return state_==WalkState::Running && ladder_ && ladder_->reportJumpDispatch(dispatch);
}
WalkDecision Walk::updateLadder(WalkDecision out,const runtime::MovementSnapshot& s,const query::NavSpatialIndex& index,
    std::uint64_t nowUs,std::uint32_t reserved,const std::optional<LadderObservation>& observation) noexcept {
    out.queries=reserved; out.ladderState=ladder_ ? ladder_->state():LadderState::Approach; out.ladderPlan=ladderPlan_;
    const auto fail=[&](LadderReason reason) {
        out.ladderReason=reason; out.ladderState=LadderState::Failed;
        return finish(out,WalkState::Failed,WalkReason::LadderFailed);
    };
    const auto selected=selectedLadderLink();
    if(!selected || !limits_.ladder) return finish(out,WalkState::Failed,WalkReason::UnsupportedTraversal);
    if(!observation) return fail(LadderReason::MissingObservation);
    const auto& incoming=observation->plan;
    if(!same(*selected,incoming.link) || (ladderPlan_ && !same(*ladderPlan_,incoming))) return fail(LadderReason::StaleInspection);
    const auto maximum=(std::min)({limits_.probe.maxQueries,limits_.ladder->maxQueries,21U});
    if(reserved>maximum || observation->inspection.queries>maximum-reserved) return fail(LadderReason::StaleInspection);
    out.queries+=observation->inspection.queries;
    const auto goal=index.containing(goal_,limits_.probe.navTolerance);
    if(!goal || !*goal.value || (**goal.value).areaId!=corridor_->goal()) return finish(out,WalkState::Failed,WalkReason::InvalidGoal);
    const bool entering=!ladder_;
    if(entering) {
        ladderPlan_=incoming; ladder_.emplace(out.binding,incoming,*limits_.ladder);
    }
    auto movement=s; movement.ladder=observation->contact;
    const auto decision=ladder_->update({out.binding,movement,nowUs,observation->inspection,observation->climbing});
    out.ladderState=decision.state; out.ladderReason=decision.reason; out.ladderPlan=ladderPlan_;
    out.ladderPressTick=decision.pressTick; out.intent=decision.intent; out.support=observation->inspection.support;
    if(decision.state==LadderState::Failed || decision.state==LadderState::Aborted)
        return finish(out,decision.state==LadderState::Aborted ? WalkState::Aborted:WalkState::Failed,WalkReason::LadderFailed);
    if(entering) {
        // Invalid initial observation must not leave a primitive entered on the
        // same tick that its terminal feedback would be rejected as stale.
        const auto entered=primitive_.enter(out.binding,corridor_->transitions()[cursor_.index()],s.tick);
        out.primitiveEvent=entered.event;
        if(!entered.accepted || entered.state!=PrimitiveState::Running) return fail(LadderReason::InvalidInput);
        postureAction_=ActionRequest::Release;
    }
    if(decision.state==LadderState::Complete) {
        if(!out.support || out.support->area!=selected->to) return fail(LadderReason::MissingSupport);
        const auto complete=primitive_.update({out.binding,s.tick,Progress::Complete,{},out.support->area,true});
        if(!complete.accepted || complete.state!=PrimitiveState::Complete || !cursor_.advance(out.binding.step,out.support->area,true))
            return fail(LadderReason::WrongLanding);
        out.primitiveEvent=complete.event; primitive_=Primitive{}; ladder_.reset(); ladderPlan_.reset();
        out.intent={}; out.intent.jump=out.intent.forward=out.intent.back=ActionRequest::Release;
    }
    return out;
}
}
