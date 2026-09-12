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
    switch(f.status) {
    case runtime::FloorObservationStatus::TraceNoHit: return ProbeReason::TraceNoHit;
    case runtime::FloorObservationStatus::StartSolid: return ProbeReason::StartSolid;
    case runtime::FloorObservationStatus::AllSolid: return ProbeReason::AllSolid;
    case runtime::FloorObservationStatus::InvalidTrace: return ProbeReason::InvalidResult;
    case runtime::FloorObservationStatus::UnsupportedNormal: return ProbeReason::UnsupportedFloor;
    case runtime::FloorObservationStatus::HeightMismatch: return ProbeReason::FloorHeightMismatch;
    case runtime::FloorObservationStatus::NavContainmentMissing: return ProbeReason::NavContainmentMissing;
    case runtime::FloorObservationStatus::Unknown:
    case runtime::FloorObservationStatus::Supported: break;
    }
    if(!std::isfinite(f.height) || !f.normal.isFinite()) return ProbeReason::InvalidResult;
    if(!f.supported) return ProbeReason::ActorNotGrounded;
    if(f.normal.z<minimumNormal) return ProbeReason::UnsupportedFloor;
    const double n=double(f.normal.x)*f.normal.x+double(f.normal.y)*f.normal.y+double(f.normal.z)*f.normal.z;
    return n>=0.99 && n<=1.01 ? ProbeReason::None:ProbeReason::InvalidResult;
}
ProbeResult probe(const runtime::MovementSnapshot& s, std::uint64_t generation,
    model::NavAreaId currentArea, float x, float y, const query::NavSpatialIndex& index, core::MapGeneration indexMap,
    runtime::IWorldQueries& port, GroundProbeLimits limits, bool locateOnly) noexcept {
    ProbeResult result;
    result.stamp={s.agent,s.actor,s.map,s.tick,generation,0};
    const auto fail=[&](ProbeReason reason) { result.reason=reason; result.target.reset(); return result; };
    if(!s.agent.isValid() || !s.actor.isValid() || !s.map.isValid() || !s.tick.isValid() || !generation ||
       (!locateOnly && !currentArea.isValid()) || s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true ||
       s.alive!=true || s.joined!=true || !s.position || !s.position->isFinite() || !s.hull || !valid(*s.hull) ||
       !std::isfinite(x) || !std::isfinite(y)) return fail(ProbeReason::InvalidInput);
    if(indexMap!=s.map) return fail(ProbeReason::StaleNavigation);
    for(double v : {limits.maxDistance,limits.sampleSpacing,limits.maxStepUp,limits.maxDrop,limits.probeDepth,
                    limits.supportTolerance,limits.navTolerance,limits.minNormalZ})
        if(!std::isfinite(v) || v<0) return fail(ProbeReason::InvalidInput);
    if(limits.sampleSpacing==0 || limits.probeDepth<limits.maxDrop || limits.minNormalZ<=0 || limits.minNormalZ>1)
        return fail(ProbeReason::InvalidInput);
    const bool actorGrounded=s.grounded==true;
    const double dx=double(x)-s.position->x, dy=double(y)-s.position->y;
    const double distance=std::hypot(dx,dy);
    const double count=locateOnly ? 0.0:std::max(1.0,std::ceil(distance/limits.sampleSpacing));
    if(distance>limits.maxDistance || count>limits.maxSamples || limits.maxQueries<1 ||
       count>(limits.maxQueries-1)/2) return fail(ProbeReason::BudgetExceeded);
    const auto samples=static_cast<std::uint32_t>(count);
    const auto fetch=[&](runtime::QueryKind kind, model::NavVector3 start, model::NavVector3 end)
        ->std::optional<runtime::WorldQueryResult> {
        if(!start.isFinite() || !end.isFinite()) { result.reason=ProbeReason::InvalidInput; return {}; }
        if(result.queries==limits.maxQueries) { result.reason=ProbeReason::BudgetExceeded; return {}; }
        runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,generation,++result.queries},kind,start,end,s.hull,limits.navTolerance};
        try {
            auto reply=port.query(q);
        if(!(reply.stamp==q.stamp) || reply.kind!=kind) {
            result.reason=ProbeReason::StaleQuery;
            return {};
        }
            else if(reply.error==runtime::QueryError::BudgetExceeded) result.reason=ProbeReason::BudgetExceeded;
        else if(reply.error==runtime::QueryError::Unavailable) result.reason=ProbeReason::QueryUnavailable;
        else if(reply.error==runtime::QueryError::InvalidResult) result.reason=ProbeReason::InvalidResult;
            else if(reply.error!=runtime::QueryError::None) result.reason=ProbeReason::QueryFailed;
            else return reply;
        } catch(...) { result.reason=ProbeReason::QueryFailed; }
        return {};
    };
    bool stairCandidate=false;
    const auto clearance=[&](model::NavVector3 a,model::NavVector3 b) {
        stairCandidate=false;
        auto sweep=fetch(runtime::QueryKind::SweptHull,a,b);
        if(!sweep) return false;
        if(!sweep->hull) { result.reason=ProbeReason::InvalidResult; return false; }
        const auto& hit=*sweep->hull;
        if(!std::isfinite(hit.fraction) || hit.fraction<0 || hit.fraction>1 || !hit.end.isFinite() || !hit.normal.isFinite())
            result.reason=ProbeReason::InvalidResult;
        else if(hit.startSolid) result.reason=ProbeReason::Blocked;
        else if(hit.fraction<1) { result.reason=ProbeReason::Blocked; stairCandidate=true; }
        else if(std::abs(double(hit.end.x)-b.x)>0.001 || std::abs(double(hit.end.y)-b.y)>0.001 ||
                std::abs(double(hit.end.z)-b.z)>0.001) result.reason=ProbeReason::InvalidResult;
        else { result.reason=ProbeReason::None; return true; }
        return false;
    };
    auto ground=fetch(runtime::QueryKind::GroundedArea,*s.position,*s.position);
    if(!ground) return result;
    if(!ground->ground || !ground->ground->floor) return fail(ProbeReason::QueryUnavailable);
    auto floor=*ground->ground->floor;
    result.initialTrace=floor.trace;
 auto reason=floorReason(floor,limits.minNormalZ);
 result.initialReason=reason;
 if(reason==ProbeReason::AllSolid || reason==ProbeReason::StartSolid) {
     result.supportFallbackAttempted=true;
     const double feet=double(s.position->z)+s.hull->minimum.z;
     const double top=feet+limits.supportTolerance;
     const double bottom=feet-limits.probeDepth;
     if(!representable(top) || !representable(bottom)) return fail(ProbeReason::InvalidInput);
     const auto fallback=fetch(runtime::QueryKind::Floor,
         {s.position->x,s.position->y,static_cast<float>(top)},
         {s.position->x,s.position->y,static_cast<float>(bottom)});
     if(!fallback || !fallback->floor) return result;
        floor=*fallback->floor;
        result.fallbackTrace=floor.trace;
     reason=floorReason(floor,limits.minNormalZ);
     if(reason!=ProbeReason::None) return fail(reason);
     result.supportFallbackAccepted=true;
 }
 if(reason!=ProbeReason::None) return fail(reason);
    if(!actorGrounded) return fail(ProbeReason::ActorGroundFlagMismatch);
    const double feet=double(s.position->z)+s.hull->minimum.z;
    if(std::abs(double(floor.height)-feet)>limits.supportTolerance) return fail(ProbeReason::FloorHeightMismatch);
    result.floorEvidenceValid=true;
    result.startFloorHeight=floor.height;
    result.lastFloorHeight=floor.height;
 std::optional<model::NavAreaId> observedArea=ground->ground->area;
 if(!observedArea) {
     const auto fallbackArea=index.containing({s.position->x,s.position->y,floor.height},limits.navTolerance);
     if(!fallbackArea || !*fallbackArea.value) return fail(ProbeReason::NavContainmentMissing);
     observedArea=(**fallbackArea.value).areaId;
 }
 if(!observedArea->isValid()) return fail(ProbeReason::NavContainmentMissing);
 if(!locateOnly && *observedArea!=currentArea) return fail(ProbeReason::WrongStartArea);
 currentArea=*observedArea;
    auto match=index.containing({s.position->x,s.position->y,floor.height},limits.navTolerance);
    if(!match || !*match.value || (**match.value).areaId!=currentArea)
        return fail(ProbeReason::NavContainmentMissing);
    auto position=*s.position;
    double previousFloorHeight=floor.height;
    model::NavAreaId area=currentArea;
    for(std::uint32_t i=0;i<samples;++i) {
        const double f=double(i+1)/samples;
        const float tx=static_cast<float>(s.position->x+dx*f), ty=static_cast<float>(s.position->y+dy*f);
        const double top=floor.height+limits.maxStepUp, bottom=floor.height-limits.probeDepth;
        if(!representable(top) || !representable(bottom)) return fail(ProbeReason::InvalidInput);
        const model::NavVector3 start{tx,ty,static_cast<float>(top)}, end{tx,ty,static_cast<float>(bottom)};
        auto reply=fetch(runtime::QueryKind::Floor,start,end);
        if(!reply) return result;
        if(!reply->floor) return fail(ProbeReason::QueryUnavailable);
        const auto next=*reply->floor;
        reason=floorReason(next,limits.minNormalZ);
        if(reason!=ProbeReason::None) return fail(reason);
        if(next.height>start.z || next.height<end.z) return fail(ProbeReason::InvalidResult);
        const double intervalDrop=previousFloorHeight-double(next.height);
        result.floorDelta=intervalDrop;
        result.cumulativeDownDrop=(std::max)(0.0,result.startFloorHeight-double(next.height));
        result.maxDownStep=(std::max)(result.maxDownStep,(std::max)(0.0,intervalDrop));
        if(intervalDrop>limits.maxDrop) return fail(ProbeReason::UnsafeDrop);
        match=index.containing({tx,ty,next.height},limits.navTolerance);
        if(!match || !*match.value) return fail(ProbeReason::NavContainmentMissing);
        const double originZ=double(next.height)-s.hull->minimum.z;
        if(!representable(originZ)) return fail(ProbeReason::InvalidInput);
        const model::NavVector3 destination{tx,ty,static_cast<float>(originZ)};
        if(!clearance(position,destination)) {
            // A hull can hit a riser before the floor trace reaches the higher
            // tread. A same-height sample is therefore still a valid step
            // candidate; the lifted sweeps decide whether it is walkable.
            if(!stairCandidate) return result;
            const std::uint32_t extra=3U;
            if(extra>limits.maxQueries-result.queries) return fail(ProbeReason::BudgetExceeded);
            const double liftedZ=double(position.z)+limits.maxStepUp;
            if(!representable(liftedZ)) return fail(ProbeReason::InvalidInput);
            const model::NavVector3 lifted{position.x,position.y,static_cast<float>(liftedZ)};
            const model::NavVector3 across{tx,ty,lifted.z};
            if(!clearance(position,lifted) || !clearance(lifted,across) || !clearance(across,destination))
                return result;
            result.lastStep=StepEvidence{position,lifted,across,
                GroundedTarget{destination,(**match.value).areaId,next}};
            ++result.steps;
        }
        position=destination; floor=next; area=(**match.value).areaId; ++result.samples;
        previousFloorHeight=next.height;
        result.lastFloorHeight=next.height;
    }
    result.target=GroundedTarget{position,area,floor}; return result;
}
}
ProbeResult GroundProbe::locate(const runtime::MovementSnapshot& s, std::uint64_t generation,
    const query::NavSpatialIndex& index, core::MapGeneration indexMap,
    runtime::IWorldQueries& port, GroundProbeLimits limits) noexcept {
    return probe(s,generation,{},s.position ? s.position->x:0,s.position ? s.position->y:0,
                 index,indexMap,port,limits,true);
}
ProbeResult GroundProbe::inspect(const runtime::MovementSnapshot& s, std::uint64_t generation,
    model::NavAreaId currentArea, float x, float y, const query::NavSpatialIndex& index,
    core::MapGeneration indexMap, runtime::IWorldQueries& port, GroundProbeLimits limits) noexcept {
    return probe(s,generation,currentArea,x,y,index,indexMap,port,limits,false);
}
}
