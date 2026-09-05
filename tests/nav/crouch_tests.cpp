// SPDX-License-Identifier: MPL-2.0
#include "nav/local/crouch.hpp"
#include "nav/local/traversal_constraints.hpp"
#include <cassert>
#include <limits>
using namespace astrabot;
using namespace astrabot::nav;
namespace {
constexpr local::Binding binding{{1},{2,{3}},{4},5,0};
const local::CrouchLimits limits{{{-16,-16,-36},{16,16,36}},{{-16,-16,-18},{16,16,18}},1000000};
runtime::MovementSnapshot actor() {
    runtime::MovementSnapshot s; s.agent=binding.agent; s.actor=binding.actor; s.map=binding.map; s.tick={1};
    s.kind=runtime::ActorKind::ManagedBot; s.connected=s.alive=s.joined=s.grounded=true; s.ducked=false;
    s.position=model::NavVector3{50,50,36}; s.hull=limits.standing; return s;
}
struct World final:runtime::IWorldQueries {
    unsigned calls{}; bool clear=true; int failure{};
    runtime::QueryRequest last{};
    runtime::WorldQueryResult query(const runtime::QueryRequest& q) override {
        ++calls; last=q; assert(q.kind==runtime::QueryKind::Clearance && q.start==q.end && q.hull);
        // Both postures preserve feet at z=0; the standing query must include the head.
        assert(q.start.z+q.hull->minimum.z==0);
        runtime::WorldQueryResult r; r.stamp=q.stamp; r.kind=q.kind; r.error=runtime::QueryError::None;
        r.clearance=runtime::ClearanceObservation{clear};
        if(failure==1) ++r.stamp.ordinal;
        if(failure==2) r.error=runtime::QueryError::Unavailable;
        if(failure==3) r.clearance.reset();
        if(failure==4) throw 1;
        return r;
    }
};
void hints() {
    using T=model::NavTraversalKind; using R=local::ConstraintReason;
    assert(local::constraints(T::Walk,0,0).kind==T::Walk);
    assert(local::constraints(T::Walk,0,1).kind==T::Crouch);
    assert(local::constraints(T::Walk,1,0).kind==T::Crouch);
    assert(local::constraints(T::Walk,0,2).kind==T::Jump);
    assert(local::constraints(T::Walk,8,0).noJump);
    assert(local::constraints(T::Crouch,8,0).kind==T::Crouch);
    assert(local::constraints(T::Jump,8,0).reason==R::ConflictingJump);
    assert(local::constraints(T::Walk,2,8).reason==R::ConflictingJump);
    assert(local::constraints(T::Walk,1,2).reason==R::DuckJumpUnsupported);
    assert(local::constraints(T::Jump,1,0).reason==R::DuckJumpUnsupported);
    assert(local::constraints(T::Walk,4,0).reason==R::PreciseUnsupported);
    for(unsigned bit : {16U,32U,64U,128U}) assert(local::constraints(T::Walk,0,static_cast<std::uint8_t>(bit)).reason==R::UnknownAttributes);
    for(auto kind : {T::Ladder,T::Drop,static_cast<T>(255)}) assert(local::constraints(kind,0,0).reason==R::UnsupportedTraversal);
}
void posture() {
    for(std::uint64_t frame : {8000U,16000U,100000U}) {
        auto s=actor(); World world; local::Crouch crouch(binding,limits);
        auto d=crouch.update(s,true,0,world,3,4);
        assert(d.accepted && !d.movementAllowed && d.queries==1 && d.intent.duck==core::ActionRequest::Hold);
        assert(world.last.stamp.ordinal==4 && world.last.start.z==18);
        assert(crouch.update(s,true,frame,world,0,1).reason==local::CrouchReason::StaleTick && world.calls==1);
        ++s.tick.value; s.ducked=true; // flag without observed crouched hull is insufficient
        d=crouch.update(s,true,frame,world,0,1); assert(!d.movementAllowed && d.queries==0);
        ++s.tick.value; s.hull=limits.crouched; s.position->z=18;
        d=crouch.update(s,true,2*frame,world,0,1);
        assert(d.movementAllowed && d.state==local::CrouchState::Crouched && d.intent.duck==core::ActionRequest::Hold);
        // Movement permission is only posture permission, never an arrival event.
        assert(!d.terminalEvent && d.intent.speed==0);
        ++s.tick.value; world.clear=false;
        d=crouch.update(s,false,3*frame,world,0,1);
        assert(!d.movementAllowed && d.reason==local::CrouchReason::Blocked && d.intent.duck==core::ActionRequest::Hold);
        assert(world.last.start.z==36 && world.last.hull->maximum.z==36);
        ++s.tick.value; world.clear=true;
        d=crouch.update(s,false,4*frame,world,0,1);
        assert(!d.movementAllowed && d.intent.duck==core::ActionRequest::Release);
        const auto motor=core::Motor::command(d.intent,{},250,frame,true);
        assert(motor && motor.command->buttons==0 && motor.command->movement==core::Movement{});
        ++s.tick.value; s.ducked=false; s.hull=limits.standing; s.position->z=36;
        d=crouch.update(s,false,5*frame,world,0,1);
        assert(d.movementAllowed && d.state==local::CrouchState::Standing);
    }
}
void failures() {
    for(int mode=0;mode<8;++mode) {
        auto s=actor(); World world; local::Crouch crouch(binding,limits);
        auto d=crouch.update(s,true,0,world,0,1); assert(d.accepted);
        ++s.tick.value; s.ducked=true; s.hull=limits.crouched; s.position->z=18;
        assert(crouch.update(s,true,1,world,0,1).movementAllowed);
        ++s.tick.value;
        if(mode<4) world.failure=mode+1;
        if(mode==4) world.clear=false;
        if(mode==5) ++s.actor.generation.value;
        if(mode==6) {
            d=crouch.abort(); assert(d.terminalEvent && !crouch.abort().terminalEvent);
        } else d=crouch.update(s,false,2,world,0,mode==7 ? 0U:1U);
        if(mode==4) {
            assert(d.state==local::CrouchState::Raising); ++s.tick.value;
            d=crouch.update(s,false,2+limits.transitionTimeoutUs,world,0,1);
            assert(d.reason==local::CrouchReason::TimedOut);
        }
        assert(d.terminalEvent && !d.movementAllowed && d.intent.speed==0 && d.intent.duck==core::ActionRequest::Hold);
        const auto count=world.calls;
        assert(!crouch.update(s,false,3000000,world,0,1).terminalEvent && world.calls==count);
    }
    auto s=actor(); World world; local::Crouch crouch(binding,limits);
    crouch.update(s,true,0,world,0,1); ++s.tick.value;
    assert(crouch.update(s,true,limits.transitionTimeoutUs,world,0,1).reason==local::CrouchReason::TimedOut);
    auto bad=limits; bad.transitionTimeoutUs=0; local::Crouch invalid(binding,bad);
    assert(invalid.update(s,true,0,world,0,1).reason==local::CrouchReason::InvalidInput);
    local::Crouch stoppedClock(binding,limits); s=actor();
    stoppedClock.update(s,true,0,world,0,1); ++s.tick.value;
    assert(stoppedClock.update(s,true,0,world,0,1).reason==local::CrouchReason::InvalidInput);
    local::Crouch missing(binding,limits); s=actor(); s.ducked=true; s.hull.reset();
    assert(missing.update(s,false,0,world,0,1).intent.duck==core::ActionRequest::Hold);
    local::Crouch huge(binding,limits); s=actor();
    const auto maximum=(std::numeric_limits<std::uint64_t>::max)();
    huge.update(s,true,maximum-10,world,0,1); ++s.tick.value;
    assert(huge.update(s,true,maximum,world,0,1).state==local::CrouchState::Lowering);
}
}
int main() { hints(); posture(); failures(); }
