// SPDX-License-Identifier: MPL-2.0
#include "nav/local/walk.hpp"
#include "nav/local/intent_pump.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include "steering_fixture.hpp"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <limits>
using namespace astrabot;
using namespace astrabot::nav;
namespace {
constexpr local::WalkLimits limits{{21,4,48,16,18,18,64,4,2,0.7},160,1,1,3,0,0,12,8,40,25};
local::Binding binding() { return {{1},{1,{1}},{1},1,0}; }
struct World final : runtime::IWorldQueries {
    int mode{};
    std::vector<runtime::QueryRequest> calls;
    explicit World(int m):mode(m) {}
    runtime::HullObservation sweep(model::NavVector3 a,model::NavVector3 b) const {
        return steering_fixture::sweep(mode,a,b);
    }
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        assert(q.stamp.ordinal==(calls.empty() || calls.back().stamp.tick!=q.stamp.tick ? 1:calls.back().stamp.ordinal+1));
        calls.push_back(q);
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        const runtime::FloorObservation floor{0,{0,0,1},true};
        if(q.kind==runtime::QueryKind::GroundedArea) r.ground=runtime::GroundedAreaObservation{model::NavAreaId{q.start.x<=100 ? 1U:2U},floor};
        else if(q.kind==runtime::QueryKind::Floor) {
            r.floor=floor;
            if(mode==3 && q.start.y<45) r.floor.reset();
        } else if(q.kind==runtime::QueryKind::SweptHull) {
            r.hull=sweep(q.start,q.end);
            if(mode==4) r.hull.reset();
        } else if(q.kind==runtime::QueryKind::Blocker) {
            r.blocker=runtime::BlockerObservation{0,runtime::BlockerKind::Geometry};
        } else assert(false);
        return r;
    }
};
void scenario(int mode,std::uint64_t frameUs) {
    auto a=route_test::Area{1,{{0,0,0},{100,100,0},0,0}},b=route_test::Area{2,{{100,0,0},{200,100,0},0,0}};
    a.targets[1]={2}; const auto mesh=route_test::snapshot({a,b});
    const auto index=query::NavSpatialIndex::build(mesh,{2,3,1000000}); assert(index);
    const auto graph=query::NavGraph::build(mesh,{2,1,1000000}); assert(graph);
    const auto route=query::NavRouteSearch::search(**graph.value,{{1},{2},{2,1000000},false}); assert(route);
    const auto corridor=corridor::Corridor::build(**graph.value,*route.value,{16,16},{1,1000000,2}); assert(corridor);
    runtime::MovementSnapshot s; const auto bind=binding(); s.agent=bind.agent; s.actor=bind.actor; s.map=bind.map;
    s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true;
    s.position=model::NavVector3{50,mode==0 ? 49.0f:50.0f,36}; s.view=model::NavVector3{0,90,0};
    s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f; s.elapsedUs=frameUs;
    auto profile=limits; if(mode==5) profile.probe.maxQueries=5;
    if(mode==6) profile.sideProbeDistance=(std::numeric_limits<double>::quiet_NaN)();
    if(mode==7) profile.narrowMargin=0;
    if(mode==8) profile.maxAvoidanceDecisions=0;
    if(mode==9) profile.maxAvoidanceDecisions=1;
    World world(mode); local::Walk walk(bind,corridor.value,{150,50,0},profile); local::IntentPump pump(bind);
    std::optional<core::BotCommand> pending; bool finished=false,narrow=false,corrected=false,avoided=false;
    for(std::uint64_t tick=1;tick<4000;++tick) {
        s.tick={tick};
        if(pending) {
            assert(pending->buttons==0); const auto old=*s.position;
            const double yaw=pending->view.yaw*3.14159265358979323846/180;
            const double dt=double(pending->msec)/1000;
            s.position->x+=static_cast<float>((pending->movement.forward*std::cos(yaw)+pending->movement.side*std::sin(yaw))*dt);
            s.position->y+=static_cast<float>((pending->movement.forward*std::sin(yaw)-pending->movement.side*std::cos(yaw))*dt);
            assert(world.sweep(old,*s.position).fraction==1); // emitted motion never hits the wall
            if(mode==0) assert(s.position->y>=48 && s.position->y<=52);
            pending.reset();
        }
        const auto schedule=pump.beginFrame(s); assert(schedule.accepted);
        if(schedule.decisionDue) {
            const auto before=world.calls.size(); const auto d=walk.update(s,**index.value,s.map,world,pump.timeUs());
            assert(d.queries==world.calls.size()-before && d.queries<=profile.probe.maxQueries && d.samples<=4);
            narrow=narrow || d.narrow; corrected=corrected || std::abs(d.intent.lateralCorrection)>0.001; avoided=avoided || d.avoiding;
            if(mode==0 && d.narrow) assert(d.intent.speed<limits.speed);
            if(d.state!=local::WalkState::Running) {
                if(mode<2) assert(d.state==local::WalkState::Arrived);
                else assert(d.state==local::WalkState::Failed && d.intent.speed==0);
                finished=true; break;
            }
            assert(pump.publish(d.binding,s.tick,d.intent));
        }
        const auto output=pump.take(); if(output.emit) {
            const auto motor=core::Motor::command(output.intent,{0,90,0},250,frameUs,output.firstFrame); assert(motor); pending=motor.command;
        }
    }
    assert(finished);
    if(mode==0) assert(narrow && corrected);
    if(mode==1) assert(avoided);
}
}
int main() { for(std::uint64_t us : {8000U,16000U,100000U}) for(int mode=0;mode<10;++mode) scenario(mode,us); }
