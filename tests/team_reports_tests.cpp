// SPDX-License-Identifier: MPL-2.0
#include "core/world_model.hpp"
#include <cassert>
#include <cmath>
#include <memory>
namespace w=astrabot::core::world;
namespace p=astrabot::core::perception;
using astrabot::core::PlayerId;
constexpr PlayerId reporter{1,{1}}, receiver{2,{1}}, target{32,{1}};
struct Fixture {
    std::unique_ptr<w::WorldModel> world=std::make_unique<w::WorldModel>();
    w::MemoryFrame frame{}; p::TeamRoster teams{}; std::array<bool,32> eligibility{};
    Fixture(unsigned actors=2) {
        frame.map={1}; frame.round={1}; frame.timeMicros=1000000;
        assert(teams.activate(frame.map));
        for(std::uint16_t slot=1;slot<=actors;++slot) {
            frame.players[slot-1]={{slot,{1}},{slot},true}; eligibility[slot-1]=true;
            assert(teams.bind(frame.map,{slot,{1}})); assert(teams.update(frame.map,{slot,{1}},p::Team::Terrorist));
        }
        frame.players[31]={target,{},true}; eligibility[31]=true;
        assert(teams.bind(frame.map,target)); assert(teams.update(frame.map,target,p::Team::CounterTerrorist));
    }
    void step(std::uint64_t dt,PlayerId viewer={},bool all=false,double x=100) {
        ++frame.tick.value; frame.timeMicros+=dt; assert(world->advance(frame,teams));
        if(viewer.isValid()) {
            p::ObservationBatch batch{}; batch.stamp={{viewer.slot},viewer,frame.map,frame.tick,frame.timeMicros,frame.round};
            batch.identity={frame.map,frame.round,p::ObservationSource::Vision,frame.tick.value,frame.timeMicros,frame.timeMicros};
            if(all) for(const auto& player:frame.players) {
                if(player.eligible && player.player!=viewer) batch.observations[batch.count++]={player.player,{x,10,64}};
            } else { batch.count=1; batch.observations[0]={target,{x,10,64}}; }
            assert(world->stage(batch));
        }
        assert(world->publish());
        assert(world->reports().diagnostics().frameDelivered<=32 && world->reports().diagnostics().frameVisits<=32*31);
    }
    w::ReportResult send(PlayerId who=reporter,PlayerId subject=target) {
        return world->requestReport(who,subject,frame.timeMicros,eligibility,teams);
    }
    const w::ReportSnapshot* reports(PlayerId actor=receiver) const { return world->latest(actor)->reports; }
};
void deliveryAndPriority() {
    Fixture f; f.step(0,reporter); assert(!f.world->latest(receiver)->known(target));
    assert(f.send().recipients==1); assert(f.reports()->count==0);
    assert(f.send().reason==w::ReportReason::Duplicate);
    f.step(100000); assert(f.reports()->count==1);
    const auto report=f.reports()->reports[0];
    assert(report.report.origin.source==p::ObservationSource::Vision && report.report.identity.source==p::ObservationSource::TeamReport);
    assert(report.report.origin.observedMicros==1000000 && report.report.sentMicros==1000000 && report.report.identity.receivedMicros==1100000);
    assert(std::abs(report.confidence-0.49)<1e-12);
    assert(f.world->latest(receiver)->oldestReportAgeMicros==100000 && f.world->latest(receiver)->maxReceiptDelayMicros==100000);
    const auto knowledge=f.world->latest(receiver)->known(target); assert(knowledge && knowledge->source==p::ObservationSource::TeamReport && knowledge->reporter==reporter);
    assert(f.send(receiver).reason==w::ReportReason::NoDirectSight);
    f.step(0,receiver,false,200); assert(f.reports()->count==1);
    assert(f.world->latest(receiver)->known(target)->source==p::ObservationSource::Vision);
    assert(f.world->latest(receiver)->known(target)->position.x==200 && f.reports()->reports[0].report.position.x==100);
    f.step(4900000); assert(f.reports()->count==0 && f.world->latest(receiver)->known(target)->source==p::ObservationSource::Vision);
}
void validationAndRetirement() {
    Fixture f; f.step(0,reporter); f.step(500000); assert(f.send().accepted());
    f.step(1); assert(f.send().reason==w::ReportReason::TooOld);
    f.world->forget(reporter); assert(f.reports()->count==0);
    f.step(1,reporter); assert(f.send().accepted());
    f.frame.players[0].eligible=false; f.step(1); assert(f.reports()->count==0);
    f.frame.players[0].eligible=true; f.step(1); assert(f.reports()->count==0);
    f.step(1,reporter); assert(f.send().accepted());
    f.world->forgetReports(receiver); assert( !f.world->latest(receiver)->reports );
    f.step(1); assert(f.reports()->count==0); // No pre-invalidation delivery after reactivation.
    assert(f.teams.update(f.frame.map,receiver,p::Team::CounterTerrorist)); f.step(1,reporter);
    assert(f.send().reason==w::ReportReason::NoAllies);
    assert(f.teams.update(f.frame.map,receiver,p::Team::Unknown)); f.step(1);
    assert(f.send().reason==w::ReportReason::NoAllies);
    assert(f.teams.update(f.frame.map,receiver,p::Team::Terrorist)); f.step(1,reporter); assert(f.send().accepted());
    f.frame.round={2}; f.step(1); assert(f.reports()->count==0);
    f.step(1,reporter); assert(f.send().accepted());
    auto bad=f.frame; --bad.timeMicros; ++bad.tick.value; assert(!f.world->advance(bad,f.teams));
    assert(!f.world->latest(receiver)); f.step(1); assert(f.reports()->count==0);
    assert(f.send({1,{99}}).reason==w::ReportReason::InvalidActor);
}
void queue() {
    Fixture f(31); f.step(0,reporter,true);
    unsigned accepted=0;
    for(std::uint16_t subject=2;subject<=32;++subject) if(f.send(reporter,{subject,{1}}).accepted()) ++accepted;
    assert(accepted==8 && f.world->reports().diagnostics().queued==232 && f.world->reports().diagnostics().overflow>0);
    f.step(100000); assert(f.world->reports().diagnostics().frameDelivered==32 && f.world->reports().diagnostics().queued==200);
    f.step(5000000); assert(f.reports()->count==0);
    for(unsigned i=0;i<7;++i) f.step(1);
    assert(f.world->reports().diagnostics().queued==0 && f.reports()->count==0);
}
void simultaneousTeamChange() {
    Fixture f; f.step(0,reporter); assert(f.send().accepted()); f.step(1); assert(f.reports()->count==1);
    assert(f.teams.update(f.frame.map,reporter,p::Team::CounterTerrorist));
    assert(f.teams.update(f.frame.map,receiver,p::Team::CounterTerrorist));
    f.step(1); assert(f.reports()->count==0);
}
void generationsAndMap() {
    Fixture f; f.step(0,reporter); assert(f.send().accepted()); f.step(1); assert(f.reports()->count==1);
    const PlayerId next{1,{2}}; f.frame.players[0].player=next;
    f.teams.forget(reporter); assert(f.teams.bind(f.frame.map,next)); assert(f.teams.update(f.frame.map,next,p::Team::Terrorist));
    f.step(1,next); assert(f.reports()->count==0 && f.send().reason==w::ReportReason::InvalidActor);
    assert(f.send(next).accepted()); f.step(1); assert(f.reports()->reports[0].report.reporter==next);
    f.frame.map={2}; f.frame.round={1}; f.frame.timeMicros=0; assert(f.teams.activate(f.frame.map));
    for(const auto& player:f.frame.players) if(player.player.isValid()) {
        assert(f.teams.bind(f.frame.map,player.player)); assert(f.teams.update(f.frame.map,player.player,p::Team::Terrorist));
    }
    f.step(1); assert(f.reports()->count==0);
}
void sameTimeFreshObservation() {
    Fixture f; f.step(0,reporter); assert(f.send().accepted()); f.step(0);
    f.step(0,reporter,false,200); assert(f.send().accepted()); f.step(0);
    assert(f.reports()->count==1 && f.reports()->reports[0].report.position.x==200);
    assert(f.reports()->reports[0].confidence==0.5 && f.reports()->reports[0].report.origin.observedMicros==1000000);
}
int main() { deliveryAndPriority(); validationAndRetirement(); queue(); simultaneousTeamChange(); generationsAndMap(); sameTimeFreshObservation(); }
