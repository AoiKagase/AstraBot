// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/world_model.hpp"
#include "nav/query/distribution.hpp"
#include <cassert>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace perception_evidence {
namespace w=astrabot::core::world;
namespace q=astrabot::nav::query;
struct Row {
    std::string scenario;
    unsigned actors{}, frameUs{};
    bool nav{};
    std::vector<std::string> events;
    void sample(const char* phase,const w::WorldModel& world,const q::DistributionModel& model,
                astrabot::core::PlayerId actor,astrabot::core::PlayerId target,
                std::size_t traces=0,std::size_t audience=0,std::size_t queue=0) {
        assert(events.size()<32768);
        const auto snapshot=world.latest(actor); assert(snapshot);
        const auto& s=*snapshot; const auto known=s.known(target);
        const auto& wd=world.diagnostics(); const auto& rd=world.reports().diagnostics();
        const auto& dd=model.diagnostics();
        const w::ReportMemory* report=nullptr;
        for(std::size_t i=0;i<s.reports->count;++i) if(s.reports->reports[i].report.target==target) report=&s.reports->reports[i];
        std::ostringstream out; out<<std::setprecision(17);
        out<<"{\"phase\":\""<<phase<<"\",\"actor\":"<<actor.slot<<",\"generation\":"<<actor.generation.value
           <<",\"targetGeneration\":"<<target.generation.value<<",\"map\":"<<s.stamp.map.value
           <<",\"round\":"<<s.stamp.round.value<<",\"time\":"<<s.stamp.timeMicros
           <<",\"visual\":"<<s.visual->count<<",\"sounds\":"<<s.sounds->count<<",\"reports\":"<<s.reports->count
           <<",\"source\":"<<(known ? static_cast<unsigned>(known->source):0)
           <<",\"x\":"<<(known ? known->position.x:0)<<",\"confidence\":"<<(known ? known->confidence:0)
           <<",\"origin\":"<<(known ? known->origin.observedMicros:0)
           <<",\"sequence\":"<<(known ? known->origin.sequence:0)
           <<",\"reporter\":"<<(known ? known->reporter.slot:0)
           <<",\"visualAge\":"<<s.oldestVisualAgeMicros<<",\"soundAge\":"<<s.oldestSoundAgeMicros
           <<",\"reportAge\":"<<s.oldestReportAgeMicros<<",\"delay\":"<<s.maxReceiptDelayMicros
           <<",\"processed\":"<<wd.frameProcessed<<",\"visualVisits\":"<<world.visual().diagnostics().frameVisits
           <<",\"soundVisits\":"<<world.sounds().diagnostics().frameVisits
           <<",\"reportVisits\":"<<rd.frameVisits<<",\"deliveries\":"<<rd.frameDelivered
           <<",\"reportQueue\":"<<rd.queued<<",\"connections\":"<<dd.frameConnections
           <<",\"mappings\":"<<dd.frameMappings<<",\"distributionVisits\":"<<dd.frameVisits
           <<",\"distributionDelay\":"<<dd.maxDelayMicros<<",\"traces\":"<<traces
           <<",\"audience\":"<<audience<<",\"soundQueue\":"<<queue
           <<",\"reportX\":"<<(report ? report->report.position.x:0)
           <<",\"reportOrigin\":"<<(report ? report->report.origin.observedMicros:0)
           <<",\"reportReceived\":"<<(report ? report->report.identity.receivedMicros:0)
           <<",\"reportConfidence\":"<<(report ? report->confidence:0)
           <<",\"soundOrigin\":"<<(s.sounds->count ? s.sounds->sounds[0].observation.identity.observedMicros:0)
           <<",\"soundReceived\":"<<(s.sounds->count ? s.sounds->sounds[0].observation.identity.receivedMicros:0)
           <<",\"soundRegionX\":"<<(s.sounds->count ? s.sounds->sounds[0].observation.region.x:0)
           <<",\"soundConfidence\":"<<(s.sounds->count ? s.sounds->sounds[0].confidence:0)<<",\"areas\":[";
        const w::PositionDistribution* distribution=nullptr;
        for(std::size_t i=0;i<s.visual->count;++i) if(s.visual->memories[i].target==target) distribution=s.distributions[i];
        if(distribution) for(std::size_t i=0;i<distribution->count;++i) {
            if(i) out<<',';
            out<<'['<<distribution->areas[i].area<<','<<distribution->areas[i].weight<<']';
        }
        out<<"],\"unknown\":"<<(distribution ? distribution->unknownMass:0)<<'}'; events.push_back(out.str());
    }
};
inline void writeEvidence(const char* path,const char* producer,const std::vector<Row>& rows) {
    std::ofstream out(path); assert(out); out<<"{\"schemaVersion\":1,\"producer\":\""<<producer
        <<"\",\"worldBytes\":"<<sizeof(w::WorldModel)<<",\"distributionBytes\":"<<sizeof(q::DistributionModel)<<",\"results\":[";
    for(std::size_t i=0;i<rows.size();++i) {
        if(i) out<<',';
        const auto& row=rows[i];
        out<<"{\"scenario\":\""<<row.scenario<<"\",\"actors\":"<<row.actors<<",\"frameUs\":"<<row.frameUs
           <<",\"nav\":"<<(row.nav ? "true":"false")<<",\"events\":[";
        for(std::size_t j=0;j<row.events.size();++j) { if(j) out<<','; out<<row.events[j]; }
        out<<"]}";
    }
    out<<"]}\n"; out.close(); assert(out);
}
}
