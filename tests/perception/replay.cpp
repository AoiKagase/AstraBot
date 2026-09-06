// SPDX-License-Identifier: MPL-2.0
#include "evidence.hpp"
#include "route_fixture.hpp"
#include <memory>
namespace e=perception_evidence;
namespace p=astrabot::core::perception;
using astrabot::core::PlayerId;
struct Replay {
    std::unique_ptr<e::w::WorldModel> world=std::make_unique<e::w::WorldModel>();
    std::unique_ptr<e::q::DistributionModel> distribution=std::make_unique<e::q::DistributionModel>();
    e::w::MemoryFrame f{}; p::TeamRoster teams{}; std::array<bool,32> eligible{};
    std::shared_ptr<const e::q::DistributionTopology> nav{};
    e::Row row; PlayerId target{32,{1}};
    Replay(unsigned actors,unsigned us,bool hasNav):row{"timeline",actors,us,hasNav,{}} {
        f.map={1}; f.round={1}; f.timeMicros=1000000; assert(teams.activate(f.map));
        for(std::uint16_t i=1;i<=actors;++i) {
            f.players[i-1]={{i,{1}},{i},true}; eligible[i-1]=true;
            assert(teams.bind(f.map,{i,{1}})); assert(teams.update(f.map,{i,{1}},p::Team::Terrorist));
        }
        f.players[31]={target,{},true}; eligible[31]=true; assert(teams.bind(f.map,target));
        assert(teams.update(f.map,target,p::Team::CounterTerrorist));
        if(hasNav) {
            std::vector<route_test::Area> areas;
            for(unsigned i=1;i<=1000;++i) {
                const auto x=static_cast<float>((i-1)*200);
                areas.push_back({i,{{x,0,0},{x+150,100,0},0,0}});
                if(i>1) areas[0].targets[1].push_back(i);
            }
            auto mesh=route_test::snapshot(areas); auto graph=e::q::NavGraph::build(mesh,{1000,4000,1000000});
            auto index=e::q::NavSpatialIndex::build(mesh,{1000,1999,1000000}); assert(graph && index);
            nav=e::q::DistributionTopology::build(f.map,*graph.value,*index.value); assert(nav);
        }
    }
    p::ObservationBatch batch(PlayerId viewer,double x) {
        p::ObservationBatch b{}; b.stamp={{viewer.slot},viewer,f.map,f.tick,f.timeMicros,f.round};
        b.identity={f.map,f.round,p::ObservationSource::Vision,f.tick.value,f.timeMicros,f.timeMicros};
        b.count=1; b.observations[0]={target,{x,10,64}}; return b;
    }
    void step(std::uint64_t delta,bool see=false,double x=100,bool sound=false) {
        f.timeMicros+=delta; ++f.tick.value; assert(world->advance(f,teams));
        for(std::uint16_t slot=1;slot<=row.actors;++slot) {
            if(see && slot!=2) assert(world->stage(batch({slot,{1}},x)));
            if(sound) assert(world->stage({slot,{1}},{{f.map,f.round,p::ObservationSource::Sound,f.tick.value,f.timeMicros-delta,f.timeMicros},p::SoundKind::Footstep,{0,0,0}}));
        }
        assert(world->publish()); distribution->update(*world,nav);
    }
    void record(const char* phase) {
        for(std::uint16_t slot=1;slot<=row.actors;++slot) row.sample(phase,*world,*distribution,{slot,{1}},target);
    }
    void advanceTo(std::uint64_t elapsed) {
        const auto end=1000000+elapsed;
        while(f.timeMicros<end) {
            step((std::min)(static_cast<std::uint64_t>(row.frameUs),end-f.timeMicros)); record("decay");
        }
    }
    e::Row run() {
        step(0,true); record("sight"); const auto old=batch({1,{1}},100);
        step(100000); record("occluded");
        step(100000,false,100,true); record("sound");
        const auto sent=world->requestReport({1,{1}},target,f.timeMicros,eligible,teams);
        assert(sent.accepted()==(row.actors>1));
        step(100000); record("sound_report");
        // Dense 999-edge topology is advanced fairly under a 256/frame cap.
        for(unsigned i=0;i<80;++i) { step(1000); record("fairness"); }
        if(nav) for(std::uint16_t slot=1;slot<=row.actors;++slot) if(slot!=2) {
            const auto* d=world->latest({slot,{1}})->distributions[0]; assert(d && d->updatedMicros>=1200000);
        }
        advanceTo(2500000); record("half"); advanceTo(5000000); record("expired");
        step(row.frameUs,true,210); record("resee");
        ++f.round.value; step(row.frameUs); record("round");
        ++target.generation.value; f.players[31].player=target; step(row.frameUs);
        // Old observations cannot resurrect after a round and slot reuse.
        ++f.tick.value; ++f.timeMicros; assert(world->advance(f,teams)); assert(world->stage(old));
        assert(world->publish()); distribution->update(*world,nav); record("reuse");
        ++f.map.value; f.round={1}; f.timeMicros=0; f.tick={0}; nav.reset(); step(0); record("map");
        assert(teams.activate(f.map));
        for(std::uint16_t slot=1;slot<=row.actors;++slot) {
            assert(teams.bind(f.map,{slot,{1}})); assert(teams.update(f.map,{slot,{1}},p::Team::Terrorist));
        }
        for(unsigned cycle=0;cycle<64;++cycle) {
            assert(teams.bind(f.map,target)); assert(teams.update(f.map,target,p::Team::CounterTerrorist));
            step(100000,true,100,true);
            const auto queued=world->requestReport({1,{1}},target,f.timeMicros,eligible,teams);
            assert(queued.accepted()==(row.actors>1));
            ++f.round.value; ++target.generation.value; f.players[31].player=target;
            step(row.frameUs); record("soak");
        }
        return row;
    }
};
int main(int argc,char** argv) {
    assert(argc==2); std::vector<e::Row> rows;
    for(unsigned actors:{1U,8U,16U}) for(unsigned us:{8000U,16000U,100000U}) for(bool nav:{false,true})
        rows.push_back(Replay(actors,us,nav).run());
    e::writeEvidence(argv[1],"portable",rows);
}
