// SPDX-License-Identifier: MPL-2.0
#include "nav/local/locomotion_controller.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <vector>
using namespace astrabot;
using namespace astrabot::nav;
namespace {
constexpr local::WalkLimits limits{{64,8,64,16,18,18,64,4,2,0.7},160,1,1,3};
local::Binding binding() { return {{1},{2,{3}},{4},7,0}; }
runtime::MovementSnapshot actor(model::NavVector3 position={50,50,36}) {
    runtime::MovementSnapshot s; const auto b=binding();
    s.agent=b.agent; s.actor=b.actor; s.map=b.map; s.tick={1}; s.elapsedUs=40000;
    s.kind=runtime::ActorKind::ManagedBot;
    s.connected=s.alive=s.joined=s.grounded=true;
    s.position=position; s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}};
    s.speedLimit=250.0f; s.view=model::NavVector3{};
    s.velocity=model::NavVector3{};
    return s;
}
std::vector<route_test::Area> areas() {
    route_test::Area a{1,{{0,0,0},{100,100,0},0,0},{}};
    route_test::Area b{2,{{100,0,0},{200,100,0},0,0},{}};
    route_test::Area c{3,{{200,0,0},{300,100,0},0,0},{}};
    a.targets[1]={2}; b.targets[1]={3};
    return {a,b,c};
}
struct Fixture {
    std::shared_ptr<const query::NavSpatialIndex> index;
    std::shared_ptr<const corridor::Corridor> corridor;
    explicit Fixture(std::uint32_t goal=3) {
        const auto mesh=route_test::snapshot(areas());
        const auto graph=query::NavGraph::build(mesh,{10,20,1000000}); assert(graph);
        const auto spatial=query::NavSpatialIndex::build(mesh,{10,19,1000000}); assert(spatial);
        index=*spatial.value;
        const auto route=query::NavRouteSearch::search(**graph.value,{{1},{goal},{10,1000000},false});
        assert(route);
        const auto built=corridor::Corridor::build(**graph.value,*route.value,{16,16},{10,1000000,20});
        assert(built); corridor=built.value;
    }
};
struct World final : runtime::IWorldQueries {
    bool exhausted{}, wall{};
    std::vector<runtime::QueryRequest> calls;
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        calls.push_back(q);
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind;
        r.error=runtime::QueryError::None;
        if(exhausted) { r.error=runtime::QueryError::BudgetExceeded; return r; }
        if(q.kind==runtime::QueryKind::HullSupport || q.kind==runtime::QueryKind::FloorCandidate) {
            r.floor=runtime::FloorObservation{0,{0,0,1},true,runtime::FloorObservationStatus::Supported};
        } else if(q.kind==runtime::QueryKind::SweptHull || q.kind==runtime::QueryKind::Feeler) {
            r.hull=runtime::HullObservation{1,q.end,{0,0,0},false,false};
            if(wall && q.kind==runtime::QueryKind::SweptHull && q.start.x!=q.end.x) {
                r.hull->fraction=0.5f;
                r.hull->normal={-1,0,0};
            }
        } else {
            // The native ground path must use the new physical query seam.
            assert(false);
            r.error=runtime::QueryError::Unavailable;
        }
        return r;
    }
};
void ordinaryBoundariesKeepMoving() {
    Fixture f; World world;
    local::LocomotionController controller(binding(),f.corridor,{250,50,0},limits);
    assert(controller.native());
    auto s=actor();
    std::uint64_t now=0;
    for(float x : {50.0f,90.0f,110.0f,190.0f,210.0f}) {
        s.position->x=x;
        const auto d=controller.update(s,*f.index,s.map,world,now);
        assert(d.accepted && d.state==local::WalkState::Running);
        assert(d.disposition==local::MotionDisposition::Execute);
        assert(core::Motor::requestedSpeed(d.intent)>0 && d.intent.direction.x>0);
        assert(controller.envelope());
        ++s.tick.value; now+=40000;
    }
    assert(controller.step()==2);
}
void unknownBudgetHoldsWithoutTerminating() {
    Fixture f; World world; world.exhausted=true;
    local::LocomotionController controller(binding(),f.corridor,{250,50,0},limits);
    auto s=actor();
    auto d=controller.update(s,*f.index,s.map,world,0);
    assert(d.accepted && d.state==local::WalkState::Running && !d.terminalEvent);
    assert(d.disposition==local::MotionDisposition::Hold);
    assert(d.probeReason==local::ProbeReason::BudgetExceeded);
    assert(core::Motor::requestedSpeed(d.intent)==0 && !controller.envelope());
    world.exhausted=false; ++s.tick.value;
    d=controller.update(s,*f.index,s.map,world,40000);
    assert(d.state==local::WalkState::Running && d.disposition==local::MotionDisposition::Execute);
}
void changedActorAbortsBeforeQueries() {
    Fixture f; World world;
    local::LocomotionController controller(binding(),f.corridor,{250,50,0},limits);
    auto s=actor(); s.actor={9,{3}};
    const auto d=controller.update(s,*f.index,s.map,world,0);
    assert(d.state==local::WalkState::Aborted && d.reason==local::WalkReason::InvalidActor);
    assert(d.terminalEvent && world.calls.empty() && !controller.envelope());
}
void sameAreaGoalArrives() {
    Fixture f(1); World world;
    local::LocomotionController controller(binding(),f.corridor,{75,50,0},limits);
    auto s=actor({75,50,36});
    const auto d=controller.update(s,*f.index,s.map,world,0);
    assert(d.state==local::WalkState::Arrived && d.terminalEvent);
    assert(core::Motor::requestedSpeed(d.intent)==0 && !controller.envelope());
}
void lateralDriftRequiresFreshPhysicalGuard() {
    Fixture f; World world;
    local::LocomotionController controller(binding(),f.corridor,{250,50,0},limits);
    auto s=actor();
    const auto d=controller.update(s,*f.index,s.map,world,0);
    assert(d.disposition==local::MotionDisposition::Execute && controller.envelope());
    const auto envelope=*controller.envelope();
    ++s.tick.value; s.position->y+=2;
    assert(envelope.matches(s,40000));
    world.calls.clear();
    const model::NavVector3 destination{s.position->x+8,s.position->y,s.position->z};
    auto proof=controller.guard(s,envelope,destination,*f.index,s.map,world,0,{});
    assert(proof && proof.target->origin.y==52);
    assert(!world.calls.empty() && world.calls.front().start.y==52);
    bool freshSweep=false;
    for(const auto& q:world.calls)
        if(q.kind==runtime::QueryKind::SweptHull && q.start.y==52 && q.end.x>q.start.x) freshSweep=true;
    assert(freshSweep);
    world.wall=true; world.calls.clear();
    proof=controller.guard(s,envelope,destination,*f.index,s.map,world,0,{});
    assert(!proof && proof.reason==local::ProbeReason::Blocked);
    // Spatial permission cannot authorize passage through newly observed walls.
    assert(envelope.matches(s,40000) && !world.calls.empty());
}
void blockedForwardProbeAttemptsBothDetourSides() {
    Fixture f; World world; world.wall=true;
    local::LocomotionController controller(binding(),f.corridor,{250,50,0},limits);
    auto s=actor();
    const auto d=controller.update(s,*f.index,s.map,world,0);
    assert(d.state==local::WalkState::Running);
    std::size_t swept=0;
    for(const auto& q:world.calls)
        if(q.kind==runtime::QueryKind::SweptHull && q.start.x!=q.end.x) ++swept;
    // Straight inspection plus preferred and opposite diagonal detours.
    assert(swept>=3);
    assert(!d.avoiding);
}
}
int main() {
    ordinaryBoundariesKeepMoving();
    unknownBudgetHoldsWithoutTerminating();
    changedActorAbortsBeforeQueries();
    sameAreaGoalArrives();
    lateralDriftRequiresFreshPhysicalGuard();
    blockedForwardProbeAttemptsBothDetourSides();
}
