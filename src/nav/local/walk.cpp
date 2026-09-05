// SPDX-License-Identifier: MPL-2.0
#include "nav/local/walk.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::nav::local {
namespace {
// The one ground observation is reused only inside this decision. Real engine
// ordinals remain 1..N; inspect's ordinal 1 is an exact synchronous cache hit.
class DecisionQueries final : public runtime::IWorldQueries {
public:
    DecisionQueries(runtime::IWorldQueries& port, const query::NavSpatialIndex& index,
                    double tolerance) noexcept : port_(port), index_(index), tolerance_(tolerance) {}
    model::NavAreaId source{}, target{};
    bool restrictAreas{}, offCorridor{};
    std::uint32_t issued{};
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        if(q.kind==runtime::QueryKind::GroundedArea && cached_) {
            if(q.stamp==request_.stamp && q.start==request_.start && q.end==request_.end)
                return *cached_;
            runtime::WorldQueryResult invalid; invalid.stamp=q.stamp; invalid.kind=q.kind;
            invalid.error=runtime::QueryError::InvalidResult; return invalid;
        }
        ++issued;
        auto r=port_.query(q);
        if(q.kind==runtime::QueryKind::GroundedArea) { request_=q; cached_=r; }
        if(restrictAreas && q.kind==runtime::QueryKind::Floor && r.stamp==q.stamp &&
           r.kind==q.kind && r.error==runtime::QueryError::None && r.floor && r.floor->supported) {
            const auto match=index_.containing({q.start.x,q.start.y,r.floor->height},tolerance_);
            if(match && *match.value && (**match.value).areaId!=source && (**match.value).areaId!=target) {
                offCorridor=true; r.error=runtime::QueryError::InvalidResult;
            }
        }
        return r;
    }
private:
    runtime::IWorldQueries& port_;
    const query::NavSpatialIndex& index_;
    double tolerance_{};
    runtime::QueryRequest request_{};
    std::optional<runtime::WorldQueryResult> cached_{};
};
bool inside(const model::NavExtent& e, model::NavVector3 p, runtime::HullDimensions h) noexcept {
    return double(p.x)+h.minimum.x>=e.northWest.x && double(p.x)+h.maximum.x<=e.southEast.x &&
           double(p.y)+h.minimum.y>=e.northWest.y && double(p.y)+h.maximum.y<=e.southEast.y;
}
float inward(double value, float origin) noexcept {
    const auto rounded=static_cast<float>(value);
    if((value>origin && rounded>value) || (value<origin && rounded<value))
        return std::nextafter(rounded,origin);
    return rounded;
}
}
Walk::Walk(Binding b, std::shared_ptr<const corridor::Corridor> c, model::NavVector3 goal,
           WalkLimits limits) noexcept
    : binding_(b), corridor_(std::move(c)), cursor_(corridor_), goal_(goal), limits_(limits) {}

WalkDecision Walk::finish(WalkDecision out, WalkState state, WalkReason reason) noexcept {
    out.terminalEvent=state_==WalkState::Running;
    state_=state; reason_=reason; out.state=state; out.reason=reason; out.intent={};
    if(primitive_.state()==PrimitiveState::Running) {
        if(state==WalkState::Failed)
            out.primitiveEvent=primitive_.update({out.binding,out.tick,Progress::Failed,{},std::nullopt,false}).event;
        else out.primitiveEvent=primitive_.abort().event;
    }
    return out;
}
WalkDecision Walk::abort() noexcept {
    WalkDecision out; out.binding=binding_; out.binding.step=cursor_.index(); out.tick=tick_;
    out.state=state_; out.reason=reason_;
    if(state_!=WalkState::Running) return out;
    out.accepted=true; return finish(out,WalkState::Aborted,WalkReason::Cancelled);
}
WalkDecision Walk::update(const runtime::MovementSnapshot& s, const query::NavSpatialIndex& index,
                         core::MapGeneration indexMap, runtime::IWorldQueries& port) noexcept {
    WalkDecision out; out.binding=binding_; out.binding.step=cursor_.index(); out.tick=s.tick;
    out.state=state_; out.reason=reason_;
    if(state_!=WalkState::Running) return out;
    out.accepted=true;
    // Identity invalidation wins over tick rejection, and retires this instance.
    if(s.agent!=binding_.agent || s.actor!=binding_.actor || s.map!=binding_.map ||
       s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true || s.alive!=true || s.joined!=true)
        return finish(out,WalkState::Aborted,WalkReason::InvalidActor);
    if(s.map!=indexMap) return finish(out,WalkState::Aborted,WalkReason::StaleNavigation);
    if(!s.tick.isValid() || (tick_.isValid() && !s.tick.isAfter(tick_))) {
        out.accepted=false; out.reason=WalkReason::StaleTick; return out;
    }
    tick_=s.tick;
    if(!corridor_ || binding_.step!=0 || !binding_.routeGeneration || !goal_.isFinite() ||
       !std::isfinite(limits_.speed) || limits_.speed<=0 || limits_.speed>400 ||
       !std::isfinite(limits_.arrivalTolerance) || limits_.arrivalTolerance<=0 ||
       !std::isfinite(limits_.crossingMargin) || limits_.crossingMargin<=0 || limits_.lookAhead==0)
        return finish(out,WalkState::Failed,WalkReason::InvalidInput);
    DecisionQueries queries(port,index,limits_.probe.navTolerance);
    const auto ground=GroundProbe::locate(s,binding_.routeGeneration,index,indexMap,queries,limits_.probe);
    out.queries=queries.issued; out.probeReason=ground.reason;
    if(!ground) return finish(out,WalkState::Failed,WalkReason::ProbeFailed);
    out.support=ground.target;
    const auto goalMatch=index.containing(goal_,limits_.probe.navTolerance);
    if(!goalMatch || !*goalMatch.value || (**goalMatch.value).areaId!=corridor_->goal())
        return finish(out,WalkState::Failed,WalkReason::InvalidGoal);
    if(corridor_->transitions().empty() && corridor_->startAttributes()!=0)
        return finish(out,WalkState::Failed,WalkReason::UnsupportedTraversal);
    const auto area=ground.target->area;
    auto aim=goal_;
    queries.source=area; queries.target=area; queries.restrictAreas=true;
    if(!cursor_.exhausted()) {
        const auto& t=corridor_->transitions()[cursor_.index()];
        if(t.edge.external || t.edge.traversal!=model::NavTraversalKind::Walk ||
           t.sourceAttributes!=0 || t.targetAttributes!=0)
            return finish(out,WalkState::Failed,WalkReason::UnsupportedTraversal);
        if(area!=t.edge.source && area!=t.edge.target)
            return finish(out,WalkState::Failed,WalkReason::OffCorridor);
        if(primitive_.state()==PrimitiveState::Idle) {
            const auto entered=primitive_.enter(out.binding,t,s.tick);
            out.primitiveEvent=entered.event;
            if(!entered.accepted || entered.state!=PrimitiveState::Running)
                return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
            return out; // lifecycle entry has its own tick, with no movement yet
        }
        if(area==t.edge.target && inside(t.targetExtent,*s.position,*s.hull)) {
            Feedback f{out.binding,s.tick,Progress::Complete,{},area,true};
            const auto completed=primitive_.update(f);
            if(!completed.accepted || completed.state!=PrimitiveState::Complete ||
               !cursor_.advance(out.binding.step,area,true))
                return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
            out.primitiveEvent=completed.event; primitive_=Primitive{};
            return out; // at most one measured transition per decision
        }
        auto reference=*s.position;
        // While straddling the portal, project from its source boundary.
        reference.x=std::clamp(reference.x,t.sourceExtent.northWest.x,t.sourceExtent.southEast.x);
        reference.y=std::clamp(reference.y,t.sourceExtent.northWest.y,t.sourceExtent.southEast.y);
        const auto portal=cursor_.target({reference.x,reference.y,reference.z},limits_.lookAhead);
        if(!portal) return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
        double x=portal.value->x, y=portal.value->y;
        const bool vertical=t.edge.direction==1 || t.edge.direction==3;
        if((vertical && (y+s.hull->minimum.y<t.sourceExtent.northWest.y ||
                         y+s.hull->maximum.y>t.sourceExtent.southEast.y)) ||
           (!vertical && (x+s.hull->minimum.x<t.sourceExtent.northWest.x ||
                          x+s.hull->maximum.x>t.sourceExtent.southEast.x)))
            return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
        switch(t.edge.direction) {
        case 0: y-=double(s.hull->maximum.y)+limits_.crossingMargin; break;
        case 1: x+=-double(s.hull->minimum.x)+limits_.crossingMargin; break;
        case 2: y+=-double(s.hull->minimum.y)+limits_.crossingMargin; break;
        case 3: x-=double(s.hull->maximum.x)+limits_.crossingMargin; break;
        default: return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
        }
        if(!std::isfinite(x) || !std::isfinite(y) || std::abs(x)>(std::numeric_limits<float>::max)() ||
           std::abs(y)>(std::numeric_limits<float>::max)())
            return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
        aim={static_cast<float>(x),static_cast<float>(y),s.position->z};
        if(!aim.isFinite() || !inside(t.targetExtent,aim,*s.hull))
            return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
        queries.source=t.edge.source; queries.target=t.edge.target;
    } else {
        if(area!=corridor_->goal()) return finish(out,WalkState::Failed,WalkReason::OffCorridor);
        if(std::hypot(double(goal_.x)-s.position->x,double(goal_.y)-s.position->y)<=limits_.arrivalTolerance &&
           std::abs(double(goal_.z)-ground.target->floor.height)<=limits_.probe.navTolerance)
            return finish(out,WalkState::Arrived,WalkReason::None);
    }
    const double dx=double(aim.x)-s.position->x, dy=double(aim.y)-s.position->y;
    const double distance=std::hypot(dx,dy);
    if(distance==0 || limits_.probe.maxDistance<=0)
        return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
    const double fraction=std::min(1.0,limits_.probe.maxDistance/distance);
    // Round toward the observed origin so a float endpoint cannot extend the
    // double-precision distance budget (notably on diagonal segments).
    const float x=inward(s.position->x+dx*fraction,s.position->x);
    const float y=inward(s.position->y+dy*fraction,s.position->y);
    const auto probe=GroundProbe::inspect(s,binding_.routeGeneration,area,x,y,index,indexMap,queries,limits_.probe);
    out.queries=queries.issued; out.samples=probe.samples; out.steps=probe.steps; out.probeReason=probe.reason;
    if(!probe) return finish(out,WalkState::Failed,queries.offCorridor ? WalkReason::OffCorridor:WalkReason::ProbeFailed);
    out.target=probe.target;
    // Even a full 120 ms fresh-intent hold cannot pass the inspected endpoint.
    const double inspected=std::hypot(double(x)-s.position->x,double(y)-s.position->y);
    if(inspected==0) return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
    out.intent.direction={(double(x)-s.position->x)/inspected,(double(y)-s.position->y)/inspected,0};
    out.intent.speed=std::min(limits_.speed,inspected/0.120);
    if(primitive_.state()==PrimitiveState::Running) {
        const auto update=primitive_.update({out.binding,s.tick,Progress::Running,out.intent,area,true});
        if(!update.accepted) return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
        out.intent=update.intent;
    }
    return out;
}
}
