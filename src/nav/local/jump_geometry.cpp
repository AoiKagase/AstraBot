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
}
JumpGeometryResult JumpGeometry::derive(const corridor::Corridor& path,Binding binding,
    const runtime::MovementSnapshot& s,JumpLimits motion,JumpGeometryLimits limits) noexcept {
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
       s.grounded!=true || s.ducked!=false) return fail(JumpGeometryReason::InvalidActor);
    if(binding.step>=path.transitions().size()) return fail(JumpGeometryReason::InvalidStep);
    const auto& t=path.transitions()[binding.step];
    const auto hints=constraints(t.edge.traversal,t.sourceAttributes,t.targetAttributes);
    if(!hints || hints.kind!=model::NavTraversalKind::Jump || t.edge.external || t.edge.direction>3)
        return fail(JumpGeometryReason::UnsupportedTransition);
    if(!query::containsXY(t.sourceExtent,*s.position)) return fail(JumpGeometryReason::InvalidActor);
    const auto source=region(t.sourceExtent,*s.hull,motion.takeoffRadius,limits.clearanceMargin);
    const auto target=region(t.targetExtent,*s.hull,motion.landingRadius,limits.clearanceMargin);
    if(!fits(source) || !fits(target)) return fail(JumpGeometryReason::NoRoom);
    const bool vertical=t.edge.direction==1 || t.edge.direction==3;
    const bool forward=t.edge.direction==1 || t.edge.direction==2;
    const double low=vertical ? (std::max)(source.lowY,target.lowY):(std::max)(source.lowX,target.lowX);
    const double high=vertical ? (std::min)(source.highY,target.highY):(std::min)(source.highX,target.highX);
    if(low>high) return fail(JumpGeometryReason::NoRoom);
    const double tangent=std::clamp(vertical ? double(s.position->y):double(s.position->x),low,high);
    const double boundary=vertical ? t.sourceLow.x:t.sourceLow.y;
    const double offset=limits.preferredDistance/2*(forward ? 1:-1);
    const double from=std::clamp(boundary-offset,vertical ? source.lowX:source.lowY,vertical ? source.highX:source.highY);
    const double to=std::clamp(boundary+offset,vertical ? target.lowX:target.lowY,vertical ? target.highX:target.highY);
    const auto a=point(t.sourceExtent,vertical ? from:tangent,vertical ? tangent:from,s.hull->minimum.z);
    const auto b=point(t.targetExtent,vertical ? to:tangent,vertical ? tangent:to,s.hull->minimum.z);
    if(!a || !b || !inside(source,*a) || !inside(target,*b)) return fail(JumpGeometryReason::InvalidGeometry);
    const double length=std::hypot(double(b->x)-a->x,double(b->y)-a->y);
    if(length<=0 || length>motion.maximumDistance) return fail(JumpGeometryReason::NoRoom);
    const double rise=double(b->z)-a->z;
    if(rise<0 || rise>motion.maximumRise) return fail(JumpGeometryReason::HeightUnsupported);
    return {JumpGeometryReason::None,JumpPlan{t.edge.source,t.edge.target,*a,*b,t.sourceAttributes,t.targetAttributes}};
}
}
