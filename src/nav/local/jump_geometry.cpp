// SPDX-License-Identifier: MPL-2.0
#include "nav/local/jump_geometry.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::nav::local {
namespace {
struct Region { double lowX{},highX{},lowY{},highY{}; };
bool positive(double n) noexcept { return std::isfinite(n) && n>0; }
Region region(const model::NavExtent& e,runtime::HullDimensions h,double radius,double margin) noexcept {
    return {double(e.northWest.x)-h.minimum.x+radius+margin,double(e.southEast.x)-h.maximum.x-radius-margin,
        double(e.northWest.y)-h.minimum.y+radius+margin,double(e.southEast.y)-h.maximum.y-radius-margin};
}
bool fits(Region r) noexcept { return r.lowX<=r.highX && r.lowY<=r.highY; }
bool inside(Region r,model::NavVector3 p) noexcept {
    return p.x>=r.lowX && p.x<=r.highX && p.y>=r.lowY && p.y<=r.highY;
}
std::optional<model::NavVector3> point(const model::NavExtent& e,double x,double y,float feetOffset) noexcept {
    if(!std::isfinite(x) || !std::isfinite(y) || std::abs(x)>(std::numeric_limits<float>::max)() ||
       std::abs(y)>(std::numeric_limits<float>::max)()) return {};
    model::NavVector3 p{static_cast<float>(x),static_cast<float>(y),0};
    const double z=query::projectToArea(e,p).z-feetOffset;
    if(!std::isfinite(z) || std::abs(z)>(std::numeric_limits<float>::max)()) return {};
    p.z=static_cast<float>(z); return p;
}
JumpPlan plan(model::NavAreaId source,model::NavAreaId target,model::NavVector3 takeoff,
              model::NavVector3 landing,std::uint8_t sourceAttributes,std::uint8_t targetAttributes,
              std::optional<runtime::HullDimensions> flightHull,model::NavAreaId landingArea,
              std::uint8_t landingAdvance) noexcept {
    JumpPlan result{source,target,takeoff,landing,sourceAttributes,targetAttributes,flightHull};
    result.landingArea=landingArea; result.landingAdvance=landingAdvance;
    return result;
}
}
JumpGeometryResult JumpGeometry::derive(const corridor::Corridor& path,Binding binding,
    const runtime::MovementSnapshot& s,JumpLimits motion,JumpGeometryLimits limits,bool observedObstacle) noexcept {
    const auto fail=[](JumpGeometryReason reason) { return JumpGeometryResult{reason,{}}; };
    if(!binding.agent.isValid() || !binding.actor.isValid() || !binding.map.isValid() || !binding.routeGeneration ||
       !s.tick.isValid() || !s.position || !s.position->isFinite() || !s.hull ||
       !s.hull->minimum.isFinite() || !s.hull->maximum.isFinite() ||
       s.hull->minimum.x>=s.hull->maximum.x || s.hull->minimum.y>=s.hull->maximum.y ||
       s.hull->minimum.z>=s.hull->maximum.z || !positive(motion.takeoffRadius) || !positive(motion.landingRadius) ||
       !positive(motion.maximumDistance) || !positive(motion.maximumRise) ||
       !positive(limits.preferredDistance) || limits.preferredDistance>motion.maximumDistance ||
       !positive(limits.clearanceMargin)) return fail(JumpGeometryReason::InvalidInput);
    if(s.agent!=binding.agent || s.actor!=binding.actor || s.map!=binding.map ||
       s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true || s.alive!=true || s.joined!=true ||
       s.grounded!=true || !s.ducked) return fail(JumpGeometryReason::InvalidActor);
    if(binding.step>=path.transitions().size()) return fail(JumpGeometryReason::InvalidStep);
    const auto& t=path.transitions()[binding.step];
    const auto jumpEdge=t.effectiveTraversal==model::NavTraversalKind::Jump || observedObstacle;
    const auto hints=constraints(jumpEdge ? model::NavTraversalKind::Jump:t.edge.traversal,
        t.sourceAttributes,t.targetAttributes);
    if(!hints || hints.kind!=model::NavTraversalKind::Jump || t.edge.direction>3)
        return fail(JumpGeometryReason::UnsupportedTransition);
    if(t.edge.external) {
        const auto a=t.sourceLow, b=t.targetLow;
        if(!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z) ||
           !std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.z) ||
           !query::containsXY(t.sourceExtent,*s.position))
            return fail(JumpGeometryReason::InvalidGeometry);
        const double length=std::hypot(b.x-a.x,b.y-a.y);
        const double sourceOffset=motion.standingHull ? motion.standingHull->minimum.z-s.hull->minimum.z:0;
        const double targetOffset=motion.standingHull && motion.flightHull ?
            motion.standingHull->minimum.z-motion.flightHull->minimum.z:sourceOffset;
        const double rise=(b.z+targetOffset)-(a.z+sourceOffset);
        if(length<=0 || length>motion.maximumDistance || (rise<0 && !motion.flightHull) || rise>motion.maximumRise)
            return fail((rise<0 && !motion.flightHull) || rise>motion.maximumRise ?
                        JumpGeometryReason::HeightUnsupported:JumpGeometryReason::NoRoom);
        const model::NavVector3 takeoff{static_cast<float>(a.x),static_cast<float>(a.y),static_cast<float>(a.z+sourceOffset)};
        const model::NavVector3 landing{static_cast<float>(b.x),static_cast<float>(b.y),static_cast<float>(b.z+targetOffset)};
        JumpLandingRegion target{t.edge.target,t.targetExtent,double(b.x),double(b.x),double(b.y),double(b.y),1,
            JumpLandingRegionKind::HullMargin};
        return {JumpGeometryReason::None,
                plan(t.edge.source,t.edge.target,takeoff,landing,t.sourceAttributes,t.targetAttributes,
                     motion.flightHull,t.edge.target,1),JumpLandingEnvelope{target,{}}};
    }
    if(!query::containsXY(t.sourceExtent,*s.position)) return fail(JumpGeometryReason::InvalidActor);
    // A micro NAV patch is not a physical enclosure. Keep its landing centre
    // inside the patch; JumpProbe still proves the complete actor hull/flight.
    const auto candidateRegion=[&](const model::NavExtent& e,corridor::AreaFit fit,double radius,bool duck) {
        if(fit==corridor::AreaFit::HullSafe && !duck)
            return region(e,*s.hull,radius,limits.clearanceMargin);
        const double inset=(std::min)({limits.clearanceMargin,
            (double(e.southEast.x)-e.northWest.x)/4,(double(e.southEast.y)-e.northWest.y)/4});
        return Region{e.northWest.x+inset,e.southEast.x-inset,e.northWest.y+inset,e.southEast.y-inset};
    };
    const auto source=candidateRegion(t.sourceExtent,t.sourceFit,motion.takeoffRadius,hints.sourceDuck);
    const auto targetBounds=candidateRegion(t.targetExtent,t.targetFit,motion.landingRadius,hints.targetDuck);
    if(!fits(source) || !fits(targetBounds)) return fail(JumpGeometryReason::NoRoom);
    JumpLandingRegion target{t.edge.target,t.targetExtent,targetBounds.lowX,targetBounds.highX,
        targetBounds.lowY,targetBounds.highY,1,t.targetFit==corridor::AreaFit::HullSafe && !hints.targetDuck ?
            JumpLandingRegionKind::HullMargin:JumpLandingRegionKind::CentreInset};
    JumpLandingEnvelope envelope{target,{}};
    // Only the direct, ordinary continuation may extend a micro landing
    // envelope.  Its world clearance is still checked by JumpProbe.
    if(binding.step+1<path.transitions().size()) {
        const auto& next=path.transitions()[binding.step+1];
        const auto nextHints=constraints(next.effectiveTraversal,next.sourceAttributes,next.targetAttributes);
        const bool ordinary=!next.edge.external && next.edge.source==t.edge.target && nextHints &&
            (next.effectiveTraversal==model::NavTraversalKind::Walk ||
             (next.effectiveTraversal==model::NavTraversalKind::Crouch && motion.flightHull));
        if(ordinary) {
            const auto nextBounds=candidateRegion(next.targetExtent,next.targetFit,motion.landingRadius,
                nextHints.targetDuck);
            if(fits(nextBounds)) envelope.successor=JumpLandingRegion{next.edge.target,next.targetExtent,
                nextBounds.lowX,nextBounds.highX,nextBounds.lowY,nextBounds.highY,2,
                JumpLandingRegionKind::SuccessorCentreInset};
        }
    }
    const bool vertical=t.edge.direction==1 || t.edge.direction==3;
    const bool forward=t.edge.direction==1 || t.edge.direction==2;
    const double low=vertical ? (std::max)(source.lowY,targetBounds.lowY):(std::max)(source.lowX,targetBounds.lowX);
    const double high=vertical ? (std::min)(source.highY,targetBounds.highY):(std::min)(source.highX,targetBounds.highX);
    if(low>high) return fail(JumpGeometryReason::NoRoom);
    const double tangent=std::clamp(vertical ? double(s.position->y):double(s.position->x),low,high);
    const double boundary=vertical ? t.sourceLow.x:t.sourceLow.y;
    const double offset=limits.preferredDistance/2*(forward ? 1:-1);
    const double from=std::clamp(boundary-offset,vertical ? source.lowX:source.lowY,vertical ? source.highX:source.highY);
    const double to=std::clamp(boundary+offset,vertical ? targetBounds.lowX:targetBounds.lowY,
        vertical ? targetBounds.highX:targetBounds.highY);
    const auto a=point(t.sourceExtent,vertical ? from:tangent,vertical ? tangent:from,s.hull->minimum.z);
    const auto b=point(t.targetExtent,vertical ? to:tangent,vertical ? tangent:to,
        motion.flightHull ? motion.flightHull->minimum.z:s.hull->minimum.z);
    if(!a || !b || !inside(source,*a) || !inside(targetBounds,*b)) return fail(JumpGeometryReason::InvalidGeometry);
    const double length=std::hypot(double(b->x)-a->x,double(b->y)-a->y);
    if(length<=0 || length>motion.maximumDistance) return fail(JumpGeometryReason::NoRoom);
    const double rise=double(b->z)-a->z;
    if((rise<0 && !motion.flightHull) || rise>motion.maximumRise) return fail(JumpGeometryReason::HeightUnsupported);
    return {JumpGeometryReason::None,plan(t.edge.source,t.edge.target,*a,*b,t.sourceAttributes,t.targetAttributes,
        motion.flightHull,t.edge.target,1),envelope};
}
}
