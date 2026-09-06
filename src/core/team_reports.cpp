// SPDX-License-Identifier: MPL-2.0
#include "core/team_reports.hpp"
#include <limits>
namespace astrabot::core::world {
namespace p=perception;
namespace { bool valid(PlayerId id) noexcept { return id.isValid() && id.slot<=32; } }
const char* reportReasonName(ReportReason r) noexcept {
    switch(r) {
    case ReportReason::None:return "Queued"; case ReportReason::NotReady:return "NotReady";
    case ReportReason::InvalidTime:return "InvalidTime"; case ReportReason::InvalidActor:return "InvalidActor";
    case ReportReason::NoDirectSight:return "NoDirectSight"; case ReportReason::TooOld:return "TooOld";
    case ReportReason::NoAllies:return "NoAllies"; case ReportReason::Duplicate:return "Duplicate";
    case ReportReason::QueueFull:return "QueueFull"; case ReportReason::Stale:return "Stale";
    case ReportReason::Expired:return "Expired"; case ReportReason::Exhausted:return "Exhausted";
    default:return "Invalid";
    }
}
ReportResult TeamReportModel::reject(ReportReason reason) noexcept {
    diagnostics_.reason=reason; ++diagnostics_.rejected[static_cast<std::size_t>(reason)]; return {reason,0};
}
void TeamReportModel::clearQueue() noexcept {
    diagnostics_.retired+=count_; head_=count_=0; diagnostics_.queued=0;
}
void TeamReportModel::reset() noexcept {
    invalidate(); frame_={}; participants_={}; generations_={}; sentOrigins_={}; timeHighWater_=0; diagnostics_={};
    // Keep the transmission sequence monotonic across transient resets.
}
void TeamReportModel::invalidate() noexcept {
    clearQueue(); ready_=false;
    for(std::size_t i=0;i<snapshots_.size();++i) { diagnostics_.retired+=snapshots_[i].count; snapshots_[i]={}; retainedEpochs_[i]={}; }
}
bool TeamReportModel::eligible(PlayerId id,bool managed) const noexcept {
    if(!valid(id)) return false;
    const auto& participant=participants_[id.slot-1U].value;
    return participant.player==id && participant.eligible && (!managed || participant.agent.isValid());
}
bool TeamReportModel::allies(PlayerId a,PlayerId b) const noexcept {
    if(a==b || !eligible(a,true) || !eligible(b,true)) return false;
    const auto team=participants_[a.slot-1U].team;
    return (team==p::Team::Terrorist || team==p::Team::CounterTerrorist) && team==participants_[b.slot-1U].team;
}
bool TeamReportModel::current(const TeamReport& report) const noexcept {
    return eligible(report.target) && report.target!=report.receiver && allies(report.reporter,report.receiver) &&
        report.origin.map==frame_.map && report.origin.round==frame_.round;
}
void TeamReportModel::prune(bool decay) noexcept {
    for(std::size_t row=0;row<snapshots_.size();++row) {
        auto& snapshot=snapshots_[row]; auto& epochs=retainedEpochs_[row];
        if(!eligible(snapshot.stamp.observer,true)) { diagnostics_.retired+=snapshot.count; snapshot={}; continue; }
        std::size_t kept=0;
        for(std::size_t i=0;i<snapshot.count;++i) {
            auto memory=snapshot.reports[i];
            if(decay) ++diagnostics_.frameVisits;
            const auto& r=memory.report; const auto e=epochs[i];
            if(!current(r) || e.reporter!=participants_[r.reporter.slot-1U].epoch ||
                e.receiver!=participants_[r.receiver.slot-1U].epoch || e.target!=participants_[r.target.slot-1U].epoch) {
                ++diagnostics_.retired; continue;
            }
            const auto age=frame_.timeMicros-memory.report.origin.observedMicros;
            if(age>=retentionMicros) { ++diagnostics_.retired; continue; }
            if(decay) memory.confidence=0.5*(1-static_cast<double>(age)/static_cast<double>(retentionMicros));
            snapshot.reports[kept]=memory; epochs[kept++]=e;
        }
        for(std::size_t i=kept;i<snapshot.count;++i) { snapshot.reports[i]={}; epochs[i]={}; }
        snapshot.count=kept;
    }
}
void TeamReportModel::forget(PlayerId player) noexcept {
    if(!valid(player)) return;
    auto& participant=participants_[player.slot-1U];
    if(participant.value.player!=player) return;
    participant.value.eligible=false; ++participant.epoch; prune(false);
}
void TeamReportModel::beginRound(p::RoundGeneration round) noexcept {
    if(round.value<=frame_.round.value) return;
    invalidate(); frame_.round=round; sentOrigins_={};
}
bool TeamReportModel::advance(const MemoryFrame& frame,const p::TeamRoster& teams) noexcept {
    diagnostics_.frameDelivered=diagnostics_.frameVisits=0;
    if(!frame.map.isValid() || !frame.round.isValid() || !frame.tick.isValid() ||
        (frame_.map.isValid() && (frame.map.value<frame_.map.value || (frame.map==frame_.map &&
         (frame.round.value<frame_.round.value || frame.tick.value<=frame_.tick.value || frame.timeMicros<timeHighWater_))))) {
        invalidate(); (void)reject(ReportReason::InvalidTime); return false;
    }
    if(frame.map!=frame_.map) { invalidate(); participants_={}; generations_={}; sentOrigins_={}; }
    else beginRound(frame.round);
    frame_=frame; timeHighWater_=frame.timeMicros;
    for(std::size_t i=0;i<participants_.size();++i) {
        auto value=frame.players[i]; auto& prior=participants_[i];
        if(!valid(value.player) || value.player.slot!=i+1 || value.player.generation<generations_[i]) value={};
        else generations_[i]=value.player.generation;
        const auto* member=teams.map()==frame.map ? teams.find(value.player):nullptr;
        const auto team=member ? member->team:p::Team::Unknown;
        if(value.player!=prior.value.player || value.agent!=prior.value.agent || value.eligible!=prior.value.eligible || team!=prior.team) ++prior.epoch;
        prior.value=value; prior.team=team;
    }
    prune(true); ready_=true;
    for(std::size_t i=0;i<participants_.size();++i) {
        const auto& value=participants_[i].value;
        if(eligible(value.player,true)) snapshots_[i].stamp={value.agent,value.player,frame.map,frame.tick,frame.timeMicros,frame.round};
    }
    return true;
}
ReportResult TeamReportModel::send(const VisualMemoryModel& visual,PlayerId reporter,PlayerId target,std::uint64_t now,
    const std::array<bool,32>& currentEligibility,const p::TeamRoster& teams) noexcept {
    if(!ready_) return reject(ReportReason::NotReady);
    if(now<timeHighWater_) { invalidate(); return reject(ReportReason::InvalidTime); }
    timeHighWater_=now;
    if(!eligible(reporter,true) || !eligible(target) || !currentEligibility[reporter.slot-1U] ||
        !currentEligibility[target.slot-1U] || reporter==target || teams.map()!=frame_.map) return reject(ReportReason::InvalidActor);
    const auto* snapshot=visual.latest(reporter); const VisualMemory* source=nullptr;
    if(snapshot && snapshot->stamp.map==frame_.map && snapshot->stamp.round==frame_.round)
        for(std::size_t i=0;i<snapshot->count;++i) if(snapshot->memories[i].target==target) source=&snapshot->memories[i];
    if(!source || source->identity.source!=p::ObservationSource::Vision) return reject(ReportReason::NoDirectSight);
    if(source->lastSeenMicros>now || now-source->lastSeenMicros>freshnessMicros) return reject(ReportReason::TooOld);
    auto& sent=sentOrigins_[reporter.slot-1U][target.slot-1U];
    if(source->identity.sequence<=sent) return reject(ReportReason::Duplicate);
    std::array<PlayerId,32> recipients{}; std::size_t count=0;
    for(const auto& participant:participants_) {
        const auto receiver=participant.value.player;
        if(eligible(receiver,true) && receiver!=target && currentEligibility[receiver.slot-1U] &&
            allies(reporter,receiver) && teams.relation(reporter,receiver)==p::Relation::Ally) recipients[count++]=receiver;
    }
    if(!count) return reject(ReportReason::NoAllies);
    if(count>queue_.size()-count_) { ++diagnostics_.overflow; return reject(ReportReason::QueueFull); }
    if(sequence_==(std::numeric_limits<std::uint64_t>::max)()) return reject(ReportReason::Exhausted);
    const p::ObservationIdentity id{frame_.map,frame_.round,p::ObservationSource::TeamReport,++sequence_,source->lastSeenMicros,now};
    for(std::size_t i=0;i<count;++i) {
        const auto receiver=recipients[i];
        queue_[(head_+count_)%queue_.size()]={{reporter,receiver,target,source->lastKnownPosition,source->identity,id,now},
            participants_[reporter.slot-1U].epoch,participants_[receiver.slot-1U].epoch,participants_[target.slot-1U].epoch};
        ++count_;
    }
    sent=source->identity.sequence; ++diagnostics_.sent; diagnostics_.queued=count_; diagnostics_.reason=ReportReason::None;
    return {ReportReason::None,count};
}
void TeamReportModel::deliver() noexcept {
    if(!ready_) return;
    while(count_ && diagnostics_.frameDelivered<deliveriesPerFrame) {
        auto pending=queue_[head_]; head_=(head_+1)%queue_.size(); --count_; ++diagnostics_.frameDelivered;
        auto& report=pending.report;
        if(!current(report) || pending.reporterEpoch!=participants_[report.reporter.slot-1U].epoch ||
            pending.receiverEpoch!=participants_[report.receiver.slot-1U].epoch || pending.targetEpoch!=participants_[report.target.slot-1U].epoch) {
            (void)reject(ReportReason::Stale); continue;
        }
        if(report.sentMicros>frame_.timeMicros || frame_.timeMicros-report.origin.observedMicros>=retentionMicros) {
            (void)reject(ReportReason::Expired); continue;
        }
        auto& snapshot=snapshots_[report.receiver.slot-1U]; std::size_t index=0;
        while(index<snapshot.count && snapshot.reports[index].report.target!=report.target) ++index;
        if(index<snapshot.count) {
            const auto& prior=snapshot.reports[index].report;
            if(report.origin.observedMicros<prior.origin.observedMicros ||
                (report.origin.observedMicros==prior.origin.observedMicros &&
                 (report.reporter.slot>prior.reporter.slot ||
                  (report.reporter==prior.reporter && report.origin.sequence<=prior.origin.sequence)))) {
                (void)reject(ReportReason::Duplicate); continue;
            }
        } else ++snapshot.count;
        report.identity.receivedMicros=frame_.timeMicros;
        snapshot.reports[index]={report,0.5*(1-static_cast<double>(frame_.timeMicros-report.origin.observedMicros)/static_cast<double>(retentionMicros))};
        retainedEpochs_[report.receiver.slot-1U][index]={pending.reporterEpoch,pending.receiverEpoch,pending.targetEpoch};
        ++diagnostics_.delivered;
    }
    diagnostics_.queued=count_;
}
const ReportSnapshot* TeamReportModel::latest(PlayerId player) const noexcept {
    if(!ready_ || !valid(player)) return nullptr;
    const auto& snapshot=snapshots_[player.slot-1U]; return snapshot.stamp.observer==player ? &snapshot:nullptr;
}
}
