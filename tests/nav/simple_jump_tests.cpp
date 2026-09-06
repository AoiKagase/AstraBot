// SPDX-License-Identifier: MPL-2.0
#include "nav/local/simple_jump.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace astrabot;
using namespace astrabot::nav;
using namespace astrabot::nav::local;
namespace {
constexpr Binding binding{{1},{2,{3}},{4},5,0};
const JumpPlan plan{{1},{2},{50,50,36},{124,50,36},0,2};
const JumpLimits limits{120,100,180,16,16,5,96,32,4,21,2000000,200000,1500000,200000};
runtime::MovementSnapshot actor() {
    runtime::MovementSnapshot s; s.agent=binding.agent; s.actor=binding.actor; s.map=binding.map; s.tick={1};
    s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true; s.ducked=false;
    s.position=plan.takeoff; s.velocity=model::NavVector3{120,0,0}; s.view=model::NavVector3{};
    s.hull=runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f; s.elapsedUs=40000; return s;
}
JumpFeedback feedback(runtime::MovementSnapshot s,std::uint64_t time) {
    JumpFeedback f; f.binding=binding; f.movement=s; f.nowUs=time;
    JumpInspection proof; proof.stamp={s.agent,s.actor,s.map,s.tick,binding.routeGeneration,0}; proof.queries=5;
    proof.origin=*s.position; proof.hull=*s.hull; proof.takeoff=plan.takeoff; proof.landing=plan.landing;
    proof.step=binding.step; proof.velocity=*s.velocity;
    const auto area=model::NavAreaId{s.position->x<100 ? 1U:2U};
    if(s.grounded==true) proof.support=GroundedTarget{*s.position,area,{0,{0,0,1},true}};
    proof.approach=GroundedTarget{plan.takeoff,{1},{0,{0,0,1},true}};
    proof.approachClear=proof.takeoffClear=proof.flightClear=proof.landingClear=true;
    f.inspection=proof; return f;
}
core::TickId press(SimpleJump& jump,runtime::MovementSnapshot& s) {
    for(std::uint64_t i=1;i<=3;++i) {
        s.tick={i}; const auto d=jump.update(feedback(s,i*40000));
        assert(d.accepted);
        if(i==3) { assert(d.intent.jump==ActionRequest::Press && d.state==JumpState::Takeoff); return d.pressTick; }
        assert(d.intent.jump!=ActionRequest::Press);
    }
    assert(false); return {};
}
void physics() {
    for(std::uint64_t us : {8000U,16000U,100000U}) {
        SimpleJump jump(binding,plan,limits); auto s=actor(); s.view->y=90; s.velocity=model::NavVector3{};
        s.position->x=30; const double dt=double(us)/1000000;
        bool airborne=false,landed=false,complete=false; unsigned presses=0;
        std::optional<JumpDispatch> dispatch; double vz=0;
        for(std::uint64_t tick=1;tick<600;++tick) {
            s.tick={tick}; s.elapsedUs=us;
            auto f=feedback(s,tick*us); f.dispatch=dispatch; dispatch.reset();
            const auto d=jump.update(f);
            if(d.state==JumpState::Failed || d.state==JumpState::Aborted)
                std::fprintf(stderr,"jump us=%llu tick=%llu reason=%u pos=(%.3f,%.3f,%.3f)\n",
                    static_cast<unsigned long long>(us),static_cast<unsigned long long>(tick),unsigned(d.reason),s.position->x,s.position->y,s.position->z);
            assert(d.state!=JumpState::Failed && d.state!=JumpState::Aborted);
            assert(jump.update(f).intent.jump!=ActionRequest::Press); // same-tick replay
            if(d.state==JumpState::Complete) { assert(landed && d.terminalEvent); complete=true; break; }
            const auto motor=core::Motor::command(d.intent,{s.view->x,s.view->y,s.view->z},250,us,true); assert(motor);
            const auto replay=core::Motor::command(d.intent,{},250,us,false); assert(replay);
            assert((replay.command->buttons&static_cast<core::ButtonMask>(core::Button::Jump))==0);
            const auto& c=*motor.command;
            if(c.buttons&static_cast<core::ButtonMask>(core::Button::Jump)) {
                assert(++presses==1 && s.grounded==true); vz=268.3281573; s.grounded=false;
                dispatch=JumpDispatch{binding,d.pressTick,{tick+1},true};
            }
            const double yaw=c.view.yaw*3.14159265358979323846/180;
            const double vx=c.movement.forward*std::cos(yaw)+c.movement.side*std::sin(yaw);
            const double vy=c.movement.forward*std::sin(yaw)-c.movement.side*std::cos(yaw);
            s.velocity=model::NavVector3{static_cast<float>(vx),static_cast<float>(vy),static_cast<float>(vz)};
            s.position->x+=static_cast<float>(vx*dt); s.position->y+=static_cast<float>(vy*dt);
            s.view=model::NavVector3{c.view.pitch,c.view.yaw,c.view.roll};
            if(s.grounded==false) {
                airborne=true; s.position->z+=static_cast<float>(vz*dt-400*dt*dt); vz-=800*dt;
                if(s.position->z<=36) { assert(s.position->x>100); s.position->z=36; s.grounded=true; vz=0; landed=true; }
            }
        }
        assert(complete && airborne && presses==1 && std::abs(s.position->x-plan.landing.x)<=limits.landingRadius);
        assert(!jump.abort().terminalEvent);
    }
}
void failures() {
    for(bool dispatched : {false,true}) {
        SimpleJump jump(binding,plan,limits); auto s=actor(); const auto p=press(jump,s);
        s.tick={4}; auto f=feedback(s,160000);
        f.dispatch=JumpDispatch{binding,p,p,dispatched};
        const auto d=jump.update(f);
        assert(d.state==JumpState::Failed && d.terminalEvent);
        assert(d.reason==(dispatched ? JumpReason::StaleDispatch:JumpReason::DispatchRejected));
        assert(d.intent.speed==0 && d.intent.jump==ActionRequest::Release);
    }
    for(int mode=0;mode<10;++mode) {
        SimpleJump jump(binding,plan,limits); auto s=actor(); const auto p=press(jump,s);
        s.tick={4}; s.grounded=false; s.position->z=45;
        auto f=feedback(s,160000); f.dispatch=JumpDispatch{binding,p,{4},true};
        if(mode==0) f.dispatch.reset();
        if(mode==1) f.dispatch->dispatched=false;
        if(mode==2) ++f.dispatch->commandTick.value;
        if(mode==3) { ++f.movement.actor.generation.value; }
        if(mode==4) { ++f.inspection->stamp.tick.value; }
        auto d=jump.update(f);
        if(mode<5) assert(d.terminalEvent && (d.state==JumpState::Failed || d.state==JumpState::Aborted));
        else {
            assert(d.state==JumpState::Airborne);
            s.tick={5};
            if(mode==5) d=jump.update(feedback(s,160000+limits.airborneTimeoutUs));
            else if(mode==6) d=jump.abort();
            else {
                s.grounded=true; s.position=model::NavVector3{134,50,36}; f=feedback(s,200000);
                if(mode==7) f.inspection->support->area={1};
                if(mode==8) f.inspection->support.reset();
                d=jump.update(f);
                if(mode==9) {
                    assert(d.state==JumpState::Recover); ++s.tick.value; s.grounded=false;
                    d=jump.update(feedback(s,240000)); assert(d.reason==JumpReason::LostSupport);
                }
            }
            assert(d.terminalEvent);
        }
        assert(d.intent.speed==0 && d.intent.jump==ActionRequest::Release);
        assert(!jump.update(f).terminalEvent && !jump.abort().terminalEvent);
    }
    for(int mode=0;mode<5;++mode) {
        SimpleJump jump(binding,plan,limits); auto s=actor();
        auto f=feedback(s,40000); assert(jump.update(f).state==JumpState::Align);
        ++s.tick.value; assert(jump.update(feedback(s,80000)).state==JumpState::Accelerate);
        ++s.tick.value; f=feedback(s,120000);
        if(mode==0) f.inspection->flightClear=false;
        if(mode==1) f.inspection->takeoffClear.reset();
        if(mode==2) f.inspection->landingClear=false;
        if(mode==3) f.inspection->origin.x++;
        if(mode==4) f.movement.ducked=true;
        const auto d=jump.update(f); assert(d.state==JumpState::Failed && d.intent.jump!=ActionRequest::Press);
    }
    auto forbidden=plan; forbidden.sourceAttributes=8; SimpleJump noJump(binding,forbidden,limits);
    assert(noJump.update(feedback(actor(),1)).reason==JumpReason::InvalidInput);
    SimpleJump noTakeoff(binding,plan,limits); auto s=actor(); const auto p=press(noTakeoff,s);
    s.tick={4}; auto f=feedback(s,120000+limits.takeoffTimeoutUs); f.dispatch=JumpDispatch{binding,p,{4},true};
    assert(noTakeoff.update(f).reason==JumpReason::TakeoffTimeout);
}
void accelerationRequiresLaunchProofBeforePress() {
    SimpleJump jump(binding,plan,limits); auto s=actor(); s.velocity=model::NavVector3{};
    for(std::uint64_t tick=1;tick<=3;++tick) {
        s.tick={tick}; auto f=feedback(s,tick*40000);
        f.inspection->flightClear.reset(); f.inspection->landingClear.reset(); f.inspection->velocity.reset();
        const auto d=jump.update(f); assert(d.accepted && d.intent.jump!=ActionRequest::Press);
        if(tick==3) assert(d.state==JumpState::Accelerate && d.intent.speed>0);
    }
    s.tick={4}; s.velocity=model::NavVector3{120,0,0}; auto f=feedback(s,160000);
    f.inspection->velocity.reset();
    const auto d=jump.update(f);
    assert(d.state==JumpState::Failed && d.reason==JumpReason::Blocked && d.intent.jump!=ActionRequest::Press);
}
}
int main() { physics(); failures(); accelerationRequiresLaunchProofBeforePress(); }
