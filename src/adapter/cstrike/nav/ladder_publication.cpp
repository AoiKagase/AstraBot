// SPDX-License-Identifier: MPL-2.0
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include "adapter/cstrike/nav/console.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include "nav/io/map_fingerprint.hpp"
#ifdef snprintf
#undef snprintf
#endif
namespace astrabot::adapter::cstrike {
bool NavConsole::publishLadders(std::istream& bsp,LadderWorld world,core::MapGeneration map,int maximum) noexcept {
    if(inRequest_ || !mesh_ || !index_ || navigation_.map!=map || !world.currentMap) return false;
    // Retire old routes and enrichment even when replacement discovery fails.
    const auto mesh=mesh_;
    if(!publish(map,mesh).isNone()) return false;
    const auto index=index_;
    inRequest_=true;
    const auto run=[&]() -> bool {
        if(world.currentMap(world.context)!=map) { line("nav ladders=StaleMap"); return false; }
        const auto fingerprint=nav::io::fingerprintMap(bsp,512ULL*1024*1024);
        if(!fingerprint) {
            char text[96]{}; std::snprintf(text,sizeof(text),"nav ladders=FingerprintFailure reason=%u",unsigned(fingerprint.reason)); line(text); return false;
        }
        if(mesh->header().bspSize && *mesh->header().bspSize!=fingerprint.bytes) { line("nav ladders=BspSizeMismatch"); return false; }
        if(ladderGeneration_==(std::numeric_limits<std::uint64_t>::max)()) { line("nav ladders=GenerationExhausted"); return false; }
        const auto result=discoverLadderLinks(world,map,*index,map,*fingerprint.fingerprint,++ladderGeneration_,maximum);
        if(!result) {
            char text[160]{}; std::snprintf(text,sizeof(text),"nav ladders=DiscoveryFailure reason=%u scan=%u probe=%u candidates=%u queries=%u",
                unsigned(result.reason),unsigned(result.scanReason),unsigned(result.probeReason),result.candidates,result.queries); line(text); return false;
        }
        const auto graph=nav::query::NavGraph::compose(mesh,*fingerprint.fingerprint,result.value->links,
            {100000,1000000,256U*1024*1024},{2048,256U*1024*1024});
        if(!graph) { line("nav ladders=GraphFailure"); return false; }
        if(deferredInvalidation_ || world.currentMap(world.context)!=map) { line("nav ladders=StaleMap"); return false; }
        const auto distribution=nav::query::DistributionTopology::build(map,*graph.value,index);
        if(!distribution) { line("nav ladders=DistributionAllocationFailure"); return false; }
        navigation_={map,*graph.value}; distributionTopology_=distribution; ladders_=result.value;
        char digest[65]{};
        for(std::size_t i=0;i<32;++i) { digest[2*i]="0123456789abcdef"[(*fingerprint.fingerprint)[i]>>4]; digest[2*i+1]="0123456789abcdef"[(*fingerprint.fingerprint)[i]&15]; }
        char text[280]{}; std::snprintf(text,sizeof(text),"nav ladders=Ready candidates=%u passages=%zu links=%zu queries=%u generation=%llu bsp_bytes=%llu bsp_sha256=%s",
            result.candidates,result.value->passages.size(),result.value->links.links.size(),result.queries,
            static_cast<unsigned long long>(ladderGeneration_),static_cast<unsigned long long>(fingerprint.bytes),digest); line(text);
        return true;
    };
    bool accepted=false;
    try { accepted=run(); } catch(...) { line("nav ladders=InputOrAllocationFailure"); }
    inRequest_=false;
    if(deferredInvalidation_) { (void)applyDeferredInvalidation(); return false; }
    return accepted;
}
void NavConsole::loadCurrentLadders(metamod::LifecycleCoordinator& owner) noexcept {
    if(!engine_ || !globals_ || !engine_->pfnGetGameDir || !engine_->pfnSzFromIndex) { line("nav ladders=Unavailable"); return; }
    try {
        // Engine map name selects the BSP, independent of the explicit NAV path.
        const auto* raw=engine_->pfnSzFromIndex(globals_->mapname);
        std::string name;
        if(raw) for(std::size_t i=0;i<64 && raw[i];++i) {
            const char c=raw[i];
            if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-')) {
                line("nav ladders=InvalidMapName"); return;
            }
            name+=c;
        }
        if(name.empty() || name.size()==64) { line("nav ladders=InvalidMapName"); return; }
        char directory[261]{}; engine_->pfnGetGameDir(directory);
        std::size_t length=0; while(length<260 && directory[length]) ++length;
        if(!length || length==260) { line("nav ladders=InvalidGameDirectory"); return; }
        std::ifstream bsp(std::string(directory,length)+"/maps/"+name+".bsp",std::ios::binary);
        if(!bsp) { line("nav ladders=BspUnavailable"); return; }
        const LadderWorld world{engine_,&owner.registry(),[](const void* context) noexcept {
            const auto& registry=*static_cast<const host::PlayerRegistry*>(context);
            return registry.isMapActive() ? registry.mapGeneration():core::MapGeneration{};
        }};
        (void)publishLadders(bsp,world,owner.registry().mapGeneration(),globals_->maxEntities);
    } catch(...) { line("nav ladders=InputOrAllocationFailure"); }
}
}
