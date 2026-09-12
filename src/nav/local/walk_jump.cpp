// SPDX-License-Identifier: MPL-2.0
#include "nav/local/walk.hpp"
#include <algorithm>
#include <cmath>

namespace astrabot::nav::local {
namespace {
bool sameHull(const std::optional<runtime::HullDimensions>& a,
              const std::optional<runtime::HullDimensions>& b) noexcept {
    return a.has_value()==b.has_value() && (!a || (a->minimum==b->minimum && a->maximum==b->maximum));
}
bool same(Binding a,Binding b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
        a.routeGeneration==b.routeGeneration && a.step==b.step;
}
bool inside(const model::NavExtent& e,model::NavVector3 p,runtime::HullDimensions h) noexcept {
    return double(p.x)+h.minimum.x>=e.northWest.x && double(p.x)+h.maximum.x<=e.southEast.x &&
        double(p.y)+h.minimum.y>=e.northWest.y && double(p.y)+h.maximum.y<=e.southEast.y;
}

std::size_t buildJumpCandidates(std::array<JumpPlan,5>& candidates,const JumpPlan& base,
    const model::NavExtent& target,const runtime::HullDimensions& hull,JumpLimits limits,
    JumpGeometryLimits geometry,JumpCandidateRegionMode& regionMode) noexcept {
    const auto finite=[](double value) noexcept { return std::isfinite(value); };
    const auto bounds=[&](double margin) noexcept {
        return std::array<double,4>{double(target.northWest.x)-hull.minimum.x+margin,
            double(target.southEast.x)-hull.maximum.x-margin,
            double(target.northWest.y)-hull.minimum.y+margin,
            double(target.southEast.y)-hull.maximum.y-margin};
    };
    auto inner=bounds(geometry.clearanceMargin);
    const auto valid=[&](const std::array<double,4>& value) noexcept {
        return finite(value[0]) && finite(value[1]) && finite(value[2]) && finite(value[3]) &&
            value[0]<=value[1] && value[2]<=value[3];
    };
    regionMode=JumpCandidateRegionMode::Margin;
    if(!valid(inner)) {
        inner=bounds(0.0);
        regionMode=JumpCandidateRegionMode::PhysicalHull;
    }
    if(!valid(inner) || !finite(limits.maximumDistance) || limits.maximumDistance<=0) {
        regionMode=JumpCandidateRegionMode::Collapsed;
        return 0;
    }
    const auto lowX=inner[0],highX=inner[1],lowY=inner[2],highY=inner[3];

    const auto dx=double(base.landing.x)-base.takeoff.x;
    const auto dy=double(base.landing.y)-base.takeoff.y;
    const auto length=std::hypot(dx,dy);
    if(!finite(length) || length<=0) return 0;
    const auto ux=dx/length,uy=dy/length;
    const auto tx=-uy,ty=ux;
    const auto lateral=(std::min)(16.0,(std::min)((highX-lowX)*0.5,(highY-lowY)*0.5));
    std::size_t count{};
    const auto append=[&](double depth,double offset) noexcept {
        if(count==candidates.size()) return;
        const auto x=std::clamp(double(base.landing.x)+ux*depth+tx*offset,lowX,highX);
        const auto y=std::clamp(double(base.landing.y)+uy*depth+ty*offset,lowY,highY);
        const model::NavVector3 landing{static_cast<float>(x),static_cast<float>(y),base.landing.z};
        if(!landing.isFinite() || !inside(target,landing,hull) ||
            std::hypot(x-double(base.takeoff.x),y-double(base.takeoff.y))>limits.maximumDistance) return;
        for(std::size_t index{};index<count;++index)
            if(candidates[index].landing==landing) return;
        candidates[count]=base;
        candidates[count++].landing=landing;
    };
    append(5.0,0.0);
    if(lateral>0.001) append(5.0,lateral);
    if(lateral>0.001) append(5.0,-lateral);
    append(12.0,0.0);
    if(lateral>0.001) append(12.0,-lateral);
    return count;
}
std::size_t buildEnvelopeCandidates(std::array<JumpPlan,5>& candidates,const JumpPlan& base,
    const JumpLandingEnvelope& envelope,JumpLimits limits,JumpPhysics physics,double speedLimit,
    JumpCandidateRegionMode& regionMode) noexcept {
    const auto finite=[](double value) noexcept { return std::isfinite(value); };
    if(!envelope.target || !finite(limits.maximumDistance) || limits.maximumDistance<=0) {
        regionMode=JumpCandidateRegionMode::Collapsed; return 0;
    }
    const double dx=double(base.landing.x)-base.takeoff.x,dy=double(base.landing.y)-base.takeoff.y;
    const double length=std::hypot(dx,dy);
    if(length<=0) { regionMode=JumpCandidateRegionMode::NoBallisticLanding; return 0; }
    const double ux=dx/length,uy=dy/length,tx=-uy,ty=ux;
    std::size_t count=0;
    const auto appendRegion=[&](const JumpLandingRegion& region) noexcept {
        const double lateral=(std::min)(16.0,(std::min)((region.highX-region.lowX)*0.5,
            (region.highY-region.lowY)*0.5));
        const auto append=[&](double depth,double offset) noexcept {
            if(count==candidates.size()) return;
            const double x=std::clamp(double(base.landing.x)+ux*depth+tx*offset,region.lowX,region.highX);
            const double y=std::clamp(double(base.landing.y)+uy*depth+ty*offset,region.lowY,region.highY);
            if(std::hypot(x-double(base.takeoff.x),y-double(base.takeoff.y))>limits.maximumDistance) return;
            model::NavVector3 landing{static_cast<float>(x),static_cast<float>(y),0};
            const double z=query::projectToArea(region.extent,landing).z-
                (base.flightHull ? base.flightHull->minimum.z:0.0F);
            if(!landing.isFinite() || !finite(z)) return;
            landing.z=static_cast<float>(z);
            for(std::size_t i=0;i<count;++i)
                if(candidates[i].landingArea==region.area &&
                   std::hypot(double(candidates[i].landing.x)-landing.x,double(candidates[i].landing.y)-landing.y)<0.01)
                    return;
            const double range=std::hypot(double(landing.x)-base.takeoff.x,double(landing.y)-base.takeoff.y);
            if(range<=0 || !finite(speedLimit) || speedLimit<=0) return;
            const double desired=(std::min)((std::max)(limits.approachSpeed,limits.minimumSpeed),
                (std::min)(limits.maximumSpeed,speedLimit));
            if(desired<limits.minimumSpeed) return;
            const model::NavVector3 velocity{static_cast<float>((landing.x-base.takeoff.x)/range*desired),
                static_cast<float>((landing.y-base.takeoff.y)/range*desired),0};
            auto candidate=base;
            candidate.landing=landing; candidate.landingArea=region.area; candidate.landingAdvance=region.cursorAdvance;
            if(!solveJumpTrajectory(candidate.takeoff,velocity,candidate.landing,limits,physics)) return;
            candidates[count++]=candidate;
        };
        append(5.0,0.0); if(lateral>0.001) append(5.0,lateral); if(lateral>0.001) append(5.0,-lateral);
        append(12.0,0.0); if(lateral>0.001) append(12.0,-lateral);
    };
    appendRegion(envelope.target);
    if(envelope.successor) appendRegion(*envelope.successor);
    if(!count) { regionMode=JumpCandidateRegionMode::Collapsed; return 0; }
    regionMode=candidates[0].landingAdvance==2 ? JumpCandidateRegionMode::SuccessorCentreInset:
        (envelope.target.kind==JumpLandingRegionKind::HullMargin ? JumpCandidateRegionMode::Margin:
         JumpCandidateRegionMode::CentreInset);
    return count;
}
class JumpQueries final : public runtime::IWorldQueries {
public:
    JumpQueries(runtime::IWorldQueries& port,std::uint32_t reserved,std::uint32_t maximum) noexcept
        : issued(reserved),port_(port),maximum_(maximum) {}
    std::uint32_t issued;
    runtime::WorldQueryResult query(const runtime::QueryRequest& request) override {
        if(issued==maximum_) {
            runtime::WorldQueryResult r; r.stamp=request.stamp; r.kind=request.kind;
            r.error=runtime::QueryError::BudgetExceeded; return r;
        }
        auto wire=request; wire.stamp.ordinal=++issued;
        auto reply=port_.query(wire);
        if(reply.stamp==wire.stamp) reply.stamp=request.stamp;
        else reply.stamp={};
        return reply;
    }
private:
    runtime::IWorldQueries& port_;
    std::uint32_t maximum_;
};
}
WalkDecision Walk::updateDrop(WalkDecision out,const runtime::MovementSnapshot& s,
    const query::NavSpatialIndex& index,core::MapGeneration indexMap,runtime::IWorldQueries& port,
    std::uint64_t now,std::uint32_t reserved,std::optional<JumpPhysics> physics) noexcept {
    out.queries=reserved; out.dropState=dropState_; out.dropPlan=dropPlan_; out.jumpPhysics=physics;
    const auto fail=[&](DropReason reason) {
        out.dropReason=reason; out.dropState=DropState::Failed; out.intent={};
        return finish(out,WalkState::Failed,WalkReason::ProbeFailed);
    };
    if(!limits_.drop) return fail(DropReason::Disabled);
    const auto& l=*limits_.drop;
    const auto positive=[](double v) { return std::isfinite(v) && v>0; };
    if(cursor_.exhausted() || !positive(l.maximumFall) || l.maximumFall>256 ||
       !std::isfinite(l.maximumGap) || l.maximumGap<0 || l.maximumGap>64 || !positive(l.speed) ||
       l.speed>400 || !positive(l.arrivalTolerance) || l.arrivalTolerance>8 ||
       !std::isfinite(l.maximumDamage) || l.maximumDamage<0 || l.maximumDamage>100 ||
       !std::isfinite(l.minimumLandingHealth) || l.minimumLandingHealth<0 ||
       l.minimumLandingHealth>100 || !std::isfinite(l.maximumHealthFraction) ||
       l.maximumHealthFraction<0 || l.maximumHealthFraction>1 ||
       !l.approachTimeoutUs || !l.airborneTimeoutUs || !l.maxSegments || l.maxSegments>12 ||
       !l.maxQueries || l.maxQueries>21 || reserved>=l.maxQueries || reserved>=limits_.probe.maxQueries)
        return fail(DropReason::InvalidInput);
    if(!s.position || !s.position->isFinite() || !s.velocity || !s.velocity->isFinite() ||
       !s.hull || !s.hull->minimum.isFinite() || !s.hull->maximum.isFinite() ||
       s.hull->minimum.x>=s.hull->maximum.x || s.hull->minimum.y>=s.hull->maximum.y ||
       s.hull->minimum.z>=s.hull->maximum.z || !s.grounded || s.ducked!=false ||
       !s.speedLimit || !positive(*s.speedLimit) || !s.elapsedUs || s.elapsedUs>250000)
        return fail(DropReason::MissingObservation);
    if(!physics || !same(physics->binding,out.binding) || physics->tick!=s.tick ||
       !positive(physics->gravity) || (dropPlan_ && dropGravity_!=physics->gravity))
        return fail(DropReason::StalePhysics);
    const auto& t=corridor_->transitions()[cursor_.index()];
    const auto hints=constraints(t.edge.traversal,t.sourceAttributes,t.targetAttributes);
    if(t.effectiveTraversal!=model::NavTraversalKind::Drop ||
       !hints || hints.kind!=model::NavTraversalKind::Walk)
        return fail(DropReason::UnsafeGeometry);
    const auto goal=index.containing(goal_,limits_.probe.navTolerance);
    if(!goal || !*goal.value || (**goal.value).areaId!=corridor_->goal())
        return fail(DropReason::UnsafeGeometry);
    const auto maximum=(std::min)(l.maxQueries,limits_.probe.maxQueries);
    JumpQueries queries(port,reserved,maximum);
    auto groundLimits=limits_.probe; groundLimits.maxQueries=maximum-reserved;
    const auto doneFail=[&](DropReason reason) { out.queries=queries.issued; return fail(reason); };
    const auto& p=*s.position;
    const bool external=t.edge.external.has_value();
    const bool vertical=t.edge.direction==1 || t.edge.direction==3;
    const double sign=(t.edge.direction==1 || t.edge.direction==2) ? 1.0:-1.0;
    double ux=vertical ? sign:0, uy=vertical ? 0:sign;
    if(!dropPlan_) {
        if(s.grounded!=true) return doneFail(DropReason::MissingSupport);
        model::NavVector3 takeoff{}, landing{};
        double gap=0;
        if(external) {
            takeoff={static_cast<float>(t.sourceLow.x),static_cast<float>(t.sourceLow.y),
                     static_cast<float>(t.sourceLow.z)};
            landing={static_cast<float>(t.targetLow.x),static_cast<float>(t.targetLow.y),
                     static_cast<float>(t.targetLow.z)};
            gap=std::hypot(double(landing.x)-takeoff.x,double(landing.y)-takeoff.y);
            const double length=gap;
            if(length<=0) return doneFail(DropReason::UnsafeGeometry);
            ux=(landing.x-takeoff.x)/length;
            uy=(landing.y-takeoff.y)/length;
        } else {
            const auto projected=corridor_->target(cursor_.index(),{p.x,p.y,p.z},1);
            if(!projected) return doneFail(DropReason::UnsafeGeometry);
            const auto q=*projected.value;
            const double tangent=vertical ? q.y:q.x;
            const double boundary=vertical ? q.x:q.y;
            const double targetBoundary=vertical ? t.targetLow.x:t.targetLow.y;
            const double targetInset=(vertical ? (sign>0 ? -s.hull->minimum.x:s.hull->maximum.x)
                                                             : (sign>0 ? -s.hull->minimum.y:s.hull->maximum.y))+l.arrivalTolerance;
            const double landingNormal=std::clamp(targetBoundary+sign*targetInset,
                vertical ? double(t.targetExtent.northWest.x):double(t.targetExtent.northWest.y),
                vertical ? double(t.targetExtent.southEast.x):double(t.targetExtent.southEast.y));
            takeoff={static_cast<float>(vertical ? boundary-sign:tangent),
                static_cast<float>(vertical ? tangent:boundary-sign),0};
            landing={static_cast<float>(vertical ? landingNormal:tangent),
                static_cast<float>(vertical ? tangent:landingNormal),0};
            takeoff.z=static_cast<float>(query::projectToArea(t.sourceExtent,takeoff).z-s.hull->minimum.z);
            landing.z=static_cast<float>(query::projectToArea(t.targetExtent,landing).z-s.hull->minimum.z);
            gap=(targetBoundary-boundary)*sign;
        }
        const double fall=double(takeoff.z)-landing.z;
        if(!takeoff.isFinite() || !landing.isFinite() || fall<=limits_.probe.maxStepUp ||
           fall>l.maximumFall || gap<0 || gap>l.maximumGap ||
           !query::containsXY(t.targetExtent,landing))
            return doneFail(DropReason::UnsafeGeometry);
        const double impactSpeed=std::sqrt((std::max)(0.0,2.0*physics->gravity*fall));
        const double predictedDamage=(std::max)(0.0,impactSpeed-500.0) *
            (100.0/(1100.0-500.0)) * 1.25;
        if (!std::isfinite(predictedDamage) || predictedDamage>l.maximumDamage ||
            (predictedDamage>0 && (!s.health || !std::isfinite(*s.health) ||
             *s.health-predictedDamage<l.minimumLandingHealth ||
             predictedDamage>*s.health*l.maximumHealthFraction)))
            return doneFail(DropReason::UnsafeGeometry);
        dropPlan_=DropPlan{t.edge.source,t.edge.target,takeoff,landing,fall,gap,predictedDamage};
        dropGravity_=physics->gravity; dropStartedUs_=now; dropLastUs_=now;
    }
    out.dropPlan=dropPlan_;
    if(now<dropLastUs_) return doneFail(DropReason::Timeout);
    dropLastUs_=now;
    const auto plan=*dropPlan_;
    if(dropState_!=DropState::Airborne && now-dropStartedUs_>=l.approachTimeoutUs)
        return doneFail(DropReason::Timeout);
    if(s.grounded==false) {
        if(dropState_==DropState::Approach) return doneFail(DropReason::MissingSupport);
        if(dropState_!=DropState::Airborne) { dropState_=DropState::Airborne; dropAirborneUs_=now; }
        if(now-dropAirborneUs_>=l.airborneTimeoutUs) return doneFail(DropReason::Timeout);
    } else {
        const auto supported=GroundProbe::locate(s,binding_.routeGeneration,index,indexMap,queries,groundLimits);
        out.queries=queries.issued; out.probeReason=supported.reason;
        if(!supported) return doneFail(DropReason::MissingSupport);
        out.support=supported.target;
        if(dropState_==DropState::Airborne) {
            if(supported.target->area!=plan.target || !query::containsXY(t.targetExtent,p) ||
               std::abs(double(p.z)-plan.landing.z)>limits_.probe.supportTolerance ||
               !cursor_.advance(out.binding.step,supported.target->area,true))
                return doneFail(DropReason::WrongLanding);
            out.dropState=DropState::Landed; out.primitiveEvent=PrimitiveEvent::Complete;
            dropPlan_.reset(); dropState_=DropState::Approach; primitive_=Primitive{};
            return out;
        }
        if(supported.target->area!=plan.source) return doneFail(DropReason::WrongLanding);
    }
    const double distance=std::hypot(double(plan.takeoff.x)-p.x,double(plan.takeoff.y)-p.y);
    if(dropState_==DropState::Approach && distance>l.arrivalTolerance) {
        groundLimits.maxQueries=maximum-queries.issued;
        const auto proof=GroundProbe::inspect(s,binding_.routeGeneration,plan.source,
            plan.takeoff.x,plan.takeoff.y,index,indexMap,queries,groundLimits);
        out.queries=queries.issued; out.probeReason=proof.reason;
        if(!proof || proof.target->area!=plan.source) return doneFail(DropReason::MissingSupport);
        out.target=proof.target;
        const double dx=double(proof.target->origin.x)-p.x,dy=double(proof.target->origin.y)-p.y;
        const double range=std::hypot(dx,dy);
        if(range<=0) return doneFail(DropReason::Blocked);
        out.intent.direction={dx/range,dy/range,0};
        if(!core::Motor::bindLocomotion(out.intent,core::LocomotionMode::Walk,*s.speedLimit,range) ||
           out.intent.validForUs<s.elapsedUs) return doneFail(DropReason::Blocked);
        out.progressDirection={ux,uy,0}; return out;
    }
    DropReason queryFailure=DropReason::None;
    auto sweptHull=*s.hull;
    const auto fetch=[&](runtime::QueryKind kind,model::NavVector3 a,model::NavVector3 b)
        ->std::optional<runtime::WorldQueryResult> {
        if(queries.issued>=maximum) { queryFailure=DropReason::BudgetExceeded; return {}; }
        runtime::QueryRequest request{{binding_.agent,binding_.actor,binding_.map,s.tick,
            binding_.routeGeneration,1},kind,a,b,s.hull,limits_.probe.navTolerance};
        if(kind==runtime::QueryKind::SweptHull) request.hull=sweptHull;
        try {
            const auto r=queries.query(request); out.queries=queries.issued;
            if(!(r.stamp==request.stamp) || r.kind!=kind) queryFailure=DropReason::StaleQuery;
            else if(r.error!=runtime::QueryError::None) queryFailure=DropReason::QueryFailed;
            else return r;
        } catch(...) { queryFailure=DropReason::QueryFailed; }
        return {};
    };
    const auto sweep=[&](model::NavVector3 a,model::NavVector3 b) {
        const auto r=fetch(runtime::QueryKind::SweptHull,a,b);
        if(!r) return false;
        if(!r->hull || r->hull->startSolid || r->hull->fraction!=1 ||
           !r->hull->end.isFinite() || !r->hull->normal.isFinite() ||
           std::hypot(double(r->hull->end.x)-b.x,double(r->hull->end.y)-b.y)>0.001 ||
           std::abs(double(r->hull->end.z)-b.z)>0.001) { queryFailure=DropReason::Blocked; return false; }
        return true;
    };
    // Release starts after the complete hull has cleared the source ledge.
    const double leading=external ? 0.0 :
        (vertical ? (sign>0 ? -s.hull->minimum.x:s.hull->maximum.x)
                  : (sign>0 ? -s.hull->minimum.y:s.hull->maximum.y));
    model::NavVector3 release=plan.takeoff;
    release.x=static_cast<float>(release.x+ux*(leading+2));
    release.y=static_cast<float>(release.y+uy*(leading+2));
    const bool airborne=dropState_==DropState::Airborne;
    const auto origin=airborne ? p:release;
    const double height=double(origin.z)-plan.landing.z;
    const double vz=airborne ? s.velocity->z:0;
    if(height<0 || height>l.maximumFall || vz>1) return doneFail(DropReason::UnsafeGeometry);
    const double flight=(vz+std::sqrt(vz*vz+2*physics->gravity*height))/physics->gravity;
    if(!positive(flight) || flight>double(l.airborneTimeoutUs)/1000000)
        return doneFail(DropReason::UnsafeGeometry);
    const double desired=std::hypot(double(plan.landing.x)-release.x,double(plan.landing.y)-release.y)/flight;
    const double forward=s.velocity->x*ux+s.velocity->y*uy;
    const double lateral=std::abs(s.velocity->x*uy-s.velocity->y*ux);
    if(forward<0 || forward>l.speed || forward>*s.speedLimit ||
       (airborne && lateral>4)) return doneFail(DropReason::UnsafeVelocity);
    if(!airborne && (desired<=0 || desired>l.speed || desired>*s.speedLimit))
        return doneFail(DropReason::UnsafeVelocity);
    if(!airborne && (forward<desired*0.8 || forward>desired*1.2 || lateral>4)) {
        // Accelerating is allowed only while the entire issued segment remains
        // physically supported on the source. Never push over the lip blind.
        const double travel=desired*double(s.elapsedUs)/1000000;
        const float x=static_cast<float>(p.x+ux*travel),y=static_cast<float>(p.y+uy*travel);
        if(!query::containsXY(t.sourceExtent,{x,y,p.z})) return doneFail(DropReason::UnsafeVelocity);
        groundLimits.maxQueries=maximum-queries.issued;
        const auto proof=GroundProbe::inspect(s,binding_.routeGeneration,plan.source,x,y,index,indexMap,queries,groundLimits);
        if(!proof || proof.target->area!=plan.source) return doneFail(DropReason::MissingSupport);
        if(std::hypot(double(proof.target->origin.x)-x,double(proof.target->origin.y)-y)>0.01)
            return doneFail(DropReason::MissingSupport);
        out.queries=queries.issued; out.target=proof.target; out.intent.direction={ux,uy,0};
        out.intent.speed=desired; out.progressDirection={ux,uy,0}; return out;
    }
    model::NavVector3 landing{static_cast<float>(origin.x+s.velocity->x*flight),
        static_cast<float>(origin.y+s.velocity->y*flight),plan.landing.z};
    if(!landing.isFinite() || !query::containsXY(t.targetExtent,landing)) return doneFail(DropReason::UnsafeVelocity);
    const auto support=fetch(runtime::QueryKind::GroundedArea,landing,landing);
    if(!support) return doneFail(queryFailure);
    if(!support->ground || support->ground->area!=plan.target || !support->ground->floor)
        return doneFail(DropReason::MissingSupport);
    const auto floor=*support->ground->floor;
    const double normal=double(floor.normal.x)*floor.normal.x+double(floor.normal.y)*floor.normal.y+double(floor.normal.z)*floor.normal.z;
    const auto area=index.containing({landing.x,landing.y,floor.height},limits_.probe.navTolerance);
    if(!floor.supported || !floor.normal.isFinite() || normal<0.99 || normal>1.01 ||
       floor.normal.z<limits_.probe.minNormalZ || !std::isfinite(floor.height) ||
       std::abs(double(landing.z)+s.hull->minimum.z-floor.height)>limits_.probe.supportTolerance ||
       !area || !*area.value || (**area.value).areaId!=plan.target)
        return doneFail(DropReason::MissingSupport);
    const auto clear=fetch(runtime::QueryKind::Clearance,landing,landing);
    if(!clear) return doneFail(queryFailure);
    if(!clear->clearance || !clear->clearance->clear) return doneFail(DropReason::Blocked);
    if(!airborne && !sweep(p,release)) return doneFail(queryFailure);
    const double count=std::ceil(flight/0.05);
    if(count<1 || count>l.maxSegments) return doneFail(DropReason::BudgetExceeded);
    const auto segments=static_cast<std::uint32_t>(count);
    // A falling parabola is above its chord. Sweep the complete upper envelope
    // so discrete chords cannot certify a ceiling the true hull would hit.
    const double chordSeconds=flight/segments;
    sweptHull.maximum.z=static_cast<float>(sweptHull.maximum.z+physics->gravity*chordSeconds*chordSeconds/8+0.01);
    if(!sweptHull.maximum.isFinite()) return doneFail(DropReason::UnsafeGeometry);
    auto previous=origin;
    for(std::uint32_t i=1;i<=segments;++i) {
        const double time=flight*i/segments;
        model::NavVector3 next{static_cast<float>(origin.x+s.velocity->x*time),
            static_cast<float>(origin.y+s.velocity->y*time),
            static_cast<float>(origin.z+vz*time-0.5*physics->gravity*time*time)};
        if(!next.isFinite() || !sweep(previous,next)) return doneFail(queryFailure);
        previous=next;
    }
    if(!airborne) {
        dropState_=DropState::StepOff;
        out.intent.direction={ux,uy,0}; out.intent.speed=forward;
        out.intent.view=core::IntentVector{0,std::atan2(uy,ux)*180/3.14159265358979323846,0};
    }
    out.dropState=dropState_; out.progressDirection={ux,uy,0}; return out;
}

bool Walk::reportJumpDispatch(const JumpDispatch& dispatch) noexcept {
    auto expected=binding_; expected.step=cursor_.index();
    if(!jump_ || jump_->state()!=JumpState::Takeoff || !jumpPressTick_.isValid() || jumpDispatchSeen_ ||
       !same(dispatch.binding,expected) || dispatch.commandTick!=jumpPressTick_) return false;
    jumpDispatch_=dispatch; jumpDispatchSeen_=true; return true;
}
WalkDecision Walk::updateJump(WalkDecision out,const runtime::MovementSnapshot& s,const query::NavSpatialIndex& index,
    core::MapGeneration indexMap,runtime::IWorldQueries& port,std::uint64_t nowUs,std::uint32_t reserved,
    std::optional<JumpPhysics> physics) noexcept {
    out.queries=reserved; out.jumpState=jump_ ? jump_->state():JumpState::Approach;
    out.jumpPlan=jumpPlan_; out.jumpPhysics=physics; out.jumpPressTick=jumpPressTick_;
    out.jumpLandingEnvelope=jumpLandingEnvelope_;
    out.jumpCandidateIndex=static_cast<std::uint8_t>(jumpCandidateIndex_);
    out.jumpCandidateCount=static_cast<std::uint8_t>(jumpCandidateCount_);
    out.jumpCandidateRegionMode=jumpCandidateRegionMode_;
    if(jumpPlan_) {
        out.jumpCandidateLanding=jumpPlan_->landing;
        out.jumpCandidateArea=jumpPlan_->landingArea;
        out.jumpCandidateAdvance=jumpPlan_->landingAdvance;
    }
    const bool obstacleStampMatches = observedObstacleHull_ &&
        observedObstacleHull_->stamp.agent == s.agent &&
        observedObstacleHull_->stamp.actor == s.actor &&
        observedObstacleHull_->stamp.map == s.map &&
        observedObstacleHull_->stamp.tick == s.tick &&
        observedObstacleHull_->stamp.routeGeneration == binding_.routeGeneration &&
        observedObstacleHull_->step == cursor_.index();
    if (observedObstacleHull_ && !obstacleStampMatches)
        observedObstacleHull_.reset();
    if (obstacleStampMatches) {
        out.obstacleHull=observedObstacleHull_->hull;
        out.obstacleClass=ObstacleClass::Unknown;
    }
    const auto fail=[&](JumpReason reason) {
        observedJumpCandidate_=false;
        if(jumpAttemptKey_) jumpAttempts_->erase(*jumpAttemptKey_);
        jumpAttempt_=nullptr; jumpAttemptKey_.reset();
        jumpCandidateCount_=0;
        jumpCandidateIndex_=0;
        out.jumpReason=reason; out.jumpState=JumpState::Failed;
        return finish(out,WalkState::Failed,WalkReason::JumpFailed);
    };
    if(!limits_.jump || cursor_.exhausted()) return finish(out,WalkState::Failed,WalkReason::UnsupportedTraversal);
    const auto& t=corridor_->transitions()[cursor_.index()];
    const auto hints=constraints(t.effectiveTraversal==model::NavTraversalKind::Jump || observedJumpCandidate_ ?
        model::NavTraversalKind::Jump:t.edge.traversal,t.sourceAttributes,t.targetAttributes);
    out.constraintReason=hints.reason;
    if(!hints || hints.kind!=model::NavTraversalKind::Jump)
        return finish(out,WalkState::Failed,WalkReason::UnsupportedTraversal);
    const auto goal=index.containing(goal_,limits_.probe.navTolerance);
    if(!goal || !*goal.value || (**goal.value).areaId!=corridor_->goal())
        return finish(out,WalkState::Failed,WalkReason::InvalidGoal);
    if(!physics) return fail(JumpReason::MissingObservation);
    if(!same(physics->binding,out.binding) || physics->tick!=s.tick || !std::isfinite(physics->gravity) ||
       physics->gravity<=0 || !std::isfinite(physics->verticalImpulse) || physics->verticalImpulse<=0 ||
       (jumpPhysics_ && (jumpPhysics_->gravity!=physics->gravity || jumpPhysics_->verticalImpulse!=physics->verticalImpulse ||
         jumpPhysics_->crouchSpeedMultiplier!=physics->crouchSpeedMultiplier ||
         !sameHull(jumpPhysics_->standingHull,physics->standingHull) ||
         !sameHull(jumpPhysics_->crouchingHull,physics->crouchingHull))))
        return fail(JumpReason::StaleInspection);
    auto profile=*limits_.jump;
    const auto capability=deriveJumpLimits(profile.motion,*physics,s,hints);
    if(!capability) return fail(JumpReason::MissingObservation);
    profile.motion=*capability;
    profile.geometry.preferredDistance=(std::min)(profile.geometry.preferredDistance,profile.motion.maximumDistance*0.9);
    const auto maximum=(std::min)({limits_.probe.maxQueries,profile.motion.maxQueries,profile.flight.maxQueries,21U});
    if(!maximum || reserved>maximum) return fail(JumpReason::InvalidInput);
    jumpPhysics_=physics;
    if(jump_ && jump_->state()==JumpState::Complete && limits_.crouch.transitionTimeoutUs) {
        if(!s.position || !s.hull || s.grounded!=true) return fail(JumpReason::WrongLanding);
        if(!crouch_) crouch_.emplace(binding_,limits_.crouch);
        const auto pose=crouch_->update(s,hints.targetDuck,nowUs,port,reserved,maximum);
        out.queries+=pose.queries; posture_=pose.state; postureReason_=pose.reason; postureAction_=pose.intent.duck;
        out.intent=pose.intent; out.intent.jump=ActionRequest::Release;
        if(pose.terminalEvent) return finish(out,WalkState::Failed,WalkReason::PostureFailed);
        if(!pose.movementAllowed) return out;
        auto ground=limits_.probe; ground.maxQueries=maximum-out.queries;
        JumpQueries queries(port,out.queries,maximum);
        const auto support=GroundProbe::locate(s,out.binding.routeGeneration,index,indexMap,queries,ground);
        out.queries=queries.issued;
        if(!support || support.target->area!=jumpPlan_->target || !query::containsXY(t.targetExtent,*s.position))
            return fail(JumpReason::WrongLanding);
        out.support=support.target;
        const auto completed=primitive_.update({out.binding,s.tick,Progress::Complete,{},out.support->area,true});
        if(!completed.accepted || completed.state!=PrimitiveState::Complete || !cursor_.advance(out.binding.step,out.support->area,true))
            return fail(JumpReason::WrongLanding);
        out.primitiveEvent=completed.event; completedJumpStep_=out.binding.step; observedJumpCandidate_=false;
        if(jumpAttemptKey_) jumpAttempts_->erase(*jumpAttemptKey_);
        jumpAttempt_=nullptr; jumpAttemptKey_.reset();
        primitive_=Primitive{}; jump_.reset(); jumpPlan_.reset(); jumpLandingEnvelope_.reset(); jumpPhysics_.reset(); jumpDispatch_.reset();
        jumpPressTick_={}; out.intent={}; out.intent.jump=ActionRequest::Release;
        return out;
    }
    if(!jump_) {
        if(limits_.crouch.transitionTimeoutUs) {
            if(!crouch_) crouch_.emplace(binding_,limits_.crouch);
            const auto pose=crouch_->update(s,hints.sourceDuck,nowUs,port,reserved,maximum);
            out.queries+=pose.queries; posture_=pose.state; postureReason_=pose.reason; postureAction_=pose.intent.duck;
            if(pose.terminalEvent) return finish(out,WalkState::Failed,WalkReason::PostureFailed);
            if(!pose.movementAllowed) return out;
        }
        const auto candidate=JumpGeometry::derive(*corridor_,out.binding,s,profile.motion,profile.geometry,observedJumpCandidate_);
        out.jumpGeometryReason=candidate.reason;
        if(!candidate) return fail(JumpReason::InvalidInput);
        if(observedJumpCandidate_ && (primitive_.state()!=PrimitiveState::Running ||
           !same(primitive_.binding(),out.binding))) return fail(JumpReason::InvalidInput);
        if(!observedJumpCandidate_) {
            const auto entered=primitive_.enter(out.binding,t,s.tick);
            out.primitiveEvent=entered.event;
            if(!entered.accepted || entered.state!=PrimitiveState::Running) return fail(JumpReason::InvalidInput);
        }
    jumpLandingEnvelope_=candidate.landingEnvelope;
    out.jumpLandingEnvelope=jumpLandingEnvelope_;
    if(!jumpLandingEnvelope_) return fail(JumpReason::InvalidInput);
    jumpCandidateCount_=buildEnvelopeCandidates(jumpCandidates_,*candidate.plan,*jumpLandingEnvelope_,
        profile.motion,*physics,*s.speedLimit,jumpCandidateRegionMode_);
    out.jumpCandidateRegionMode=jumpCandidateRegionMode_;
    if(!jumpCandidateCount_) {
        out.disposition=MotionDisposition::Recovery;
        out.intent={};
        return fail(JumpReason::NoLandingCandidate);
    }
    jumpCandidateIndex_=0;
    jumpPlan_=jumpCandidates_[jumpCandidateIndex_];
        jumpAttemptKey_=JumpAttemptKey{out.binding.agent,out.binding.actor,out.binding.map,
            jumpPlan_->source,jumpPlan_->target,model::NavTraversalKind::Jump};
    jumpAttempt_=&jumpAttempts_->acquire(*jumpAttemptKey_,nowUs);
    jump_.emplace(out.binding,*jumpPlan_,profile.motion,jumpAttempt_);
    observedJumpCandidate_=false;
        jumpDispatch_.reset(); jumpDispatchSeen_=false; jumpPressTick_={};
    out.jumpPlan=jumpPlan_; out.jumpPressTick={};
    out.jumpCandidateIndex=static_cast<std::uint8_t>(jumpCandidateIndex_);
                out.jumpCandidateCount=static_cast<std::uint8_t>(jumpCandidateCount_);
                out.jumpCandidateLanding=jumpPlan_->landing;
                out.jumpCandidateArea=jumpPlan_->landingArea;
                out.jumpCandidateAdvance=jumpPlan_->landingAdvance;
    out.jumpCandidateArea=jumpPlan_->landingArea;
    out.jumpCandidateAdvance=jumpPlan_->landingAdvance;
        out.disposition=MotionDisposition::Hold;
        if(jumpPlan_) {
            const double dx=double(jumpPlan_->landing.x)-jumpPlan_->takeoff.x;
            const double dy=double(jumpPlan_->landing.y)-jumpPlan_->takeoff.y;
            const double length=std::hypot(dx,dy);
            if(length>0) out.progressDirection={dx/length,dy/length,0};
        }
        return out; // Primitive entry owns its own tick.
    }
    JumpFeedback feedback{out.binding,s,nowUs,{},jumpDispatch_}; jumpDispatch_.reset();
    const auto state=jump_->state();
    if(s.grounded==true && state!=JumpState::Takeoff) {
        out.jumpProofPhase=(state==JumpState::Airborne || state==JumpState::Recover) ?
            JumpProofPhase::Land:(state==JumpState::Accelerate ? JumpProofPhase::Launch:JumpProofPhase::Prepare);
    }
    const auto proofRequired=out.jumpProofPhase==JumpProofPhase::None ? 0U:maximum;
    out.jumpLaunchRequiredQueries=proofRequired;
    out.jumpReservedQueries=reserved;
    out.jumpLaunchAvailableQueries=maximum>reserved ? maximum-reserved:0;
    if(out.jumpProofPhase!=JumpProofPhase::None && reserved) {
        out.jumpLaunchDeferred=true;
        out.jumpProofDeferReason=JumpProofDeferReason::ReservedQueries;
        out.disposition=MotionDisposition::Hold;
        return out;
    }
    JumpQueries queries(port,reserved,maximum);
    // Takeoff waits for dispatch/observed airborne without issuing ground motion.
    // Airborne decisions need no fictitious support. Landing always does.
    JumpProbeResult proof;
    if(s.grounded==true && state!=JumpState::Takeoff) {
        if(reserved==maximum) proof.reason=JumpProbeReason::BudgetExceeded;
        else {
        auto ground=limits_.probe; ground.maxQueries=maximum;
            if(state==JumpState::Airborne || state==JumpState::Recover)
                proof=JumpProbe::land(s,out.binding,*jumpPlan_,profile.motion,ground,index,indexMap,queries);
            else if(state==JumpState::Accelerate) {
            auto flight=profile.flight; flight.maxQueries=maximum;
                flight.supportTolerance=profile.motion.supportTolerance;
                flight.supportProbeDepth=limits_.probe.probeDepth;
                proof=JumpProbe::launch(s,out.binding,*jumpPlan_,profile.motion,*physics,flight,index,indexMap,queries);
            } else proof=JumpProbe::prepare(s,out.binding,*jumpPlan_,profile.motion,ground,index,indexMap,queries);
        }
    out.jumpProbeReason=proof.reason;
    out.jumpSupportReason=proof.supportReason;
    out.jumpSupportInitialReason=proof.supportInitialReason;
    out.jumpSupportFallbackAttempted=proof.supportFallbackAttempted;
    out.jumpSupportFallbackAccepted=proof.supportFallbackAccepted;
    out.jumpSupportInitialTrace=proof.supportInitialTrace;
    out.jumpSupportFallbackTrace=proof.supportFallbackTrace;
    out.jumpSupportEvidence=proof.supportEvidence;
    out.jumpLandingFailure=proof.landingFailure;
    out.jumpPredictedLanding=proof.touchdown.value_or(model::NavVector3{});
    out.jumpLandingError=proof.landingError;
    out.jumpTrajectoryReady=proof.trajectoryReady;
        if(proof) {
            feedback.inspection=proof.inspection;
            feedback.inspection->queries=queries.issued;
            feedback.inspection->attemptId=jumpAttempt_ ? jumpAttempt_->attemptId:0;
            out.support=proof.inspection->support; out.target=proof.inspection->approach;
        }
        feedback.takeoffProof=proof.takeoffProof;
        feedback.flightProof=proof.flightProof;
        feedback.landingProof=proof.landingProof;
        feedback.proofProvenance=proof.provenance;
        if(!proof && proof.reason==JumpProbeReason::BudgetExceeded)
            feedback.proofProvenance=JumpProofProvenance::QueryBudget;
        else if(!proof && (proof.reason==JumpProbeReason::StaleNavigation ||
                           proof.reason==JumpProbeReason::StalePhysics ||
                           proof.reason==JumpProbeReason::StaleQuery))
            feedback.proofProvenance=JumpProofProvenance::Stale;
        else if(!proof && proof.reason!=JumpProbeReason::None &&
                feedback.proofProvenance==JumpProofProvenance::None)
            feedback.proofProvenance=JumpProofProvenance::QueryUnavailable;
    }
    out.queries=queries.issued;
    const auto plannedStaticFailure=[&]() noexcept {
        if(proof.landingFailure!=JumpLandingFailure::PlannedSupport ||
            proof.provenance==JumpProofProvenance::QueryBudget ||
            proof.provenance==JumpProofProvenance::QueryUnavailable ||
            proof.provenance==JumpProofProvenance::Stale ||
            proof.provenance==JumpProofProvenance::DynamicBlocker) return false;
        const auto& support=proof.supportEvidence[static_cast<std::size_t>(JumpSupportRole::PlannedLanding)];
        return support.reason==ProbeReason::AllSolid || support.reason==ProbeReason::StartSolid ||
            support.reason==ProbeReason::FloorHeightMismatch || support.reason==ProbeReason::NavContainmentMissing;
    };
    if(state==JumpState::Accelerate && plannedStaticFailure() && jumpCandidateIndex_+1<jumpCandidateCount_) {
        ++jumpCandidateIndex_;
        jumpPlan_=jumpCandidates_[jumpCandidateIndex_];
        jump_.emplace(out.binding,*jumpPlan_,profile.motion,jumpAttempt_);
        jumpDispatch_.reset();
        jumpPressTick_={};
        out.jumpPlan=jumpPlan_;
        out.jumpCandidateIndex=static_cast<std::uint8_t>(jumpCandidateIndex_);
        out.jumpCandidateCount=static_cast<std::uint8_t>(jumpCandidateCount_);
        out.jumpCandidateLanding=jumpPlan_->landing;
        out.jumpCandidateSwitchReason=JumpCandidateSwitchReason::PlannedLandingStaticFailure;
        out.disposition=MotionDisposition::Hold;
        return out;
    }
    const auto decision=jump_->update(feedback);
    out.jumpState=decision.state; out.jumpReason=decision.reason; out.jumpPressTick=decision.pressTick;
    out.jumpAttemptId=decision.attemptId; out.jumpAttemptStartedUs=decision.attemptStartedUs;
    out.jumpTakeoffProof=decision.takeoffProof; out.jumpFlightProof=decision.flightProof;
    out.jumpLandingProof=decision.landingProof; out.jumpProofProvenance=decision.proofProvenance;
    out.jumpReadiness=decision.readiness; out.jumpRecoveryDisposition=decision.recoveryDisposition;
    out.jumpAxis=decision.jumpAxis; out.jumpCommandDirection=decision.commandDirection;
    out.jumpFromTakeoff=decision.fromTakeoff; out.jumpAlong=decision.along;
    out.jumpLateral=decision.lateral; out.jumpMinimumSpeed=decision.minimumSpeed;
    out.jumpMaximumSpeed=decision.maximumSpeed; out.jumpSpeedLimit=decision.speedLimit;
    out.jumpDesiredSpeed=decision.desiredSpeed; out.jumpValidatedDistance=decision.validatedDistance;
    out.jumpValidForUs=decision.validForUs;
    if(observedObstacleHull_) {
        const bool transient=decision.proofProvenance==JumpProofProvenance::QueryBudget ||
            decision.proofProvenance==JumpProofProvenance::QueryUnavailable ||
            decision.proofProvenance==JumpProofProvenance::Stale ||
            decision.proofProvenance==JumpProofProvenance::DynamicBlocker ||
            decision.reason==JumpReason::StaleInspection ||
            decision.reason==JumpReason::ProofTimeout;
        if(decision.state==JumpState::Takeoff || decision.state==JumpState::Airborne ||
           decision.state==JumpState::Recover || decision.state==JumpState::Complete) {
            out.obstacleClass=ObstacleClass::Jumpable;
        } else if(transient) {
            out.obstacleClass=ObstacleClass::Unknown;
        } else if(decision.state==JumpState::Failed || decision.state==JumpState::Aborted) {
            out.obstacleClass=ObstacleClass::TooHigh;
        } else {
            out.obstacleClass=ObstacleClass::Unknown;
        }
    }
    out.intent=decision.intent; postureAction_=decision.intent.duck;
    if(jumpPlan_) {
        const double dx=double(jumpPlan_->landing.x)-jumpPlan_->takeoff.x;
        const double dy=double(jumpPlan_->landing.y)-jumpPlan_->takeoff.y;
        const double length=std::hypot(dx,dy);
        if(length>0) out.progressDirection={dx/length,dy/length,0};
    }
    const bool zeroMovement=out.intent.speed<=0 && out.intent.direction.x==0 &&
        out.intent.direction.y==0 && out.intent.jump!=ActionRequest::Press;
    if(zeroMovement && (decision.state==JumpState::Approach ||
                        decision.state==JumpState::Align ||
                        decision.state==JumpState::Accelerate ||
                        decision.state==JumpState::Takeoff)) {
        out.disposition=decision.recoveryDisposition==RecoveryDisposition::Retry ||
            decision.recoveryDisposition==RecoveryDisposition::EdgeCooldown ||
            decision.recoveryDisposition==RecoveryDisposition::StructuralEdgeExclusion
            ? MotionDisposition::Recovery : MotionDisposition::Hold;
    }
    if(decision.intent.jump==ActionRequest::Press) jumpPressTick_=decision.pressTick;
    if(decision.state==JumpState::Failed || decision.state==JumpState::Aborted) {
        if(decision.state==JumpState::Failed && jumpAttemptKey_) jumpAttempts_->erase(*jumpAttemptKey_);
        jumpAttempt_=nullptr; jumpAttemptKey_.reset();
        jumpCandidateCount_=0;
        jumpCandidateIndex_=0;
        return finish(out,decision.state==JumpState::Aborted ? WalkState::Aborted:WalkState::Failed,WalkReason::JumpFailed);
    }
    if(decision.state==JumpState::Complete && !limits_.crouch.transitionTimeoutUs) {
        if(!jumpPlan_ || !out.support || !s.position || !s.hull ||
           out.support->area!=jumpLandingArea(*jumpPlan_)) return fail(JumpReason::WrongLanding);
        const auto extentFor=[&]() -> const JumpLandingRegion* {
            if(!jumpLandingEnvelope_) return nullptr;
            if(jumpLandingEnvelope_->target.area==jumpLandingArea(*jumpPlan_)) return &jumpLandingEnvelope_->target;
            if(jumpLandingEnvelope_->successor && jumpLandingEnvelope_->successor->area==jumpLandingArea(*jumpPlan_))
                return &*jumpLandingEnvelope_->successor;
            return nullptr;
        };
        const auto* landingRegion=extentFor();
        if(!landingRegion || !query::containsXY(landingRegion->extent,*s.position) ||
           (landingRegion->kind==JumpLandingRegionKind::HullMargin &&
            !inside(landingRegion->extent,*s.position,*s.hull))) return fail(JumpReason::WrongLanding);
        const auto completed=primitive_.update({out.binding,s.tick,Progress::Complete,{},out.support->area,true});
        if(!completed.accepted || completed.state!=PrimitiveState::Complete ||
           !cursor_.advanceLanding(out.binding.step,out.support->area,jumpLandingAdvance(*jumpPlan_),true))
            return fail(JumpReason::WrongLanding);
        out.primitiveEvent=completed.event;
        completedJumpStep_=out.binding.step+jumpLandingAdvance(*jumpPlan_)-1;
        observedJumpCandidate_=false;
        if(jumpAttemptKey_) jumpAttempts_->erase(*jumpAttemptKey_);
    jumpAttempt_=nullptr; jumpAttemptKey_.reset();
    jumpCandidateCount_=0;
    jumpCandidateIndex_=0;
    primitive_=Primitive{}; jump_.reset(); jumpPlan_.reset(); jumpLandingEnvelope_.reset(); jumpPhysics_.reset(); jumpDispatch_.reset();
        jumpPressTick_={};
        out.intent={}; out.intent.jump=ActionRequest::Release;
    }
    return out;
}
}
