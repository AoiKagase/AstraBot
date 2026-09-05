// SPDX-License-Identifier: MPL-2.0
#include "nav/local/ground_probe.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <limits>
#include <stdexcept>
using namespace astrabot::nav;
namespace {
runtime::MovementSnapshot actor() {
    runtime::MovementSnapshot s; s.agent={1}; s.actor={2,{3}}; s.map={4}; s.tick={5};
    s.kind=runtime::ActorKind::ManagedBot; s.connected=true; s.alive=true; s.joined=true; s.grounded=true;
    s.position=model::NavVector3{20,50,36}; s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}};
    return s;
}
auto index() {
    auto mesh=route_test::snapshot({{1,{{0,0,0},{100,100,0},0,0},{}},
                                   {2,{{0,0,100},{100,100,100},100,100},{}}});
    auto r=query::NavSpatialIndex::build(mesh,{2,3,1000000}); assert(r); return *r.value;
}
constexpr local::GroundProbeLimits limits{9,4,64,16,18,18,64,4,2,0.7};
struct Script final : runtime::IWorldQueries {
    int mode{}; float height{};
    std::vector<runtime::QueryRequest> calls;
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        calls.push_back(q);
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        if(mode==1) ++r.stamp.tick.value;
        if(mode==2) throw std::runtime_error("script");
        if(mode==3) { r.error=runtime::QueryError::Unavailable; return r; }
        const runtime::FloorObservation f{height,{0,0,1},true};
        if(q.kind==runtime::QueryKind::GroundedArea) {
            r.ground=runtime::GroundedAreaObservation{model::NavAreaId{height==100 ? 2U:1U},f};
        } else if(q.kind==runtime::QueryKind::Floor) {
            r.floor=f;
            if(mode==4) r.floor->height=-40;
            if(mode==5) r.floor->supported=false;
            if(mode==6) r.floor->normal.z=0.1f;
            if(mode==7) r.floor->height=std::numeric_limits<float>::quiet_NaN();
            if(mode==8) r.floor->height=10; // support exists but NAV floor disagrees
        } else if(q.kind==runtime::QueryKind::SweptHull) {
            r.hull=runtime::HullObservation{1,q.end,{0,0,0},false};
            if(mode==9) r.hull->fraction=0.5f;
            if(mode==10) r.hull->startSolid=true;
            if(mode==11) r.hull->end.x+=5;
        } else assert(false);
        return r;
    }
};
auto inspect(Script& port, local::GroundProbeLimits l=limits) {
    return local::GroundProbe::inspect(actor(),7,{1},52,50,*index(),{4},port,l);
}
void successReplayAndFloors() {
    Script a,b; auto r=inspect(a); auto replay=inspect(b);
    assert(r && replay && r.queries==5 && r.samples==2 && r.target->origin.z==36);
    assert(r.stamp.actor==actor().actor && r.stamp.routeGeneration==7 && r.stamp.ordinal==0);
    assert(a.calls.size()==b.calls.size());
    for(std::size_t i=0;i<a.calls.size();++i) {
        assert(a.calls[i].stamp==b.calls[i].stamp && a.calls[i].kind==b.calls[i].kind);
        assert(a.calls[i].start==b.calls[i].start && a.calls[i].end==b.calls[i].end);
        assert(a.calls[i].stamp.ordinal==i+1);
    }
    Script upper; upper.height=100; auto s=actor(); s.position->z=136;
    r=local::GroundProbe::inspect(s,7,{2},52,50,*index(),{4},upper,limits);
    assert(r && r.target->area==model::NavAreaId{2} && r.target->origin.z==136);
    Script wrong;
    assert(local::GroundProbe::inspect(s,7,{2},52,50,*index(),{4},wrong,limits).reason==local::ProbeReason::NoSupport);
}
void failures() {
    const local::ProbeReason reasons[]={local::ProbeReason::None,local::ProbeReason::StaleQuery,
        local::ProbeReason::QueryFailed,local::ProbeReason::QueryFailed,local::ProbeReason::UnsafeDrop,
        local::ProbeReason::NoSupport,local::ProbeReason::NoSupport,local::ProbeReason::InvalidResult,
        local::ProbeReason::NoArea,local::ProbeReason::Blocked,local::ProbeReason::Blocked,local::ProbeReason::InvalidResult};
    for(int mode=1;mode<=11;++mode) { Script p; p.mode=mode; const auto r=inspect(p);
        assert(!r && !r.target && r.reason==reasons[mode]); assert(p.calls.size()<=3); }
    for(auto l : {local::GroundProbeLimits{4,4,64,16,18,18,64,4,2,0.7},
                   local::GroundProbeLimits{9,1,64,16,18,18,64,4,2,0.7}}) {
        Script p; auto r=inspect(p,l); assert(r.reason==local::ProbeReason::BudgetExceeded && p.calls.empty());
    }
    Script p; auto l=limits; l.sampleSpacing=0;
    assert(inspect(p,l).reason==local::ProbeReason::InvalidInput && p.calls.empty());
    l=limits; l.maxStepUp=std::numeric_limits<double>::max();
    assert(inspect(p,l).reason==local::ProbeReason::InvalidInput && p.calls.size()==1);
    p.calls.clear();
    auto s=actor(); s.grounded=false;
    assert(local::GroundProbe::inspect(s,7,{1},52,50,*index(),{4},p,limits).reason==local::ProbeReason::NoSupport);
    s=actor(); s.kind=runtime::ActorKind::Human;
    assert(local::GroundProbe::inspect(s,7,{1},52,50,*index(),{4},p,limits).reason==local::ProbeReason::InvalidInput);
    assert(local::GroundProbe::inspect(actor(),7,{1},52,50,*index(),{3},p,limits).reason==local::ProbeReason::StaleNavigation);
    assert(p.calls.empty());
}
}
int main() {
    successReplayAndFloors(); failures();
    Script port; auto s=actor(); auto l=limits; l.maxQueries=1; l.maxSamples=0;
    const auto located=local::GroundProbe::locate(s,7,*index(),s.map,port,l);
    assert(located && located.queries==1 && located.samples==0 && located.target->area==model::NavAreaId{1});
    port.calls.clear(); port.height=100; s.position->z=136;
    const auto upper=local::GroundProbe::locate(s,7,*index(),s.map,port,l);
    assert(upper && upper.target->area==model::NavAreaId{2} && port.calls.size()==1);
    port.calls.clear(); l.maxQueries=0;
    assert(local::GroundProbe::locate(s,7,*index(),s.map,port,l).reason==local::ProbeReason::BudgetExceeded);
    assert(port.calls.empty());
}
