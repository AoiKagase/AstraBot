// SPDX-License-Identifier: MPL-2.0
#include <cmath>
#include <limits>
#include <cstring>
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
std::uint64_t doorIdentity(enginefuncs_t* e,edict_t* door,int maximum) noexcept {
    if(!door || door->free || !e->pfnIndexOfEdict || !e->pfnSzFromIndex) return 0;
    const int id=e->pfnIndexOfEdict(door);
    if(id<=0 || id>=maximum) return 0;
    const auto* name=e->pfnSzFromIndex(door->v.classname);
    if(!name || (std::strcmp(name,"func_door") && std::strcmp(name,"func_door_rotating"))) return 0;
    return (std::uint64_t(static_cast<std::uint32_t>(door->serialnumber))<<32)|static_cast<std::uint32_t>(id);
}
edict_t* findDoor(enginefuncs_t* e,std::uint64_t id,int maximum) noexcept {
    if(!e || !e->pfnPEntityOfEntIndex) return nullptr;
    const auto slot=static_cast<std::uint32_t>(id);
    if(maximum<=0 || slot==0 || slot>=static_cast<std::uint32_t>(maximum)) return nullptr;
    auto* door=e->pfnPEntityOfEntIndex(static_cast<int>(slot));
    return doorIdentity(e,door,maximum)==id ? door:nullptr;
}
}
std::optional<nav::model::NavVector3> doorUseView(enginefuncs_t* e,edict_t* actor,
    std::uint64_t id,int maximum) noexcept {
    if(!e || !actor || actor->free || !e->pfnFindEntityInSphere) return {};
    auto* door=findDoor(e,id,maximum);
    // ReGameDLL CBaseDoor::ObjectCaps: only the USE_ONLY flag grants impulse Use.
    if(!door || !(door->v.spawnflags&(1<<8)) ||
       !value(actor->v.origin).isFinite() || !value(actor->v.view_ofs).isFinite() ||
       !value(door->v.absmin).isFinite() || !value(door->v.size).isFinite()) return {};
    bool found=false; edict_t* previous=nullptr;
    const float origin[]{actor->v.origin.x,actor->v.origin.y,actor->v.origin.z};
    for(unsigned n=0;n<=32;++n) {
        auto* candidate=e->pfnFindEntityInSphere(previous,origin,64);
        if(!candidate) {
            if(!found || actor->free || findDoor(e,id,maximum)!=door || !(door->v.spawnflags&(1<<8))) return {};
            const auto& lo=door->v.absmin; const auto& size=door->v.size;
            if(size.x<0 || size.y<0 || size.z<0) return {};
            // ReGameDLL VecBModelOrigin uses absmin + size/2 (not absmax).
            const double dx=double(lo.x)+double(size.x)/2-actor->v.origin.x-actor->v.view_ofs.x;
            const double dy=double(lo.y)+double(size.y)/2-actor->v.origin.y-actor->v.view_ofs.y;
            const double dz=double(lo.z)+double(size.z)/2-actor->v.origin.z-actor->v.view_ofs.z;
            const double xy=std::hypot(dx,dy);
            if(std::hypot(xy,dz)<0.001) return {};
            constexpr double degrees=180/3.14159265358979323846;
            const double pitch=-std::atan2(dz,xy)*degrees, yaw=std::atan2(dy,dx)*degrees;
            if(!std::isfinite(pitch) || !std::isfinite(yaw) || pitch< -89 || pitch>89) return {};
            return nav::model::NavVector3{static_cast<float>(pitch),static_cast<float>(yaw),0};
        }
        if(n==32 || candidate==previous || candidate->free) return {};
        // Private ObjectCaps cannot be safely inspected. Any other entity is an
        // unknown competitor, including buttons/hostages/other doors/players.
        if(candidate!=actor && candidate!=door) return {};
        if(candidate==door) { if(found) return {}; found=true; }
        previous=candidate;
    }
    return {};
}
nav::runtime::WorldQueryResult queryNavWorld(enginefuncs_t* engine, edict_t* entity,
    const nav::query::NavSpatialIndex* index, const nav::runtime::QueryRequest& q,int maxEntities) noexcept {
    using namespace nav::runtime;
    WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind;
    if(!engine || !entity || entity->free) return r;
    if(!q.start.isFinite() || !q.end.isFinite()) { r.error=QueryError::InvalidResult; return r; }
    if(q.kind==QueryKind::SweptHull || q.kind==QueryKind::Clearance || q.kind==QueryKind::Door) {
        if(!engine->pfnTraceHull || !q.hull) return r;
        const int hull=hullIndex(*q.hull); if(hull<0) return r;
        const float start[]{q.start.x,q.start.y,q.start.z}, end[]{q.end.x,q.end.y,q.end.z};
        TraceResult hit{};
        // Include actors/dynamic blockers; ignore only the validated querying actor.
        engine->pfnTraceHull(start,end,0,hull,entity,&hit);
        if(!valid(hit)) { r.error=QueryError::InvalidResult; return r; }
        r.error=QueryError::None;
        const bool solid=hit.fStartSolid || hit.fAllSolid;
        if(q.kind==QueryKind::Door) {
            if(solid) return r;
            edict_t* door=q.doorId ? findDoor(engine,q.doorId,maxEntities):hit.pHit;
            const auto id=doorIdentity(engine,door,maxEntities);
            if(!id || (q.doorId && id!=q.doorId)) return r;
            if(hit.flFraction==1) {
                if(!q.doorId || std::abs(double(hit.vecEndPos.x)-q.end.x)>0.001 ||
                   std::abs(double(hit.vecEndPos.y)-q.end.y)>0.001 ||
                   std::abs(double(hit.vecEndPos.z)-q.end.z)>0.001) return r;
                r.door=DoorObservation{id,true,false,{}}; return r;
            }
            if(hit.pHit!=door) return r;
            const auto view=doorUseView(engine,entity,id,maxEntities);
            const bool touch=door->v.solid==SOLID_BSP && door->v.targetname==0 &&
                !(static_cast<unsigned>(door->v.spawnflags)&((1U<<8)|(1U<<31)));
            r.door=DoorObservation{id,false,view.has_value(),view,touch};
            r.hull=HullObservation{hit.flFraction,value(hit.vecEndPos),value(hit.vecPlaneNormal),false};
            return r;
        }
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
