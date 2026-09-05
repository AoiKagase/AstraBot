// SPDX-License-Identifier: MPL-2.0
#include "nav/local/door_wait.hpp"
#include <cassert>
#include <limits>
using namespace astrabot;
using namespace astrabot::nav;
using namespace astrabot::nav::local;
namespace {
Binding binding() { return {{1},{2,{3}},{4},5,0}; }
DoorWaitFeedback observation(std::uint64_t tick=10, std::uint64_t now=0) {
    DoorWaitFeedback f; f.binding=binding(); f.nowUs=now;
    f.requested={f.binding.agent,f.binding.actor,f.binding.map,{tick},f.binding.routeGeneration,2};
    f.observed.stamp=f.requested; f.observed.kind=runtime::QueryKind::Door;
    f.observed.error=runtime::QueryError::None;
    f.observed.door=runtime::DoorObservation{42,false,true};
    f.useView=core::IntentVector{0,90,0}; return f;
}
void neutral(const DoorWaitDecision& d) {
    assert(d.intent.speed==0 && d.intent.direction.x==0 && d.intent.direction.y==0 &&
        d.intent.direction.z==0 && d.intent.lateralCorrection==0 && !d.intent.view &&
        d.intent.use==ActionRequest::None && d.intent.jump==ActionRequest::None &&
        d.intent.duck==ActionRequest::None);
}
void opensAndPressReplay() {
    for(std::uint64_t frame : {8000ULL,16000ULL,100000ULL}) {
        DoorWait door(binding(),1000000); auto f=observation();
        const auto press=door.update(f);
        assert(press.accepted && !press.terminalEvent && press.state==DoorWaitState::Waiting);
        assert(press.intent.use==ActionRequest::Press && press.intent.speed==0);
        for(int replay=0;replay<4;++replay) {
            const auto motor=core::Motor::command(press.intent,{},250,frame,replay==0);
            assert(motor && motor.command->movement==core::Movement{});
            assert(motor.command->buttons==(replay==0 ? static_cast<core::ButtonMask>(core::Button::Use):0U));
        }
        const auto duplicate=door.update(f);
        assert(!duplicate.accepted && duplicate.reason==DoorWaitReason::StaleTick); neutral(duplicate);
        f=observation(11,frame); f.observed.door->canUse=false;
        auto wait=door.update(f); assert(wait.accepted && wait.state==DoorWaitState::Waiting); neutral(wait);
        f=observation(12,2*frame); f.observed.door->open=true;
        const auto clear=door.update(f);
        assert(clear.accepted && clear.terminalEvent && clear.state==DoorWaitState::Clear); neutral(clear);
        assert(!door.update(f).terminalEvent && !door.abort().terminalEvent);
    }
    DoorWait clear(binding(),1); auto f=observation(); f.observed.door->open=true;
    f.observed.door->canUse=false; f.useView.reset();
    assert(clear.update(f).state==DoorWaitState::Clear);
}
void finiteWait() {
    DoorWait door(binding(),1000000); assert(door.update(observation()).state==DoorWaitState::Waiting);
    neutral(door.update(observation(11,999999)));
    auto f=observation(12,1000000); f.observed.door->open=true;
    auto expired=door.update(f); assert(expired.reason==DoorWaitReason::TimedOut && expired.terminalEvent);
    neutral(expired); assert(!door.update(f).accepted);
    // Large skipped time and near-overflow clocks never wrap a deadline addition.
    const auto max=(std::numeric_limits<std::uint64_t>::max)();
    DoorWait huge(binding(),10); huge.update(observation(10,max-20));
    assert(huge.update(observation(11,max)).reason==DoorWaitReason::TimedOut);
    DoorWait end(binding(),10); end.update(observation(10,max-5));
    f=observation(11,max); f.observed.door->open=true;
    assert(end.update(f).state==DoorWaitState::Clear);
    for(auto now : {0ULL,1ULL}) {
        DoorWait stoppedClock(binding(),10); stoppedClock.update(observation(10,1));
        assert(stoppedClock.update(observation(11,now)).reason==DoorWaitReason::InvalidInput);
    }
}
void passiveTouch() {
    for(bool allowed : {false,true}) {
        DoorWait door(binding(),100); auto f=observation();
        f.passive=true; f.useView.reset(); f.observed.door->canUse=false; f.observed.door->canTouch=allowed;
        auto d=door.update(f); neutral(d);
        if(!allowed) { assert(d.reason==DoorWaitReason::Unusable); continue; }
        assert(d.state==DoorWaitState::Waiting);
        f.requested.tick={11}; f.observed.stamp=f.requested; f.nowUs=100;
        d=door.update(f); assert(d.reason==DoorWaitReason::TimedOut); neutral(d);
    }
}
void invalidEvidence() {
    for(int mode=0;mode<16;++mode) {
        DoorWait door(binding(),1000000); auto f=observation();
        switch(mode) {
        case 0: f.observed.error=runtime::QueryError::Unavailable; break;
        case 1: f.observed.error=runtime::QueryError::BudgetExceeded; break;
        case 2: f.observed.kind=runtime::QueryKind::Clearance; break;
        case 3: f.observed.door.reset(); break;
        case 4: f.observed.door->id=0; break;
        case 5: ++f.observed.stamp.ordinal; break;
        case 6: ++f.observed.stamp.tick.value; break;
        case 7: ++f.observed.stamp.actor.generation.value; break;
        case 8: f.useView.reset(); break;
        case 9: f.useView->x=(std::numeric_limits<double>::quiet_NaN)(); break;
        case 10: f.useView->y=(std::numeric_limits<double>::infinity)(); break;
        case 11: f.useView->x=90; break;
        case 12: f.useView->y=181; break;
        case 13: f.useView->z=51; break;
        case 14: f.requested.ordinal=0; f.observed.stamp=f.requested; break;
        case 15: f.observed.door->canUse=false; break;
        }
        auto rejected=door.update(f); assert(rejected.state==DoorWaitState::Failed && rejected.terminalEvent);
        assert(rejected.reason==(mode==15 ? DoorWaitReason::Unusable:DoorWaitReason::InvalidObservation));
        neutral(rejected); assert(!door.update(observation()).accepted);
    }
    DoorWait invalid(binding(),0); assert(invalid.update(observation()).reason==DoorWaitReason::InvalidInput);
    auto b=binding(); b.agent={}; DoorWait noAgent(b,1);
    assert(noAgent.update(observation()).reason==DoorWaitReason::InvalidInput);
}
void retirement() {
    for(int field=0;field<10;++field) {
        DoorWait door(binding(),1000000); door.update(observation()); auto f=observation(11,1);
        switch(field) {
        case 0: ++f.binding.agent.value; break;
        case 1: ++f.binding.actor.generation.value; break;
        case 2: ++f.binding.map.value; break;
        case 3: ++f.binding.routeGeneration; break;
        case 4: ++f.binding.step; break;
        case 5: ++f.requested.agent.value; break;
        case 6: ++f.requested.actor.generation.value; break;
        case 7: ++f.requested.map.value; break;
        case 8: ++f.requested.routeGeneration; break;
        case 9: ++f.observed.door->id; break;
        }
        const auto r=door.update(f); neutral(r); assert(r.terminalEvent);
        assert(r.reason==(field==9 ? DoorWaitReason::Replaced:DoorWaitReason::InvalidBinding));
        assert(!door.update(observation(12,2)).accepted);
    }
    DoorWait door(binding(),1000000); door.update(observation());
    const auto aborted=door.abort(); neutral(aborted);
    assert(aborted.state==DoorWaitState::Aborted && aborted.terminalEvent);
    assert(!door.abort().terminalEvent && !door.update(observation(11,1)).accepted);
    DoorWait ready(binding(),1000000); assert(ready.abort().terminalEvent);
}
}
int main() { opensAndPressReplay(); finiteWait(); passiveTouch(); invalidEvidence(); retirement(); }
