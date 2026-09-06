// SPDX-License-Identifier: MPL-2.0
#include "nav/local/ladder.hpp"
#include "nav/local/ladder_physics.hpp"
#include <cassert>
#include <cmath>
#include <utility>
#include <limits>
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
    const LadderAirPhysics physics{800,10,1,250};
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
    const auto limited=ladderAirStep(c,{}, {800,10,1,20}); assert(limited && close(limited->velocity.y,3.2));
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
        d=ladder.update(feedback(ladder,p,s,240000)); assert(d.state==LadderState::Support && !d.terminalEvent);
        s.tick={7}; d=ladder.update(feedback(ladder,p,s,280000)); assert(d.state==LadderState::Support && !d.terminalEvent);
        s.tick={8}; s.grounded=true;
        d=ladder.update(feedback(ladder,p,s,320000)); assert(d.state==LadderState::Complete && d.terminalEvent && d.intent.speed==0);
        assert(!ladder.abort().terminalEvent && !ladder.update(feedback(ladder,p,s,320000)).terminalEvent);
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
}
int main() { directionalMotor(); physicalProjection(); airborneProjection(); climbingAndObservedExit(); failuresAndReacquire(); observedModeHandoff(); }
