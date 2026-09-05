// SPDX-License-Identifier: MPL-2.0
#include "nav/local/jump_probe.hpp"
#include "route_fixture.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace astrabot;
using namespace astrabot::nav;
using namespace astrabot::nav::local;
namespace {
constexpr Binding binding{{1},{2,{3}},{4},5,6};
const JumpPlan plan{{1},{2},{50,50,36},{124,50,36},0,2};
const JumpLimits motion{120,100,180,16,16,5,96,32,4,21,2000000,200000,1500000,200000};
const JumpProbeLimits limits{21,8,0.1,2,2};
runtime::MovementSnapshot actor() {
    runtime::MovementSnapshot s; s.agent=binding.agent; s.actor=binding.actor; s.map=binding.map; s.tick={10};
    s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true; s.ducked=false;
    s.position=plan.takeoff; s.velocity=model::NavVector3{120,0,0}; s.view=model::NavVector3{};
    s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f; return s;
}
auto index(float height=0) {
    const auto r=query::NavSpatialIndex::build(route_test::snapshot({
        {1,{{0,0,0},{99,200,0},0,0},{}},{2,{{100,0,height},{200,200,height},height,height},{}}}),{2,3,1000000});
    assert(r); return *r.value;
}
struct Box { model::NavVector3 minimum{},maximum{}; };
// Independent Minkowski-box/slab collision fixture, not canned clear replies.
bool intersects(const runtime::QueryRequest& q,Box box) {
    double lo=0,hi=1;
    const double a[]{q.start.x,q.start.y,q.start.z}, b[]{q.end.x,q.end.y,q.end.z};
    const double mn[]{double(box.minimum.x)-q.hull->maximum.x,double(box.minimum.y)-q.hull->maximum.y,double(box.minimum.z)-q.hull->maximum.z};
    const double mx[]{double(box.maximum.x)-q.hull->minimum.x,double(box.maximum.y)-q.hull->minimum.y,double(box.maximum.z)-q.hull->minimum.z};
    for(unsigned axis=0;axis<3;++axis) {
        const double d=b[axis]-a[axis];
        if(d==0) { if(a[axis]<=mn[axis] || a[axis]>=mx[axis]) return false; }
        else {
            const double t1=(mn[axis]-a[axis])/d,t2=(mx[axis]-a[axis])/d;
            lo=(std::max)(lo,(std::min)(t1,t2)); hi=(std::min)(hi,(std::max)(t1,t2));
            if(lo>hi) return false;
        }
    }
    return lo<hi && lo<1 && hi>0; // Surface-only contact is not penetration.
}
struct World final : runtime::IWorldQueries {
    std::vector<runtime::QueryRequest> calls;
    std::vector<Box> obstacles;
    unsigned faultAt{}; int fault{};
    float targetHeight{};
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        calls.push_back(q); assert(q.stamp.ordinal==calls.size());
        assert(q.hull && q.hull->minimum==actor().hull->minimum && q.hull->maximum==actor().hull->maximum);
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        if(q.kind==runtime::QueryKind::GroundedArea) {
            r.ground=runtime::GroundedAreaObservation{model::NavAreaId{q.start.x<100 ? 1U:2U},
                runtime::FloorObservation{q.start.x<100 ? 0:targetHeight,{0,0,1},true}};
        } else {
            assert(q.kind==runtime::QueryKind::SweptHull);
            r.hull=runtime::HullObservation{1,q.end,{},false};
            for(const auto box : obstacles) if(intersects(q,box)) r.hull->fraction=0;
        }
        if(q.stamp.ordinal==faultAt) {
            switch(fault) {
            case 1: ++r.stamp.tick.value; break;
            case 2: r.kind=runtime::QueryKind::Floor; break;
            case 3: r.error=runtime::QueryError::Unavailable; break;
            case 4: r.error=runtime::QueryError::BudgetExceeded; break;
            case 5: throw std::runtime_error("query failure");
            case 6: r.ground.reset(); break;
            case 7: r.ground->floor->normal.z=2; break;
            case 8: r.ground->area={9}; break;
            case 9: r.ground->floor->supported=false; break;
            case 10: r.ground->floor->height=1; break;
            case 11: r.hull.reset(); break;
            case 12: r.hull->startSolid=true; break;
            case 13: r.hull->fraction=std::numeric_limits<float>::quiet_NaN(); break;
            case 14: r.hull->end.y++; break;
            default: assert(false);
            }
        }
        return r;
    }
};
JumpPhysics physics() { return {binding,actor().tick,800,268.3281573}; }
auto launch(World& world,JumpProbeLimits allowance=limits) {
    return JumpProbe::launch(actor(),binding,plan,motion,physics(),allowance,*index(),binding.map,world);
}
void clearAndEnvelope() {
    World world,replay; const auto r=launch(world),again=launch(replay);
    assert(r && again && r.inspection->step==binding.step && r.inspection->velocity==actor().velocity);
    assert(r.queries==19 && r.segments==7 && world.calls.size()==19 && r.touchdown && r.flightSeconds>0.67 && r.flightSeconds<0.68);
    assert(r.inspection->queries==r.queries && r.inspection->flightClear==true);
    for(std::size_t i=0;i<world.calls.size();++i) {
        assert(world.calls[i].stamp==replay.calls[i].stamp && world.calls[i].start==replay.calls[i].start && world.calls[i].end==replay.calls[i].end);
    }
    // Independently sample each original parabola; both top and bottom of its
    // moving hull must lie in the union of the two overlapping swept boxes.
    for(unsigned segment=0;segment<r.segments;++segment) {
        const auto& low=world.calls[5+segment*2]; const auto& high=world.calls[6+segment*2];
        for(unsigned point=0;point<=100;++point) {
            const double f=double(point)/100,t=r.flightSeconds*(segment+f)/r.segments;
            const double z=36+268.3281573*t-400*t*t;
            const double lowZ=low.start.z+(double(low.end.z)-low.start.z)*f;
            const double highZ=high.start.z+(double(high.end.z)-high.start.z)*f;
            assert(lowZ-1e-4<=z && highZ+1e-4>=z && highZ-lowZ<72);
            const double x=50+120*t, sweepX=low.start.x+(double(low.end.x)-low.start.x)*f;
            assert(std::abs(x-sweepX)<1e-4);
        }
    }
    auto exact=limits; exact.maxQueries=19; World full; assert(launch(full,exact));
    auto eight=limits; eight.maxSegmentSeconds=0.09; World maximum; const auto bounded=launch(maximum,eight);
    assert(bounded && bounded.queries==21 && bounded.segments==8);
    exact.maxQueries=18; World over; const auto exhausted=launch(over,exact);
    assert(!exhausted && exhausted.reason==JumpProbeReason::BudgetExceeded && over.calls.size()==3);
    World obstacle; obstacle.obstacles.push_back({{80,20,0},{90,80,18}}); assert(launch(obstacle));
    World wall; wall.obstacles.push_back({{80,20,0},{90,80,80}});
    assert(launch(wall).reason==JumpProbeReason::Blocked);
    World ceiling; ceiling.obstacles.push_back({{60,20,115},{120,80,130}});
    assert(launch(ceiling).reason==JumpProbeReason::Blocked);
}
void supportedTransitions() {
    for(float height : {16.0f,32.0f}) {
        World world; world.targetHeight=height;
        world.obstacles.push_back({{100,0,-100},{200,200,height}});
        auto jump=plan; jump.landing.z+=height;
        const auto r=JumpProbe::launch(actor(),binding,jump,motion,physics(),limits,*index(height),binding.map,world);
        assert(r && r.touchdown && r.touchdown->z==36+height && r.flightSeconds<0.67);
    }
    World rotated; auto s=actor(); s.velocity=model::NavVector3{84.8528137f,84.8528137f,0}; s.view->y=45;
    auto jump=plan; jump.landing={104,104,36};
    assert(JumpProbe::launch(s,binding,jump,motion,physics(),limits,*index(),binding.map,rotated));
}
void failures() {
    const JumpProbeReason errors[]{JumpProbeReason::None,JumpProbeReason::StaleQuery,JumpProbeReason::StaleQuery,
        JumpProbeReason::QueryFailed,JumpProbeReason::BudgetExceeded,JumpProbeReason::QueryFailed,
        JumpProbeReason::NoSupport,JumpProbeReason::InvalidResult,JumpProbeReason::WrongArea,
        JumpProbeReason::NoSupport,JumpProbeReason::CannotLand,JumpProbeReason::InvalidResult,
        JumpProbeReason::Blocked,JumpProbeReason::InvalidResult,JumpProbeReason::InvalidResult};
    for(int fault=1;fault<=14;++fault) {
        World world; world.fault=fault; world.faultAt=fault<=9 ? 1U:(fault==10 ? 3U:6U);
        const auto r=launch(world); assert(!r && !r.inspection && r.reason==errors[fault]);
        assert(world.calls.size()==world.faultAt && r.queries==world.faultAt);
    }
    for(unsigned ordinal=1;ordinal<=19;++ordinal) {
        World world; world.fault=1; world.faultAt=ordinal;
        assert(launch(world).reason==JumpProbeReason::StaleQuery && world.calls.size()==ordinal);
    }
    for(int mode=0;mode<14;++mode) {
        World world; auto s=actor(); auto p=physics(); auto l=limits; auto jump=plan; auto map=binding.map;
        if(mode==0) ++p.binding.step;
        if(mode==1) ++p.tick.value;
        if(mode==2) p.gravity=0;
        if(mode==3) p.verticalImpulse=std::numeric_limits<double>::infinity();
        if(mode==4) s.velocity->x=99;
        if(mode==5) s.velocity->y=11;
        if(mode==6) s.velocity->z=1;
        if(mode==7) s.view->y=6;
        if(mode==8) s.position->x=10;
        if(mode==9) jump.sourceAttributes=8;
        if(mode==10) ++map.value;
        if(mode==11) l.maxQueries=22;
        if(mode==12) l.maxSegments=9;
        if(mode==13) s.grounded=false;
        assert(!JumpProbe::launch(s,binding,jump,motion,p,l,*index(),map,world) && world.calls.empty());
    }
    auto p=physics(); p.verticalImpulse=400; World far;
    assert(JumpProbe::launch(actor(),binding,plan,motion,p,limits,*index(),binding.map,far).reason==JumpProbeReason::CannotLand);
    auto l=limits; l.maxSegments=6; World segments;
    assert(launch(segments,l).reason==JumpProbeReason::BudgetExceeded);
    l=limits; l.maxChordRise=0.01; World chord;
    assert(launch(chord,l).reason==JumpProbeReason::BudgetExceeded);
}
void controllerConsumesRealQueries() {
    SimpleJump jump(binding,plan,motion); auto s=actor();
    for(unsigned tick=10;tick<13;++tick) {
        s.tick={tick}; auto p=physics(); p.tick=s.tick; World world;
        const auto probe=JumpProbe::launch(s,binding,plan,motion,p,limits,*index(),binding.map,world); assert(probe);
        JumpFeedback f{binding,s,std::uint64_t(tick)*40000,probe.inspection,{}};
        const auto d=jump.update(f); assert(d.accepted);
        assert((d.intent.jump==ActionRequest::Press)==(tick==12));
    }
    for(bool changedStep : {false,true}) {
        SimpleJump other(binding,plan,motion); World world; auto proof=launch(world).inspection;
        if(changedStep) ++proof->step; else proof->velocity->x++;
        const auto d=other.update({binding,actor(),40000,proof,{}});
        assert(d.reason==JumpReason::StaleInspection && d.intent.jump!=ActionRequest::Press);
    }
}
}
int main() { clearAndEnvelope(); supportedTransitions(); failures(); controllerConsumesRealQueries(); }
