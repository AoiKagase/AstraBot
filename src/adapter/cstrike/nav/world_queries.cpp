// SPDX-License-Identifier: MPL-2.0
#include <cmath>
#include <limits>
#include "adapter/cstrike/nav/world_queries.hpp"

namespace astrabot::adapter::cstrike {
namespace {
nav::model::NavVector3 value(const Vector& v) noexcept { return {v.x,v.y,v.z}; }
bool valid(const TraceResult& t) noexcept {
    return std::isfinite(t.flFraction) && t.flFraction>=0 && t.flFraction<=1 &&
        value(t.vecEndPos).isFinite() && value(t.vecPlaneNormal).isFinite();
}
int hullIndex(const nav::runtime::HullDimensions& h) noexcept {
    if(h.minimum==nav::model::NavVector3{-16,-16,-36} && h.maximum==nav::model::NavVector3{16,16,36}) return 1;
    if(h.minimum==nav::model::NavVector3{-16,-16,-18} && h.maximum==nav::model::NavVector3{16,16,18}) return 3;
    return -1; // TraceHull cannot represent arbitrary hull dimensions.
}
}
nav::runtime::WorldQueryResult queryNavWorld(enginefuncs_t* engine, edict_t* entity,
    const nav::query::NavSpatialIndex* index, const nav::runtime::QueryRequest& q) noexcept {
    using namespace nav::runtime;
    WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind;
    if(!engine || !entity || entity->free) return r;
    if(!q.start.isFinite() || !q.end.isFinite()) { r.error=QueryError::InvalidResult; return r; }
    if(q.kind==QueryKind::SweptHull || q.kind==QueryKind::Clearance) {
        if(!engine->pfnTraceHull || !q.hull) return r;
        const int hull=hullIndex(*q.hull); if(hull<0) return r;
        const float start[]{q.start.x,q.start.y,q.start.z}, end[]{q.end.x,q.end.y,q.end.z};
        TraceResult hit{};
        // Include actors/dynamic blockers; ignore only the validated querying actor.
        engine->pfnTraceHull(start,end,0,hull,entity,&hit);
        if(!valid(hit)) { r.error=QueryError::InvalidResult; return r; }
        r.error=QueryError::None;
        const bool solid=hit.fStartSolid || hit.fAllSolid;
        if(q.kind==QueryKind::SweptHull) r.hull=HullObservation{hit.flFraction,value(hit.vecEndPos),value(hit.vecPlaneNormal),solid};
        else r.clearance=ClearanceObservation{!solid && hit.flFraction==1};
        return r;
    }
    if(q.kind!=QueryKind::GroundedArea && q.kind!=QueryKind::Floor) return r;
    if(!engine->pfnTraceHull || !q.hull || hullIndex(*q.hull)<0) return r;
    if(!std::isfinite(q.navTolerance) || q.navTolerance<0) { r.error=QueryError::InvalidResult; return r; }
    auto start=q.start, end=q.end;
    float feet=0;
    if(q.kind==QueryKind::GroundedArea) {
        if(!index) return r;
        feet=q.start.z+q.hull->minimum.z;
        start.z=q.start.z+2; end={q.start.x,q.start.y,q.start.z-4};
    } else {
        // Floor requests use feet heights; TraceHull consumes actor origins.
        const double top=double(start.z)-q.hull->minimum.z, bottom=double(end.z)-q.hull->minimum.z;
        if(std::abs(top)>(std::numeric_limits<float>::max)() || std::abs(bottom)>(std::numeric_limits<float>::max)()) {
            r.error=QueryError::InvalidResult; return r;
        }
        start.z=static_cast<float>(top); end.z=static_cast<float>(bottom);
    }
    if(!start.isFinite() || !end.isFinite() || start.x!=end.x || start.y!=end.y || start.z<=end.z) {
        r.error=QueryError::InvalidResult; return r;
    }
    const float a[]{start.x,start.y,start.z}, b[]{end.x,end.y,end.z};
    // Static hull support includes the footprint on a stair tread. Player
    // bodies are not floor proof. Each request issues exactly one engine trace.
    TraceResult hit{}; engine->pfnTraceHull(a,b,1,hullIndex(*q.hull),entity,&hit);
    if(!valid(hit)) { r.error=QueryError::InvalidResult; return r; }
    r.error=QueryError::None;
    if(hit.fAllSolid || hit.fStartSolid || hit.flFraction==1) return r;
    if(std::abs(hit.vecEndPos.x-start.x)>0.001f || std::abs(hit.vecEndPos.y-start.y)>0.001f ||
       hit.vecEndPos.z>start.z || hit.vecEndPos.z<end.z) { r.error=QueryError::InvalidResult; return r; }
    const FloorObservation floor{hit.vecEndPos.z+q.hull->minimum.z,value(hit.vecPlaneNormal),true};
    if(q.kind==QueryKind::Floor) { r.floor=floor; return r; }
    if(floor.normal.z<0.7f || std::abs(floor.height-feet)>4) return r;
    const auto match=index->containing({q.start.x,q.start.y,floor.height},q.navTolerance);
    if(!match || !*match.value) return r;
    r.ground=GroundedAreaObservation{(**match.value).areaId,floor}; return r;
}
}
