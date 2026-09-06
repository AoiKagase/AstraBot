// SPDX-License-Identifier: MPL-2.0
#include "nav/local/walk.hpp"
#include "nav/local/intent_pump.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace astrabot;
using namespace astrabot::nav;
namespace {
constexpr local::WalkLimits limits{{9,4,48,16,18,18,64,4,2,0.7},160,1,1,3};
local::Binding binding() { return {{1},{2,{3}},{4},7,0}; }
runtime::MovementSnapshot actor(model::NavVector3 position={50,50,36}) {
    runtime::MovementSnapshot s; const auto b=binding();
    s.agent=b.agent; s.actor=b.actor; s.map=b.map; s.tick={1}; s.elapsedUs=40000;
    s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true;
    s.position=position; s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}};
    s.speedLimit=250.0f; s.view=model::NavVector3{}; return s;
}
route_test::Area square(std::uint32_t id, float x, float y) {
    return {id,{{x,y,0},{x+100,y+100,0},0,0},{}};
}
std::vector<route_test::Area> zigzag() {
    auto a=square(1,0,0), b=square(2,100,0), c=square(3,100,100), d=square(4,200,100);
    a.targets[1]={2}; b.targets[2]={3}; c.targets[1]={4};
    return {a,b,c,d};
}
struct Fixture {
    std::shared_ptr<const model::NavMeshSnapshot> mesh;
    std::shared_ptr<const query::NavGraph> graph;
    std::shared_ptr<const query::NavSpatialIndex> index;
    std::shared_ptr<const corridor::Corridor> corridor;
    Fixture(const std::vector<route_test::Area>& areas, std::uint32_t start, std::uint32_t goal) {
        mesh=route_test::snapshot(areas);
        auto g=query::NavGraph::build(mesh,{10,20,1000000}); assert(g); graph=*g.value;
        auto i=query::NavSpatialIndex::build(mesh,{10,19,1000000}); assert(i); index=*i.value;
        auto route=query::NavRouteSearch::search(*graph,{{start},{goal},{10,1000000},false}); assert(route);
        auto c=corridor::Corridor::build(*graph,*route.value,{16,16},{10,1000000,20}); assert(c); corridor=c.value;
    }
};
struct World final : runtime::IWorldQueries {
    const std::vector<route_test::Area>& areas;
    int mode{};
    std::vector<runtime::QueryRequest> calls;
    explicit World(const std::vector<route_test::Area>& a) : areas(a) {}
    std::optional<model::NavAreaId> area(model::NavVector3 p) const {
        for(const auto& a:areas) if(p.x>=a.extent.northWest.x && p.x<=a.extent.southEast.x &&
                                   p.y>=a.extent.northWest.y && p.y<=a.extent.southEast.y)
            return model::NavAreaId{a.id};
        return {};
    }
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        assert(q.stamp.ordinal==(calls.empty() || calls.back().stamp.tick!=q.stamp.tick ?
            1:calls.back().stamp.ordinal+1));
        calls.push_back(q);
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        if(mode==5) ++r.stamp.routeGeneration;
        if(mode==6) throw std::runtime_error("world unavailable");
        const runtime::FloorObservation floor{0,{0,0,1},true};
        if(q.kind==runtime::QueryKind::GroundedArea) {
            r.ground=runtime::GroundedAreaObservation{area(q.start),floor};
            if(mode==2) r.ground->floor->supported=false;
        } else if(q.kind==runtime::QueryKind::Floor) {
            r.floor=floor;
            if(!area(q.start) || mode==3) r.floor->supported=false;
            if(mode==4) r.floor->height=-32;
        } else if(q.kind==runtime::QueryKind::SweptHull) {
            r.hull=runtime::HullObservation{mode==1 ? 0.5f:1,q.end,{0,0,0},false};
        } else if(q.kind==runtime::QueryKind::Clearance) {
            r.clearance=runtime::ClearanceObservation{true};
        } else assert(false);
        return r;
    }
};
struct Trace { std::size_t step; local::WalkState state; local::PrimitiveEvent event; model::NavVector3 position; };
struct DoorWorld final : runtime::IWorldQueries {
    World world;
    bool open{}, stale{}, missingFloor{}, throws{}, touch{};
    std::uint64_t id{42};
    explicit DoorWorld(const std::vector<route_test::Area>& areas):world(areas) {}
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        if(q.kind==runtime::QueryKind::Door) {
            assert(!world.calls.empty() && q.stamp.ordinal==world.calls.back().stamp.ordinal+1);
            world.calls.push_back(q);
            if(throws) throw std::runtime_error("door unavailable");
            runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
            r.door=runtime::DoorObservation{id,open,true,model::NavVector3{0,0,0}};
            if(touch) {
                r.door->canUse=false; r.door->canTouch=true; r.door->useView.reset();
                r.hull=runtime::HullObservation{(83.96875f-q.start.x)/(q.end.x-q.start.x),
                    {83.96875f,q.start.y,q.start.z},{-1,0,0},false};
            }
            if(stale) ++r.stamp.ordinal;
            return r;
        }
        auto r=world.query(q);
        if(!open && q.end.x>=84 && q.kind==runtime::QueryKind::SweptHull)
            r.hull=runtime::HullObservation{0.5f,q.end,{-1,0,0},false};
        if(!open && missingFloor && q.start.x>=84 && q.kind==runtime::QueryKind::Floor) r.floor.reset();
        return r;
    }
};
void doors() {
    auto areas=zigzag(); Fixture f(areas,1,2);
    for(int mode=0;mode<8;++mode) {
        DoorWorld world(areas); auto s=actor(); auto profile=limits; profile.doorTimeoutUs=100000;
        world.missingFloor=mode==1; world.stale=mode==4; world.throws=mode==5;
        if(mode==6) profile.probe.maxQueries=7;
        local::Walk walk(binding(),f.corridor,{150,50,0},profile);
        assert(walk.update(s,*f.index,s.map,world,0).state==local::WalkState::Running);
        ++s.tick.value;
        auto d=walk.update(s,*f.index,s.map,world,40000);
        assert(d.queries<=profile.probe.maxQueries);
        if(mode==4 || mode==5 || mode==6) {
            assert(d.state==local::WalkState::Failed && d.reason==local::WalkReason::DoorBlocked);
            if(mode==6) assert(d.probeReason==local::ProbeReason::BudgetExceeded && d.queries==7);
            continue;
        }
        assert(d.state==local::WalkState::Running && d.intent.use==local::ActionRequest::Press && d.intent.speed==0);
        const auto count=world.world.calls.size();
        assert(!walk.update(s,*f.index,s.map,world,50000).accepted && world.world.calls.size()==count);
        ++s.tick.value; if(mode==3) ++world.id;
        d=walk.update(s,*f.index,s.map,world,mode==2 ? 140000:80000);
        assert(d.intent.use==local::ActionRequest::None && d.intent.speed==0 && d.queries==2);
        if(mode==2 || mode==3) {
            assert(d.state==local::WalkState::Failed && d.reason==local::WalkReason::DoorBlocked);
            assert(d.doorReason==(mode==2 ? local::DoorWaitReason::TimedOut:local::DoorWaitReason::Replaced));
            continue;
        }
        world.open=true; ++s.tick.value;
        d=walk.update(s,*f.index,s.map,world,100000);
        assert(d.doorState==local::DoorWaitState::Clear && d.state==local::WalkState::Running && d.intent.speed==0);
        if(mode==7) world.open=false;
        ++s.tick.value; d=walk.update(s,*f.index,s.map,world,120000);
        if(mode==7) assert(d.state==local::WalkState::Failed && d.doorReason==local::DoorWaitReason::Reblocked);
        else assert(d.state==local::WalkState::Running && d.intent.speed>0 && d.intent.use==local::ActionRequest::None);
    }
}
void touchAndReservedQueries() {
    auto areas=zigzag(); Fixture f(areas,1,2); auto s=actor(); auto profile=limits;
    profile.probe.maxQueries=21; profile.doorTimeoutUs=1000000; profile.touchTimeoutUs=3000000;
    DoorWorld world(areas); world.touch=true; world.missingFloor=true;
    local::Walk walk(binding(),f.corridor,{150,50,0},profile);
    int contacts=0; bool arrived=false;
    for(std::uint64_t tick=1;tick<200;++tick) {
        s.tick={tick};
        // Reserve ordinal 1 as though the host just guarded a queued contact.
        runtime::QueryRequest guard{{s.agent,s.actor,s.map,s.tick,binding().routeGeneration,1},runtime::QueryKind::Door};
        world.world.calls.push_back(guard);
        const auto before=world.world.calls.size()-1;
        const auto d=walk.update(s,*f.index,s.map,world,tick*40000,1);
        assert(d.queries==world.world.calls.size()-before && d.queries<=21 && d.samples<=4);
        assert(d.state==local::WalkState::Running || d.state==local::WalkState::Arrived);
        assert(d.intent.use==local::ActionRequest::None);
        if(d.contact) { assert(++contacts==1); s.position->x=83.96875f; world.open=true; }
        else s.position->x+=static_cast<float>(d.intent.direction.x*d.intent.speed*0.040);
        if(d.state==local::WalkState::Arrived) { arrived=true; break; }
    }
    assert(arrived && contacts==1);
    s=actor(); DoorWorld empty(areas); local::Walk exhausted(binding(),f.corridor,{150,50,0},profile);
    const auto d=exhausted.update(s,*f.index,s.map,empty,0,21);
    assert(d.state==local::WalkState::Failed && d.probeReason==local::ProbeReason::BudgetExceeded && empty.world.calls.empty());
}
std::vector<Trace> simulate(const std::vector<route_test::Area>& areas, std::uint32_t start,
    std::uint32_t goalArea, model::NavVector3 from, model::NavVector3 goal, std::uint64_t frameUs) {
    Fixture f(areas,start,goalArea); World world(areas); auto s=actor(from); s.elapsedUs=frameUs;
    local::Walk walk(binding(),f.corridor,goal,limits); local::IntentPump pump(binding());
    std::optional<core::BotCommand> pending; core::TickId queued{};
    std::vector<Trace> trace; std::size_t completions=0;
    for(std::uint64_t tick=1;tick<=4000;++tick) {
        s.tick={tick};
        if(pending) {
            assert(s.tick.isAfter(queued));
            assert(pending->buttons==0);
            // Flat scripted physics, yaw zero. Real HLDS movement remains unverified.
            const float dt=static_cast<float>(frameUs)/1000000.0f;
            s.position->x+=pending->movement.forward*dt;
            s.position->y-=pending->movement.side*dt;
            pending.reset();
        }
        const auto schedule=pump.beginFrame(s); assert(schedule.accepted);
        if(schedule.decisionDue) {
            const auto before=world.calls.size();
            auto d=walk.update(s,*f.index,s.map,world);
            if(d.state!=local::WalkState::Running && d.state!=local::WalkState::Arrived)
                std::fprintf(stderr,"Walk failure goal=%u frame=%llu tick=%llu step=%zu reason=%d probe=%d position=(%g,%g,%g)\n",
                    goalArea,static_cast<unsigned long long>(frameUs),static_cast<unsigned long long>(tick),
                    d.binding.step,static_cast<int>(d.reason),static_cast<int>(d.probeReason),
                    double(s.position->x),double(s.position->y),double(s.position->z));
            assert(d.accepted && d.state!=local::WalkState::Failed && d.state!=local::WalkState::Aborted);
            assert(d.queries==world.calls.size()-before && d.queries<=limits.probe.maxQueries);
            assert(d.samples<=limits.probe.maxSamples && core::Motor::valid(d.intent));
            trace.push_back({d.binding.step,d.state,d.primitiveEvent,*s.position});
            if(d.primitiveEvent==local::PrimitiveEvent::Complete) {
                ++completions; assert(d.support && d.support->area==f.corridor->transitions()[d.binding.step].edge.target);
            }
            assert(pump.publish(d.binding,s.tick,d.intent));
            if(d.state==local::WalkState::Arrived) {
                assert(d.terminalEvent && d.support && d.support->area==model::NavAreaId{goalArea});
                assert(std::hypot(double(goal.x)-s.position->x,double(goal.y)-s.position->y)<=limits.arrivalTolerance);
                assert(completions==f.corridor->transitions().size());
                const auto output=pump.take(); assert(output.emit && output.intent.speed==0);
                ++s.tick.value; const auto count=world.calls.size();
                auto again=walk.update(s,*f.index,s.map,world);
                assert(!again.accepted && !again.terminalEvent && again.intent.speed==0 && count==world.calls.size());
                return trace;
            }
        }
        auto output=pump.take(); assert(output.emit);
        const auto command=core::Motor::command(output.intent,{0,0,0},250,frameUs,output.firstFrame);
        assert(command); pending=command.command; queued=s.tick;
    }
    assert(false && "bounded simulation did not arrive"); return {};
}
void arrivals() {
    const auto path=zigzag();
    for(std::uint64_t frameUs : {8000U,16000U,100000U}) {
        for(std::uint32_t goal : {1U,2U,3U,4U}) {
            const model::NavVector3 end=goal==1 ? model::NavVector3{70,50,0}:
                goal==2 ? model::NavVector3{170,50,0}:goal==3 ? model::NavVector3{150,170,0}:model::NavVector3{270,170,0};
            const auto a=simulate(path,1,goal,{50,50,36},end,frameUs);
            const auto b=simulate(path,1,goal,{50,50,36},end,frameUs);
            assert(a.size()==b.size());
            for(std::size_t i=0;i<a.size();++i) assert(a[i].step==b[i].step && a[i].state==b[i].state &&
                a[i].event==b[i].event && a[i].position==b[i].position);
        }
    }
    auto reverse=path; reverse[3].targets={}; reverse[3].targets[3]={3};
    reverse[2].targets={}; reverse[2].targets[0]={2}; reverse[1].targets={}; reverse[1].targets[3]={1};
    simulate(reverse,4,1,{270,150,36},{30,30,0},16000);
}
void stops() {
    const auto areas=zigzag(); Fixture f(areas,1,2);
    for(int mode=1;mode<=6;++mode) {
        World world(areas); local::Walk walk(binding(),f.corridor,{170,50,0},limits); auto s=actor();
        assert(walk.update(s,*f.index,s.map,world).primitiveEvent==local::PrimitiveEvent::Entered);
        world.mode=mode; ++s.tick.value;
        const auto d=walk.update(s,*f.index,s.map,world);
        assert(d.state==local::WalkState::Failed && d.terminalEvent && d.intent.speed==0);
        assert(d.primitiveEvent==local::PrimitiveEvent::Failed);
        assert(d.queries<=limits.probe.maxQueries && d.reason==local::WalkReason::ProbeFailed);
    }
    World world(areas); auto s=actor(); local::Walk walk(binding(),f.corridor,{170,50,0},limits);
    auto d=walk.update(s,*f.index,s.map,world); assert(d.accepted);
    const auto count=world.calls.size();
    d=walk.update(s,*f.index,s.map,world);
    assert(!d.accepted && d.reason==local::WalkReason::StaleTick && world.calls.size()==count);
    ++s.actor.generation.value;
    d=walk.update(s,*f.index,s.map,world);
    assert(d.state==local::WalkState::Aborted && d.terminalEvent && world.calls.size()==count);
    s=actor(); ++s.tick.value; d=walk.update(s,*f.index,s.map,world);
    assert(!d.accepted && d.intent.speed==0 && !d.terminalEvent);
    local::Walk cancel(binding(),f.corridor,{170,50,0},limits);
    assert(cancel.abort().terminalEvent && !cancel.abort().terminalEvent);
    local::Walk wrongGoal(binding(),f.corridor,{50,50,0},limits); s=actor(); world.calls.clear();
    assert(wrongGoal.update(s,*f.index,s.map,world).reason==local::WalkReason::InvalidGoal);
    local::Walk stale(binding(),f.corridor,{170,50,0},limits);
    assert(stale.update(s,*f.index,{5},world).reason==local::WalkReason::StaleNavigation);
    local::Walk off(binding(),f.corridor,{170,50,0},limits); world.calls.clear(); s.position=model::NavVector3{150,150,36};
    assert(off.update(s,*f.index,s.map,world).reason==local::WalkReason::OffCorridor);
}
void measuredCompletion() {
    const auto areas=zigzag(); Fixture f(areas,1,2); World world(areas); auto s=actor();
    local::Walk walk(binding(),f.corridor,{170,50,0},limits);
    walk.update(s,*f.index,s.map,world);
    ++s.tick.value; s.position->x=101;
    auto d=walk.update(s,*f.index,s.map,world);
    assert(d.state==local::WalkState::Running && walk.step()==0 && d.intent.speed>0 && d.target);
    assert(d.target->origin.x>=116); // cross beyond the portal, not an area center
    ++s.tick.value; s.position->x=116;
    d=walk.update(s,*f.index,s.map,world);
    assert(d.primitiveEvent==local::PrimitiveEvent::Complete && walk.step()==1 && d.intent.speed==0);
    ++s.tick.value; d=walk.update(s,*f.index,s.map,world);
    assert(d.state==local::WalkState::Running && d.intent.speed>0); // exhaustion is not arrival
    ++s.tick.value; s.position->x=170; s.grounded=false;
    d=walk.update(s,*f.index,s.map,world);
    assert(d.state==local::WalkState::Failed && d.probeReason==local::ProbeReason::NoSupport);
}
void crouchCrossing() {
    for(int mode=0;mode<3;++mode) {
        auto a=square(1,0,0),b=square(2,100,0),c=square(3,200,0);
        a.targets[1]={2}; b.targets[1]={3};
        if(mode==0) b.attributes=1;
        if(mode==1) a.attributes=1;
        if(mode==2) a.attributes=8;
        const std::vector<route_test::Area> areas{a,b,c};
        Fixture f(areas,1,mode==1 ? 1:3); World world(areas); auto s=actor(); s.ducked=false;
        auto profile=limits; profile.probe.maxQueries=21;
        profile.crouch={{{-16,-16,-36},{16,16,36}},{{-16,-16,-18},{16,16,18}},1000000};
        const model::NavVector3 goal{mode==1 ? 50.0f:217.0f,50,0};
        local::Walk walk(binding(),f.corridor,goal,profile); bool arrived=false,ducked=false;
        for(std::uint64_t tick=1;tick<500;++tick) {
            s.tick={tick}; const auto before=world.calls.size();
            const auto d=walk.update(s,*f.index,s.map,world,tick*40000);
            assert(d.queries==world.calls.size()-before && d.queries<=21 && d.samples<=4);
            assert(d.state==local::WalkState::Running || d.state==local::WalkState::Arrived);
            if(d.state==local::WalkState::Arrived) {
                assert(d.support && d.support->area.value==(mode==1 ? 1U:3U));
                assert(s.ducked==(mode==1)); arrived=true; break;
            }
            const auto command=core::Motor::command(d.intent,{},250,40000,true); assert(command);
            const bool duck=(command.command->buttons&static_cast<core::ButtonMask>(core::Button::Duck))!=0;
            s.ducked=duck; s.hull=duck ? profile.crouch.crouched:profile.crouch.standing;
            s.position->z=-s.hull->minimum.z; ducked=ducked || duck;
            assert(command.command->buttons==(duck ? static_cast<core::ButtonMask>(core::Button::Duck):0U));
            s.position->x+=static_cast<float>(command.command->movement.forward*0.04);
            s.position->y-=static_cast<float>(command.command->movement.side*0.04);
        }
        assert(arrived && ducked==(mode!=2));
    }
}
void recoverySafety() {
    const auto areas=zigzag(); Fixture f(areas,1,2);
    for(int mode=0;mode<7;++mode) {
        World world(areas); auto s=actor();
        auto profile=limits; profile.probe.maxQueries=21;
        local::Walk walk(binding(),f.corridor,{117,50,0},profile);
        local::RecoveryDecision r; r.state=local::RecoveryState::Sidestep; r.forward={1,0,0};
        if(mode==1) world.mode=3; // Missing future floor.
        if(mode==2) world.mode=5; // Stale stamped response.
        if(mode==3) s.grounded=false;
        if(mode==4) s.position=nav::model::NavVector3{20,80,36}; // Hull cannot fit the detour.
        if(mode==5) r.state=local::RecoveryState::Reverse;
        if(mode==6) world.mode=1; // Swept hull collision.
        const auto d=walk.recover(s,*f.index,s.map,world,r);
        assert(d.queries==world.calls.size() && d.queries<=21 && d.samples<=4);
        assert(d.intent.jump==core::ActionRequest::None && d.intent.use==core::ActionRequest::None);
        if(mode==0 || mode==5) {
            assert(d.target && d.intent.speed>0 && d.target->area==model::NavAreaId{1});
            if(mode==0) assert(d.intent.direction.y==1); // Stable even actor chooses left on tie.
            else assert(d.intent.direction.x==-1);
        } else assert(!d.target && d.intent.speed==0);
        ++s.tick.value;
        r.state=local::RecoveryState::Aborted;
        assert(walk.recover(s,*f.index,s.map,world,r).terminalEvent);
        ++s.tick.value; assert(!walk.recover(s,*f.index,s.map,world,r).terminalEvent);
    }
    World world(areas); auto s=actor(); local::Walk walk(binding(),f.corridor,{117,50,0},limits);
    local::RecoveryDecision r; r.state=local::RecoveryState::Reverse; r.forward={1,0,0};
    const auto d=walk.recover(s,*f.index,s.map,world,r,limits.probe.maxQueries);
    assert(world.calls.empty() && !d.target && d.intent.speed==0);
    local::WalkDecision cause;
    assert(local::observedStuckCause(cause)==local::StuckCause::Unknown);
    cause.reason=local::WalkReason::DoorBlocked; cause.doorId=42;
    assert(local::observedStuckCause(cause)==local::StuckCause::DoorBlocked);
    cause={}; cause.probeReason=local::ProbeReason::Blocked;
    assert(local::observedStuckCause(cause)==local::StuckCause::GeometryBlocked);
    cause={}; cause.reason=local::WalkReason::LadderFailed; cause.ladderReason=local::LadderReason::Fall;
    assert(local::observedStuckCause(cause)==local::StuckCause::TraversalFailed);
    cause={}; cause.blocker.emplace(); cause.blocker->kind=runtime::BlockerKind::Player;
    cause.blocker->player=core::PlayerId{5,{6}};
    assert(local::observedStuckCause(cause)==local::StuckCause::PlayerBlocked);
}
void invalidAndBudgets() {
    auto areas=zigzag(); Fixture f(areas,1,2); auto s=actor(); World world(areas);
    auto bad=limits; bad.probe.maxQueries=2;
    local::Walk budget(binding(),f.corridor,{170,50,0},bad);
    budget.update(s,*f.index,s.map,world); ++s.tick.value;
    const auto d=budget.update(s,*f.index,s.map,world);
    assert(d.state==local::WalkState::Failed && d.probeReason==local::ProbeReason::BudgetExceeded && d.queries==1);
    for(double value : {0.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        bad=limits; bad.speed=value; local::Walk invalid(binding(),f.corridor,{170,50,0},bad);
        const auto count=world.calls.size();
        assert(invalid.update(s,*f.index,s.map,world).reason==local::WalkReason::InvalidInput);
        assert(count==world.calls.size());
    }
    local::Walk missing(binding(),{},model::NavVector3{170,50,0},limits);
    assert(missing.update(s,*f.index,s.map,world).reason==local::WalkReason::InvalidInput);
    areas[0].attributes=1; Fixture crouch(areas,1,2); World constrained(areas);
    local::Walk unsupported(binding(),crouch.corridor,{170,50,0},limits);
    assert(unsupported.update(s,*crouch.index,s.map,constrained).reason==local::WalkReason::UnsupportedTraversal);
    assert(constrained.calls.size()==1); // no generic Walk fallback for special hints
    Fixture same(areas,1,1); World noFloor(areas); noFloor.mode=2;
    local::Walk atGoal(binding(),same.corridor,{50,50,0},limits); s=actor();
    assert(atGoal.update(s,*same.index,s.map,noFloor).state==local::WalkState::Failed);
    World hinted(areas); local::Walk sameHint(binding(),same.corridor,{70,50,0},limits);
    assert(sameHint.update(s,*same.index,s.map,hinted).reason==local::WalkReason::UnsupportedTraversal);
}
}
int main() { arrivals(); stops(); measuredCompletion(); invalidAndBudgets(); doors(); touchAndReservedQueries(); crouchCrossing(); recoverySafety(); }
