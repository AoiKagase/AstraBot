// SPDX-License-Identifier: MPL-2.0
#include "nav/query/distribution.hpp"
#include "route_fixture.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <iomanip>
namespace q=astrabot::nav::query;
namespace w=astrabot::core::world;
namespace p=astrabot::core::perception;
using astrabot::core::PlayerId;
constexpr PlayerId observer{1,{1}}, target{32,{1}};
route_test::Area area(std::uint32_t id) {
    const auto x=static_cast<float>((id-1)*200);
    return {id,{{x,0,0},{x+100,100,0},0,0},{},0};
}
std::shared_ptr<const q::DistributionTopology> topology(const std::vector<route_test::Area>& areas,unsigned map=1) {
    auto mesh=route_test::snapshot(areas);
    auto graph=q::NavGraph::build(mesh,{1000,4000,1000000});
    auto index=q::NavSpatialIndex::build(mesh,{1000,1999,1000000}); assert(graph && index);
    auto result=q::DistributionTopology::build({map},*graph.value,*index.value); assert(result); return result;
}
struct Fixture {
    std::unique_ptr<w::WorldModel> world=std::make_unique<w::WorldModel>();
    std::unique_ptr<q::DistributionModel> model=std::make_unique<q::DistributionModel>();
    w::MemoryFrame frame{};
    std::shared_ptr<const q::DistributionTopology> nav{};
    explicit Fixture(std::shared_ptr<const q::DistributionTopology> t,unsigned observers=1):nav(std::move(t)) {
        frame.map={1}; frame.round={1}; frame.tick={1}; frame.timeMicros=1000000;
        for(std::uint16_t i=1;i<=observers;++i) frame.players[i-1]={{i,{1}},{i},true};
        frame.players[31]={target,{},true};
    }
    void step(std::uint64_t dt,bool see=false,double x=10) {
        ++frame.tick.value; frame.timeMicros+=dt; assert(world->advance(frame));
        if(see) for(const auto& player:frame.players) if(player.agent.isValid()) {
            p::ObservationBatch batch{}; batch.stamp={player.agent,player.player,frame.map,frame.tick,frame.timeMicros,frame.round};
            batch.identity={frame.map,frame.round,p::ObservationSource::Vision,frame.tick.value,frame.timeMicros,frame.timeMicros};
            batch.count=1; batch.observations[0]={target,{x,10,64}}; assert(world->stage(batch));
        }
        assert(world->publish()); model->update(*world,nav);
        assert(model->diagnostics().frameConnections<=256 && model->diagnostics().frameMappings<=32);
        assert(model->diagnostics().frameVisits<=2048);
    }
    const w::PositionDistribution* distribution(PlayerId actor=observer) const {
        const auto s=world->latest(actor); assert(s && s->visual->count==1); return s->distributions[0];
    }
};
double weight(const w::PositionDistribution& d,unsigned id) {
    double sum=d.unknownMass; for(std::size_t i=0;i<d.count;++i) sum+=d.areas[i].weight;
    assert(std::abs(sum-1)<1e-12);
    for(std::size_t i=0;i<d.count;++i) if(d.areas[i].area==id) return d.areas[i].weight;
    return 0;
}
void branchAndCycle() {
    auto a=area(1),b=area(2),c=area(3); a.targets[1]={3,2};
    Fixture f(topology({c,a,b})); f.step(0,true); assert(f.distribution()->available && weight(*f.distribution(),1)==1);
    f.step(199999); assert(weight(*f.distribution(),1)==1); f.step(1);
    assert(weight(*f.distribution(),1)==0.5 && weight(*f.distribution(),2)==0.25 && weight(*f.distribution(),3)==0.25);
    f.step(200000); assert(weight(*f.distribution(),1)==0.25 && weight(*f.distribution(),2)==0.375);
    f.step(1,true,210); assert(weight(*f.distribution(),2)==1 && f.distribution()->count==1);
    f.step(600000); assert(weight(*f.distribution(),2)==1); // isolated area and no reverse edge
    b.targets[3]={1}; a.targets[1]={2}; f.nav=topology({b,a});
    f.step(0,true); f.step(400000); assert(weight(*f.distribution(),1)==0.5 && weight(*f.distribution(),2)==0.5);
}
void truncationAndFairness() {
    std::vector<route_test::Area> areas; for(unsigned i=1;i<=1000;++i) areas.push_back(area(i));
    for(unsigned i=2;i<=1000;++i) areas[0].targets[1].push_back(i);
    Fixture f(topology(areas),16); f.step(0,true); f.step(200000);
    assert(f.model->diagnostics().frameConnections==256);
    assert(f.model->diagnostics().maxDelayMicros>=200000);
    for(std::uint16_t i=1;i<=16;++i) assert(f.distribution({i,{1}})->updatedMicros==1000000);
    for(unsigned i=0;i<70;++i) f.step(1000);
    for(std::uint16_t i=1;i<=16;++i) {
        const auto* d=f.distribution({i,{1}}); assert(d && d->count==32 && d->updatedMicros==1200000);
        assert(weight(*d,1)==0.5 && d->areas[31].area==32 && d->unknownMass>0.48);
        assert(d->delayMicros==70000);
    }
    assert(f.model->diagnostics().pending==0);
    std::cout << "DistributionModel bytes: " << sizeof(q::DistributionModel) << '\n';
}
std::string replay(bool reverse) {
    auto a=area(1),b=area(2),c=area(3); a.targets[1]=reverse ? std::vector<std::uint32_t>{3,2}:std::vector<std::uint32_t>{2,3};
    b.targets[0]={1}; c.targets[0]={2};
    Fixture f(topology(reverse ? std::vector<route_test::Area>{c,b,a}:std::vector<route_test::Area>{a,b,c}));
    f.step(0,true); std::ostringstream out; out<<std::setprecision(17);
    for(unsigned i=0;i<20;++i) {
        f.step(200000); const auto* d=f.distribution();
        out<<d->count<<':'<<d->updatedMicros<<':'<<d->unknownMass<<':'<<d->delayMicros;
        for(std::size_t j=0;j<d->count;++j) out<<':'<<d->areas[j].area<<':'<<d->areas[j].weight;
        out<<'\n';
    }
    return out.str();
}
void pendingRetirement() {
    std::vector<route_test::Area> areas; for(unsigned i=1;i<=1000;++i) areas.push_back(area(i));
    for(unsigned i=2;i<=1000;++i) areas[0].targets[1].push_back(i);
    Fixture f(topology(areas)); f.step(0,true); f.step(200000);
    assert(f.model->diagnostics().pending==1 && f.distribution()->count==1);
    f.frame.players[31].eligible=false; f.step(1);
    assert(f.model->diagnostics().pending==0 && f.model->diagnostics().expiredJobs==1);
    assert(f.world->latest(observer)->visual->count==0);
    f.frame.players[31].eligible=true; f.step(1); assert(f.world->latest(observer)->visual->count==0);
    f.step(1,true); auto bad=f.frame; --bad.timeMicros; ++bad.tick.value;
    assert(!f.world->advance(bad)); f.model->update(*f.world,f.nav); assert(!f.world->latest(observer));
    f.step(1); assert(f.world->latest(observer)->visual->count==0);
}
void retirement() {
    Fixture f({}); f.step(0,true); assert(!f.distribution());
    f.nav=topology({area(1)}); f.step(1); assert(f.distribution()->available);
    auto invalid=*f.distribution(); const auto identity=f.world->latest(observer)->visual->memories[0].identity;
    invalid.areas[0].weight=0.5; assert(!f.world->setDistribution(observer,target,identity,invalid));
    invalid=*f.distribution(); invalid.updatedMicros=f.frame.timeMicros+1;
    assert(!f.world->setDistribution(observer,target,identity,invalid));
    const auto revision=f.distribution()->navRevision;
    f.nav=topology({area(2)}); f.step(1); assert(!f.distribution()->available && f.distribution()->navRevision!=revision);
    assert(f.world->latest(observer)->visual->memories[0].lastKnownPosition.x==10);
    f.nav=topology({area(1)}); f.step(1); assert(f.distribution()->available);
    f.frame.round={2}; f.step(1); assert(f.world->latest(observer)->visual->count==0);
    f.step(0,true); f.step(5000000); assert(f.world->latest(observer)->visual->count==0);
    f.frame.map={2}; f.frame.round={1}; f.frame.timeMicros=0; f.step(1,true); assert(!f.distribution());
    f.nav=topology({area(1)},2); f.step(1); assert(f.distribution()->available);
    f.frame.players[0].player.generation={2}; f.step(1); assert(!f.world->latest(observer));
}
int main() { branchAndCycle(); truncationAndFairness(); assert(replay(false)==replay(true)); pendingRetirement(); retirement(); }
