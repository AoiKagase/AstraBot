// SPDX-License-Identifier: MPL-2.0
#include "nav/local/ground_probe.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::nav::local {
namespace {
bool valid(runtime::HullDimensions h) noexcept {
    return h.minimum.isFinite() && h.maximum.isFinite() && h.minimum.x<h.maximum.x &&
        h.minimum.y<h.maximum.y && h.minimum.z<h.maximum.z;
}
bool representable(double value) noexcept {
    return std::isfinite(value) && value>=std::numeric_limits<float>::lowest() &&
        value<=(std::numeric_limits<float>::max)();
}
ProbeReason floorReason(const runtime::FloorObservation& f, double minimumNormal) noexcept {
    if(!std::isfinite(f.height) || !f.normal.isFinite()) return ProbeReason::InvalidResult;
    if(!f.supported || f.normal.z<minimumNormal) return ProbeReason::NoSupport;
    const double n=double(f.normal.x)*f.normal.x+double(f.normal.y)*f.normal.y+double(f.normal.z)*f.normal.z;
    return n>=0.99 && n<=1.01 ? ProbeReason::None:ProbeReason::InvalidResult;
}
}
ProbeResult GroundProbe::inspect(const runtime::MovementSnapshot& s, std::uint64_t generation,
    model::NavAreaId currentArea, float x, float y, const query::NavSpatialIndex& index, core::MapGeneration indexMap,
    runtime::IWorldQueries& port, GroundProbeLimits limits) noexcept {
    ProbeResult result;
    result.stamp={s.agent,s.actor,s.map,s.tick,generation,0};
    const auto fail=[&](ProbeReason reason) { result.reason=reason; result.target.reset(); return result; };
    if(!s.agent.isValid() || !s.actor.isValid() || !s.map.isValid() || !s.tick.isValid() || !generation ||
       !currentArea.isValid() || s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true ||
       s.alive!=true || s.joined!=true || !s.position || !s.position->isFinite() || !s.hull || !valid(*s.hull) ||
       !std::isfinite(x) || !std::isfinite(y)) return fail(ProbeReason::InvalidInput);
    if(indexMap!=s.map) return fail(ProbeReason::StaleNavigation);
    for(double v : {limits.maxDistance,limits.sampleSpacing,limits.maxStepUp,limits.maxDrop,limits.probeDepth,
                    limits.supportTolerance,limits.navTolerance,limits.minNormalZ})
        if(!std::isfinite(v) || v<0) return fail(ProbeReason::InvalidInput);
    if(limits.sampleSpacing==0 || limits.probeDepth<limits.maxDrop || limits.minNormalZ<=0 || limits.minNormalZ>1)
        return fail(ProbeReason::InvalidInput);
    if(s.grounded!=true) return fail(ProbeReason::NoSupport);
    const double dx=double(x)-s.position->x, dy=double(y)-s.position->y;
    const double distance=std::hypot(dx,dy);
    const double count=std::max(1.0,std::ceil(distance/limits.sampleSpacing));
    if(distance>limits.maxDistance || count>limits.maxSamples || limits.maxQueries<1 ||
       count>(limits.maxQueries-1)/2) return fail(ProbeReason::BudgetExceeded);
    const auto samples=static_cast<std::uint32_t>(count);
    const auto fetch=[&](runtime::QueryKind kind, model::NavVector3 start, model::NavVector3 end)
        ->std::optional<runtime::WorldQueryResult> {
        if(!start.isFinite() || !end.isFinite()) { result.reason=ProbeReason::InvalidInput; return {}; }
        if(result.queries==limits.maxQueries) { result.reason=ProbeReason::BudgetExceeded; return {}; }
        runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,generation,++result.queries},kind,start,end,s.hull};
        try {
            auto reply=port.query(q);
            if(!(reply.stamp==q.stamp) || reply.kind!=kind) result.reason=ProbeReason::StaleQuery;
            else if(reply.error==runtime::QueryError::BudgetExceeded) result.reason=ProbeReason::BudgetExceeded;
            else if(reply.error!=runtime::QueryError::None) result.reason=ProbeReason::QueryFailed;
            else return reply;
        } catch(...) { result.reason=ProbeReason::QueryFailed; }
        return {};
    };
    auto ground=fetch(runtime::QueryKind::GroundedArea,*s.position,*s.position);
    if(!ground) return result;
    if(!ground->ground || !ground->ground->floor) return fail(ProbeReason::NoSupport);
    auto floor=*ground->ground->floor;
    auto reason=floorReason(floor,limits.minNormalZ);
    if(reason!=ProbeReason::None) return fail(reason);
    const double feet=double(s.position->z)+s.hull->minimum.z;
    if(std::abs(double(floor.height)-feet)>limits.supportTolerance) return fail(ProbeReason::NoSupport);
    if(!ground->ground->area || *ground->ground->area!=currentArea) return fail(ProbeReason::WrongStartArea);
    auto match=index.containing({s.position->x,s.position->y,floor.height},limits.navTolerance);
    if(!match || !*match.value || (**match.value).areaId!=currentArea) return fail(ProbeReason::NoArea);
    auto position=*s.position;
    model::NavAreaId area=currentArea;
    for(std::uint32_t i=0;i<samples;++i) {
        const double f=double(i+1)/samples;
        const float tx=static_cast<float>(s.position->x+dx*f), ty=static_cast<float>(s.position->y+dy*f);
        const double top=floor.height+limits.maxStepUp, bottom=floor.height-limits.probeDepth;
        if(!representable(top) || !representable(bottom)) return fail(ProbeReason::InvalidInput);
        const model::NavVector3 start{tx,ty,static_cast<float>(top)}, end{tx,ty,static_cast<float>(bottom)};
        auto reply=fetch(runtime::QueryKind::Floor,start,end);
        if(!reply) return result;
        if(!reply->floor) return fail(ProbeReason::NoSupport);
        const auto next=*reply->floor;
        reason=floorReason(next,limits.minNormalZ);
        if(reason!=ProbeReason::None) return fail(reason);
        if(next.height>start.z || next.height<end.z) return fail(ProbeReason::InvalidResult);
        if(double(floor.height)-next.height>limits.maxDrop) return fail(ProbeReason::UnsafeDrop);
        match=index.containing({tx,ty,next.height},limits.navTolerance);
        if(!match || !*match.value) return fail(ProbeReason::NoArea);
        const double originZ=double(next.height)-s.hull->minimum.z;
        if(!representable(originZ)) return fail(ProbeReason::InvalidInput);
        const model::NavVector3 destination{tx,ty,static_cast<float>(originZ)};
        auto sweep=fetch(runtime::QueryKind::SweptHull,position,destination);
        if(!sweep) return result;
        if(!sweep->hull) return fail(ProbeReason::InvalidResult);
        const auto& hit=*sweep->hull;
        if(!std::isfinite(hit.fraction) || hit.fraction<0 || hit.fraction>1 || !hit.end.isFinite() || !hit.normal.isFinite())
            return fail(ProbeReason::InvalidResult);
        if(hit.startSolid || hit.fraction<1) return fail(ProbeReason::Blocked);
        if(std::abs(double(hit.end.x)-destination.x)>0.001 || std::abs(double(hit.end.y)-destination.y)>0.001 ||
           std::abs(double(hit.end.z)-destination.z)>0.001) return fail(ProbeReason::InvalidResult);
        position=destination; floor=next; area=(**match.value).areaId; ++result.samples;
    }
    result.target=GroundedTarget{position,area,floor}; return result;
}
}
