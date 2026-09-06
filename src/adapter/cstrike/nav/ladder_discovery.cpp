// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/ladder_discovery.hpp"
#include <algorithm>
namespace astrabot::adapter::cstrike {
LadderBindingResult bindLadderPlan(LadderWorld world,core::MapGeneration map,
    const nav::enrichment::NavMapFingerprint& fingerprint,const LadderDiscovery& batch,
    const nav::enrichment::NavTraversalLink& selected,int maximum) noexcept {
    LadderBindingResult result;
    if(!map.isValid() || !world.currentMap || maximum<1 || maximum>8192 ||
       !batch.generation || batch.passages.size()>1024 ||
       batch.links.links.size()!=batch.passages.size()*2) return result;
    const auto fail=[&](LadderBindingReason reason) { result.reason=reason; return result; };
    if(batch.map!=map || batch.links.fingerprint!=fingerprint ||
       selected.sourceId!=ladderSourceId || selected.generation!=batch.generation)
        return fail(LadderBindingReason::WrongPublication);
    if(world.currentMap(world.context)!=map) return fail(LadderBindingReason::StaleWorld);
    const auto point=[](nav::enrichment::NavLinkPoint a,nav::enrichment::NavLinkPoint b) {
        return a.x==b.x && a.y==b.y && a.z==b.z;
    };
    std::size_t found=batch.links.links.size();
    for(std::size_t i=0;i<batch.links.links.size();++i) {
        const auto& link=batch.links.links[i];
        if(link.linkId!=selected.linkId) continue;
        if(found!=batch.links.links.size()) return fail(LadderBindingReason::ChangedLink);
        if(link.sourceId!=selected.sourceId || link.generation!=selected.generation ||
           link.from!=selected.from || link.to!=selected.to || !point(link.entry,selected.entry) ||
           !point(link.exit,selected.exit) || link.traversal!=selected.traversal ||
           link.direction!=selected.direction || link.additionalCost!=selected.additionalCost)
            return fail(LadderBindingReason::ChangedLink);
        found=i;
    }
    if(found==batch.links.links.size()) return fail(LadderBindingReason::MissingLink);
    const auto& p=batch.passages[found/2];
    const bool up=found%2==0;
    const auto& start=up ? p.bottom:p.top; const auto& end=up ? p.top:p.bottom;
    const auto expectedId=std::uint64_t{static_cast<std::uint32_t>(p.entityId)}*16+
        static_cast<unsigned>(p.face)*4+static_cast<unsigned>(p.exit)*2+(up ? 1:2);
    if(p.map!=map || p.entityId!=p.candidate.entityId ||
       static_cast<unsigned>(p.face)>3 || static_cast<unsigned>(p.exit)>1 ||
       selected.linkId!=expectedId || selected.traversal!=nav::model::NavTraversalKind::Ladder ||
       selected.direction!=(up ? nav::enrichment::NavLinkDirection::Up:nav::enrichment::NavLinkDirection::Down) ||
       selected.from!=start.area || selected.to!=end.area || selected.additionalCost!=0 ||
       !point(selected.entry,{start.origin.x,start.origin.y,start.origin.z-36}) ||
       !point(selected.exit,{end.origin.x,end.origin.y,end.origin.z-36}))
        return fail(LadderBindingReason::ChangedLink);
    if(!ladderPassageCurrent(world,p,maximum) || world.currentMap(world.context)!=map)
        return fail(LadderBindingReason::StaleWorld);
    BoundLadderPlan bound; bound.passage=p; bound.plan.link=selected;
    bound.plan.start=start.origin; bound.plan.end=end.origin; bound.plan.normal=p.normal;
    bound.plan.mount=up ? p.mount:p.dismount; bound.plan.dismount=up ? p.dismount:p.mount;
    result.reason=LadderBindingReason::None; result.value=bound; return result;
}
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
