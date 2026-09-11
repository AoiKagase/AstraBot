// SPDX-License-Identifier: MPL-2.0
#include "nav/local/walk.hpp"
#include "nav/local/intent_pump.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace astrabot;
using namespace astrabot::nav;
using namespace astrabot::nav::local;
namespace {
constexpr Binding binding{{1},{2,{3}},{4},5,0};
WalkLimits limits() {
    WalkLimits l{{21,4,48,16,18,18,64,4,2,0.7},120,1,1,3};
    l.jump=WalkJumpLimits{{120,100,180,16,16,5,96,32,4,21,2000000,200000,1500000,200000},{80,1},{21,8,0.1,2,2}};
    return l;
}
runtime::MovementSnapshot actor() {
    runtime::MovementSnapshot s; s.agent=binding.agent; s.actor=binding.actor; s.map=binding.map; s.tick={1};
    s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true; s.ducked=false;
    s.position=model::NavVector3{20,50,36}; s.velocity=model::NavVector3{}; s.view=model::NavVector3{};
    s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f; s.elapsedUs=40000; return s;
}
struct Fixture {
    std::shared_ptr<const query::NavSpatialIndex> index;
    std::shared_ptr<const corridor::Corridor> corridor;
    explicit Fixture(bool multiple=false,std::uint8_t targetAttributes=2) {
        route_test::Area a{1,{{0,0,0},{100,100,0},0,0},{}};
        route_test::Area b{2,{{100,0,0},{200,100,0},0,0},{},2};
        b.attributes=targetAttributes;
        route_test::Area c{3,{{200,0,0},{300,100,0},0,0},{}};
        route_test::Area d{4,{{300,0,0},{400,100,0},0,0},{}};
        a.targets[1]={2}; if(multiple) { b.targets[1]={3}; c.targets[1]={4}; }
        const auto mesh=route_test::snapshot(multiple ? std::vector<route_test::Area>{a,b,c,d}:std::vector<route_test::Area>{a,b});
        const auto g=query::NavGraph::build(mesh,{4,3,1000000}); assert(g);
        const auto r=query::NavRouteSearch::search(**g.value,{{1},{multiple ? 4U:2U},{4,1000000},false}); assert(r);
        const auto p=nav::corridor::Corridor::build(**g.value,*r.value,{16,16},{3,1000000,3}); assert(p); corridor=p.value;
        const auto i=query::NavSpatialIndex::build(mesh,{4,7,1000000}); assert(i); index=*i.value;
    }
};
struct World final : runtime::IWorldQueries {
    const query::NavSpatialIndex& index;
    core::TickId tick{}; unsigned issued{},reserved{},total{}; bool missing{},ceiling{};
    explicit World(const query::NavSpatialIndex& i):index(i) {}
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        if(q.stamp.tick!=tick) { tick=q.stamp.tick; issued=reserved; }
        assert(q.stamp.ordinal==++issued && issued<=21); ++total;
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        const runtime::FloorObservation floor{0,{0,0,1},true};
        if(q.kind==runtime::QueryKind::GroundedArea) {
            const auto area=index.containing({q.start.x,q.start.y,0},0);
            if(area && *area.value && !missing) r.ground=runtime::GroundedAreaObservation{(**area.value).areaId,floor};
        } else if(q.kind==runtime::QueryKind::Floor) r.floor=floor;
        else if(q.kind==runtime::QueryKind::Clearance) r.clearance=runtime::ClearanceObservation{!ceiling};
        else { assert(q.kind==runtime::QueryKind::SweptHull); r.hull=runtime::HullObservation{1,q.end,{},false}; }
        return r;
    }
};
JumpPhysics physics(const Walk& walk,const runtime::MovementSnapshot& s) {
    auto b=binding; b.step=walk.step(); return {b,s.tick,800,268.3281573};
}
void pumpPipeline() {
    for(bool multiple : {false,true}) for(std::uint64_t us : {8000U,16000U,100000U}) {
        Fixture f(multiple); Walk walk(binding,f.corridor,{multiple ? 350.0f:150.0f,50,0},limits());
        IntentPump pump(binding); World world(*f.index); auto s=actor();
        unsigned presses=0,completed=0; double vz=0; bool arrived=false; WalkDecision decision;
        std::optional<JumpDispatch> dispatched; std::optional<std::uint64_t> recoveryAt;
        for(std::uint64_t tick=1;tick<1800;++tick) {
            s.tick={tick}; s.elapsedUs=us;
            if(dispatched) { assert(walk.reportJumpDispatch(*dispatched)); assert(!walk.reportJumpDispatch(*dispatched)); dispatched.reset(); }
            const auto schedule=pump.beginFrame(s); assert(schedule.accepted);
            if(schedule.decisionDue) {
                const auto step=walk.step();
                decision=walk.update(s,*f.index,binding.map,world,pump.timeUs(),0,physics(walk,s));
                if(decision.state==WalkState::Failed || decision.state==WalkState::Aborted)
                    std::fprintf(stderr,"walk jump us=%llu tick=%llu step=%zu reason=%u jump=%u probe=%u x=%.6f\n",
                        static_cast<unsigned long long>(us),static_cast<unsigned long long>(tick),step,unsigned(decision.reason),
                        unsigned(decision.jumpReason),unsigned(decision.jumpProbeReason),s.position->x);
                assert(decision.accepted && decision.state!=WalkState::Failed && decision.state!=WalkState::Aborted && decision.queries<=21);
                if(decision.jumpState && decision.primitiveEvent==PrimitiveEvent::Entered) assert(!decision.jumpPressTick.isValid());
                if(decision.jumpState==JumpState::Recover) {
                    if(!recoveryAt) recoveryAt=pump.timeUs();
                    assert(walk.step()==step);
                }
                if(decision.jumpState==JumpState::Complete) {
                    assert(recoveryAt && pump.timeUs()-*recoveryAt>=limits().jump->motion.cooldownUs);
                    assert(walk.step()==step+1 && decision.primitiveEvent==PrimitiveEvent::Complete);
                    ++completed; recoveryAt.reset();
                }
                if(decision.state==WalkState::Arrived) { arrived=true; break; }
                assert(pump.publish(decision.binding,s.tick,decision.intent));
            }
            const auto output=pump.take(); assert(output.emit);
            const auto motor=core::Motor::command(output.intent,{s.view->x,s.view->y,s.view->z},250,us,output.firstFrame); assert(motor);
            const auto& command=*motor.command;
            if(command.buttons&static_cast<core::ButtonMask>(core::Button::Jump)) {
                assert(output.firstFrame && s.grounded==true && decision.jumpPressTick==s.tick);
                ++presses; s.grounded=false; vz=268.3281573;
                dispatched=JumpDispatch{decision.binding,decision.jumpPressTick,{tick+1},true};
            }
            const double dt=double(us)/1000000,yaw=command.view.yaw*3.14159265358979323846/180;
            const double vx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
            const double vy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw);
            s.position->x+=static_cast<float>(vx*dt); s.position->y+=static_cast<float>(vy*dt);
            s.velocity=model::NavVector3{static_cast<float>(vx),static_cast<float>(vy),static_cast<float>(vz)};
            s.view=model::NavVector3{command.view.pitch,command.view.yaw,command.view.roll};
            if(s.grounded==false) {
                s.position->z+=static_cast<float>(vz*dt-400*dt*dt); vz-=800*dt;
                if(s.position->z<=36) { s.position->z=36; s.grounded=true; vz=0; s.velocity->z=0; }
            }
        }
        assert(arrived && presses==(multiple ? 2U:1U) && completed==presses);
    }
}
WalkDecision launch(Walk& walk,World& world,const Fixture& f,runtime::MovementSnapshot& s) {
    s.position=model::NavVector3{60,50,36}; s.velocity=model::NavVector3{120,0,0};
    WalkDecision d;
    for(std::uint64_t tick=1;tick<=4;++tick) { s.tick={tick}; d=walk.update(s,*f.index,binding.map,world,tick*40000,0,physics(walk,s)); }
    assert(d.intent.jump==ActionRequest::Press && d.jumpPressTick==s.tick); return d;
}
void failuresAndBudgets() {
    for(int mode=0;mode<7;++mode) {
        Fixture f; World world(*f.index); Walk walk(binding,f.corridor,{150,50,0},limits()); auto s=actor();
        const auto d=launch(walk,world,f,s); s.tick={5}; s.grounded=false; s.position->z=45;
        JumpDispatch dispatch{d.binding,d.jumpPressTick,s.tick,true};
        if(mode==1) dispatch.dispatched=false;
        if(mode==2) dispatch.dispatchTick=d.jumpPressTick;
        if(mode==3) ++dispatch.binding.actor.generation.value;
        if(mode!=0) assert(walk.reportJumpDispatch(dispatch)==(mode!=3));
        auto p=physics(walk,s); if(mode==4) p.gravity=600;
        if(mode==5) ++s.actor.generation.value;
        auto failed=walk.update(s,*f.index,binding.map,world,200000,0,p);
        if(mode==6) {
            assert(failed.jumpState==JumpState::Airborne && walk.step()==0);
            ++s.tick.value; s.grounded=true; s.position=model::NavVector3{50,50,36};
            failed=walk.update(s,*f.index,binding.map,world,240000,0,physics(walk,s));
            assert(failed.jumpReason==JumpReason::WrongLanding);
        }
        assert((failed.state==WalkState::Failed || failed.state==WalkState::Aborted) && failed.terminalEvent);
        assert(failed.intent.speed==0 && failed.intent.jump==ActionRequest::Release && walk.step()==0);
        assert(!walk.reportJumpDispatch(dispatch) && !walk.abort().terminalEvent);
    }
    Fixture f; World world(*f.index); Walk walk(binding,f.corridor,{150,50,0},limits()); auto s=actor();
    const auto d=launch(walk,world,f,s); ++s.tick.value; world.reserved=21;
    assert(walk.reportJumpDispatch({d.binding,d.jumpPressTick,s.tick,true}));
    auto waiting=walk.update(s,*f.index,binding.map,world,200000,21,physics(walk,s));
    assert(waiting.jumpState==JumpState::Takeoff && waiting.queries==21 && waiting.intent.speed==0);
    ++s.tick.value; s.grounded=false; s.position->z=45;
    waiting=walk.update(s,*f.index,binding.map,world,240000,21,physics(walk,s));
    assert(waiting.jumpState==JumpState::Airborne && waiting.queries==21);
    assert(walk.abort().intent.jump==ActionRequest::Release && walk.step()==0);
    World exhausted(*f.index); Walk low(binding,f.corridor,{150,50,0},limits()); s=actor();
    assert(low.update(s,*f.index,binding.map,exhausted,40000,0,physics(low,s)).primitiveEvent==PrimitiveEvent::Entered);
    ++s.tick.value; exhausted.reserved=20;
    const auto failure=low.update(s,*f.index,binding.map,exhausted,80000,20,physics(low,s));
    assert(failure.state==WalkState::Failed && failure.jumpProbeReason==JumpProbeReason::BudgetExceeded && failure.queries<=21);
    World none(*f.index); Walk missing(binding,f.corridor,{150,50,0},limits()); s=actor();
    assert(missing.update(s,*f.index,binding.map,none,40000).jumpReason==JumpReason::MissingObservation && none.total==0);
}
void standingToDuckLandingObservations() {
    const runtime::HullDimensions standing{{-16,-16,-36},{16,16,36}},duck{{-16,-16,-18},{16,16,18}};
    for(int mode=0;mode<3;++mode) {
        Fixture f(false,3); World world(*f.index); auto profile=limits();
        profile.crouch={standing,duck,1000000};
        Walk walk(binding,f.corridor,{150,50,0},profile); auto s=actor();
        s.position=model::NavVector3{60,50,36}; s.velocity=model::NavVector3{120,0,0};
        const auto observedPhysics=[&] {
            auto b=binding; b.step=walk.step();
            return JumpPhysics{b,s.tick,800,268.3281573,standing,duck,1.0/3.0};
        };
        WalkDecision d; std::optional<JumpPlan> plan;
        for(std::uint64_t tick=1;tick<=4;++tick) {
            s.tick={tick}; auto p=observedPhysics();
            if(tick==4 && mode==1) p.tick={3};
            if(tick==4 && mode==2) p.crouchingHull->maximum.x=17;
            d=walk.update(s,*f.index,binding.map,world,tick*40000,0,p);
            if(d.jumpPlan) plan=d.jumpPlan;
        }
        assert(plan && plan->flightHull && plan->takeoff.z==36 && plan->landing.z==18);
        if(mode) {
            assert(d.state==WalkState::Failed && d.jumpReason==JumpReason::StaleInspection);
            assert(d.intent.jump!=ActionRequest::Press && d.intent.speed==0 && walk.step()==0);
            continue;
        }
        assert(d.intent.jump==ActionRequest::Press && d.intent.duck!=ActionRequest::Hold);
        assert(walk.reportJumpDispatch({d.binding,d.jumpPressTick,{5},true}));
        s.tick={5}; s.grounded=false; s.position=model::NavVector3{65,50,45}; s.velocity->z=200;
        d=walk.update(s,*f.index,binding.map,world,200000,0,observedPhysics());
        assert(d.jumpState==JumpState::Airborne && d.intent.duck==ActionRequest::Hold);
        assert(walk.step()==0 && !d.support);
        // The next engine observation shrinks the airborne hull without moving
        // its origin; only a later measured grounded feet height proves landing.
        s.tick={6}; s.ducked=true; s.hull=duck;
        d=walk.update(s,*f.index,binding.map,world,240000,0,observedPhysics());
        assert(s.position->z==45 && d.jumpState==JumpState::Airborne && d.intent.duck==ActionRequest::Hold);
        s.tick={7}; s.grounded=true; s.position=plan->landing; s.velocity->z=0;
        d=walk.update(s,*f.index,binding.map,world,280000,0,observedPhysics());
        assert(d.jumpState==JumpState::Recover && d.support && d.support->floor.height==0);
        assert(s.position->z+s.hull->minimum.z==0 && walk.step()==0);
        s.tick={8}; d=walk.update(s,*f.index,binding.map,world,480000,0,observedPhysics());
        assert(d.jumpState==JumpState::Complete && walk.step()==0);
        s.tick={9}; d=walk.update(s,*f.index,binding.map,world,520000,0,observedPhysics());
        assert(d.primitiveEvent==PrimitiveEvent::Complete && walk.step()==1);
        assert(d.support && d.support->area==model::NavAreaId{2} && d.posture==CrouchState::Crouched);
        assert(d.intent.jump!=ActionRequest::Press);
    }
}
void standingBeforeJump() {
    Fixture f;
    for(bool blocked : {false,true}) {
        auto l=limits(); l.crouch={{{-16,-16,-36},{16,16,36}},{{-16,-16,-18},{16,16,18}},1000000};
        Walk walk(binding,f.corridor,{150,50,0},l); World world(*f.index); world.ceiling=blocked;
        auto s=actor(); s.ducked=true; s.position->z=18; s.hull=l.crouch.crouched;
        const auto waiting=walk.update(s,*f.index,binding.map,world,40000,0,physics(walk,s));
        assert(waiting.state==WalkState::Running && waiting.intent.speed==0 && waiting.queries==1);
        assert(waiting.intent.duck==(blocked ? ActionRequest::Hold:ActionRequest::Release));
        assert(waiting.primitiveEvent==PrimitiveEvent::None && waiting.intent.jump!=ActionRequest::Press);
        ++s.tick.value;
        if(!blocked) { s.ducked=false; s.position->z=36; s.hull=l.crouch.standing; }
        const auto next=walk.update(s,*f.index,binding.map,world,blocked ? 1040000:80000,0,physics(walk,s));
        if(blocked) assert(next.state==WalkState::Failed && next.reason==WalkReason::PostureFailed && next.intent.duck==ActionRequest::Hold);
        else assert(next.primitiveEvent==PrimitiveEvent::Entered && next.posture==CrouchState::Standing && next.intent.duck==ActionRequest::None);
    }
}
}
void microJumpGeometryAndWorldProof() {
    route_test::Area a{5,{{0,0,0},{100,100,0},0,0},{}};
    route_test::Area b{1665,{{100,37.5F,0},{125,62.5F,0},0,0},{},2};
    route_test::Area c{9,{{125,0,0},{225,100,0},0,0},{}};
    a.targets[1]={1665}; b.targets[1]={9};
    const auto mesh=route_test::snapshot({a,b,c});
    const auto graph=query::NavGraph::build(mesh,{3,2,1000000}); assert(graph);
    const auto route=query::NavRouteSearch::search(**graph.value,{{5},{9},{3,1000000},false}); assert(route);
    const auto path=corridor::Corridor::build(**graph.value,*route.value,{16,16},{2,1000000,2},
        corridor::PortalPolicy::AllowMicroTransit); assert(path);
    const auto spatial=query::NavSpatialIndex::build(mesh,{3,5,1000000}); assert(spatial);
    auto s=actor(); const auto l=limits();
    const auto geometry=JumpGeometry::derive(*path.value,binding,s,l.jump->motion,l.jump->geometry);
    assert(geometry && query::containsXY(b.extent,geometry.plan->landing));
    // A NAV hint and a centre that fits are still not clearance evidence.
    World blocked(**spatial.value); blocked.ceiling=true;
    Walk walk(binding,path.value,{175,50,0},l);
    const auto d=walk.update(s,**spatial.value,binding.map,blocked,40000,0,physics(walk,s));
    assert(d.intent.jump!=ActionRequest::Press && walk.step()==0);
    assert(d.jumpPlan && d.jumpGeometryReason==JumpGeometryReason::None);
}
void observedObstacleReusesRunningWalkPrimitive() {
    Fixture fixture(false,0);
    struct ObstacleWorld final : runtime::IWorldQueries {
        World clear;
        bool blocked{};
        unsigned classifications{};
        explicit ObstacleWorld(const query::NavSpatialIndex& index):clear(index) {}
        runtime::WorldQueryResult query(const runtime::QueryRequest& request) override {
            auto wire=request;
            if(wire.kind==runtime::QueryKind::Blocker) wire.kind=runtime::QueryKind::SweptHull;
            auto result=clear.query(wire);
            result.kind=request.kind;
            if(request.kind==runtime::QueryKind::Blocker) {
                ++classifications;
                result.hull.reset();
                result.blocker=runtime::BlockerObservation{1,runtime::BlockerKind::Geometry,{}};
            } else if(blocked && request.kind==runtime::QueryKind::SweptHull &&
                      request.end.x>request.start.x && request.end.y==request.start.y &&
                      request.end.z==request.start.z) {
                auto contact=request.start;
                contact.x+=(request.end.x-request.start.x)*0.5f;
                result.hull=runtime::HullObservation{0.5f,contact,{-1,0,0},false};
            }
            return result;
        }
    } world(*fixture.index);
    auto profile=limits();
    profile.sideProbeDistance=16; profile.narrowMargin=1; profile.narrowSpeed=60;
    profile.maxAvoidanceDecisions=3;
    Walk walk(binding,fixture.corridor,{150,50,0},profile);
    auto s=actor();
    auto d=walk.update(s,*fixture.index,binding.map,world,40000,0,physics(walk,s));
    assert(d.primitiveEvent==PrimitiveEvent::Entered && !d.jumpPlan);
    world.blocked=true;
    for(std::uint64_t tick=2;tick<=6 && world.classifications==0;++tick) {
        s.tick={tick};
        d=walk.update(s,*fixture.index,binding.map,world,tick*40000,0,physics(walk,s));
        assert(d.state==WalkState::Running && d.intent.jump!=ActionRequest::Press);
    }
    assert(world.classifications==1 && !d.jumpPlan && walk.step()==0);
    world.blocked=false;
    ++s.tick.value;
    d=walk.update(s,*fixture.index,binding.map,world,s.tick.value*40000,0,physics(walk,s));
    assert(d.state==WalkState::Running && d.jumpPlan && d.jumpState==JumpState::Approach);
    assert(d.jumpReason==JumpReason::None && d.jumpGeometryReason==JumpGeometryReason::None);
    assert(d.primitiveEvent!=PrimitiveEvent::Entered && walk.step()==0);
}
int main() { pumpPipeline(); failuresAndBudgets(); standingBeforeJump(); standingToDuckLandingObservations(); microJumpGeometryAndWorldProof(); observedObstacleReusesRunningWalkPrimitive(); }
