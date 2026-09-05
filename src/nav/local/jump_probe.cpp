// SPDX-License-Identifier: MPL-2.0
#include "nav/local/jump_probe.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::nav::local {
namespace {
bool same(Binding a,Binding b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
        a.routeGeneration==b.routeGeneration && a.step==b.step;
}
double distance(model::NavVector3 a,model::NavVector3 b) noexcept {
    return std::hypot(double(a.x)-b.x,double(a.y)-b.y);
}
bool positive(double n) noexcept { return std::isfinite(n) && n>0; }
bool representable(double n) noexcept {
    return std::isfinite(n) && std::abs(n)<=(std::numeric_limits<float>::max)();
}
}
JumpProbeResult JumpProbe::launch(const runtime::MovementSnapshot& s,Binding binding,
    JumpPlan plan,JumpLimits motion,JumpPhysics physics,JumpProbeLimits limits,
    const query::NavSpatialIndex& index,core::MapGeneration indexMap,runtime::IWorldQueries& port) noexcept {
    JumpProbeResult result;
    const auto fail=[&](JumpProbeReason reason) { result.reason=reason; result.inspection.reset(); return result; };
    if(!binding.agent.isValid() || !binding.actor.isValid() || !binding.map.isValid() || !binding.routeGeneration ||
       s.agent!=binding.agent || s.actor!=binding.actor || s.map!=binding.map || !s.tick.isValid() ||
       s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true || s.alive!=true || s.joined!=true ||
       s.grounded!=true || s.ducked!=false || !s.position || !s.position->isFinite() ||
       !s.velocity || !s.velocity->isFinite() || !s.view || !s.view->isFinite() ||
       !s.speedLimit || !positive(*s.speedLimit) || !s.hull ||
       !s.hull->minimum.isFinite() || !s.hull->maximum.isFinite() ||
       s.hull->minimum.x>=s.hull->maximum.x || s.hull->minimum.y>=s.hull->maximum.y ||
       s.hull->minimum.z>=s.hull->maximum.z || !plan.source.isValid() || !plan.target.isValid() ||
       plan.source==plan.target || !plan.takeoff.isFinite() || !plan.landing.isFinite())
        return fail(JumpProbeReason::InvalidInput);
    if(indexMap!=s.map) return fail(JumpProbeReason::StaleNavigation);
    if(!same(binding,physics.binding) || physics.tick!=s.tick) return fail(JumpProbeReason::StalePhysics);
    if(!constraints(model::NavTraversalKind::Jump,plan.sourceAttributes,plan.targetAttributes))
        return fail(JumpProbeReason::UnsupportedConstraints);
    for(double v : {physics.gravity,physics.verticalImpulse,motion.minimumSpeed,motion.maximumSpeed,
                   motion.takeoffRadius,motion.landingRadius,motion.maximumDistance,motion.maximumRise,
                   motion.supportTolerance,motion.facingDegrees,limits.maxSegmentSeconds,limits.maxChordRise})
        if(!positive(v)) return fail(JumpProbeReason::InvalidInput);
    if(motion.minimumSpeed>motion.maximumSpeed || motion.maximumSpeed>400 || motion.facingDegrees>45 ||
       !motion.airborneTimeoutUs || !std::isfinite(limits.navTolerance) || limits.navTolerance<0 ||
       !limits.maxQueries || limits.maxQueries>21 || !limits.maxSegments || limits.maxSegments>8 ||
       limits.maxQueries>motion.maxQueries) return fail(JumpProbeReason::InvalidInput);
    const double length=distance(plan.takeoff,plan.landing);
    if(length<=0 || length>motion.maximumDistance || plan.landing.z<plan.takeoff.z ||
       double(plan.landing.z)-plan.takeoff.z>motion.maximumRise) return fail(JumpProbeReason::InvalidInput);
    if(distance(*s.position,plan.takeoff)>motion.takeoffRadius ||
       std::abs(double(s.position->z)-plan.takeoff.z)>motion.supportTolerance)
        return fail(JumpProbeReason::OutsideTakeoff);
    const double ux=(double(plan.landing.x)-plan.takeoff.x)/length,uy=(double(plan.landing.y)-plan.takeoff.y)/length;
    const double speed=s.velocity->x*ux+s.velocity->y*uy, lateral=std::abs(s.velocity->x*uy-s.velocity->y*ux);
    const double yaw=std::atan2(uy,ux)*180/3.14159265358979323846;
    if(speed<motion.minimumSpeed || speed>motion.maximumSpeed || speed>*s.speedLimit ||
       lateral>motion.minimumSpeed*0.1 || std::abs(double(s.velocity->z))>0.01 ||
       std::abs(std::remainder(double(s.view->y)-yaw,360.0))>motion.facingDegrees)
        return fail(JumpProbeReason::InvalidVelocity);
    const runtime::QueryStamp stamp{s.agent,s.actor,s.map,s.tick,binding.routeGeneration,0};
    const auto fetch=[&](runtime::QueryKind kind,model::NavVector3 start,model::NavVector3 end)
        ->std::optional<runtime::WorldQueryResult> {
        if(!start.isFinite() || !end.isFinite()) { result.reason=JumpProbeReason::InvalidInput; return {}; }
        if(result.queries==limits.maxQueries) { result.reason=JumpProbeReason::BudgetExceeded; return {}; }
        runtime::QueryRequest q{stamp,kind,start,end,s.hull,limits.navTolerance}; q.stamp.ordinal=++result.queries;
        try {
            const auto r=port.query(q);
            if(!(r.stamp==q.stamp) || r.kind!=kind) result.reason=JumpProbeReason::StaleQuery;
            else if(r.error==runtime::QueryError::BudgetExceeded) result.reason=JumpProbeReason::BudgetExceeded;
            else if(r.error!=runtime::QueryError::None) result.reason=JumpProbeReason::QueryFailed;
            else return r;
        } catch(...) { result.reason=JumpProbeReason::QueryFailed; }
        return {};
    };
    const auto ground=[&](model::NavVector3 origin,model::NavAreaId area)->std::optional<GroundedTarget> {
        const auto r=fetch(runtime::QueryKind::GroundedArea,origin,origin); if(!r) return {};
        if(!r->ground || !r->ground->floor) { result.reason=JumpProbeReason::NoSupport; return {}; }
        const auto& floor=*r->ground->floor;
        const double normal=double(floor.normal.x)*floor.normal.x+double(floor.normal.y)*floor.normal.y+double(floor.normal.z)*floor.normal.z;
        if(!std::isfinite(floor.height) || !floor.normal.isFinite() || normal<0.99 || normal>1.01) {
            result.reason=JumpProbeReason::InvalidResult; return {};
        }
        if(!floor.supported || floor.normal.z<0.7f ||
           std::abs(double(origin.z)+s.hull->minimum.z-floor.height)>motion.supportTolerance) {
            result.reason=JumpProbeReason::NoSupport; return {};
        }
        const auto match=index.containing({origin.x,origin.y,floor.height},limits.navTolerance);
        if(r->ground->area!=area || !match || !*match.value || (**match.value).areaId!=area) {
            result.reason=JumpProbeReason::WrongArea; return {};
        }
        return GroundedTarget{origin,area,floor};
    };
    const auto sweep=[&](model::NavVector3 start,model::NavVector3 end) {
        const auto r=fetch(runtime::QueryKind::SweptHull,start,end); if(!r) return false;
        if(!r->hull || !std::isfinite(r->hull->fraction) || r->hull->fraction<0 || r->hull->fraction>1 ||
           !r->hull->end.isFinite() || !r->hull->normal.isFinite()) result.reason=JumpProbeReason::InvalidResult;
        else if(r->hull->startSolid || r->hull->fraction!=1) result.reason=JumpProbeReason::Blocked;
        else if(distance(r->hull->end,end)>0.001 || std::abs(double(r->hull->end.z)-end.z)>0.001)
            result.reason=JumpProbeReason::InvalidResult;
        else return true;
        return false;
    };
    const auto source=ground(*s.position,plan.source); if(!source) return fail(result.reason);
    const auto destination=ground(plan.landing,plan.target); if(!destination) return fail(result.reason);
    const double landingZ=double(destination->floor.height)-s.hull->minimum.z;
    const double rise=landingZ-s.position->z;
    const double discriminant=physics.verticalImpulse*physics.verticalImpulse-2*physics.gravity*rise;
    if(!representable(landingZ) || rise<0 || rise>motion.maximumRise || !std::isfinite(discriminant) || discriminant<=0)
        return fail(JumpProbeReason::CannotLand);
    const double time=(physics.verticalImpulse+std::sqrt(discriminant))/physics.gravity;
    const double x=s.position->x+double(s.velocity->x)*time,y=s.position->y+double(s.velocity->y)*time;
    if(!positive(time) || time>=double(motion.airborneTimeoutUs)/1000000 || !representable(x) || !representable(y))
        return fail(JumpProbeReason::CannotLand);
    const model::NavVector3 touchdown{static_cast<float>(x),static_cast<float>(y),static_cast<float>(landingZ)};
    if(distance(touchdown,plan.landing)>motion.landingRadius || distance(touchdown,*s.position)>motion.maximumDistance)
        return fail(JumpProbeReason::CannotLand);
    // A second support measurement checks where the observed velocity actually
    // lands. A different-height surface invalidates this trajectory, never retries.
    const auto landing=ground(touchdown,plan.target); if(!landing) return fail(result.reason);
    if(std::abs(double(landing->floor.height)-destination->floor.height)>0.001)
        return fail(JumpProbeReason::CannotLand);
    const double count=std::ceil(time/limits.maxSegmentSeconds);
    const auto remaining=limits.maxQueries-result.queries;
    if(!std::isfinite(count) || count<1 || count>limits.maxSegments ||
       remaining<2 || count>(remaining-2)/2) return fail(JumpProbeReason::BudgetExceeded);
    const auto segments=static_cast<std::uint32_t>(count);
    const double dt=time/segments;
    const double bulge=physics.gravity*dt*dt/8;
    const double hullHeight=double(s.hull->maximum.z)-s.hull->minimum.z;
    if(!positive(bulge) || bulge>limits.maxChordRise || bulge>hullHeight)
        return fail(JumpProbeReason::BudgetExceeded);
    if(!sweep(*s.position,*s.position) || !sweep(touchdown,touchdown)) return fail(result.reason);
    auto previous=*s.position;
    for(std::uint32_t i=1;i<=segments;++i) {
        const double t=time*i/segments;
        const double px=s.position->x+double(s.velocity->x)*t,py=s.position->y+double(s.velocity->y)*t;
        const double pz=s.position->z+physics.verticalImpulse*t-0.5*physics.gravity*t*t;
        if(!representable(px) || !representable(py) || !representable(pz)) return fail(JumpProbeReason::InvalidInput);
        const auto next=i==segments ? touchdown:model::NavVector3{static_cast<float>(px),static_cast<float>(py),static_cast<float>(pz)};
        // The parabola is above its chord by at most g*dt^2/8. Two vertically
        // overlapping swept boxes cover every intermediate vertical offset.
        // Native standing hulls are retained: no arbitrary expanded TraceHull.
        auto upperStart=previous,upperEnd=next;
        if(!representable(double(previous.z)+bulge) || !representable(double(next.z)+bulge))
            return fail(JumpProbeReason::InvalidInput);
        upperStart.z=std::nextafter(static_cast<float>(double(previous.z)+bulge),std::numeric_limits<float>::infinity());
        upperEnd.z=std::nextafter(static_cast<float>(double(next.z)+bulge),std::numeric_limits<float>::infinity());
        if(!upperStart.isFinite() || !upperEnd.isFinite() || double(upperStart.z)-previous.z>hullHeight ||
           double(upperEnd.z)-next.z>hullHeight) return fail(JumpProbeReason::InvalidInput);
        if(!sweep(previous,next) || !sweep(upperStart,upperEnd)) return fail(result.reason);
        previous=next; ++result.segments;
    }
    JumpInspection proof; proof.stamp=stamp; proof.step=binding.step; proof.queries=result.queries;
    proof.origin=*s.position; proof.hull=*s.hull; proof.velocity=*s.velocity; proof.support=source;
    proof.takeoff=plan.takeoff; proof.landing=plan.landing;
    proof.takeoffClear=proof.flightClear=proof.landingClear=true;
    result.inspection=proof; result.touchdown=touchdown; result.flightSeconds=time; return result;
}
}
