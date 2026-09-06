// SPDX-License-Identifier: MPL-2.0
#include "nav/query/distribution.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
namespace astrabot::nav::query {
namespace w = core::world;
std::shared_ptr<const DistributionTopology> DistributionTopology::build(core::MapGeneration map,
    std::shared_ptr<const NavGraph> graph,std::shared_ptr<const NavSpatialIndex> spatial) noexcept {
    if (!map.isValid() || !graph || !spatial) return {};
    try {
        auto topology = std::shared_ptr<DistributionTopology>(new DistributionTopology); topology->map=map;
        topology->graph=std::move(graph); topology->spatial=std::move(spatial);
        topology->offsets.reserve(topology->graph->areaCount()+1);
        topology->targets.reserve(topology->graph->edgeCount());
        for (std::size_t i=0;i<topology->graph->areaCount();++i) {
            const auto begin=topology->targets.size(); topology->offsets.push_back(begin);
            for (auto e=topology->graph->edgeBegin(i);e<topology->graph->edgeEnd(i);++e)
                topology->targets.push_back(topology->graph->edge(e).target.value);
            auto first=topology->targets.begin()+static_cast<std::vector<std::uint32_t>::difference_type>(begin);
            std::sort(first,topology->targets.end());
            topology->targets.erase(std::unique(first,topology->targets.end()),topology->targets.end());
        }
        topology->offsets.push_back(topology->targets.size()); return topology;
    } catch (const std::bad_alloc&) { return {}; } catch (const std::length_error&) { return {}; }
}
void DistributionModel::reset() noexcept {
    for (auto& job:jobs_) { job={}; }
    topology_.reset(); map_={}; round_={}; time_=0; cursor_=0;
    diagnostics_={}; // Revision remains monotonic across transient resets.
}
void DistributionModel::retain(w::PositionDistribution& output,std::uint32_t area,double mass) noexcept {
    if (mass<=0) return;
    if (output.count<output.areas.size()) { output.areas[output.count++]={area,mass}; return; }
    std::size_t worst=0;
    for (std::size_t i=1;i<output.count;++i)
        if (output.areas[i].weight<output.areas[worst].weight ||
            (output.areas[i].weight==output.areas[worst].weight && output.areas[i].area>output.areas[worst].area)) worst=i;
    const auto prior=output.areas[worst];
    if (mass>prior.weight || (mass==prior.weight && area<prior.area)) {
        output.unknownMass+=prior.weight; output.areas[worst]={area,mass};
    } else output.unknownMass+=mass;
}
void DistributionModel::start(Job& job,const w::PositionDistribution& input) noexcept {
    job.input=input; job.output={}; job.output.available=true; job.output.navRevision=revision_;
    job.output.updatedMicros=input.updatedMicros+stepMicros; job.output.unknownMass=input.unknownMass;
    job.stay=0; job.pendingArea=0; job.pendingMass=0; job.active=true;
    for (std::size_t i=0;i<input.count;++i) {
        const auto vertex=topology_->graph->find({input.areas[i].area});
        // All inputs originate from this immutable topology.
        if (!vertex) { job.active=false; return; }
        job.edge[i]=topology_->offsets[*vertex]; job.end[i]=topology_->offsets[*vertex+1];
        const auto degree=job.end[i]-job.edge[i];
        job.share[i]=degree ? input.areas[i].weight*0.5/static_cast<double>(degree):0;
    }
}
bool DistributionModel::progress(Job& job,std::size_t quantum) noexcept {
    std::size_t consumed=0;
    // Merge up to32 sorted outgoing streams plus the sorted retained-area stream.
    // Finish each destination's exact sum before applying the top32 cap.
    for (std::size_t operation=0;operation<128;++operation) {
        std::uint32_t next=job.stay<job.input.count ? job.input.areas[job.stay].area:0;
        for (std::size_t i=0;i<job.input.count;++i) if (job.edge[i]<job.end[i]) {
            const auto target=topology_->targets[job.edge[i]]; if (!next || target<next) next=target;
        }
        if (job.pendingArea && next!=job.pendingArea) {
            retain(job.output,job.pendingArea,job.pendingMass); job.pendingArea=0; job.pendingMass=0;
        }
        if (!next) {
            std::sort(job.output.areas.begin(),job.output.areas.begin()+job.output.count,
                [](w::AreaWeight a,w::AreaWeight b){return a.area<b.area;});
            job.active=false; return true;
        }
        job.pendingArea=next;
        if (job.stay<job.input.count && job.input.areas[job.stay].area==next) {
            job.pendingMass+=job.input.areas[job.stay].weight*(job.share[job.stay]>0 ? 0.5:1.0); ++job.stay;
        }
        for (std::size_t i=0;i<job.input.count;++i) if (job.edge[i]<job.end[i] && topology_->targets[job.edge[i]]==next) {
            if (consumed==quantum) return false;
            job.pendingMass+=job.share[i]; ++job.edge[i]; ++consumed; ++diagnostics_.frameConnections;
        }
    }
    return false;
}
void DistributionModel::update(w::WorldModel& world,std::shared_ptr<const DistributionTopology> topology) noexcept {
    diagnostics_.frameConnections=diagnostics_.frameMappings=diagnostics_.frameVisits=diagnostics_.pending=0;
    const auto* frame=world.publishedFrame();
    if (!frame || !topology || topology->map!=frame->map) {
        for (auto& job:jobs_) if(job.observer.isValid()) { if(job.active) ++diagnostics_.expiredJobs; job={}; }
        world.clearDistributions(); topology_.reset(); return;
    }
    if (topology!=topology_ || map_!=frame->map || round_!=frame->round || frame->timeMicros<time_) {
        for (auto& job:jobs_) { job={}; }
        world.clearDistributions(); ++diagnostics_.resets;
        if (revision_==(std::numeric_limits<std::uint64_t>::max)()) { topology_.reset(); return; }
        ++revision_; topology_=std::move(topology); map_=frame->map; round_=frame->round; cursor_=0;
    }
    time_=frame->timeMicros;
    std::array<const w::VisualMemory*,32*32> memories{};
    for (auto& job:jobs_) job.present=false;
    for (const auto& player:frame->players) {
        const auto snapshot=world.latest(player.player); if (!snapshot) continue;
        for (std::size_t i=0;i<snapshot->visual->count;++i) {
            const auto& memory=snapshot->visual->memories[i];
            const auto index=(player.player.slot-1U)*32+memory.target.slot-1U;
            auto& job=jobs_[index]; memories[index]=&memory;
            if (job.observer!=player.player || job.target!=memory.target || job.identity.sequence!=memory.identity.sequence) {
                if(job.active) ++diagnostics_.expiredJobs;
                job={}; job.observer=player.player; job.target=memory.target; job.identity=memory.identity;
            }
            job.present=true;
        }
    }
    for (auto& job:jobs_) if (!job.present && job.observer.isValid()) { if(job.active) ++diagnostics_.expiredJobs; job={}; }
    std::size_t first=jobs_.size();
    for(std::size_t i=0;i<jobs_.size();++i) if(jobs_[i].present) { first=i; break; }
    if(first==jobs_.size()) return;
    std::size_t activeCount=0; for(const auto& job:jobs_) if(job.present) ++activeCount;
    std::array<std::size_t,32*32> nextActive{};
    auto next=first;
    for(std::size_t i=jobs_.size();i>0;--i) { if(jobs_[i-1].present) next=i-1; nextActive[i-1]=next; }
    // Persistent round-robin cursor, eight edges per turn, bounded active-job visits.
    std::size_t idleVisits=0;
    while (idleVisits<activeCount && diagnostics_.frameVisits<jobs_.size()*2 && diagnostics_.frameConnections<connectionsPerFrame) {
        cursor_=nextActive[cursor_];
        const auto index=cursor_; cursor_=(cursor_+1)%jobs_.size(); ++diagnostics_.frameVisits;
        auto& job=jobs_[index]; if (!job.present) continue;
        const auto& memory=*memories[index];
        if(job.mapped && (!job.input.available || (!job.active && time_-job.input.updatedMicros<stepMicros))) { ++idleVisits; continue; }
        if (!job.mapped) {
            if (diagnostics_.frameMappings==mappingsPerFrame) { ++idleVisits; continue; }
            ++diagnostics_.frameMappings; job.mapped=true;
            job.input.updatedMicros=memory.lastSeenMicros; job.input.navRevision=revision_;
            const auto& point=memory.lastKnownPosition;
            const double maxFloat=(std::numeric_limits<float>::max)();
            if (point.x>=-maxFloat && point.x<=maxFloat && point.y>=-maxFloat && point.y<=maxFloat && point.z>=-maxFloat && point.z<=maxFloat) {
                const auto match=topology_->spatial->containing({static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)},128);
                if (match && *match.value && topology_->graph->find((*match.value)->areaId)) {
                    job.input.available=true; job.input.count=1; job.input.areas[0]={(*match.value)->areaId.value,1};
                }
            }
            if (!job.input.available) ++diagnostics_.unavailable;
            (void)world.setDistribution(job.observer,job.target,job.identity,job.input);
        }
        idleVisits=0;
        if (!job.input.available) continue;
        if (!job.active && time_-job.input.updatedMicros>=stepMicros) start(job,job.input);
        if (job.active && progress(job,(std::min)(std::size_t{8},connectionsPerFrame-diagnostics_.frameConnections))) {
            job.input=job.output; ++diagnostics_.completed;
        }
        job.input.delayMicros=time_-job.input.updatedMicros;
        diagnostics_.maxDelayMicros=(std::max)(diagnostics_.maxDelayMicros,job.input.delayMicros);
        (void)world.setDistribution(job.observer,job.target,job.identity,job.input);
    }
    for (const auto& job:jobs_) if (job.present) {
        const auto delay=time_-(job.mapped ? job.input.updatedMicros:job.identity.observedMicros);
        diagnostics_.maxDelayMicros=(std::max)(diagnostics_.maxDelayMicros,delay);
        if (!job.mapped || job.active || (job.input.available && delay>=stepMicros)) ++diagnostics_.pending;
    }
}
}
