// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/ladder_discovery.hpp"
#include <algorithm>
namespace astrabot::adapter::cstrike {
LadderDiscoveryResult discoverLadderLinks(LadderWorld world,core::MapGeneration map,
    const nav::query::NavSpatialIndex& index,core::MapGeneration indexMap,
    const nav::enrichment::NavMapFingerprint& fingerprint,std::uint64_t generation,int maximum,
    LadderDiscoveryLimits limits) noexcept {
    LadderDiscoveryResult result;
    if(!map.isValid() || !generation || limits.maxQueries>12288 || limits.maxPassages>1024 || !world.currentMap) return result;
    if(map!=indexMap || world.currentMap(world.context)!=map) { result.reason=LadderDiscoveryReason::StaleWorld; return result; }
    const auto scanned=scanLadderCandidates(world.engine,map,maximum);
    if(!scanned) { result.reason=LadderDiscoveryReason::ScanFailed; result.scanReason=scanned.reason; return result; }
    result.candidates=static_cast<std::uint32_t>(scanned.candidates.count);
    try {
        auto batch=std::make_shared<LadderDiscovery>(); batch->map=map; batch->generation=generation; batch->links.fingerprint=fingerprint;
        for(std::size_t i=0;i<scanned.candidates.count;++i) {
            const auto& candidate=scanned.candidates.values[i]; const auto before=batch->passages.size();
            for(unsigned face=0;face<4;++face) for(unsigned exit=0;exit<2;++exit) {
                const auto budget=(std::min)(12U,limits.maxQueries-result.queries);
                const auto proof=inspectLadderPassage(world,map,candidate,static_cast<LadderFace>(face),static_cast<LadderExit>(exit),
                    index,indexMap,maximum,budget);
                result.queries+=proof.queries;
                if(!proof) {
                    result.probeReason=proof.reason;
                    if(proof.reason==LadderProbeReason::NoFace || proof.reason==LadderProbeReason::NoSupport ||
                       proof.reason==LadderProbeReason::NoArea || proof.reason==LadderProbeReason::Blocked) continue;
                    result.reason=proof.reason==LadderProbeReason::BudgetExceeded ? LadderDiscoveryReason::BudgetExceeded:LadderDiscoveryReason::ProbeFailed;
                    return result;
                }
                if(batch->passages.size()==limits.maxPassages) { result.reason=LadderDiscoveryReason::BudgetExceeded; return result; }
                const auto& p=*proof.passage;
                const std::uint64_t linkId=std::uint64_t{static_cast<std::uint32_t>(candidate.entityId)}*16+face*4+exit*2+1;
                const auto point=[](const LadderEndpoint& e) { return nav::enrichment::NavLinkPoint{e.origin.x,e.origin.y,e.origin.z-36}; };
                batch->links.links.push_back({ladderSourceId,generation,linkId,p.bottom.area,p.top.area,point(p.bottom),point(p.top),
                    nav::model::NavTraversalKind::Ladder,nav::enrichment::NavLinkDirection::Up,0});
                batch->links.links.push_back({ladderSourceId,generation,linkId+1,p.top.area,p.bottom.area,point(p.top),point(p.bottom),
                    nav::model::NavTraversalKind::Ladder,nav::enrichment::NavLinkDirection::Down,0});
                batch->passages.push_back(p);
            }
            if(batch->passages.size()==before) { result.reason=LadderDiscoveryReason::UnlinkedCandidate; return result; }
        }
        // Refresh the candidate set too: a new or removed ladder invalidates the
        // claimed whole-map batch. Contact/clearance is still temporal evidence.
        const auto finalScan=scanLadderCandidates(world.engine,map,maximum);
        if(!finalScan || finalScan.candidates.count!=scanned.candidates.count) { result.reason=LadderDiscoveryReason::StaleWorld; return result; }
        for(std::size_t i=0;i<scanned.candidates.count;++i) {
            const auto& a=scanned.candidates.values[i]; const auto& b=finalScan.candidates.values[i];
            if(a.entityId!=b.entityId || a.minimum!=b.minimum || a.maximum!=b.maximum) { result.reason=LadderDiscoveryReason::StaleWorld; return result; }
        }
        for(const auto& p:batch->passages) if(!ladderPassageCurrent(world,p,maximum)) { result.reason=LadderDiscoveryReason::StaleWorld; return result; }
        if(world.currentMap(world.context)!=map) { result.reason=LadderDiscoveryReason::StaleWorld; return result; }
        result.reason=LadderDiscoveryReason::None; result.probeReason=LadderProbeReason::None; result.value=std::move(batch);
    } catch(...) { result.reason=LadderDiscoveryReason::AllocationFailure; }
    return result;
}
}
