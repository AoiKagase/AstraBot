// SPDX-License-Identifier: MPL-2.0
#include "nav/local/intent_pump.hpp"
#include <cassert>
#include <limits>
using namespace astrabot::nav;
using namespace astrabot::nav::local;
namespace {
Binding binding() { return {{1},{2,{3}},{4},5,0}; }
runtime::MovementSnapshot snapshot(std::uint64_t tick,std::uint64_t delta) {
    auto b=binding(); runtime::MovementSnapshot s; s.agent=b.agent; s.actor=b.actor; s.map=b.map; s.tick={tick};
    s.elapsedUs=delta; s.kind=runtime::ActorKind::ManagedBot; s.connected=true; s.alive=true; s.joined=true; return s;
}
MovementIntent moving() { MovementIntent i; i.direction={1,0,0}; i.speed=100; i.jump=ActionRequest::Press; return i; }
void rates() {
    for(std::uint64_t delta : {5000ULL,10000ULL,16667ULL,40000ULL,100000ULL}) {
        IntentPump p(binding()); std::uint32_t decisions=0,frames=0,presses=0;
        for(std::uint64_t t=1;t<=1000000/delta;++t) {
            auto s=snapshot(t,delta); auto schedule=p.beginFrame(s); assert(schedule.accepted);
            if(schedule.decisionDue) { ++decisions; assert(p.publish(binding(),s.tick,moving())); }
            auto out=p.take(); assert(out.emit && out.tick==s.tick && out.frameUs==delta); ++frames;
            auto command=astrabot::core::Motor::command(out.intent,{},250,out.frameUs,out.firstFrame);
            assert(command && command.command->movement.forward==100 && command.command->validate());
            const auto jump=static_cast<astrabot::core::ButtonMask>(astrabot::core::Button::Jump);
            assert((command.command->buttons&jump)==(out.firstFrame ? jump:0));
            if(out.firstFrame) ++presses;
            assert(!p.take().emit); // at most one queue submission per frame
        }
        assert(decisions==presses && frames==1000000/delta);
        if(delta<=40000) assert(decisions>=24 && decisions<=25);
        else assert(decisions==frames);
    }
}
void freshness() {
    IntentPump p(binding()); auto s=snapshot(1,10000);
    assert(p.beginFrame(s).decisionDue && p.publish(binding(),s.tick,moving()));
    assert(p.take().firstFrame);
    assert(!p.beginFrame(s).accepted && !p.take().emit);
    s=snapshot(2,120000); auto schedule=p.beginFrame(s);
    assert(schedule.accepted && schedule.decisionDue && schedule.missedDeadlines==2);
    auto out=p.take(); assert(out.emit && out.intent.speed==100 && !out.firstFrame && out.intentAgeUs==120000);
    s=snapshot(3,1); assert(p.beginFrame(s).accepted);
    out=p.take(); assert(out.emit && out.reason==PumpReason::StaleIntent && out.intent.speed==0 && out.intent.jump==ActionRequest::None);
    s=snapshot(4,40000); assert(p.beginFrame(s).decisionDue);
    assert(p.publish(binding(),s.tick,moving())); assert(p.take().intent.speed==100); p.submissionRejected();
    s=snapshot(5,1); assert(p.beginFrame(s).accepted);
    out=p.take(); assert(out.emit && out.intent.speed==0 && out.reason==PumpReason::SubmissionRejected);
    s=snapshot(6,0); assert(p.beginFrame(s).accepted && !p.take().emit);
    s=snapshot(7,1000000); schedule=p.beginFrame(s);
    assert(schedule.decisionDue && schedule.missedDeadlines>=24);
}
void invalidation() {
    for(int n=0;n<5;++n) {
        IntentPump p(binding()); auto s=snapshot(1,10000); assert(p.beginFrame(s).accepted);
        auto b=binding();
        if(n==0) ++b.routeGeneration;
        if(n==1) ++b.map.value;
        if(n==2) ++b.actor.generation.value;
        if(n==3) ++b.agent.value;
        auto tick=s.tick; if(n==4) ++tick.value;
        assert(!p.publish(b,tick,moving()) && p.take().intent.speed==0);
    }
    IntentPump p(binding()); auto s=snapshot(1,10000); assert(p.beginFrame(s).accepted);
    auto bad=moving(); bad.speed=-1;
    assert(!p.publish(binding(),s.tick,bad)); assert(p.take().reason==PumpReason::InvalidIntent);
    s=snapshot(2,10000); s.alive=false;
    assert(!p.beginFrame(s).accepted && !p.take().emit);
    s=snapshot(3,10000); assert(!p.beginFrame(s).accepted); // retired route never revives
    IntentPump overflow(binding());
    s=snapshot(1,std::numeric_limits<std::uint64_t>::max());
    assert(overflow.beginFrame(s).reason==PumpReason::ClockOverflow && !overflow.take().emit);
}
}
int main() { rates(); freshness(); invalidation(); }
