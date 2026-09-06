// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/ladder_scanner.hpp"
#include <cstring>

namespace astrabot::adapter::cstrike {
LadderScanResult scanLadderCandidates(enginefuncs_t* engine,core::MapGeneration map,int maxEntities,
    std::uint32_t maxSlots,std::size_t maxCandidates) noexcept {
    LadderScanResult result;
    if(!map.isValid() || maxEntities<1 || maxSlots>8192 || maxCandidates>128) return result;
    if(static_cast<std::uint32_t>(maxEntities)>maxSlots) {
        result.reason=LadderScanReason::EntityLimit; return result;
    }
    if(!engine || !engine->pfnPEntityOfEntIndex || !engine->pfnIndexOfEdict || !engine->pfnSzFromIndex) {
        result.reason=LadderScanReason::Unavailable; return result;
    }
    const auto fail=[&result](LadderScanReason reason) {
        result.reason=reason; result.candidates={}; return result;
    };
    for(int slot=1;slot<maxEntities;++slot) {
        ++result.inspected;
        auto* entity=engine->pfnPEntityOfEntIndex(slot);
        if(!entity || entity->free || !entity->v.classname) continue;
        const auto serial=entity->serialnumber;
        const auto classname=entity->v.classname;
        const auto* name=engine->pfnSzFromIndex(classname);
        if(!name) return fail(LadderScanReason::InvalidEntity);
        if(std::strcmp(name,"func_ladder")!=0) continue;
        if(engine->pfnIndexOfEdict(entity)!=slot || engine->pfnPEntityOfEntIndex(slot)!=entity ||
           entity->free || entity->serialnumber!=serial || entity->v.classname!=classname)
            return fail(LadderScanReason::InvalidEntity);
        if(result.candidates.count==maxCandidates) return fail(LadderScanReason::CandidateLimit);
        const auto& v=entity->v;
        const nav::model::NavVector3 low{v.absmin.x,v.absmin.y,v.absmin.z},high{v.absmax.x,v.absmax.y,v.absmax.z};
        if(!low.isFinite() || !high.isFinite() || low.x>=high.x || low.y>=high.y || low.z>=high.z ||
           v.angles.x!=0 || v.angles.y!=0 || v.angles.z!=0 ||
           v.velocity.x!=0 || v.velocity.y!=0 || v.velocity.z!=0 ||
           v.avelocity.x!=0 || v.avelocity.y!=0 || v.avelocity.z!=0)
            return fail(LadderScanReason::UnsupportedGeometry);
        const auto id=(std::uint64_t{static_cast<std::uint32_t>(serial)}<<32)|static_cast<std::uint32_t>(slot);
        result.candidates.values[result.candidates.count++]={id,low,high};
    }
    result.reason=LadderScanReason::None; result.candidates.map=map; return result;
}
}
