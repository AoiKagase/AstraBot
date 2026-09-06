// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/ladder_probe.hpp"
#include <cmath>
#include <cstring>
#include <limits>

namespace astrabot::adapter::cstrike {
namespace {
using V=nav::model::NavVector3;
V value(const Vector& v) noexcept { return {v.x,v.y,v.z}; }
bool closeValue(float a,float b) noexcept { return std::abs(double(a)-b)<=0.05; }
bool same(V a,V b) noexcept { return closeValue(a.x,b.x) && closeValue(a.y,b.y) && closeValue(a.z,b.z); }
class Probe {
public:
    LadderWorld world; core::MapGeneration map; const LadderCandidate& candidate;
    int maximum; std::uint32_t limit; LadderProbeResult result{};
    edict_t* entity{};
    int modelIndex{},modelName{};
    bool fail(LadderProbeReason reason) noexcept { result.reason=reason; return false; }
    bool fresh() noexcept {
        if(world.currentMap(world.context)!=map) return fail(LadderProbeReason::StaleMap);
        const auto slot=static_cast<std::uint32_t>(candidate.entityId);
        auto* e=world.engine->pfnPEntityOfEntIndex(static_cast<int>(slot));
        if(!e || e->free || (entity && e!=entity)) return fail(LadderProbeReason::StaleEntity);
        const auto serial=static_cast<std::uint32_t>(candidate.entityId>>32);
        const auto* name=world.engine->pfnSzFromIndex(e->v.classname);
        const auto* model=world.engine->pfnSzFromIndex(e->v.model);
        if(world.engine->pfnIndexOfEdict(e)!=static_cast<int>(slot) || !name || std::strcmp(name,"func_ladder") ||
           !model || model[0]!='*' || model[1]<'0' || model[1]>'9' ||
           e->v.modelindex<=0 || e->v.modelindex>=512 || e->v.skin!=CONTENTS_LADDER ||
           (entity && (e->v.modelindex!=modelIndex || e->v.model!=modelName)) ||
           static_cast<std::uint32_t>(e->serialnumber)!=serial || e->free ||
           value(e->v.absmin)!=candidate.minimum || value(e->v.absmax)!=candidate.maximum ||
           e->v.angles.x!=0 || e->v.angles.y!=0 || e->v.angles.z!=0 ||
           e->v.velocity.x!=0 || e->v.velocity.y!=0 || e->v.velocity.z!=0 ||
           e->v.avelocity.x!=0 || e->v.avelocity.y!=0 || e->v.avelocity.z!=0 ||
           world.currentMap(world.context)!=map) return fail(LadderProbeReason::StaleEntity);
        entity=e; modelIndex=e->v.modelindex; modelName=e->v.model; return true;
    }
    bool trace(V from,V to,bool model,TraceResult& hit,bool contact=false) noexcept {
        if(!fresh()) return false;
        if(result.queries>=limit) return fail(LadderProbeReason::BudgetExceeded);
        if(!from.isFinite() || !to.isFinite()) return fail(LadderProbeReason::InvalidInput);
        const float a[]{from.x,from.y,from.z},b[]{to.x,to.y,to.z};
        ++result.queries;
        // NaN sentinel distinguishes a missing/incomplete callback result from clear.
        hit.flFraction=(std::numeric_limits<float>::quiet_NaN)();
        if(model) world.engine->pfnTraceModel(a,b,contact ? 1:0,entity,&hit);
        else world.engine->pfnTraceHull(a,b,0,1,nullptr,&hit);
        if(!fresh()) return false;
        const float f=hit.flFraction;
        if(!std::isfinite(f) || f<0 || f>1 || !value(hit.vecEndPos).isFinite() || !value(hit.vecPlaneNormal).isFinite() ||
           !same(value(hit.vecEndPos),{from.x+(to.x-from.x)*f,from.y+(to.y-from.y)*f,from.z+(to.z-from.z)*f}))
            return fail(LadderProbeReason::InvalidTrace);
        if(!contact && (hit.fStartSolid || hit.fAllSolid)) return fail(LadderProbeReason::Blocked);
        return true;
    }
    bool contact(V at) noexcept {
        TraceResult t{};
        return trace(at,at,true,t,true) && ((t.pHit==entity && (t.fStartSolid || t.fAllSolid)) || fail(LadderProbeReason::NoFace));
    }
    bool clear(V from,V to) noexcept {
        TraceResult t{}; return trace(from,to,false,t) && (t.flFraction==1 || fail(LadderProbeReason::Blocked));
    }
    bool endpoint(V point,float bottom,float top,const nav::query::NavSpatialIndex& index,LadderEndpoint& out) noexcept {
        TraceResult t{};
        if(!trace({point.x,point.y,top+36},{point.x,point.y,bottom+36},false,t)) return false;
        if(t.flFraction==1 || t.vecPlaneNormal.z<0.7f) return fail(LadderProbeReason::NoSupport);
        // Discovery links must not use a player or movable entity as a floor.
        // World BSP is the initial supported floor profile; dynamic supports
        // need a separate lifetime contract before they can enrich a route.
        auto* support=world.engine->pfnPEntityOfEntIndex(0);
        if(!support || support!=t.pHit || support->free) return fail(LadderProbeReason::NoSupport);
        if(!fresh()) return false;
        const auto n=value(t.vecPlaneNormal);
        if(std::abs(double(n.x)*n.x+double(n.y)*n.y+double(n.z)*n.z-1)>0.02) return fail(LadderProbeReason::InvalidTrace);
        out.origin=value(t.vecEndPos);
        auto area=index.containing({out.origin.x,out.origin.y,out.origin.z-36},2);
        if(!area || !area.value->has_value()) return fail(LadderProbeReason::NoArea);
        out.area=(**area.value).areaId;
        // A small lift avoids a contact-plane startSolid ambiguity while retaining
        // the measured floor origin in the packet.
        auto raised=out.origin; raised.z+=0.05f;
        return clear(raised,raised);
    }
};
}
LadderProbeResult inspectLadderPassage(LadderWorld world,core::MapGeneration map,const LadderCandidate& c,
    LadderFace face,LadderExit exit,const nav::query::NavSpatialIndex& index,core::MapGeneration indexMap,
    int maximum,std::uint32_t maxQueries) noexcept {
    LadderProbeResult invalid;
    const auto slot=static_cast<std::uint32_t>(c.entityId);
    if(!map.isValid() || maximum<1 || maximum>8192 || !slot || slot>=static_cast<std::uint32_t>(maximum) ||
       maxQueries>12 || !c.minimum.isFinite() || !c.maximum.isFinite() ||
       c.minimum.x>=c.maximum.x || c.minimum.y>=c.maximum.y || double(c.maximum.z)-c.minimum.z<72 ||
       (face!=LadderFace::MinX && face!=LadderFace::MaxX && face!=LadderFace::MinY && face!=LadderFace::MaxY) ||
       (exit!=LadderExit::SameFace && exit!=LadderExit::AcrossTop)) return invalid;
    if(map!=indexMap) { invalid.reason=LadderProbeReason::StaleMap; return invalid; }
    if(!world.engine || !world.currentMap || !world.engine->pfnTraceModel || !world.engine->pfnTraceHull ||
       !world.engine->pfnPEntityOfEntIndex || !world.engine->pfnIndexOfEdict || !world.engine->pfnSzFromIndex) {
        invalid.reason=LadderProbeReason::Unavailable; return invalid;
    }
    Probe probe{world,map,c,maximum,maxQueries};
    LadderPassage p; p.map=map; p.entityId=c.entityId; p.face=face; p.exit=exit;
    p.normal=face==LadderFace::MinX ? V{-1,0,0}:face==LadderFace::MaxX ? V{1,0,0}:
        face==LadderFace::MinY ? V{0,-1,0}:V{0,1,0};
    V center{static_cast<float>((double(c.minimum.x)+c.maximum.x)/2),
        static_cast<float>((double(c.minimum.y)+c.maximum.y)/2),0};
    V edge=center;
    if(p.normal.x) edge.x=p.normal.x<0 ? c.minimum.x:c.maximum.x;
    else edge.y=p.normal.y<0 ? c.minimum.y:c.maximum.y;
    for(int sample=0;sample<3;++sample) {
        const float z=sample==0 ? c.minimum.z+18:sample==2 ? c.maximum.z-18:
            static_cast<float>((double(c.minimum.z)+c.maximum.z)/2);
        TraceResult t{};
        if(!probe.trace({edge.x+p.normal.x*33,edge.y+p.normal.y*33,z},{center.x,center.y,z},true,t)) return probe.result;
        const auto n=value(t.vecPlaneNormal);
        if(t.flFraction==1 || t.pHit!=probe.entity || std::abs(n.x-p.normal.x)>0.001f ||
           std::abs(n.y-p.normal.y)>0.001f || std::abs(n.z-p.normal.z)>0.001f) {
            probe.fail(LadderProbeReason::NoFace); return probe.result;
        }
        const V hit=value(t.vecEndPos);
        if(sample==0) p.lowContact=hit;
        else if(!closeValue(hit.x,p.lowContact.x) || !closeValue(hit.y,p.lowContact.y)) {
            probe.fail(LadderProbeReason::NoFace); return probe.result;
        }
        if(sample==2) p.highContact=hit;
    }
    const V bottom{p.lowContact.x+p.normal.x*33,p.lowContact.y+p.normal.y*33,0};
    const float sign=exit==LadderExit::SameFace ? 1.0f:-1.0f;
    const V top{p.highContact.x+p.normal.x*33*sign,p.highContact.y+p.normal.y*33*sign,0};
    if(!probe.endpoint(bottom,c.minimum.z-64,c.minimum.z+18,index,p.bottom) ||
       !probe.endpoint(top,c.maximum.z-18,c.maximum.z+18,index,p.top)) return probe.result;
    if(p.top.origin.z<=p.bottom.origin.z || p.top.area==p.bottom.area) {
        probe.fail(LadderProbeReason::NoArea); return probe.result;
    }
    p.mount={p.lowContact.x+p.normal.x*15,p.lowContact.y+p.normal.y*15,p.bottom.origin.z+0.05f};
    p.dismount={p.mount.x,p.mount.y,p.top.origin.z+0.05f};
    auto from=p.bottom.origin,to=p.top.origin; from.z+=0.05f; to.z+=0.05f;
    if(!probe.contact(p.mount) || !probe.contact({p.dismount.x,p.dismount.y,p.dismount.z-18})) return probe.result;
    if(!probe.clear(from,p.mount) || !probe.clear(p.mount,p.dismount) || !probe.clear(p.dismount,to)) return probe.result;
    probe.result.reason=LadderProbeReason::None; probe.result.passage=p; return probe.result;
}
}
