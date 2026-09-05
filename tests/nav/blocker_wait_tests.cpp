// SPDX-License-Identifier: MPL-2.0
#include "nav/local/blocker_wait.hpp"
#include <cassert>
#include <limits>
#include <vector>

using namespace astrabot;
using namespace nav;
using namespace nav::local;
namespace {
Binding binding(std::uint16_t slot=2) { return {{1},{slot,{1}},{1},1,0}; }
BlockerFeedback feedback(Binding b, std::uint64_t tick, std::uint64_t now) {
    BlockerFeedback f; f.binding=b; f.nowUs=now;
    f.requested={b.agent,b.actor,b.map,{tick},b.routeGeneration,1};
    f.observed.stamp=f.requested; f.observed.kind=runtime::QueryKind::Blocker;
    f.observed.error=runtime::QueryError::None;
    f.observed.blocker=runtime::BlockerObservation{123,runtime::BlockerKind::Teammate,core::PlayerId{1,{1}}};
    return f;
}
void expiryAndBudget() {
    const auto b=binding(); BlockerWait wait(b,{120,200,1000});
    auto f=feedback(b,1,0);
    assert(wait.update(f).action==BlockerAction::Yield);
    assert(wait.fact(119) && !wait.fact(120));
    // A replay cannot refresh the fact or consume the finite attempt.
    f.nowUs=900;
    const auto stale=wait.update(f);
    assert(!stale.accepted && stale.reason==BlockerReason::StaleTick);
    assert(!wait.fact(120));
    f=feedback(b,2,200);
    assert(wait.update(f).action==BlockerAction::InspectAvoidance);
    assert(wait.fact(319) && !wait.fact(320));
    // Replacement observations cannot restart the attempt's deadline.
    f=feedback(b,3,900); f.observed.blocker->id=456;
    f.observed.blocker->player=core::PlayerId{3,{2}};
    assert(wait.update(f).action==BlockerAction::InspectAvoidance);
    assert(wait.fact(999) && !wait.fact(1000));
    f=feedback(b,4,1000);
    f.observed.kind=runtime::QueryKind::Clearance;
    f.observed.clearance=runtime::ClearanceObservation{true};
    const auto timed=wait.update(f);
    assert(timed.action==BlockerAction::Replan && timed.reason==BlockerReason::TimedOut && timed.terminalEvent);
    assert(!wait.fact(1000) && !wait.update(f).terminalEvent && !wait.abort().terminalEvent);
}
void priorityAndClear() {
    const auto a=binding(1), b=binding(2);
    BlockerWait first(a,{120,200,1000}), second(b,{120,200,1000});
    auto f=feedback(a,1,100); f.observed.blocker->player=b.actor;
    assert(first.update(f).action==BlockerAction::InspectAvoidance);
    assert(second.update(feedback(b,1,100)).action==BlockerAction::Yield);
    f=feedback(b,2,150); f.observed.kind=runtime::QueryKind::Clearance;
    f.observed.blocker.reset(); f.observed.clearance=runtime::ClearanceObservation{true};
    const auto clear=second.update(f);
    assert(clear.action==BlockerAction::ReinspectPassage && clear.terminalEvent && !second.fact(150));
    // A clear passage (including an opened door) invalidates the local fact;
    // it never grants movement or completes a route.
    assert(!second.update(f).accepted);
    BlockerWait geometry(b,{120,200,1000}); f=feedback(b,1,0);
    f.observed.blocker=runtime::BlockerObservation{0,runtime::BlockerKind::Geometry,{}};
    assert(geometry.update(f).action==BlockerAction::InspectAvoidance);
    assert(geometry.abort().terminalEvent && !geometry.fact(0));
    BlockerWait verified(b,{120,200,1000});
    assert(verified.update(feedback(b,1,0)).accepted);
    assert(verified.clear(b,{2},999).action==BlockerAction::ReinspectPassage);
    assert(!verified.clear(b,{3},1000).accepted);
    BlockerWait deadline(b,{120,200,1000});
    assert(deadline.update(feedback(b,1,0)).accepted);
    assert(deadline.clear(b,{2},1000).reason==BlockerReason::TimedOut);
}
void invalidations() {
    for(int mode=0;mode<11;++mode) {
        auto b=binding(); BlockerWait wait(b,{120,200,1000});
        auto f=feedback(b,1,1);
        assert(wait.update(f).accepted);
        f=feedback(b,2,2);
        if(mode==0) f.binding.actor.generation.value++;
        if(mode==1) f.binding.map.value++;
        if(mode==2) f.binding.routeGeneration++;
        if(mode==3) f.binding.step++;
        if(mode==4) f.observed.stamp.tick.value--;
        if(mode==5) f.observed.blocker.reset();
        if(mode==6) f.observed.blocker->player.reset();
        if(mode==7) f.observed.blocker->player=b.actor;
        if(mode==8) f.nowUs=0;
        if(mode==9) f.observed.error=runtime::QueryError::Unavailable;
        if(mode==10) f.observed.blocker->player=core::PlayerId{b.actor.slot,{2}};
        const auto d=wait.update(f);
        assert(d.terminalEvent && !wait.fact(2));
        assert(d.action==(mode<4 ? BlockerAction::Aborted : BlockerAction::Replan));
        if(mode==9) assert(d.reason==BlockerReason::Unavailable);
        assert(!wait.abort().terminalEvent);
    }
    for(const auto limits : {BlockerLimits{0,200,1000},BlockerLimits{120,0,1000},
        BlockerLimits{120,1000,1000},BlockerLimits{1001,200,1000}}) {
        BlockerWait wait(binding(),limits);
        assert(wait.update(feedback(binding(),1,1)).reason==BlockerReason::InvalidInput);
    }
}
std::vector<BlockerAction> replay(std::uint64_t start) {
    BlockerWait wait(binding(),{120,200,1000}); std::vector<BlockerAction> trace;
    for(std::uint64_t i=0;i<=10;++i) {
        trace.push_back(wait.update(feedback(binding(),i+1,start+i*100)).action);
    }
    assert(trace.front()==BlockerAction::Yield && trace.back()==BlockerAction::Replan);
    return trace;
}
}
int main() {
    expiryAndBudget(); priorityAndClear(); invalidations();
    assert(replay(0)==replay(0));
    assert(replay(0)==replay(std::numeric_limits<std::uint64_t>::max()-1000));
}
