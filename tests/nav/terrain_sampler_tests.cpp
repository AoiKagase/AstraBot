// SPDX-License-Identifier: MPL-2.0
#include "nav/local/terrain_sampler.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <vector>
using namespace astrabot::nav;
namespace {
runtime::MovementSnapshot actor() {
    runtime::MovementSnapshot s;
    s.agent={1}; s.actor={2,{3}}; s.map={4}; s.tick={5};
    s.kind=runtime::ActorKind::ManagedBot;
    s.connected=true; s.alive=true; s.joined=true; s.grounded=true;
    s.position=model::NavVector3{20,50,36};
    s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}};
    return s;
}
auto index() {
    auto mesh=route_test::snapshot({{1,{{0,0,0},{100,100,0},0,0},{}}});
    auto result=query::NavSpatialIndex::build(mesh,{1,1,1000000});
    assert(result);
    return *result.value;
}
constexpr local::GroundProbeLimits limits{32,4,64,16,18,18,64,4,20,0.7};
struct World final : runtime::IWorldQueries {
    bool stairs{}, candidateSolid{}, ceiling{}, penetration{}, unavailable{}, steep{};
    std::vector<runtime::QueryRequest> calls;
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        calls.push_back(q);
        runtime::WorldQueryResult r;
        r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        if(unavailable) { r.error=runtime::QueryError::BudgetExceeded; return r; }
        if(q.kind==runtime::QueryKind::FloorCandidate || q.kind==runtime::QueryKind::HullSupport) {
            const float height=stairs ? (q.start.x>40 ? 16.0f:(q.start.x>25 ? 8.0f:0.0f)):0.0f;
            r.floor=runtime::FloorObservation{height,{0,0,1},true,runtime::FloorObservationStatus::Supported};
            if(steep) r.floor->normal={0.8f,0,0.6f};
            if(candidateSolid && q.kind==runtime::QueryKind::FloorCandidate) {
                r.floor->supported=false; r.floor->status=runtime::FloorObservationStatus::StartSolid;
            }
            if(penetration && q.kind==runtime::QueryKind::HullSupport)
                r.floor->status=runtime::FloorObservationStatus::StartSolid;
        } else {
            assert(q.kind==runtime::QueryKind::SweptHull);
            r.hull=runtime::HullObservation{1,q.end,{0,0,0},false,false};
            // A riser obstructs direct diagonal travel, but lifted travel fits.
            if(stairs && q.end.x>q.start.x && q.end.z!=q.start.z) r.hull->fraction=0.5f;
            // Physical head clearance blocks the upward leg.
            if(ceiling && q.end.z>q.start.z && q.start.x==q.end.x) r.hull->fraction=0.25f;
        }
        return r;
    }
};
void stairCandidateRescue() {
    World world; world.stairs=true; world.candidateSolid=true;
    const auto r=local::TerrainSampler::inspect(actor(),7,{1},52,50,*index(),{4},world,limits);
    assert(r && r.samples==2 && r.steps==2 && r.target->origin.z==52);
    assert(r.supportFallbackAccepted && r.lastStep);
    assert(r.lastStep->lifted.z==62 && r.lastStep->landing.floor.height==16);
}
void directBlockedStillStepsAndCeilingRejects() {
    World world; world.stairs=true;
    auto r=local::TerrainSampler::inspect(actor(),7,{1},52,50,*index(),{4},world,limits);
    assert(r && r.steps==2);
    World low; low.stairs=true; low.ceiling=true; low.candidateSolid=true;
    r=local::TerrainSampler::inspect(actor(),7,{1},52,50,*index(),{4},low,limits);
    assert(!r && r.reason==local::ProbeReason::Blocked);
    World solid; solid.penetration=true;
    r=local::TerrainSampler::locate(actor(),7,*index(),{4},solid,limits);
    assert(!r && r.reason==local::ProbeReason::StartSolid);
}
void descentGroundFlagTransition() {
    auto s=actor(); s.grounded=false; s.position->z=44;
    World world;
    auto r=local::TerrainSampler::locate(s,7,*index(),{4},world,limits);
    assert(r && r.target->origin.z==36 && world.calls.size()==2);
    assert(world.calls.back().kind==runtime::QueryKind::SweptHull);
    assert(world.calls.back().start.z==44 && world.calls.back().end.z==36);
    s.position->z=60;
    r=local::TerrainSampler::locate(s,7,*index(),{4},world,limits);
    assert(!r && r.reason==local::ProbeReason::FloorHeightMismatch);
}
void budgetAndNormalRemainDistinct() {
    World world; world.stairs=true; world.candidateSolid=true;
    auto l=limits; l.maxQueries=5;
    auto r=local::TerrainSampler::inspect(actor(),7,{1},52,50,*index(),{4},world,l);
    assert(!r && r.reason==local::ProbeReason::BudgetExceeded && r.queries==5);
    World denied; denied.unavailable=true;
    r=local::TerrainSampler::locate(actor(),7,*index(),{4},denied,limits);
    assert(!r && r.reason==local::ProbeReason::BudgetExceeded);
    World steep; steep.steep=true;
    r=local::TerrainSampler::locate(actor(),7,*index(),{4},steep,limits);
    assert(!r && r.reason==local::ProbeReason::UnsupportedFloor);
}
void boundedNavBoundaryFallback() {
    World world;
    auto s=actor(); s.position->x=110;
    auto r=local::TerrainSampler::locate(s,7,*index(),{4},world,limits);
    assert(r && r.target->area==model::NavAreaId{1});
    assert(r.target->origin.x==110 && r.target->floor.height==0);
    // NAV projection never moves the physical destination or bypasses clearance.
    assert(world.calls.size()==2 && world.calls.back().kind==runtime::QueryKind::SweptHull);
    s.position->x=120;
    r=local::TerrainSampler::locate(s,7,*index(),{4},world,limits);
    assert(!r && r.reason==local::ProbeReason::NavContainmentMissing);

    auto mesh=route_test::snapshot({{1,{{0,0,0},{30,100,0},0,0},{}},
                                   {2,{{40,0,0},{100,100,0},0,0},{}}});
    auto gaps=query::NavSpatialIndex::build(mesh,{2,3,1000000}); assert(gaps);
    // A nearest area across a gap cannot silently change inspect's source area.
    r=local::TerrainSampler::inspect(actor(),7,{1},37,50,**gaps.value,{4},world,limits);
    assert(!r && r.reason==local::ProbeReason::NavContainmentMissing);

    mesh=route_test::snapshot({{1,{{0,0,10},{100,100,10},10,10},{}}});
    auto raised=query::NavSpatialIndex::build(mesh,{1,1,1000000}); assert(raised);
    auto strict=limits; strict.navTolerance=2;
    r=local::TerrainSampler::locate(actor(),7,**raised.value,{4},world,strict);
    // XY containment with wrong height is not the boundary fallback case.
    assert(!r && r.reason==local::ProbeReason::NavContainmentMissing);
}
}
int main() {
    stairCandidateRescue();
    directBlockedStillStepsAndCeilingRejects();
    descentGroundFlagTransition();
    budgetAndNormalRemainDistinct();
    boundedNavBoundaryFallback();
}
