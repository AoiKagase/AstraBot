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
                    double tolerance, std::uint32_t reserved, std::uint32_t maximum) noexcept
        : issued(reserved), port_(port), index_(index), tolerance_(tolerance), maximum_(maximum) {}
    model::NavAreaId source{}, target{};
    bool restrictAreas{}, offCorridor{};
    std::uint32_t issued{};
    std::optional<runtime::QueryRequest> blocked{};
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        if(q.kind==runtime::QueryKind::GroundedArea && cached_) {
            if(q.stamp==request_.stamp && q.start==request_.start && q.end==request_.end)
                return *cached_;
            runtime::WorldQueryResult invalid; invalid.stamp=q.stamp; invalid.kind=q.kind;
            invalid.error=runtime::QueryError::InvalidResult; return invalid;
        }
        if(issued==maximum_) {
            runtime::WorldQueryResult exhausted; exhausted.stamp=q.stamp; exhausted.kind=q.kind;
            exhausted.error=runtime::QueryError::BudgetExceeded; return exhausted;
        }
        auto wire=q; wire.stamp.ordinal=++issued;
        auto r=port_.query(wire);
        // Map a validated wire stamp back to a helper's local ordinal. Each
        // additional inspection reuses only ground, never an engine ordinal.
        if(r.stamp==wire.stamp) r.stamp=q.stamp;
        else { r.error=runtime::QueryError::InvalidResult; }
        if(q.kind==runtime::QueryKind::SweptHull && r.stamp==q.stamp && r.kind==q.kind &&
           r.error==runtime::QueryError::None && r.hull && !r.hull->startSolid &&
           std::isfinite(r.hull->fraction) && r.hull->fraction>=0 && r.hull->fraction<1 &&
           r.hull->end.isFinite() && r.hull->normal.isFinite()) blocked=q;
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
    std::uint32_t maximum_{};
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
ProbeReason sides(const runtime::MovementSnapshot& s, std::uint64_t generation,
    double ux,double uy,double range,std::uint32_t maximum,runtime::IWorldQueries& port,WalkDecision& out) noexcept {
    if(out.queries>maximum || maximum-out.queries<2) return ProbeReason::BudgetExceeded;
    for(int side=0;side<2;++side) {
        const double sign=side==0 ? -1.0:1.0;
        const model::NavVector3 end{inward(s.position->x+sign*uy*range,s.position->x),
            inward(s.position->y-sign*ux*range,s.position->y),s.position->z};
        if(!end.isFinite()) return ProbeReason::InvalidInput;
        runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,generation,++out.queries},
            runtime::QueryKind::SweptHull,*s.position,end,s.hull};
        runtime::WorldQueryResult r;
        try { r=port.query(q); } catch(...) { return ProbeReason::QueryFailed; }
        if(!(r.stamp==q.stamp) || r.kind!=q.kind) return ProbeReason::StaleQuery;
        if(r.error==runtime::QueryError::BudgetExceeded) return ProbeReason::BudgetExceeded;
        if(r.error!=runtime::QueryError::None || !r.hull) return ProbeReason::QueryFailed;
        const auto& h=*r.hull;
        if(!std::isfinite(h.fraction) || h.fraction<0 || h.fraction>1 || !h.end.isFinite() || !h.normal.isFinite())
            return ProbeReason::InvalidResult;
        if(h.startSolid) return ProbeReason::Blocked;
        const double dx=double(end.x)-s.position->x,dy=double(end.y)-s.position->y;
        if(std::abs(double(h.end.x)-s.position->x-dx*h.fraction)>0.01 ||
           std::abs(double(h.end.y)-s.position->y-dy*h.fraction)>0.01 ||
           std::abs(double(h.end.z)-s.position->z)>0.01) return ProbeReason::InvalidResult;
        const auto clearance=std::hypot(dx,dy)*h.fraction;
        if(side==0) out.leftClearance=clearance; else out.rightClearance=clearance;
    }
    return ProbeReason::None;
}
}
Walk::Walk(Binding b, std::shared_ptr<const corridor::Corridor> c, model::NavVector3 goal,
           WalkLimits limits) noexcept
    : binding_(b), corridor_(std::move(c)), cursor_(corridor_), goal_(goal), limits_(limits) {}

WalkDecision Walk::finish(WalkDecision out, WalkState state, WalkReason reason) noexcept {
    if(door_ && door_->state()==DoorWaitState::Waiting) (void)door_->abort();
    if(blocker_) { (void)blocker_->abort(); blocker_.reset(); }
    out.terminalEvent=state_==WalkState::Running;
    state_=state; reason_=reason; out.state=state; out.reason=reason; out.intent={}; out.contact.reset();
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
WalkDecision Walk::updateDoor(WalkDecision out, const runtime::MovementSnapshot& s,
    const query::NavSpatialIndex& index, core::MapGeneration indexMap,
    runtime::IWorldQueries& port, model::NavVector3 end, std::uint64_t nowUs) noexcept {
    if(out.queries>=limits_.probe.maxQueries) {
        out.probeReason=ProbeReason::BudgetExceeded;
        return finish(out,WalkState::Failed,WalkReason::DoorBlocked);
    }
    runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,binding_.routeGeneration,++out.queries},
        runtime::QueryKind::Door,*s.position,end,s.hull,limits_.probe.navTolerance,doorId_};
    runtime::WorldQueryResult r;
    try { r=port.query(q); } catch(...) {}
    if(!door_) {
        if(r.stamp==q.stamp && r.kind==q.kind && r.error==runtime::QueryError::None &&
           r.door && r.door->id==lastDoorId_ && lastDoorId_) {
            out.doorReason=DoorWaitReason::Reblocked;
            return finish(out,WalkState::Failed,WalkReason::DoorBlocked);
        }
        touch_=r.door && r.door->canTouch && !r.door->canUse && limits_.touchTimeoutUs;
        door_.emplace(out.binding,touch_ ? limits_.touchTimeoutUs:limits_.doorTimeoutUs);
        doorStart_=*s.position; doorEnd_=end;
        contactSent_=false;
    }
    DoorWaitFeedback f{out.binding,q.stamp,r,nowUs,{}};
    f.passive=touch_;
    if(r.door && r.door->useView) {
        const auto v=*r.door->useView; f.useView=core::IntentVector{v.x,v.y,v.z};
    }
    const auto decision=door_->update(f);
    out.doorState=decision.state; out.doorReason=decision.reason;
    if(r.stamp==q.stamp && r.kind==q.kind && r.error==runtime::QueryError::None && r.door) doorId_=r.door->id;
    out.doorId=doorId_; out.intent=decision.intent;
    if(decision.state==DoorWaitState::Failed || decision.state==DoorWaitState::Aborted)
        return finish(out,WalkState::Failed,WalkReason::DoorBlocked);
    if(decision.state==DoorWaitState::Clear) {
        lastDoorId_=doorId_; doorId_=0; door_.reset();
        // Clearance alone cannot advance a corridor or move. Next decision
        // repeats the full measured ground/segment inspection.
    } else if(touch_ && !contactSent_) return approachDoor(out,s,index,indexMap,port,r);
    return out;
}
WalkDecision Walk::approachDoor(WalkDecision out, const runtime::MovementSnapshot& s,
    const query::NavSpatialIndex& index, core::MapGeneration indexMap,
    runtime::IWorldQueries& port, const runtime::WorldQueryResult& r) noexcept {
    const auto fail=[&] { return finish(out,WalkState::Failed,WalkReason::DoorBlocked); };
    if(!r.door || !r.door->canTouch || !r.hull || r.hull->startSolid || !r.hull->end.isFinite() ||
       !r.hull->normal.isFinite() || !std::isfinite(r.hull->fraction) || r.hull->fraction<0 || r.hull->fraction>=1)
        return fail();
    const double dx=double(doorEnd_.x)-s.position->x,dy=double(doorEnd_.y)-s.position->y;
    const double length=std::hypot(dx,dy);
    if(length<=0 || length>limits_.probe.maxDistance || std::abs(double(doorEnd_.z)-s.position->z)>0.1) return fail();
    const double ux=dx/length,uy=dy/length;
    const auto hit=r.hull->end;
    const double hx=double(hit.x)-s.position->x,hy=double(hit.y)-s.position->y;
    const double distance=hx*ux+hy*uy;
    if(distance<0 || distance>length || std::abs(hx*uy-hy*ux)>0.01 ||
       std::abs(double(hit.z)-s.position->z)>0.1 ||
       std::abs(distance-length*r.hull->fraction)>0.02 ||
       r.hull->normal.x*ux+r.hull->normal.y*uy> -0.7 || std::abs(r.hull->normal.z)>0.2) return fail();
    if(distance<=0.125) {
        if(!s.elapsedUs) return fail();
        // One measured-frame pulse, never cached translation. Host requires
        // a fresh same-door collision within 0.125 and caps actual travel at .75.
        out.intent.direction={ux,uy,0};
        out.intent.speed=(std::min)(400.0,500000.0/double(s.elapsedUs));
        out.contact=DoorContact{doorId_,{inward(s.position->x+ux*0.75,s.position->x),
            inward(s.position->y+uy*0.75,s.position->y),s.position->z}};
        contactSent_=true; doorStart_=*s.position; return out;
    }
    if(out.samples>=limits_.probe.maxSamples || out.queries>=limits_.probe.maxQueries) {
        out.probeReason=ProbeReason::BudgetExceeded; return fail();
    }
    auto budget=limits_.probe;
    budget.maxQueries=limits_.probe.maxQueries-out.queries+1; // ground is an exact cache hit
    budget.maxSamples-=out.samples;
    const auto travel=(std::min)(distance-0.0625,double(budget.maxSamples)*budget.sampleSpacing);
    const auto x=inward(s.position->x+ux*travel,s.position->x),y=inward(s.position->y+uy*travel,s.position->y);
    const auto probe=GroundProbe::inspect(s,binding_.routeGeneration,out.support->area,x,y,index,indexMap,port,budget);
    out.queries+=probe.queries ? probe.queries-1:0; out.samples+=probe.samples; out.steps+=probe.steps;
    out.probeReason=probe.reason;
    if(!probe) return fail();
    out.target=probe.target; out.intent.direction={ux,uy,0};
    out.intent.speed=(std::min)(limits_.speed,std::hypot(double(x)-s.position->x,double(y)-s.position->y)/0.120);
    return out;
}
WalkDecision Walk::update(const runtime::MovementSnapshot& s, const query::NavSpatialIndex& index,
                         core::MapGeneration indexMap, runtime::IWorldQueries& port, std::uint64_t nowUs,
                         std::uint32_t reservedQueries) noexcept {
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
    if(!std::isfinite(limits_.sideProbeDistance) || limits_.sideProbeDistance<0 ||
       (limits_.sideProbeDistance>0 && (!std::isfinite(limits_.narrowMargin) || limits_.narrowMargin<=0 ||
        limits_.narrowMargin>limits_.sideProbeDistance || limits_.sideProbeDistance>limits_.probe.maxDistance ||
        !std::isfinite(limits_.narrowSpeed) || limits_.narrowSpeed<=0 || limits_.narrowSpeed>limits_.speed ||
        !limits_.maxAvoidanceDecisions))) return finish(out,WalkState::Failed,WalkReason::InvalidInput);
    if(limits_.blocker.timeoutUs && (!limits_.blocker.factLifetimeUs || !limits_.blocker.yieldUs ||
       limits_.blocker.yieldUs>=limits_.blocker.timeoutUs ||
       limits_.blocker.factLifetimeUs>limits_.blocker.timeoutUs || limits_.sideProbeDistance<=0))
        return finish(out,WalkState::Failed,WalkReason::InvalidInput);
    if(reservedQueries>=limits_.probe.maxQueries) {
        out.queries=reservedQueries; out.probeReason=ProbeReason::BudgetExceeded;
        return finish(out,WalkState::Failed,WalkReason::ProbeFailed);
    }
    auto probeLimits=limits_.probe; probeLimits.maxQueries-=reservedQueries;
    DecisionQueries queries(port,index,limits_.probe.navTolerance,reservedQueries,limits_.probe.maxQueries);
    const auto ground=GroundProbe::locate(s,binding_.routeGeneration,index,indexMap,queries,probeLimits);
    out.queries=queries.issued; out.probeReason=ground.reason;
    if(!ground) return finish(out,WalkState::Failed,WalkReason::ProbeFailed);
    out.support=ground.target;
    const auto clearBlocker=[&] {
        const auto d=blocker_->clear(out.binding,s.tick,nowUs);
        out.blockerAction=d.action; out.blockerReason=d.reason;
        blocker_.reset(); avoidSide_=0; avoidDecisions_=0;
        return d.action==BlockerAction::ReinspectPassage;
    };
    const auto goalMatch=index.containing(goal_,limits_.probe.navTolerance);
    if(!goalMatch || !*goalMatch.value || (**goalMatch.value).areaId!=corridor_->goal())
        return finish(out,WalkState::Failed,WalkReason::InvalidGoal);
    if(corridor_->transitions().empty() && corridor_->startAttributes()!=0)
        return finish(out,WalkState::Failed,WalkReason::UnsupportedTraversal);
    const auto area=ground.target->area;
    if(door_) {
        const auto dx=double(s.position->x)-doorStart_.x, dy=double(s.position->y)-doorStart_.y;
        if(((!touch_ || contactSent_) && std::hypot(dx,dy)>(touch_ ? 0.75:0.5)) ||
           std::abs(double(s.position->z)-doorStart_.z)>limits_.probe.supportTolerance)
            return finish(out,WalkState::Failed,WalkReason::DoorBlocked);
        queries.source=area; queries.target=area; queries.restrictAreas=true;
        if(!cursor_.exhausted()) {
            const auto& t=corridor_->transitions()[cursor_.index()];
            if(area!=t.edge.source && area!=t.edge.target) return finish(out,WalkState::Failed,WalkReason::OffCorridor);
            queries.source=t.edge.source; queries.target=t.edge.target;
        } else if(area!=corridor_->goal()) return finish(out,WalkState::Failed,WalkReason::OffCorridor);
        return updateDoor(out,s,index,indexMap,queries,doorEnd_,nowUs);
    }
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
            if(blocker_) {
                if(!clearBlocker()) return finish(out,WalkState::Failed,WalkReason::DynamicBlocked);
                return out; // Measured passage progress retires its old portal fact first.
            }
            Feedback f{out.binding,s.tick,Progress::Complete,{},area,true};
            const auto completed=primitive_.update(f);
            if(!completed.accepted || completed.state!=PrimitiveState::Complete ||
               !cursor_.advance(out.binding.step,area,true))
                return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
            out.primitiveEvent=completed.event; primitive_=Primitive{}; avoidSide_=0; avoidDecisions_=0;
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
           std::abs(double(goal_.z)-ground.target->floor.height)<=limits_.probe.navTolerance) {
            if(blocker_) {
                if(!clearBlocker()) return finish(out,WalkState::Failed,WalkReason::DynamicBlocked);
                return out;
            }
            return finish(out,WalkState::Arrived,WalkReason::None);
        }
    }
    const double dx=double(aim.x)-s.position->x, dy=double(aim.y)-s.position->y;
    const double distance=std::hypot(dx,dy);
    if(distance==0 || limits_.probe.maxDistance<=0)
        return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
    const double fraction=std::min(1.0,limits_.probe.maxDistance/distance);
    // Round toward the observed origin so a float endpoint cannot extend the
    // double-precision distance budget (notably on diagonal segments).
    float x=inward(s.position->x+dx*fraction,s.position->x);
    float y=inward(s.position->y+dy*fraction,s.position->y);
    const double ux=dx/distance,uy=dy/distance;
    const auto constrain=[&](float& tx,float& ty) {
        if(!cursor_.exhausted()) {
            const auto& t=corridor_->transitions()[cursor_.index()];
            if(t.edge.direction==1 || t.edge.direction==3)
                ty=static_cast<float>(std::clamp(double(ty),(std::max)(t.sourceLow.y,t.targetLow.y),(std::min)(t.sourceHigh.y,t.targetHigh.y)));
            else tx=static_cast<float>(std::clamp(double(tx),(std::max)(t.sourceLow.x,t.targetLow.x),(std::min)(t.sourceHigh.x,t.targetHigh.x)));
        }
    };
    double speedLimit=limits_.speed;
    if(limits_.sideProbeDistance>0) {
        out.probeReason=sides(s,binding_.routeGeneration,ux,uy,limits_.sideProbeDistance,
            limits_.probe.maxQueries,queries,out);
        if(out.probeReason!=ProbeReason::None) return finish(out,WalkState::Failed,WalkReason::ProbeFailed);
        const auto margin=(std::min)(out.leftClearance,out.rightClearance);
        out.narrow=margin<limits_.narrowMargin;
        if(out.narrow) {
            speedLimit=limits_.narrowSpeed+(limits_.speed-limits_.narrowSpeed)*margin/limits_.narrowMargin;
            const double lateral=std::clamp((out.rightClearance-out.leftClearance)/2,-4.0,4.0)*
                (std::min)(1.0,distance*fraction/limits_.sideProbeDistance);
            const double forward=std::sqrt((std::max)(0.0,distance*distance*fraction*fraction-lateral*lateral));
            x=inward(s.position->x+ux*forward+uy*lateral,s.position->x);
            y=inward(s.position->y+uy*forward-ux*lateral,s.position->y);
            constrain(x,y);
        }
    }
    probeLimits.maxQueries=limits_.probe.maxQueries-out.queries+1; // same-decision ground cache
    queries.blocked.reset(); // Side obstructions are not the forward obstruction.
    const auto probe=GroundProbe::inspect(s,binding_.routeGeneration,area,x,y,index,indexMap,queries,probeLimits);
    out.queries=queries.issued; out.samples=probe.samples; out.steps=probe.steps; out.probeReason=probe.reason;
    if(!probe) {
        if(limits_.sideProbeDistance>0 && !queries.offCorridor &&
           (probe.reason==ProbeReason::Blocked || probe.reason==ProbeReason::NoSupport) && out.queries<limits_.probe.maxQueries) {
            runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,binding_.routeGeneration,++out.queries},
                runtime::QueryKind::Blocker,*s.position,{x,y,s.position->z},s.hull};
            runtime::WorldQueryResult r;
            try { r=queries.query(q); } catch(...) {}
            const bool classified=r.stamp==q.stamp && r.kind==q.kind &&
                r.error==runtime::QueryError::None && r.blocker;
            const bool dynamic=classified && (r.blocker->kind==runtime::BlockerKind::Player ||
                r.blocker->kind==runtime::BlockerKind::Teammate || r.blocker->kind==runtime::BlockerKind::Enemy ||
                r.blocker->kind==runtime::BlockerKind::Other);
            if(limits_.blocker.timeoutUs && (dynamic || blocker_ || r.error!=runtime::QueryError::None ||
               !(r.stamp==q.stamp) || r.kind!=q.kind)) {
                if(!blocker_) blocker_.emplace(out.binding,limits_.blocker);
                const auto d=blocker_->update({out.binding,q.stamp,r,nowUs});
                out.blockerAction=d.action; out.blockerReason=d.reason; out.blocker=blocker_->fact(nowUs);
                // Timeout retires the wait's cached fact. A current, stamped
                // observation can still justify one expiring route exclusion.
                if(d.reason==BlockerReason::TimedOut && dynamic) out.blocker=r.blocker;
                if(d.action==BlockerAction::Replan || d.action==BlockerAction::Aborted)
                    return finish(out,WalkState::Failed,WalkReason::DynamicBlocked);
                if(d.action!=BlockerAction::InspectAvoidance) return out;
            }
            if(classified && (r.blocker->kind==runtime::BlockerKind::Geometry || blocker_)) {
                const auto noSide=[&] {
                    if(blocker_) { out.blockerAction=BlockerAction::Yield; return out; }
                    return finish(out,WalkState::Failed,WalkReason::ProbeFailed);
                };
                if(blocker_ && avoidDecisions_>=limits_.maxAvoidanceDecisions) return noSide();
                if(avoidDecisions_>=limits_.maxAvoidanceDecisions || out.samples>=limits_.probe.maxSamples ||
                   out.queries>=limits_.probe.maxQueries) return finish(out,WalkState::Failed,WalkReason::ProbeFailed);
                if(!avoidSide_) avoidSide_=out.rightClearance>=out.leftClearance ? 1:-1;
                ++avoidDecisions_;
                const double free=avoidSide_>0 ? out.rightClearance:out.leftClearance;
                if(free<=1) return noSide();
                x=inward(s.position->x+avoidSide_*uy*(free-1),s.position->x);
                y=inward(s.position->y-avoidSide_*ux*(free-1),s.position->y);
                constrain(x,y);
                if(std::hypot(double(x)-s.position->x,double(y)-s.position->y)<0.1)
                    return noSide();
                auto budget=limits_.probe; budget.maxQueries=limits_.probe.maxQueries-out.queries+1;
                budget.maxSamples-=out.samples;
                const auto alternate=GroundProbe::inspect(s,binding_.routeGeneration,area,x,y,index,indexMap,queries,budget);
                out.queries=queries.issued; out.samples+=alternate.samples; out.steps+=alternate.steps;
                out.probeReason=alternate.reason;
                if(!alternate) {
                    if(!queries.offCorridor && (alternate.reason==ProbeReason::Blocked || alternate.reason==ProbeReason::NoSupport))
                        return noSide();
                    return finish(out,WalkState::Failed,WalkReason::ProbeFailed);
                }
                out.target=alternate.target; out.avoiding=true;
                const double length=std::hypot(double(x)-s.position->x,double(y)-s.position->y);
                out.intent.direction={(double(x)-s.position->x)/length,(double(y)-s.position->y)/length,0};
                out.intent.speed=(std::min)(limits_.narrowSpeed,length/0.120); return out;
            }
        }
        if(!queries.offCorridor && probe.reason==ProbeReason::Blocked && queries.blocked && limits_.doorTimeoutUs)
            return updateDoor(out,s,index,indexMap,queries,queries.blocked->end,nowUs);
        // A tall door may make a future floor probe start solid before its
        // horizontal sweep. Current ground is already verified; only a typed
        // door hit can enter stationary waiting. A gap still fails closed.
        if(!queries.offCorridor && probe.reason==ProbeReason::NoSupport && limits_.doorTimeoutUs)
            return updateDoor(out,s,index,indexMap,queries,{x,y,s.position->z},nowUs);
        return finish(out,WalkState::Failed,queries.offCorridor ? WalkReason::OffCorridor:WalkReason::ProbeFailed);
    }
    out.target=probe.target;
    if(blocker_) {
        if(!clearBlocker()) return finish(out,WalkState::Failed,WalkReason::DynamicBlocked);
        return out; // Complete verified passage invalidates the fact; translation waits for a new decision.
    }
    avoidSide_=0; avoidDecisions_=0;
    // Even a full 120 ms fresh-intent hold cannot pass the inspected endpoint.
    const double inspected=std::hypot(double(x)-s.position->x,double(y)-s.position->y);
    if(inspected==0) return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
    out.intent.direction={(double(x)-s.position->x)/inspected,(double(y)-s.position->y)/inspected,0};
    out.intent.speed=std::min(speedLimit,inspected/0.120);
    const double forward=(double(x)-s.position->x)*ux+(double(y)-s.position->y)*uy;
    const double lateral=(double(x)-s.position->x)*uy-(double(y)-s.position->y)*ux;
    if(out.narrow && forward>0 && std::abs(lateral)>0.001 && std::abs(lateral)<=forward) {
        constexpr double degrees=180/3.14159265358979323846;
        out.intent.direction={ux,uy,0}; out.intent.lateralCorrection=lateral/forward;
        out.intent.view=core::IntentVector{0,std::atan2(uy,ux)*degrees,0};
    }
    if(primitive_.state()==PrimitiveState::Running) {
        const auto update=primitive_.update({out.binding,s.tick,Progress::Running,out.intent,area,true});
        if(!update.accepted) return finish(out,WalkState::Failed,WalkReason::InvalidPortal);
        out.intent=update.intent;
    }
    return out;
}
}
