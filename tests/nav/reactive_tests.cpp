// SPDX-License-Identifier: MPL-2.0
#include "nav/local/walk.hpp"
#include "nav/local/intent_pump.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include "steering_fixture.hpp"
#include <cassert>
#include <cmath>
#include <vector>
using namespace astrabot;
using namespace astrabot::nav;
namespace {
constexpr local::WalkLimits limits{{21,4,48,16,18,18,64,4,2,0.7},160,1,1,3,0,0,12,8,40,25,{120000,400000,3000000}};
struct World final : runtime::IWorldQueries {
    int mode{};
    std::uint64_t now{}, firstBlocked{}, lastTick{};
    std::uint32_t ordinal{}, calls{};
    bool blockedOnce{};
    explicit World(int m):mode(m) {}
    bool present() const { return mode!=2 || now<300000; }
    runtime::HullObservation sweep(model::NavVector3 a,model::NavVector3 b) const {
        if(!present()) return {1,b,{},false};
        float shift=0;
        if(mode==10 || mode==11) shift=static_cast<float>((std::min)(10.0,double(now)/1000000*5))*(mode==10 ? 1.0f:-1.0f);
        a.x-=shift; b.x-=shift;
        auto h=steering_fixture::sweep(mode==1 || mode==7 ? 2:1,a,b);
        h.end.x+=shift; return h;
    }
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        if(q.stamp.tick.value!=lastTick) { lastTick=q.stamp.tick.value; ordinal=mode==6 ? 2U:0U; }
        assert(q.stamp.ordinal==++ordinal); ++calls;
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        const runtime::FloorObservation floor{0,{0,0,1},true};
        if(q.kind==runtime::QueryKind::GroundedArea)
            r.ground=runtime::GroundedAreaObservation{model::NavAreaId{q.start.x<=100 ? 1U:2U},floor};
        else if(q.kind==runtime::QueryKind::Floor) r.floor=floor;
        else if(q.kind==runtime::QueryKind::SweptHull) r.hull=sweep(q.start,q.end);
        else if(q.kind==runtime::QueryKind::Blocker) {
            if(!blockedOnce) { firstBlocked=now; blockedOnce=true; }
            r.blocker=runtime::BlockerObservation{123,runtime::BlockerKind::Player,core::PlayerId{1,{1}}};
            if(mode==3) r.blocker=runtime::BlockerObservation{123,runtime::BlockerKind::Other,{}};
            if(mode==4) { r.blocker.reset(); r.error=runtime::QueryError::Unavailable; }
            if(mode==5) r.stamp.ordinal++;
            if(mode==7) { r.blocker->id=now; r.blocker->player->generation.value=static_cast<std::uint32_t>(now); }
        } else assert(false);
        return r;
    }
};
std::vector<unsigned> scenario(int mode,std::uint64_t frameUs) {
    route_test::Area a{1,{{0,0,0},{100,100,0},0,0}},b{2,{{100,0,0},{200,100,0},0,0}};
    a.targets[1]={2};
    const auto mesh=route_test::snapshot({a,b});
    const auto index=query::NavSpatialIndex::build(mesh,{2,3,1000000}); assert(index);
    const auto graph=query::NavGraph::build(mesh,{2,1,1000000}); assert(graph);
    const auto route=query::NavRouteSearch::search(**graph.value,{{1},{2},{2,1000000},false}); assert(route);
    const auto path=corridor::Corridor::build(**graph.value,*route.value,{16,16},{1,1000000,2}); assert(path);
    local::Binding bind{{1},{2,{1}},{1},1,0};
    runtime::MovementSnapshot s; s.agent=bind.agent; s.actor=bind.actor; s.map=bind.map;
    s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true;
    s.position=model::NavVector3{50,50,36}; s.view=model::NavVector3{0,90,0};
    s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f; s.elapsedUs=frameUs;
    auto profile=limits;
    if(mode==8) profile.probe.maxQueries=5;
    if(mode==9) profile.blocker.yieldUs=profile.blocker.timeoutUs;
    World world(mode); local::Walk walk(bind,path.value,{150,50,0},profile); local::IntentPump pump(bind);
    std::optional<core::BotCommand> pending; bool finished=false,yielded=false,avoided=false,cleared=false;
    std::vector<unsigned> trace;
    for(std::uint64_t tick=1;tick<3000;++tick) {
        s.tick={tick}; world.now=tick*frameUs;
        if(pending) {
            assert(pending->buttons==0); const auto old=*s.position;
            const double yaw=pending->view.yaw*3.14159265358979323846/180,dt=double(pending->msec)/1000;
            s.position->x+=static_cast<float>((pending->movement.forward*std::cos(yaw)+pending->movement.side*std::sin(yaw))*dt);
            s.position->y+=static_cast<float>((pending->movement.forward*std::sin(yaw)-pending->movement.side*std::cos(yaw))*dt);
            assert(world.sweep(old,*s.position).fraction==1);
            pending.reset();
        }
        const auto schedule=pump.beginFrame(s); assert(schedule.accepted);
        if(schedule.decisionDue) {
            const auto before=world.calls;
            const auto reserved=mode==6 ? 2U:0U;
            const auto d=walk.update(s,**index.value,s.map,world,pump.timeUs(),reserved);
            assert(d.queries==world.calls-before+reserved && d.queries<=profile.probe.maxQueries && d.samples<=4);
            trace.push_back(unsigned(d.blockerAction)+16*unsigned(d.blockerReason)+256*unsigned(d.state));
            yielded=yielded || d.blockerAction==local::BlockerAction::Yield;
            avoided=avoided || d.avoiding; cleared=cleared || d.blockerAction==local::BlockerAction::ReinspectPassage;
            if(d.blockerAction==local::BlockerAction::Yield || d.blockerAction==local::BlockerAction::ReinspectPassage)
                assert(d.intent.speed==0);
            if(d.state!=local::WalkState::Running) {
                if(mode==0 || mode==2 || mode==3 || mode==6 || mode==10 || mode==11) {
                    if(d.state!=local::WalkState::Arrived)
                        std::fprintf(stderr,"reactive mode=%d us=%llu reason=%u probe=%u action=%u blocker=%u\n",mode,
                            static_cast<unsigned long long>(frameUs),unsigned(d.reason),unsigned(d.probeReason),
                            unsigned(d.blockerAction),unsigned(d.blockerReason));
                    assert(d.state==local::WalkState::Arrived);
                } else assert(d.state==local::WalkState::Failed && d.intent.speed==0);
                if(mode==1 || mode==7) {
                    assert(d.reason==local::WalkReason::DynamicBlocked && d.blockerReason==local::BlockerReason::TimedOut);
                    assert(world.now-world.firstBlocked>=profile.blocker.timeoutUs &&
                        world.now-world.firstBlocked<=profile.blocker.timeoutUs+100000);
                }
                finished=true; break;
            }
            const auto duplicate=walk.update(s,**index.value,s.map,world,pump.timeUs());
            assert(!duplicate.accepted && duplicate.intent.speed==0 && world.calls-before+reserved==d.queries);
            assert(pump.publish(d.binding,s.tick,d.intent));
        }
        const auto output=pump.take(); if(output.emit) {
            const auto motor=core::Motor::command(output.intent,{0,90,0},250,frameUs,output.firstFrame);
            assert(motor); pending=motor.command;
        }
    }
    assert(finished);
    if(mode==0 || mode==2 || mode==6 || mode==10 || mode==11) assert(yielded && cleared);
    if(mode==0 || mode==3 || mode==6 || mode==10 || mode==11) assert(avoided && cleared);
    // Navigation and selected static route remain immutable across dynamic observations.
    const auto again=query::NavRouteSearch::search(**graph.value,{{1},{2},{2,1000000},false}); assert(again);
    assert(again.value->total==route.value->total && again.value->steps.size()==route.value->steps.size());
    return trace;
}
}
int main() { for(std::uint64_t us : {8000U,16000U,100000U}) for(int mode=0;mode<12;++mode) assert(scenario(mode,us)==scenario(mode,us)); }
