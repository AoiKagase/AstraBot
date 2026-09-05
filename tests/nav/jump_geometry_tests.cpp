// SPDX-License-Identifier: MPL-2.0
#include "nav/local/jump_geometry.hpp"
#include "nav/local/jump_probe.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace astrabot;
using namespace astrabot::nav;
using namespace astrabot::nav::local;
namespace {
constexpr Binding binding{{1},{2,{3}},{4},5,0};
const JumpLimits motion{120,100,180,16,16,5,96,32,4,21,2000000,200000,1500000,200000};
const GroundProbeLimits groundLimits{21,4,48,16,18,18,64,4,2,0.7};
runtime::MovementSnapshot actor() {
    runtime::MovementSnapshot s; s.agent=binding.agent; s.actor=binding.actor; s.map=binding.map; s.tick={1};
    s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true; s.ducked=false;
    s.position=model::NavVector3{50,50,36}; s.velocity=model::NavVector3{}; s.view=model::NavVector3{};
    s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f; return s;
}
struct Fixture {
    std::shared_ptr<const corridor::Corridor> path;
    std::shared_ptr<const query::NavSpatialIndex> index;
};
Fixture fixture(std::uint8_t direction=1,float height=0,float width=100,std::uint8_t attributes=2) {
    const float xs[]{0,100,0,-100},ys[]{-100,0,100,0};
    route_test::Area a{1,{{0,0,0},{100,width,0},0,0},{}};
    route_test::Area b{2,{{xs[direction],ys[direction],height},{xs[direction]+100,ys[direction]+width,height},height,height},{},attributes};
    a.targets[direction]={2}; b.targets[(direction+2U)%4U]={1};
    const auto mesh=route_test::snapshot({a,b}); const auto graph=query::NavGraph::build(mesh,{2,2,1000000}); assert(graph);
    const auto route=query::NavRouteSearch::search(**graph.value,{{1},{2},{2,1000000},false}); assert(route);
    const auto path=corridor::Corridor::build(**graph.value,*route.value,{16,16},{1,1000000,2}); assert(path);
    const auto index=query::NavSpatialIndex::build(mesh,{2,3,1000000}); assert(index);
    return {path.value,*index.value};
}
void geometry() {
    for(std::uint8_t direction=0;direction<4;++direction) {
        const auto f=fixture(direction); const auto r=JumpGeometry::derive(*f.path,binding,actor(),motion,{80,1});
        assert(r && r.plan->source==model::NavAreaId{1} && r.plan->target==model::NavAreaId{2});
        assert(r.plan->sourceAttributes==0 && r.plan->targetAttributes==2 && r.plan->takeoff.z==36 && r.plan->landing.z==36);
        const double dx=double(r.plan->landing.x)-r.plan->takeoff.x,dy=double(r.plan->landing.y)-r.plan->takeoff.y;
        assert(std::hypot(dx,dy)==80);
        assert((direction==0 && dy<0) || (direction==1 && dx>0) || (direction==2 && dy>0) || (direction==3 && dx<0));
        const auto source=f.index->containing({r.plan->takeoff.x,r.plan->takeoff.y,0},0);
        const auto target=f.index->containing({r.plan->landing.x,r.plan->landing.y,0},0);
        assert(source && *source.value && (**source.value).areaId==model::NavAreaId{1});
        assert(target && *target.value && (**target.value).areaId==model::NavAreaId{2});
    }
    const auto f=fixture(); auto s=actor(); s.position->y=1;
    auto r=JumpGeometry::derive(*f.path,binding,s,motion,{80,1}); assert(r && r.plan->takeoff.y==33 && r.plan->landing.y==33);
    s.position->y=99; r=JumpGeometry::derive(*f.path,binding,s,motion,{80,1}); assert(r && r.plan->takeoff.y==67);
    const auto upper=fixture(1,32); r=JumpGeometry::derive(*upper.path,binding,actor(),motion,{80,1});
    assert(r && r.plan->landing.z-r.plan->takeoff.z==32);
    const auto high=fixture(1,33); assert(JumpGeometry::derive(*high.path,binding,actor(),motion,{80,1}).reason==JumpGeometryReason::HeightUnsupported);
    const auto down=fixture(1,-1); assert(JumpGeometry::derive(*down.path,binding,actor(),motion,{80,1}).reason==JumpGeometryReason::HeightUnsupported);
    const auto narrow=fixture(1,0,65); assert(JumpGeometry::derive(*narrow.path,binding,actor(),motion,{80,1}).reason==JumpGeometryReason::NoRoom);
    const std::uint8_t unsupportedHints[]{0,1,4,8,10,16};
    for(const auto hint : unsupportedHints) {
        const auto unsupported=fixture(1,0,100,hint);
        assert(JumpGeometry::derive(*unsupported.path,binding,actor(),motion,{80,1}).reason==JumpGeometryReason::UnsupportedTransition);
    }
    auto wrong=binding; wrong.step=1;
    assert(JumpGeometry::derive(*f.path,wrong,actor(),motion,{80,1}).reason==JumpGeometryReason::InvalidStep);
    ++wrong.actor.generation.value;
    assert(JumpGeometry::derive(*f.path,wrong,actor(),motion,{80,1}).reason==JumpGeometryReason::InvalidActor);
    assert(!JumpGeometry::derive(*f.path,binding,actor(),motion,{97,1}));
    assert(!JumpGeometry::derive(*f.path,binding,actor(),motion,{80,0}));
    auto enormous=motion; enormous.takeoffRadius=std::numeric_limits<double>::infinity();
    assert(!JumpGeometry::derive(*f.path,binding,actor(),enormous,{80,1}));
}
void selectedStepAndExternal() {
    auto a=route_test::Area{1,{{0,0,0},{100,100,0},0,0},{}};
    auto b=route_test::Area{2,{{100,0,0},{200,100,0},0,0},{}};
    auto c=route_test::Area{3,{{200,0,0},{300,100,0},0,0},{},2};
    a.targets[1]={2}; b.targets[1]={3};
    const auto mesh=route_test::snapshot({a,b,c}); const auto graph=query::NavGraph::build(mesh,{3,2,1000000}); assert(graph);
    const auto route=query::NavRouteSearch::search(**graph.value,{{1},{3},{3,1000000},false}); assert(route);
    const auto path=corridor::Corridor::build(**graph.value,*route.value,{16,16},{2,1000000,2}); assert(path);
    auto selected=binding; selected.step=1; auto s=actor(); s.position->x=150;
    const auto r=JumpGeometry::derive(*path.value,selected,s,motion,{80,1});
    assert(r && r.plan->source==model::NavAreaId{2} && r.plan->target==model::NavAreaId{3});
    assert(r.plan->takeoff.x==160 && r.plan->landing.x==240);
    enrichment::NavMapFingerprint fingerprint{}; fingerprint[0]=1;
    enrichment::NavTraversalLinkSet links{fingerprint,{
        {1,2,3,{1},{2},{50,50,0},{150,50,0},model::NavTraversalKind::Jump,enrichment::NavLinkDirection::Forward,0}}};
    a.targets[1].clear(); b.targets[1].clear();
    const auto external=query::NavGraph::compose(route_test::snapshot({a,b}),fingerprint,links,{2,1,1000000},{1,1000000}); assert(external);
    const auto er=query::NavRouteSearch::search(**external.value,{{1},{2},{2,1000000},false}); assert(er);
    const auto ep=corridor::Corridor::build(**external.value,*er.value,{16,16},{1,1000000,1}); assert(ep);
    assert(JumpGeometry::derive(*ep.value,binding,actor(),motion,{80,1}).reason==JumpGeometryReason::UnsupportedTransition);
}
struct World final : runtime::IWorldQueries {
    const query::NavSpatialIndex& index;
    std::vector<runtime::QueryRequest> calls;
    unsigned staleOrdinal{}; bool blocked{},missing{};
    explicit World(const query::NavSpatialIndex& i):index(i) {}
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        calls.push_back(q); assert(q.stamp.ordinal==calls.size());
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        if(q.stamp.ordinal==staleOrdinal) --r.stamp.ordinal; // Must not match the helper's lower ordinal.
        if(q.kind==runtime::QueryKind::GroundedArea || q.kind==runtime::QueryKind::Floor) {
            const auto match=index.containing({q.start.x,q.start.y,0},64);
            if(match && *match.value && !missing) {
                const auto& p=**match.value;
                const runtime::FloorObservation floor{static_cast<float>(p.projectedPoint.z),{0,0,1},true};
                if(q.kind==runtime::QueryKind::Floor) r.floor=floor;
                else r.ground=runtime::GroundedAreaObservation{p.areaId,floor};
            }
        } else {
            assert(q.kind==runtime::QueryKind::SweptHull);
            r.hull=runtime::HullObservation{blocked ? 0.5f:1.0f,q.end,{},false};
        }
        return r;
    }
};
void preparationAndLanding() {
    const auto f=fixture(); const auto p=JumpGeometry::derive(*f.path,binding,actor(),motion,{80,1}); assert(p);
    auto s=actor(); s.position->x=20; World world(*f.index);
    auto proof=JumpProbe::prepare(s,binding,*p.plan,motion,groundLimits,*f.index,binding.map,world);
    assert(proof && proof.queries==8 && proof.inspection->approach->origin.x==60);
    assert(!proof.inspection->flightClear && !proof.inspection->velocity);
    s.position=p.plan->takeoff; world.calls.clear();
    proof=JumpProbe::prepare(s,binding,*p.plan,motion,groundLimits,*f.index,binding.map,world);
    assert(proof && proof.queries==4 && proof.inspection->approach->origin.x>s.position->x);
    for(unsigned ordinal=1;ordinal<=4;++ordinal) {
        world.calls.clear(); world.staleOrdinal=ordinal;
        proof=JumpProbe::prepare(s,binding,*p.plan,motion,groundLimits,*f.index,binding.map,world);
        assert(!proof && proof.reason==JumpProbeReason::StaleQuery && world.calls.size()==ordinal);
    }
    world.staleOrdinal=0; world.calls.clear(); world.blocked=true;
    assert(JumpProbe::prepare(s,binding,*p.plan,motion,groundLimits,*f.index,binding.map,world).reason==JumpProbeReason::Blocked);
    world.blocked=false; world.calls.clear(); auto low=groundLimits; low.maxQueries=3;
    assert(JumpProbe::prepare(s,binding,*p.plan,motion,low,*f.index,binding.map,world).reason==JumpProbeReason::BudgetExceeded);
    assert(world.calls.size()==1);
    world.calls.clear(); low=groundLimits; low.maxDistance=1;
    assert(JumpProbe::prepare(s,binding,*p.plan,motion,low,*f.index,binding.map,world).reason==JumpProbeReason::BudgetExceeded);
    world.calls.clear(); auto bad=*p.plan; bad.takeoff={140,50,36}; bad.landing={190,50,36}; s.position->x=90;
    const auto crossing=JumpProbe::prepare(s,binding,bad,motion,groundLimits,*f.index,binding.map,world);
    assert(!crossing && crossing.reason==JumpProbeReason::NoSupport && !crossing.inspection);
    s.position=p.plan->landing; world.calls.clear();
    proof=JumpProbe::land(s,binding,*p.plan,motion,groundLimits,*f.index,binding.map,world);
    assert(proof && proof.queries==1 && proof.inspection->support->area==p.plan->target && !proof.inspection->flightClear);
    world.calls.clear(); world.missing=true;
    assert(JumpProbe::land(s,binding,*p.plan,motion,groundLimits,*f.index,binding.map,world).reason==JumpProbeReason::NoSupport);
    world.missing=false; world.calls.clear(); s.position=p.plan->takeoff;
    assert(JumpProbe::land(s,binding,*p.plan,motion,groundLimits,*f.index,binding.map,world).reason==JumpProbeReason::WrongArea);
}
void pipeline() {
    for(std::uint8_t direction=0;direction<4;++direction) {
    const auto f=fixture(direction);
    for(std::uint64_t us : {8000U,16000U,100000U}) {
        auto s=actor();
        if(direction==0) s.position->y=80;
        if(direction==1) s.position->x=20;
        if(direction==2) s.position->y=20;
        if(direction==3) s.position->x=80;
        const auto geometry=JumpGeometry::derive(*f.path,binding,s,motion,{80,1}); assert(geometry);
        const auto plan=*geometry.plan; SimpleJump jump(binding,plan,motion);
        unsigned presses=0; bool complete=false; double vz=0;
        std::optional<JumpDispatch> dispatch;
        for(std::uint64_t tick=1;tick<700;++tick) {
            s.tick={tick}; s.elapsedUs=us; World world(*f.index); JumpFeedback feedback{binding,s,tick*us,{},dispatch}; dispatch.reset();
            if(s.grounded==true) {
                JumpProbeResult proof;
                if(jump.state()==JumpState::Airborne || jump.state()==JumpState::Recover)
                    proof=JumpProbe::land(s,binding,plan,motion,groundLimits,*f.index,binding.map,world);
                else if(jump.state()==JumpState::Accelerate && std::hypot(s.velocity->x,s.velocity->y)>=motion.minimumSpeed)
                    proof=JumpProbe::launch(s,binding,plan,motion,{binding,s.tick,800,268.3281573},{21,8,0.1,2,2},*f.index,binding.map,world);
                else proof=JumpProbe::prepare(s,binding,plan,motion,groundLimits,*f.index,binding.map,world);
                if(!proof) std::fprintf(stderr,"probe dt=%llu tick=%llu reason=%u x=%.6f\n",
                    static_cast<unsigned long long>(us),static_cast<unsigned long long>(tick),unsigned(proof.reason),s.position->x);
                assert(proof && world.calls.size()<=21); feedback.inspection=proof.inspection;
            }
            const auto d=jump.update(feedback);
            assert(d.state!=JumpState::Failed && d.state!=JumpState::Aborted);
            if(d.state==JumpState::Complete) { assert(d.terminalEvent && presses==1); complete=true; break; }
            const auto cmd=core::Motor::command(d.intent,{s.view->x,s.view->y,s.view->z},250,us,true); assert(cmd);
            if(cmd.command->buttons&static_cast<core::ButtonMask>(core::Button::Jump)) {
                assert(++presses==1); s.grounded=false; vz=268.3281573;
                dispatch=JumpDispatch{binding,d.pressTick,{tick+1},true};
            }
            const double dt=double(us)/1000000,yaw=cmd.command->view.yaw*3.14159265358979323846/180;
            const double vx=cmd.command->movement.forward*std::cos(yaw)+cmd.command->movement.side*std::sin(yaw);
            const double vy=cmd.command->movement.forward*std::sin(yaw)-cmd.command->movement.side*std::cos(yaw);
            s.position->x+=static_cast<float>(vx*dt); s.position->y+=static_cast<float>(vy*dt);
            s.velocity=model::NavVector3{static_cast<float>(vx),static_cast<float>(vy),static_cast<float>(vz)};
            s.view=model::NavVector3{cmd.command->view.pitch,cmd.command->view.yaw,cmd.command->view.roll};
            if(s.grounded==false) {
                s.position->z+=static_cast<float>(vz*dt-400*dt*dt); vz-=800*dt;
                if(s.position->z<=36) { s.position->z=36; s.grounded=true; vz=0; s.velocity->z=0; }
            }
        }
        assert(complete);
    }
    }
}
}
int main() { geometry(); selectedStepAndExternal(); preparationAndLanding(); pipeline(); }
