// SPDX-License-Identifier: MPL-2.0
#include "nav/local/ladder.hpp"
#include "nav/local/ladder_physics.hpp"
#include "nav/local/ladder_exit.hpp"
#include "nav/local/walk.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <cmath>
#include <utility>
#include <limits>
#include <cstdio>
#include <cstdlib>
using namespace astrabot;
using namespace astrabot::nav;
using namespace astrabot::nav::local;
namespace {
constexpr Binding binding{{1},{2,{3}},{4},5,6};
LadderPlan plan(bool up=true) {
    LadderPlan p; p.start={-33,32,36}; p.end={33,32,164}; p.mount={-15,32,36}; p.dismount={-15,32,164}; p.normal={-1,0,0};
    p.link={7,8,9,{1},{2},{-33,32,0},{33,32,128},model::NavTraversalKind::Ladder,enrichment::NavLinkDirection::Up,0};
    if(!up) { std::swap(p.start,p.end); std::swap(p.mount,p.dismount); std::swap(p.link.from,p.link.to);
        std::swap(p.link.entry,p.link.exit); p.link.linkId=10; p.link.direction=enrichment::NavLinkDirection::Down; }
    return p;
}
runtime::MovementSnapshot actor(const LadderPlan& p) {
    runtime::MovementSnapshot s; s.agent=binding.agent; s.actor=binding.actor; s.map=binding.map; s.tick={1};
    s.elapsedUs=40000; s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true; s.ducked=false;
    s.position=p.start; s.velocity=model::NavVector3{}; s.view=model::NavVector3{};
    s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f;
    s.ladder=runtime::LadderContact{p.link.sourceId,p.link.generation,p.link.linkId,false}; return s;
}
LadderFeedback feedback(Ladder& ladder,const LadderPlan& p,runtime::MovementSnapshot s,std::uint64_t time,bool climbing=false) {
    LadderFeedback f; f.binding=binding; f.movement=s; f.nowUs=time; f.climbing=climbing;
    LadderInspection q; q.stamp={s.agent,s.actor,s.map,s.tick,binding.routeGeneration,0}; q.step=binding.step;
    q.sourceId=p.link.sourceId; q.generation=p.link.generation; q.linkId=p.link.linkId;
    q.queries=3; q.origin=*s.position; q.velocity=*s.velocity; q.hull=*s.hull; q.target=ladder.target(*s.position); q.pathClear=true;
    if(s.grounded==true) q.support=GroundedTarget{*s.position,s.position->z==p.end.z ? p.link.to:p.link.from,{s.position->z-36,{0,0,1},true}};
    f.inspection=q; return f;
}
void enter(Ladder& ladder,const LadderPlan& p,runtime::MovementSnapshot& s) {
    auto d=ladder.update(feedback(ladder,p,s,40000)); assert(d.state==LadderState::Align);
    s.tick={2}; d=ladder.update(feedback(ladder,p,s,80000)); assert(d.state==LadderState::Contact);
    s.tick={3}; s.position=p.mount; s.grounded=false; s.ladder->touching=true;
    d=ladder.update(feedback(ladder,p,s,120000,true));
    assert(d.state==(p.link.direction==enrichment::NavLinkDirection::Up ? LadderState::ClimbUp:LadderState::ClimbDown));
}
void directionalMotor() {
    core::MovementIntent intent; intent.direction={1,0,0}; intent.speed=80; intent.forward=core::ActionRequest::Hold;
    auto c=core::Motor::command(intent,{},250,16000,false); assert(c && c.command->buttons==static_cast<core::ButtonMask>(core::Button::Forward));
    intent.back=core::ActionRequest::Hold; assert(!core::Motor::valid(intent));
    intent.forward=core::ActionRequest::Release; intent.back=core::ActionRequest::Press;
    c=core::Motor::command(intent,{},250,16000,false); assert(c && c.command->buttons==0);
    c=core::Motor::command(intent,{},250,16000,true); assert(c && c.command->buttons==static_cast<core::ButtonMask>(core::Button::Back));
    c=core::Motor::command(intent,{},0,16000,true); assert(c && c.command->buttons==0 && c.command->movement==core::Movement{});
    intent.speed=0; assert(!core::Motor::valid(intent)); intent.back=core::ActionRequest::Release; assert(core::Motor::valid(intent));
}
void physicalProjection() {
    const auto close=[](double a,double b) { return std::abs(a-b)<0.001; };
    core::BotCommand c; c.msec=16; c.movement.forward=250;
    assert(ladderVelocity(c,{-1,0,0},250,false)==model::NavVector3{}); // Analog alone does not climb.
    c.buttons=static_cast<core::ButtonMask>(core::Button::Forward);
    for(const auto n:{model::NavVector3{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}}) {
        c.view.yaw=static_cast<float>(std::atan2(-n.y,-n.x)*180/3.14159265358979323846);
        c.view.pitch=0;
        auto v=ladderVelocity(c,n,250,false); assert(v && close(v->x,0) && close(v->y,0) && close(v->z,200));
        c.view.pitch=-45; v=ladderVelocity(c,n,250,false); assert(v && close(v->z,282.84271247));
        c.view.pitch=45; v=ladderVelocity(c,n,250,false); assert(v && close(v->z,0));
        c.view.pitch=0; c.buttons=static_cast<core::ButtonMask>(core::Button::Back);
        v=ladderVelocity(c,n,100,true); assert(v && close(v->x,200*n.x) && close(v->y,200*n.y) && close(v->z,-100));
        c.buttons=static_cast<core::ButtonMask>(core::Button::Forward);
    }
    c.view={0,45,0}; auto v=ladderVelocity(c,{-1,0,0},250,false);
    assert(v && close(v->x,0) && close(v->y,141.42135624) && close(v->z,141.42135624));
    c.buttons=static_cast<core::ButtonMask>(core::Button::Jump); assert(!ladderVelocity(c,{-1,0,0},250,false));
    c.buttons=0; assert(!ladderVelocity(c,{1,0,1},250,false));
    assert(!ladderVelocity(c,{-1,0,0},0,false)); c.msec=121; assert(!ladderVelocity(c,{-1,0,0},250,false));
}
void airborneProjection() {
    const LadderAirPhysics physics{800,10,1,250,2000};
    const auto close=[](double a,double b) { return std::abs(a-b)<0.001; };
    for(const auto msec:{8,16,100}) {
        core::BotCommand c; c.msec=static_cast<std::uint8_t>(msec); c.movement.forward=250;
        const auto r=ladderAirStep(c,{0,0,200},physics); assert(r);
        const double dt=double(msec)/1000,expected=msec==8 ? 20:30;
        assert(close(r->velocity.x,expected) && r->velocity.y==0 && close(r->velocity.z,200-800*dt));
        assert(close(r->displacement.x,expected*dt) && close(r->displacement.z,200*dt-400*dt*dt));
        auto v=r->velocity;
        for(int i=0;i<4;++i) { const auto next=ladderAirStep(c,v,physics); assert(next); v=next->velocity; }
        assert(close(v.x,30)); // Holding forward does not acquire250 air speed.
        c.movement.forward=0; c.movement.side=-250;
        const auto side=ladderAirStep(c,{100,0,0},physics); assert(side && side->velocity.x==100 && close(side->velocity.y,expected));
        c.movement.side=0; c.movement.forward=250;
        const auto fast=ladderAirStep(c,{100,0,0},physics); assert(fast && fast->velocity.x==100);
    }
    core::BotCommand c; c.msec=16; c.movement.forward=500; c.view.yaw=90;
    const auto limited=ladderAirStep(c,{}, {800,10,1,20,2000}); assert(limited && close(limited->velocity.y,3.2));
    c.msec=100; c.movement.forward=0;
    const auto clamped=ladderAirStep(c,{1000,0,1000},{800,10,1,250,100});
    assert((clamped && clamped->displacement==model::NavVector3{10,0,10} && clamped->velocity==model::NavVector3{100,0,60}));
    for(int mode=0;mode<7;++mode) {
        auto p=physics; auto command=c; model::NavVector3 v{};
        if(mode==0) p.gravity=0;
        if(mode==1) p.airAcceleration=-1;
        if(mode==2) p.friction=(std::numeric_limits<double>::quiet_NaN)();
        if(mode==3) command.msec=0;
        if(mode==4) command.movement.up=1;
        if(mode==5) command.buttons=static_cast<core::ButtonMask>(core::Button::Jump);
        if(mode==6) v.x=(std::numeric_limits<float>::infinity)();
        assert(!ladderAirStep(command,v,p));
    }
}
void jumpingOffLadder() {
    const LadderAirPhysics physics{800,10,1,250,2000};
    for(const auto normal:std::array<model::NavVector3,4>{{{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}}})
        for(const auto msec:{8,16,100}) {
            core::BotCommand command; command.msec=static_cast<std::uint8_t>(msec);
            command.buttons=static_cast<core::ButtonMask>(core::Button::Jump);
            const auto r=ladderJumpAirStep(command,normal,physics);
            if(!r) { std::fprintf(stderr,"missing airborne ladder jump prediction\n"); std::exit(1); }
            const double dt=double(msec)/1000;
            assert(std::abs(r->velocity.x-270*normal.x)<0.001 && std::abs(r->velocity.y-270*normal.y)<0.001);
            assert(std::abs(r->velocity.z+800*dt)<0.001);
            assert(std::abs(r->displacement.x-270*normal.x*dt)<0.001 && std::abs(r->displacement.z+400*dt*dt)<0.001);
            command.buttons=0; assert(!ladderJumpAirStep(command,normal,physics));
            command.buttons=static_cast<core::ButtonMask>(core::Button::Jump)|static_cast<core::ButtonMask>(core::Button::Duck);
            assert(!ladderJumpAirStep(command,normal,physics));
        }
    core::BotCommand command; command.msec=16; command.buttons=static_cast<core::ButtonMask>(core::Button::Jump);
    assert(!ladderJumpAirStep(command,{0,0,1},physics));
    assert(!ladderJumpAirStep(command,{},physics));
}
void climbingAndObservedExit() {
    for(bool up:{false,true}) for(std::uint64_t us:{8000U,16000U,100000U}) {
        const auto p=plan(up); Ladder ladder(binding,p); auto s=actor(p); enter(ladder,p,s);
        std::uint64_t now=120000; bool exiting=false;
        for(unsigned n=0;n<1000;++n) {
            ++s.tick.value; now+=us; s.elapsedUs=us;
            const auto f=feedback(ladder,p,s,now,true); const auto d=ladder.update(f);
            assert(d.accepted && !d.terminalEvent && d.link.linkId==p.link.linkId);
            const auto replay=ladder.update(f); assert(!replay.accepted && replay.intent.speed==0);
            if(d.state==LadderState::Exit) { exiting=true; break; }
            const auto cmd=core::Motor::command(d.intent,{},250,us,true); assert(cmd);
            assert(cmd.command->buttons==static_cast<core::ButtonMask>(up ? core::Button::Forward:core::Button::Back));
            assert(cmd.command->movement.up==0);
            // Independent standard-CS vertical projection only. Exit physics is
            // deliberately not simulated as analog walking while on a ladder.
            const double radians=cmd.command->view.pitch*3.14159265358979323846/180;
            const double vz=(up ? 1:-1)*200*(std::cos(radians)-std::sin(radians));
            s.velocity->z=static_cast<float>(vz); s.position->z+=static_cast<float>(vz*double(us)/1000000);
        }
        assert(exiting);
        const auto aborted=ladder.abort(); assert(aborted.state==LadderState::Aborted && aborted.terminalEvent && aborted.intent.speed==0);
    }
    for(bool up:{false,true}) {
        const auto p=plan(up); Ladder ladder(binding,p); auto s=actor(p); enter(ladder,p,s);
        s.tick={4}; s.position=p.dismount;
        assert(ladder.update(feedback(ladder,p,s,160000,true)).state==LadderState::Exit);
        // Reaching the exit height has not completed traversal; verified input is required.
        s.tick={5}; auto f=feedback(ladder,p,s,200000,true);
        f.inspection->exitIntent=core::MovementIntent{};
        auto d=ladder.update(f); assert(d.state==LadderState::Exit && !d.terminalEvent);
        // Scripted trusted observation of a detached actor at the target. This
        // tests lifecycle acceptance, not a claim of simulated host dismount.
        s.tick={6}; s.position=p.end; s.ladder->touching=false;
        s.velocity->z=-280; // Validated exit arcs exceed the climb-loss threshold.
        f=feedback(ladder,p,s,240000); f.inspection->exitIntent=core::MovementIntent{};
        d=ladder.update(f); assert(d.state==LadderState::Exit && !d.terminalEvent);
        s.tick={7}; f=feedback(ladder,p,s,280000); f.inspection->exitIntent=core::MovementIntent{};
        d=ladder.update(f); assert(d.state==LadderState::Exit && !d.terminalEvent);
        s.tick={8}; s.grounded=true;
        s.velocity->z=0;
        d=ladder.update(feedback(ladder,p,s,320000)); assert(d.state==LadderState::Support && !d.terminalEvent);
        s.tick={9}; d=ladder.update(feedback(ladder,p,s,360000));
        assert(d.state==LadderState::Complete && d.terminalEvent && d.intent.speed==0);
        assert(!ladder.abort().terminalEvent && !ladder.update(feedback(ladder,p,s,360000)).terminalEvent);
    }
}
void failuresAndReacquire() {
    const auto p=plan();
    for(int mode=0;mode<7;++mode) {
        Ladder ladder(binding,p); auto s=actor(p); s.position->x-=10; auto f=feedback(ladder,p,s,40000);
        if(mode==0) f.inspection->stamp.tick={99};
        if(mode==1) f.inspection->target.x++;
        if(mode==2) f.inspection->queries=22;
        if(mode==3) f.movement.ladder->linkId++;
        if(mode==4) f.inspection->pathClear=false;
        if(mode==5) f.movement.actor.generation.value++;
        if(mode==6) f.climbing.reset();
        const auto d=ladder.update(f); assert(d.terminalEvent && d.intent.speed==0);
        assert(!ladder.update(f).terminalEvent && !ladder.abort().terminalEvent);
    }
    for(int mode=0;mode<4;++mode) {
        Ladder ladder(binding,p); auto s=actor(p); enter(ladder,p,s); s.position->z=80; s.tick={4}; s.ladder->touching=false;
        auto d=ladder.update(feedback(ladder,p,s,160000)); assert(d.state==LadderState::Reacquire && d.reacquires==1 && d.intent.speed==0);
        s.tick={5}; auto f=feedback(ladder,p,s,200000);
        if(mode==0) f.inspection->pathClear=false;
        if(mode==1) { f.movement.velocity->z=-300; f.inspection->velocity=*f.movement.velocity; }
        if(mode==2) f.nowUs=30120000;
        if(mode<3) { d=ladder.update(f); assert(d.terminalEvent && d.intent.speed==0); continue; }
        d=ladder.update(f); assert(d.state==LadderState::Reacquire);
        s.tick={6}; s.ladder->touching=true;
        d=ladder.update(feedback(ladder,p,s,240000,true)); assert(d.state==LadderState::ClimbUp && d.reacquires==1);
        s.tick={7}; s.ladder->touching=false;
        d=ladder.update(feedback(ladder,p,s,280000)); assert(d.reason==LadderReason::ReacquireExhausted && d.terminalEvent);
    }
    Ladder noExit(binding,p); auto s=actor(p); enter(noExit,p,s); s.position=p.dismount; s.tick={4};
    assert(noExit.update(feedback(noExit,p,s,160000,true)).state==LadderState::Exit); s.tick={5};
    assert(noExit.update(feedback(noExit,p,s,200000,true)).reason==LadderReason::MissingObservation);
    for(bool wrongArea:{false,true}) {
        Ladder ending(binding,p); s=actor(p); enter(ending,p,s); s.position=p.dismount; s.tick={4};
        assert(ending.update(feedback(ending,p,s,160000,true)).state==LadderState::Exit);
        s.position=p.end; s.tick={5}; s.grounded=true;
        assert(ending.update(feedback(ending,p,s,200000,true)).state==LadderState::Support);
        s.tick={6}; auto f=feedback(ending,p,s,240000,true);
        if(wrongArea) { f.movement.ladder->touching=false; f.climbing=false; f.inspection->support->area=p.link.from; }
        const auto d=ending.update(f);
        if(wrongArea) assert(d.state==LadderState::Failed && d.reason==LadderReason::MissingSupport);
        else assert(d.state==LadderState::Support && !d.terminalEvent); // Must detach even with verified ground.
    }
}
void observedModeHandoff() {
    for(bool up:{false,true}) {
        const auto p=plan(up); Ladder ladder(binding,p); auto s=actor(p); enter(ladder,p,s);
        s.tick={4}; s.position=p.dismount; s.ladder->touching=false;
        auto d=ladder.update(feedback(ladder,p,s,160000,true));
        assert(d.state==LadderState::Exit && d.intent.speed==0 && !d.terminalEvent);
        s.tick={5}; s.position=p.end; s.grounded=true;
        d=ladder.update(feedback(ladder,p,s,200000,true)); assert(d.state==LadderState::Support && !d.terminalEvent);
        s.tick={6}; d=ladder.update(feedback(ladder,p,s,240000,true));
        assert(d.state==LadderState::Support && d.intent.speed==0 && !d.terminalEvent);
        s.tick={7}; d=ladder.update(feedback(ladder,p,s,280000,false));
        assert(d.state==LadderState::Complete && d.terminalEvent);
    }
    const auto p=plan(); Ladder wrong(binding,p); auto s=actor(p); enter(wrong,p,s);
    s.tick={4}; s.position->z=80; s.ladder->touching=false;
    assert(wrong.update(feedback(wrong,p,s,160000,true)).reason==LadderReason::WrongContact);
}
void earlyLowerExit() {
    const auto p=plan(false); Ladder ladder(binding,p); auto s=actor(p); enter(ladder,p,s);
    s.position=p.dismount; s.position->z+=8; s.tick={4};
    const auto d=ladder.update(feedback(ladder,p,s,160000,true));
    if(d.state!=LadderState::Exit) { std::fprintf(stderr,"lower exit starts too late for airborne dismount\n"); std::exit(1); }
    assert(!d.terminalEvent && d.intent.speed==0);
}
void jumpDispatchLifecycle() {
    for(int mode=0;mode<5;++mode) {
        const auto p=plan(false); Ladder ladder(binding,p); auto s=actor(p); enter(ladder,p,s);
        s.position=p.dismount; s.position->z+=8; s.tick={4};
        assert(ladder.update(feedback(ladder,p,s,160000,true)).state==LadderState::Exit);
        s.tick={5}; auto f=feedback(ladder,p,s,200000,true); f.inspection->exitIntent=MovementIntent{};
        f.inspection->exitIntent->jump=ActionRequest::Press;
        const auto press=ladder.update(f);
        if(press.intent.jump!=ActionRequest::Press) { std::fprintf(stderr,"verified ladder jump was not requested\n"); std::exit(1); }
        assert(press.pressTick==s.tick && !press.terminalEvent);
        LadderDispatch sent{binding,p.link.sourceId,p.link.generation,p.link.linkId,s.tick,{6},mode!=1};
        auto wrong=sent; ++wrong.linkId; assert(!ladder.reportJumpDispatch(wrong));
        wrong=sent; ++wrong.binding.routeGeneration; assert(!ladder.reportJumpDispatch(wrong));
        wrong=sent; wrong.commandTick={4}; assert(!ladder.reportJumpDispatch(wrong));
        if(mode!=2) { assert(ladder.reportJumpDispatch(sent)); assert(!ladder.reportJumpDispatch(sent)); }
        s.tick={6}; s.position->x-=5; s.ladder->touching=false;
        f=feedback(ladder,p,s,240000,false); f.inspection->exitIntent=MovementIntent{};
        if(mode==3) f.inspection->exitIntent->jump=ActionRequest::Press;
        auto d=ladder.update(f);
        if(mode==1 || mode==2 || mode==3) {
            assert(d.terminalEvent && d.intent.jump==ActionRequest::Release);
            assert(d.reason==(mode==1 ? LadderReason::DispatchRejected:mode==2 ? LadderReason::MissingDispatch:LadderReason::MissingObservation));
            assert(!ladder.abort().terminalEvent); continue;
        }
        assert(d.state==LadderState::Exit && d.intent.jump!=ActionRequest::Press && !d.terminalEvent);
        if(mode==4) { assert(ladder.abort().terminalEvent); assert(!ladder.reportJumpDispatch(sent)); continue; }
        s.tick={7}; s.position=p.end; s.grounded=true;
        assert(ladder.update(feedback(ladder,p,s,280000)).state==LadderState::Support);
        s.tick={8}; d=ladder.update(feedback(ladder,p,s,320000));
        assert(d.state==LadderState::Complete && d.terminalEvent && d.link.linkId==p.link.linkId);
    }
    const auto p=plan(false); Ladder waiting(binding,p); auto s=actor(p); enter(waiting,p,s);
    s.tick={4}; s.position=p.dismount; s.position->z+=8;
    assert(waiting.update(feedback(waiting,p,s,160000,true)).state==LadderState::Exit);
    s.tick={5}; auto f=feedback(waiting,p,s,200000,true); f.inspection->exitIntent=MovementIntent{};
    f.inspection->exitIntent->jump=ActionRequest::Press;
    assert(waiting.update(f).intent.jump==ActionRequest::Press);
    s.tick={6}; f=feedback(waiting,p,s,240000,true); f.inspection->exitIntent=MovementIntent{};
    f.inspection->exitIntent->jump=ActionRequest::Press;
    auto d=waiting.update(f); assert(d.state==LadderState::Exit && d.intent.jump==ActionRequest::Release && !d.terminalEvent);
    s.tick={7}; f=feedback(waiting,p,s,2160000,true);
    d=waiting.update(f); assert(d.reason==LadderReason::Timeout && d.terminalEvent);
    assert(!waiting.reportJumpDispatch({binding,p.link.sourceId,p.link.generation,p.link.linkId,{5},{6},true}));
}
void measuredExitApproach() {
    for(int mode=0;mode<3;++mode) {
        const auto p=plan(false); Ladder ladder(binding,p); auto s=actor(p); enter(ladder,p,s);
        s.tick={4}; s.position=p.dismount;
        assert(ladder.update(feedback(ladder,p,s,160000,true)).state==LadderState::Exit);
        s.tick={5}; s.position=model::NavVector3{-19,32,36}; s.grounded=true; s.ladder->touching=false;
        auto f=feedback(ladder,p,s,200000); f.inspection->support.reset();
        f.inspection->worldFloor=runtime::FloorObservation{0,{0,0,1},true}; f.inspection->groundPathClear=true;
        if(mode==1) f.inspection->groundPathClear.reset();
        if(mode==2) f.inspection->worldFloor->height=20;
        const auto d=ladder.update(f);
        if(mode==0) {
            if(d.terminalEvent || d.intent.speed<=0) { std::fprintf(stderr,"measured exit approach rejected outside NAV\n"); std::exit(1); }
            assert(d.state==LadderState::Exit && d.intent.direction.x<0);
        } else assert(d.terminalEvent && d.intent.speed==0 && d.reason==LadderReason::MissingSupport);
    }
}
void walkOwnsLadder() {
    for(bool up:{false,true}) {
    auto p=plan(up); p.start.x=up ? -49.0f:49.0f; p.link.entry.x=p.start.x;
    p.end.x=up ? 49.0f:-49.0f; p.link.exit.x=p.end.x;
    auto b=binding; b.step=0;
    const auto mesh=route_test::snapshot({{1,{{-100,0,0},{-20,64,0},0,0}},
        {2,{{20,0,128},{100,64,128},128,128}}});
    const enrichment::NavMapFingerprint fp{};
    const auto graph=query::NavGraph::compose(mesh,fp,{fp,{p.link}},{2,1,100000},{1,100000}); assert(graph);
    const auto route=query::NavRouteSearch::search(**graph.value,{p.link.from,p.link.to,{2,100000},false}); assert(route);
    const auto corridor=corridor::Corridor::build(**graph.value,*route.value,{16,16},{1,100000,1}); assert(corridor);
    const auto index=query::NavSpatialIndex::build(mesh,{2,3,100000}); assert(index);
    struct NoQueries final : runtime::IWorldQueries {
        unsigned count{};
        runtime::WorldQueryResult query(const runtime::QueryRequest&) override { ++count; return {}; }
    } world;
    WalkLimits limits{{21,4,48,16,18,18,64,4,2,0.7},120,1,1,3}; limits.ladder=LadderLimits{};
    Walk walk(b,corridor.value,{p.end.x,p.end.y,p.end.z-36},limits);
    auto s=actor(p);
    const auto packet=[&](bool climbing) {
        Ladder reference(b,p); auto f=feedback(reference,p,s,s.tick.value*40000);
        f.inspection->step=0; f.inspection->target=walk.ladderTarget(p,*s.position);
        return LadderObservation{p,*f.inspection,*s.ladder,climbing};
    };
    auto observation=packet(false);
    auto d=walk.update(s,**index.value,s.map,world,40000,0,{},observation);
    if(d.ladderState!=LadderState::Align) { std::fprintf(stderr,"Walk did not enter selected ladder\n"); std::exit(1); }
    assert(d.primitiveEvent==PrimitiveEvent::Entered && walk.step()==0 && world.count==0 && !d.terminalEvent);
    assert(walk.selectedLadderLink()->linkId==p.link.linkId);
    s.tick={2}; d=walk.update(s,**index.value,s.map,world,80000,0,{},packet(false)); assert(d.ladderState==LadderState::Contact);
    s.tick={3}; s.position=p.mount; s.grounded=false; s.ladder->touching=true;
    d=walk.update(s,**index.value,s.map,world,120000,0,{},packet(true)); assert(d.ladderState==(up ? LadderState::ClimbUp:LadderState::ClimbDown));
    s.tick={4}; s.position=p.dismount; if(!up) s.position->z+=8;
    d=walk.update(s,**index.value,s.map,world,160000,0,{},packet(true)); assert(d.ladderState==LadderState::Exit && walk.step()==0);
    s.tick={5}; observation=packet(true); observation.inspection.exitIntent=MovementIntent{};
    if(!up) observation.inspection.exitIntent->jump=ActionRequest::Press;
    d=walk.update(s,**index.value,s.map,world,200000,0,{},observation);
    if(!up) {
        assert(d.ladderPressTick==s.tick && d.intent.jump==ActionRequest::Press);
        const LadderDispatch sent{b,p.link.sourceId,p.link.generation,p.link.linkId,s.tick,{6},true};
        assert(walk.reportLadderDispatch(sent)); assert(!walk.reportLadderDispatch(sent));
    }
    assert(walk.step()==0); // A predicted exit and successful dispatch cannot advance the route.
    s.tick={6}; s.position=p.end; s.grounded=true; s.ladder->touching=false;
    d=walk.update(s,**index.value,s.map,world,240000,0,{},packet(false)); assert(d.ladderState==LadderState::Support && walk.step()==0);
    s.tick={7}; d=walk.update(s,**index.value,s.map,world,280000,0,{},packet(false));
    assert(d.ladderState==LadderState::Complete && d.primitiveEvent==PrimitiveEvent::Complete && walk.step()==1);
    assert(d.state==WalkState::Running && !d.terminalEvent && d.intent.jump==ActionRequest::Release && !walk.selectedLadderLink());
    const auto replay=walk.update(s,**index.value,s.map,world,280000,0,{},packet(false));
    assert(!replay.accepted && walk.step()==1 && world.count==0);
    for(int mode=0;mode<7;++mode) {
        auto profile=limits; if(mode==6) profile.ladder.reset();
        Walk invalid(b,corridor.value,{p.end.x,p.end.y,p.end.z-36},profile); s=actor(p);
        auto input=packet(false); input.inspection.target=p.start;
        if(mode==0) ++input.plan.link.linkId;
        if(mode==1) input.plan.link.additionalCost+=1;
        if(mode==2) input.inspection.queries=22;
        if(mode==3) input.inspection.stamp.tick={2};
        if(mode==4) ++input.contact.generation;
        const auto failure=invalid.update(s,**index.value,s.map,world,40000,0,{},mode==5 ? std::optional<LadderObservation>{}:input);
        assert(failure.terminalEvent && failure.state==WalkState::Failed && invalid.step()==0 && world.count==0);
        assert(failure.intent.speed==0 && !invalid.abort().terminalEvent);
    }
    }
}
void jumpExitCandidate() {
    const LadderAirPhysics physics{800,10,1,250,2000};
    const auto mesh=route_test::snapshot({{1,{{-100,0,0},{-20,64,0},0,0}},
        {2,{{-100,0,128},{-32,64,128},128,128}}});
    const auto built=query::NavSpatialIndex::build(mesh,{100,199,100000}); assert(built);
    for(bool up:{false,true}) {
        auto p=plan(up); p.end.x=up ? -49.0f:-33.0f; p.link.exit.x=p.end.x;
        auto s=actor(p); s.position=p.dismount; s.position->z+=8; s.grounded=false;
        for(const auto msec:std::array<std::uint8_t,3>{8,16,100}) {
            const auto r=planJumpLadderExit(p,s,true,physics,msec,**built.value,s.map);
            if(!r) { std::fprintf(stderr,"missing ladder jump exit candidate\n"); std::exit(1); }
            assert(r.value->intent.jump==ActionRequest::Press && r.value->command.buttons==static_cast<core::ButtonMask>(core::Button::Jump));
            assert(r.value->landing.x>=-100 && r.value->landing.x<=(up ? -32:-20) && r.value->landing.z==p.end.z);
            assert(r.value->columnCount<=18 && r.value->simulatedFrames<=256);
        }
        s.grounded=true; assert(!planJumpLadderExit(p,s,true,physics,16,**built.value,s.map)); s.grounded=false;
        assert(!planJumpLadderExit(p,s,true,physics,0,**built.value,s.map));
        assert(planJumpLadderExit(p,s,true,physics,16,**built.value,{99}).reason==LadderExitReason::StaleNavigation);
    }
}
void upperExitCandidate() {
    const LadderAirPhysics physics{800,10,1,250,2000};
    for(bool across:{false,true}) {
        auto p=plan(); if(!across) { p.end.x=-33; p.link.exit.x=-33; }
        const float left=across ? 20.0f:-100.0f,right=across ? 100.0f:-20.0f;
        const auto mesh=route_test::snapshot({{1,{{-100,0,0},{-20,64,0},0,0}},
            {2,{{left,0,128},{right,64,128},128,128}}});
        const auto built=query::NavSpatialIndex::build(mesh,{100,199,100000}); assert(built);
        auto s=actor(p); s.position=p.dismount; s.grounded=false;
        for(const auto msec:std::array<std::uint8_t,3>{8,16,100}) {
            const auto r=planUpperLadderExit(p,s,true,164.5,physics,msec,**built.value,s.map);
            assert(r && r.value->columnCount>0 && r.value->columnCount<=18 && r.value->simulatedFrames<=256);
            assert(r.value->command.msec==msec && r.value->command.view.pitch==-45);
            assert(r.value->command.buttons==static_cast<core::ButtonMask>(core::Button::Forward));
            assert(r.value->landing.x>=left && r.value->landing.x<=right && r.value->landing.z==164);
            for(unsigned i=0;i<r.value->columnCount;++i) {
                const auto& c=r.value->columns[i]; assert(c.bottom.isFinite() && c.top.isFinite());
                assert(c.bottom.x==c.top.x && c.bottom.y==c.top.y && c.bottom.z<=c.top.z);
            }
        }
        assert(planUpperLadderExit(p,s,true,164.5,physics,0,**built.value,s.map).reason==LadderExitReason::InvalidInput);
        assert(planUpperLadderExit(p,s,true,164.5,physics,16,**built.value,{99}).reason==LadderExitReason::StaleNavigation);
        assert(planUpperLadderExit(p,s,false,164.5,physics,16,**built.value,s.map).reason==LadderExitReason::NoLanding);
        p.link.direction=enrichment::NavLinkDirection::Down;
        assert(planUpperLadderExit(p,s,true,164.5,physics,16,**built.value,s.map).reason==LadderExitReason::Unsupported);
    }
}
}
int main() { directionalMotor(); physicalProjection(); airborneProjection(); jumpingOffLadder(); climbingAndObservedExit(); failuresAndReacquire(); observedModeHandoff(); upperExitCandidate(); jumpExitCandidate(); earlyLowerExit(); jumpDispatchLifecycle(); walkOwnsLadder(); measuredExitApproach(); }
