// SPDX-License-Identifier: MPL-2.0
// Original synthetic floor model. It integrates only commands dispatched on a
// later tick; NAV targets never write observations or establish arrival.
#include "nav/local/walk.hpp"
#include "nav/local/intent_pump.hpp"
#include "nav/runtime/route_session.hpp"
#include "route_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace astrabot;
using namespace astrabot::nav;
namespace {
constexpr std::uint32_t traceLimit=50000, frameLimit=1250, queryLimit=21;
constexpr std::uint64_t timeLimit=10000000;
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
struct Trace {
    std::vector<std::string> events;
    void add(const std::string& event) {
        require(events.size()<traceLimit,"trace bound exceeded"); events.push_back(event);
    }
};
struct World final:runtime::IWorldQueries {
    const std::vector<route_test::Area>& areas;
    std::uint32_t calls{};
    bool blocked{};
    explicit World(const std::vector<route_test::Area>& a):areas(a) {}
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        ++calls;
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind;
        r.error=runtime::QueryError::None;
        const runtime::FloorObservation floor{0,{0,0,1},true};
        if(q.kind==runtime::QueryKind::GroundedArea) {
            std::optional<model::NavAreaId> id;
            for(auto i=areas.begin();i!=areas.end();++i)
                if(q.start.x>=i->extent.northWest.x && q.start.x<=i->extent.southEast.x &&
                   q.start.y>=0 && q.start.y<=100) { id=model::NavAreaId{i->id}; break; }
            r.ground=runtime::GroundedAreaObservation{id,floor};
        } else if(q.kind==runtime::QueryKind::Floor) r.floor=floor;
        else if(q.kind==runtime::QueryKind::SweptHull)
            r.hull=runtime::HullObservation{blocked ? 0.0f:1.0f,q.end,{-1,0,0},false};
        else if(q.kind==runtime::QueryKind::Clearance) r.clearance=runtime::ClearanceObservation{true};
        else r.error=runtime::QueryError::Unavailable;
        return r;
    }
};
local::WalkLimits profile() {
    local::WalkLimits p{{queryLimit,4,48,16,18,18,64,4,2,0.7},100,4,1,3};
    p.crouch={{{-16,-16,-36},{16,16,36}},{{-16,-16,-18},{16,16,18}},1000000};
    return p;
}
struct Actor {
    runtime::MovementSnapshot observation;
    std::unique_ptr<runtime::RouteSession> session;
    std::unique_ptr<local::Walk> walk;
    std::unique_ptr<local::IntentPump> pump;
    std::optional<core::BotCommand> queued;
    std::uint64_t queuedTick{};
    core::MapGeneration queuedMap{};
    local::Binding binding{};
    bool done{};
    bool duckDispatched{}, duckReleased{}, crossedCrouched{};
};
std::string prefix(const char* kind,std::uint32_t actor,std::uint64_t tick) {
    return std::string("{\"type\":\"")+kind+"\",\"actor\":"+std::to_string(actor)+",\"tick\":"+std::to_string(tick);
}
void routeTrace(Trace& t,const runtime::DecisionTrace& d) {
    std::ostringstream out; out.imbue(std::locale::classic());
    out<<prefix("route",d.agent.value,d.tick.value)<<",\"map\":"<<d.map.value
       <<",\"state\":"<<int(d.state)<<",\"reason\":"<<int(d.reason)<<",\"edges\":[";
    bool first=true;
    if(d.route) for(const auto& s:d.route->steps) {
        if(!first) out<<',';
        first=false;
        out<<'['<<s.edge.source.value<<','<<s.edge.target.value<<','<<int(s.edge.traversal)<<']';
    }
    out<<"]}"; t.add(out.str());
}
struct Result {
    Trace trace;
    std::uint32_t frames{},maxQueries{};
    std::uint64_t elapsed{},queries{};
    std::string outcome;
};
std::string expectedOutcome(const std::string& scenario,bool mapChange) {
    if(scenario=="partial-non-execution" || scenario=="unreachable-non-execution")
        return mapChange ? "cancelled-fresh-non-execution":scenario;
    if(mapChange) return "cancelled-fresh-arrived";
    return scenario=="blocked-floor" ? "probe-failed":"arrived";
}
Result replay(const std::string& scenario,std::uint64_t delta,std::uint32_t count,bool mapChange) {
    std::vector<route_test::Area> areas;
    for(std::uint32_t id=1;id<=3;++id) {
        const float x=float(id-1)*100;
        areas.push_back({id,{{x,0,0},{x+100,100,0},0,0},{}});
        if(id<3) areas.back().targets[1]={id+1};
    }
    if(scenario=="crouch-walk-release") areas[1].attributes=1;
    if(scenario=="unreachable-non-execution") areas[1].targets[1].clear();
    auto mesh=route_test::snapshot(areas);
    auto graphResult=query::NavGraph::build(mesh,{10,20,1000000}); require(bool(graphResult),"graph");
    auto indexResult=query::NavSpatialIndex::build(mesh,{10,19,1000000}); require(bool(indexResult),"index");
    const auto graph=*graphResult.value; const auto index=*indexResult.value;
    World world(areas); Result result; std::vector<Actor> actors(count);
    const bool routeOnly=scenario=="partial-non-execution" || scenario=="unreachable-non-execution";
    const auto initialize=[&](Actor& a,std::uint32_t id,std::uint64_t tick,std::uint32_t map) {
        a=Actor{}; auto& s=a.observation;
        s.agent={id}; s.actor={static_cast<std::uint16_t>(id),{map}}; s.map={map}; s.tick={tick}; s.elapsedUs=delta;
        s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true; s.ducked=false;
        s.position=model::NavVector3{50,50,36}; s.hull=profile().crouch.standing;
        s.speedLimit=250.0f; s.view=model::NavVector3{};
        a.session=std::make_unique<runtime::RouteSession>(s.agent,s.actor,s.map);
        const runtime::RouteOptions options{{scenario=="partial-non-execution" ? 1U:10U,1000000},1,true};
        world.calls=0;
        const auto request=a.session->request(s,{3},{s.map,graph},world,options);
        result.trace.add(prefix("input",id,tick)+",\"map\":"+std::to_string(map)+
            ",\"start\":[50,50,36],\"goalArea\":3,\"goal\":[250,50,0]}");
        require(request.accepted==!routeOnly,"route request acceptance mismatch");
        result.queries+=world.calls; result.maxQueries=std::max(result.maxQueries,world.calls);
        routeTrace(result.trace,a.session->trace());
        if(routeOnly) {
            require(!a.session->executable(),"partial/unreachable executable");
            require(a.session->trace().reason==(scenario=="partial-non-execution" ?
                runtime::SessionReason::ExpansionLimit:runtime::SessionReason::Unreachable),"route reason");
            result.trace.add(prefix("terminal",id,tick)+",\"map\":"+std::to_string(map)+
                ",\"outcome\":\""+(scenario=="partial-non-execution" ? "ExpansionLimit":"Unreachable")+"\"}");
            a.done=true; return;
        }
        require(a.session->executable(),"complete route not executable");
        const auto corridor=corridor::Corridor::build(*graph,*a.session->trace().route,{16,16},{10,1000000,20});
        require(bool(corridor),"corridor");
        for(const auto& portal:corridor.value->transitions()) {
            std::ostringstream o; o.imbue(std::locale::classic());
            o<<prefix("portal",id,tick)<<",\"edge\":["<<portal.edge.source.value<<','<<portal.edge.target.value
             <<"],\"source\":["<<portal.sourceLow.x<<','<<portal.sourceLow.y<<','<<portal.sourceHigh.x<<','<<portal.sourceHigh.y
             <<"],\"target\":["<<portal.targetLow.x<<','<<portal.targetLow.y<<','<<portal.targetHigh.x<<','<<portal.targetHigh.y<<"]}";
            result.trace.add(o.str());
        }
        a.binding={s.agent,s.actor,s.map,a.session->trace().routeGeneration,0};
        a.walk=std::make_unique<local::Walk>(a.binding,corridor.value,model::NavVector3{250,50,0},profile());
        a.pump=std::make_unique<local::IntentPump>(a.binding);
    };
    for(std::uint32_t id=1;id<=count;++id) initialize(actors[id-1],id,1,1);
    bool changed=false;
    for(std::uint32_t frame=1;frame<=frameLimit;++frame) {
        result.frames=frame; result.elapsed=std::uint64_t(frame)*delta;
        require(result.elapsed<=timeLimit,"simulated time bound");
        const std::uint64_t tick=std::uint64_t(frame)+1;
        // Route rejection has no queued command. Locomotion changes map while
        // a command is pending, before dispatch; fresh owners start at spawn.
        if(mapChange && !changed && (routeOnly || frame==3)) {
            for(std::uint32_t id=1;id<=count;++id) {
                auto& a=actors[id-1];
                if(!routeOnly) require(a.queued.has_value() && !a.done,"map change must retire active queued motion");
                if(a.queued) result.trace.add(prefix("receipt",id,tick)+",\"outcome\":\"map-invalidated\",\"queuedTick\":"+std::to_string(a.queuedTick)+"}");
                a.observation.map={2}; a.observation.tick={tick};
                a.session->observe(a.observation);
                require(!a.session->executable(),"old map executable");
                if(a.walk) {
                    world.calls=0;
                    const auto retired=a.walk->update(a.observation,*index,{1},world,result.elapsed);
                    require(retired.state==local::WalkState::Aborted,"old walk survived map");
                    require(world.calls==0,"old map queried world");
                    require(!a.pump->beginFrame(a.observation).accepted && !a.pump->take().emit,"old pump survived map");
                }
                result.trace.add(prefix("map-cancel",id,tick)+",\"reason\":"+std::to_string(int(a.session->trace().reason))+"}");
                initialize(a,id,tick,2);
            }
            changed=true;
            continue;
        }
        bool allDone=true;
        for(std::uint32_t id=1;id<=count;++id) {
            auto& a=actors[id-1]; if(a.done) continue; allDone=false;
            auto& s=a.observation; s.tick={tick};
            if(a.queued) {
                require(a.queuedTick<tick && a.queuedMap==s.map,"stale/same tick dispatch");
                const auto command=*a.queued;
                constexpr double rad=3.14159265358979323846/180;
                const double yaw=command.view.yaw*rad, seconds=double(command.msec)/1000;
                // Independent synthetic kinematics, driven solely by BotCommand.
                s.position->x+=float((command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw))*seconds);
                s.position->y+=float((command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw))*seconds);
                s.ducked=(command.buttons&static_cast<core::ButtonMask>(core::Button::Duck))!=0;
                if(s.ducked==true) a.duckDispatched=true;
                else if(a.duckDispatched) a.duckReleased=true;
                if(scenario=="crouch-walk-release" && s.position->x>116 && s.position->x<184) {
                    require(s.ducked==true,"crossed low passage without observed crouch"); a.crossedCrouched=true;
                }
                s.hull=s.ducked==true ? profile().crouch.crouched:profile().crouch.standing;
                s.position->z=s.ducked==true ? 18.0f:36.0f;
                std::ostringstream o; o.imbue(std::locale::classic()); o<<std::setprecision(9);
                o<<prefix("receipt",id,tick)<<",\"outcome\":\"dispatched\",\"queuedTick\":"<<a.queuedTick
                 <<",\"position\":["<<s.position->x<<','<<s.position->y<<','<<s.position->z<<"]}";
                result.trace.add(o.str()); a.queued.reset();
            }
            const auto schedule=a.pump->beginFrame(s); require(schedule.accepted,"pump frame rejected");
            // In the map variant invalidate queued motion before the obstacle
            // activates (frame 4), rather than cancelling an already failed Walk.
            world.calls=0; world.blocked=scenario=="blocked-floor" && !changed && (!mapChange || frame>=4);
            if(schedule.decisionDue) {
                const auto d=a.walk->update(s,*index,s.map,world,result.elapsed);
                require(d.queries==world.calls && d.queries<=queryLimit,"query accounting/budget");
                std::ostringstream o; o.imbue(std::locale::classic()); o<<std::setprecision(9);
                o<<prefix("decision",id,tick)<<",\"step\":"<<a.walk->step()<<",\"state\":"<<int(d.state)
                 <<",\"reason\":"<<int(d.reason)<<",\"probeReason\":"<<int(d.probeReason)
                 <<",\"primitive\":"<<int(d.primitiveEvent)<<",\"posture\":"<<(d.posture ? int(*d.posture):-1)
                 <<",\"queries\":"<<world.calls;
                if(d.target) o<<",\"target\":["<<d.target->origin.x<<','<<d.target->origin.y<<','<<d.target->origin.z<<']';
                o<<'}'; result.trace.add(o.str());
                require(a.pump->publish(a.binding,s.tick,d.intent),"intent publish");
                if(d.state!=local::WalkState::Running) {
                    if(world.blocked) require(d.state==local::WalkState::Failed && d.reason==local::WalkReason::ProbeFailed,"blocked typed outcome");
                    else {
                        if(d.state!=local::WalkState::Arrived) throw std::runtime_error(
                            scenario+" frame "+std::to_string(frame)+" state "+std::to_string(int(d.state))+
                            " reason "+std::to_string(int(d.reason))+" probe "+std::to_string(int(d.probeReason)));
                        require(std::abs(s.position->x-250)<=4 && std::abs(s.position->y-50)<=4,"unobserved arrival");
                        require(s.ducked==false,"crouch release missing");
                        if(scenario=="crouch-walk-release")
                            require(a.duckDispatched && a.crossedCrouched && a.duckReleased,"missing crouch hold/release dispatch");
                    }
                    result.trace.add(prefix("terminal",id,tick)+",\"map\":"+std::to_string(s.map.value)+
                        ",\"outcome\":\""+(world.blocked ? "ProbeFailed":"Arrived")+"\"}");
                    a.done=true;
                }
            }
            result.queries+=world.calls; result.maxQueries=std::max(result.maxQueries,world.calls);
            const auto out=a.pump->take(); require(out.emit,"missing motor frame");
            const auto motor=core::Motor::command(out.intent,{},250,out.frameUs,out.firstFrame);
            require(bool(motor) && bool(motor.command->validate()),"invalid motor command");
            std::ostringstream o; o.imbue(std::locale::classic()); o<<std::setprecision(9);
            o<<prefix("command",id,tick)<<",\"map\":"<<s.map.value<<",\"movement\":["<<motor.command->movement.forward<<','
             <<motor.command->movement.side<<','<<motor.command->movement.up<<"],\"view\":["<<motor.command->view.pitch<<','<<motor.command->view.yaw<<','<<motor.command->view.roll
             <<"],\"buttons\":"<<motor.command->buttons<<",\"msec\":"<<unsigned(motor.command->msec)<<'}'; result.trace.add(o.str());
            if(!a.done) { a.queued=motor.command; a.queuedTick=tick; a.queuedMap=s.map; }
            require(!a.pump->take().emit,"duplicate command");
        }
        if(allDone && (!mapChange || changed)) break;
        require(frame<frameLimit,"frame bound");
    }
    for(const auto& a:actors) require(a.done,"missing terminal actor");
    if(routeOnly) result.outcome=mapChange ? "cancelled-fresh-non-execution":scenario;
    else result.outcome=mapChange ? "cancelled-fresh-arrived":scenario=="blocked-floor" ? "probe-failed":"arrived";
    return result;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==1 || (argc==3 && std::string(argv[1])=="--output"),"usage: replay [--output path]");
        std::ofstream file;
        if(argc==3) { file.open(argv[2],std::ios::binary); require(bool(file),"cannot open output"); }
        std::ostream& output=argc==3 ? file:std::cout;
        output<<"{\"schemaVersion\":1,\"producer\":\"portable\",\"results\":["; bool first=true;
        for(const std::string scenario:{"floor-arrival","crouch-walk-release","blocked-floor","partial-non-execution","unreachable-non-execution"})
        for(const std::uint64_t delta:{8000ULL,16000ULL,100000ULL})
        for(const std::uint32_t count:{1U,8U,16U}) for(bool change:{false,true}) {
            std::cerr<<"case="<<scenario<<" frameUs="<<delta<<" actors="<<count<<" mapChange="<<change<<" seed=308\n";
            const auto a=replay(scenario,delta,count,change); const auto b=replay(scenario,delta,count,change);
            if(a.trace.events!=b.trace.events) {
                std::size_t i=0; while(i<a.trace.events.size() && i<b.trace.events.size() && a.trace.events[i]==b.trace.events[i]) ++i;
                std::cerr<<"first="<<(i<a.trace.events.size() ? a.trace.events[i]:"<end>")
                         <<"\nsecond="<<(i<b.trace.events.size() ? b.trace.events[i]:"<end>")<<'\n';
                throw std::runtime_error("replay divergence at event "+std::to_string(i));
            }
            require(a.outcome==b.outcome && a.frames==b.frames && a.queries==b.queries,"replay metrics mismatch");
            const auto expected=expectedOutcome(scenario,change);
            require(a.outcome==expected,"scenario expected outcome mismatch");
            if(!first) output<<',';
            first=false;
            output<<"{\"scenario\":\""<<scenario<<"\",\"frameUs\":"<<delta<<",\"actors\":"<<count
                  <<",\"variant\":\""<<(change ? "map-change":"clean")<<"\",\"seed\":308,\"outcome\":\""<<a.outcome
                  <<"\",\"expectedOutcome\":\""<<expected<<"\",\"frames\":"<<a.frames<<",\"elapsedUs\":"<<a.elapsed
                  <<",\"maxQueriesPerActorFrame\":"<<a.maxQueries<<",\"totalQueries\":"<<a.queries
                  <<",\"replans\":0,\"traceLimit\":"<<traceLimit<<",\"traceTruncated\":false,\"replayEqual\":true,\"trace\":[";
            for(std::size_t i=0;i<a.trace.events.size();++i) { if(i) output<<','; output<<a.trace.events[i]; }
            output<<"]}";
        }
        output<<"]}\n"; require(bool(output),"output write failed"); return 0;
    } catch(const std::exception& e) { std::cerr<<"portable replay: "<<e.what()<<'\n'; return 1; }
}
