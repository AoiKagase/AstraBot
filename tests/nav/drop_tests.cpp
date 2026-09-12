// SPDX-License-Identifier: MPL-2.0
#include "nav/local/walk.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <cmath>
using namespace astrabot;
using namespace astrabot::nav;
using namespace astrabot::nav::local;
namespace {
constexpr Binding binding{{1},{2,{3}},{4},5,0};
struct Fixture {
    std::shared_ptr<const query::NavSpatialIndex> index;
    std::shared_ptr<const corridor::Corridor> path;
    explicit Fixture(bool south=false,bool narrowTarget=false) {
        route_test::Area a{1,{{0,0,0},{100,100,0},0,0},{}};
        route_test::Area b{2,{{125,0,-75},{325,100,-75},-75,-75},{}};
        if(south) b.extent={{0,125,-75},{100,325,-75},-75,-75};
        if(narrowTarget) { b.extent.northWest.y=37.5F; b.extent.southEast.y=62.5F; }
        a.targets[south ? 2:1]={2};
        const auto mesh=route_test::snapshot({a,b});
        const auto graph=query::NavGraph::build(mesh,{2,1,1000000}); assert(graph);
        const auto route=query::NavRouteSearch::search(**graph.value,{{1},{2},{2,1000000},false}); assert(route);
        const auto built=corridor::Corridor::build(**graph.value,*route.value,{16,16},{1,1000000,1},
            corridor::PortalPolicy::AllowMicroTransit); assert(built); path=built.value;
        assert(path->transitions()[0].effectiveTraversal==model::NavTraversalKind::Drop);
        assert(path->transitions()[0].edge.traversal==model::NavTraversalKind::Walk);
        assert(path->transitions()[0].edge.direction==(south ? 2:1));
        const auto spatial=query::NavSpatialIndex::build(mesh,{2,3,1000000}); assert(spatial); index=*spatial.value;
    }
};
WalkLimits limits() {
    WalkLimits l{{21,4,48,16,18,18,128,4,2,0.7},120,1,1,1}; l.drop=DropLimits{}; return l;
}
runtime::MovementSnapshot actor() {
    runtime::MovementSnapshot s;
    s.agent=binding.agent; s.actor=binding.actor; s.map=binding.map; s.tick={1};
    s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true;
    s.ducked=false; s.position=model::NavVector3{96,50,36};
    s.velocity=model::NavVector3{static_cast<float>(28/std::sqrt(150.0/800)),0,0};
    s.view=model::NavVector3{}; s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}};
    s.speedLimit=250; s.elapsedUs=40000; return s;
}
struct World final : runtime::IWorldQueries {
    const query::NavSpatialIndex& index;
    bool stale{},blocked{},missing{},microSource{},missingSource{};
    core::TickId tick{}; std::uint32_t issued{};
    explicit World(const query::NavSpatialIndex& i):index(i) {}
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        if(tick!=q.stamp.tick) { tick=q.stamp.tick; issued=0; }
        assert(q.stamp.ordinal==++issued && issued<=21);
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        if(stale) r.stamp.tick={99};
        const float z=microSource ? (q.start.y>=-1400 ? -74.0F:0.0F) : (q.start.x>=125 ? -75.0F:0.0F);
        runtime::FloorObservation floor{z,{0,0,1},!missing && !(missingSource && q.start.y<-1400)};
        if(q.kind==runtime::QueryKind::GroundedArea) {
            const auto area=index.containing({q.start.x,q.start.y,z},2);
            if(area && *area.value) r.ground=runtime::GroundedAreaObservation{(**area.value).areaId,floor};
        } else if(q.kind==runtime::QueryKind::Floor) r.floor=floor;
        else if(q.kind==runtime::QueryKind::Clearance) r.clearance=runtime::ClearanceObservation{!blocked};
        else { assert(q.kind==runtime::QueryKind::SweptHull); r.hull=runtime::HullObservation{1,q.end,{},false}; }
        return r;
    }
};
JumpPhysics physics(const runtime::MovementSnapshot& s) { return {binding,s.tick,800,0}; }
void observedLifecycle() {
    Fixture f; World world(*f.index); Walk walk(binding,f.path,{175,50,-75},limits()); auto s=actor();
    const auto start=walk.update(s,*f.index,binding.map,world,40000,0,physics(s));
    assert(start.state==WalkState::Running && start.dropState==DropState::StepOff);
    assert(start.dropPlan && start.dropPlan->gap==25 && start.dropPlan->fall==75 && start.jumpPhysics);
    assert(core::Motor::requestedSpeed(start.intent)>0 && start.intent.jump==ActionRequest::None && walk.step()==0);
    ++s.tick.value; s.grounded=false; s.elapsedUs=200000;
    s.position=model::NavVector3{117+s.velocity->x*0.2F,50,20}; s.velocity->z=-160;
    const auto air=walk.update(s,*f.index,binding.map,world,240000,0,physics(s));
    assert(air.state==WalkState::Running && air.dropState==DropState::Airborne);
    assert(air.intent.speed==0 && !air.support && walk.step()==0);
    ++s.tick.value; s.grounded=true; s.position=model::NavVector3{145,50,-39}; s.velocity->z=0;
    const auto landed=walk.update(s,*f.index,binding.map,world,440000,0,physics(s));
    assert(landed.state==WalkState::Running && landed.dropState==DropState::Landed);
    assert(landed.support && landed.support->area==model::NavAreaId{2} && walk.step()==1);
}
void narrowTargetRequiresPhysicalSupport() {
    Fixture f(false,true);
    assert(f.path->transitions()[0].targetFit==corridor::AreaFit::MicroTransit);
    for(int mode=0;mode<3;++mode) {
        auto s=actor(); World world(*f.index);
        world.blocked=mode==1; world.missing=mode==2;
        Walk walk(binding,f.path,{175,50,-75},limits());
        const auto d=walk.update(s,*f.index,binding.map,world,40000,0,physics(s));
        if(mode==0) {
            assert(d.state==WalkState::Running && d.dropState==DropState::StepOff);
            assert(d.dropPlan && d.dropPlan->target==model::NavAreaId{2});
            assert(d.dropPlan->landing.y>=37.5F && d.dropPlan->landing.y<=62.5F);
        } else {
            assert(d.state==WalkState::Failed && d.dropState==DropState::Failed);
            assert(d.intent.speed==0 && walk.step()==0);
        }
    }
}
void rejectsUnprovenMotion() {
    Fixture f;
    for(int mode=0;mode<7;++mode) {
        auto l=limits(); auto s=actor(); auto p=physics(s); World world(*f.index);
        if(mode==0) l.drop.reset();
        if(mode==1) p.tick={99};
        if(mode==2) world.stale=true;
        if(mode==3) world.blocked=true;
        if(mode==4) world.missing=true;
        if(mode==5) l.drop->maximumFall=64;
        if(mode==6) l.drop->maximumGap=24;
        Walk walk(binding,f.path,{175,50,-75},l);
        const auto d=walk.update(s,*f.index,binding.map,world,40000,0,p);
        assert(d.state==WalkState::Failed && d.dropState==DropState::Failed);
        assert(d.dropReason!=DropReason::None && d.intent.speed==0 && walk.step()==0);
    }
}
void exactMicroSourceDrop() {
    // Recorded 2036 -> 141 shape: 75x25 source, 100x125 destination,
    // directed south gap 25 and fall 74. NAV bounds are not physical walls.
    route_test::Area a{2036,{{-200,-1450,0},{-125,-1425,0},0,0},{}};
    route_test::Area b{141,{{-200,-1400,-74},{-100,-1275,-74},-74,-74},{}};
    a.targets[2]={141};
    const auto mesh=route_test::snapshot({a,b});
    const auto graph=query::NavGraph::build(mesh,{2,1,1000000}); assert(graph);
    const auto route=query::NavRouteSearch::search(**graph.value,{{2036},{141},{2,1000000},false}); assert(route);
    const auto built=corridor::Corridor::build(**graph.value,*route.value,{16,16},{1,1000000,1},
        corridor::PortalPolicy::AllowMicroTransit); assert(built);
    const auto& transition=built.value->transitions()[0];
    assert(transition.sourceFit==corridor::AreaFit::MicroTransit);
    assert(transition.targetFit==corridor::AreaFit::HullSafe);
    assert(transition.effectiveTraversal==model::NavTraversalKind::Drop);
    const auto spatial=query::NavSpatialIndex::build(mesh,{2,3,1000000}); assert(spatial);
    for(bool unsupported : {false,true}) {
        World world(**spatial.value); world.microSource=true; world.missingSource=unsupported;
        auto s=actor(); s.position=model::NavVector3{-162.5F,-1429,36};
        s.velocity=model::NavVector3{0,static_cast<float>(28/std::sqrt(148.0/800)),0};
        Walk walk(binding,built.value,{-150,-1350,-74},limits());
        const auto d=walk.update(s,**spatial.value,binding.map,world,40000,0,physics(s));
        assert(d.dropPlan && d.dropPlan->fall==74 && d.dropPlan->gap==25);
        if(unsupported) {
            assert(d.state==WalkState::Failed && d.dropReason==DropReason::MissingSupport);
            assert(d.intent.speed==0 && walk.step()==0);
        } else {
            assert(d.state==WalkState::Running && d.dropState==DropState::StepOff);
            assert(d.support && d.support->area==model::NavAreaId{2036});
            assert(core::Motor::requestedSpeed(d.intent)>0);
        }
    }
}
void landingRequiresObservedFlight() {
    Fixture f; World world(*f.index); Walk walk(binding,f.path,{175,50,-75},limits()); auto s=actor();
    const auto step=walk.update(s,*f.index,binding.map,world,40000,0,physics(s));
    assert(step.dropState==DropState::StepOff);
    ++s.tick.value; s.position=model::NavVector3{145,50,-39};
    const auto teleported=walk.update(s,*f.index,binding.map,world,80000,0,physics(s));
    assert(teleported.state==WalkState::Failed && teleported.dropReason==DropReason::WrongLanding);
    assert(walk.step()==0);
}
}
int main() {
    Fixture south(true); assert(south.path->transitions()[0].edge.direction==2);
    observedLifecycle(); rejectsUnprovenMotion(); landingRequiresObservedFlight(); exactMicroSourceDrop(); narrowTargetRequiresPhysicalSupport();
}
