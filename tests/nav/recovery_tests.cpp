// SPDX-License-Identifier: MPL-2.0
#include "nav/local/recovery.hpp"
#include "nav/runtime/replan.hpp"
#include <cassert>
#include <limits>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>
#endif
using namespace astrabot;
using namespace astrabot::nav::local;
namespace {
Binding binding() { return {{1},{2,{3}},{4},1,0}; }
struct Replay {
    Recovery recovery{}; Binding b=binding(); std::uint64_t tick=1,now=0;
    Replay() { assert(recovery.bindRoute(b)); }
    RecoveryDecision frame(nav::model::NavVector3 p={},ExpectedProgress expected=ExpectedProgress::Walk,
        bool sent=true,core::IntentVector direction={1,0,0},std::uint64_t us=100000) {
        assert(recovery.report({b,{tick},{tick+1},p,direction,us,expected,sent}));
        tick+=2; now+=us;
        return recovery.observe(b,{tick},now,p);
    }
};
void detection() {
    Replay r;
    for(int i=0;i<4;++i) assert(r.frame().state==RecoveryState::Monitoring);
    auto d=r.frame(); assert(d.state==RecoveryState::Wait && d.commandedUs==500000);
    assert(d.cause==StuckCause::Unknown && d.symptom==StuckSymptom::NoProgress);
    Replay crouch;
    for(int i=0;i<9;++i) assert(crouch.frame({},ExpectedProgress::Crouch).state==RecoveryState::Monitoring);
    assert(crouch.frame({},ExpectedProgress::Crouch).state==RecoveryState::Wait);
    Replay progressing;
    for(int i=0;i<100;++i) assert(progressing.frame({float(i),0,0},ExpectedProgress::Crouch).state==RecoveryState::Monitoring);
    // Frozen target/direction at the start of a window prevents target jitter
    // and back-and-forth movement from manufacturing forward progress.
    Replay oscillating;
    for(int i=0;i<10;++i) d=oscillating.frame({i%2 ? -3.0f:0.0f,0,0},ExpectedProgress::Walk,true,{i%2 ? -1.0:1.0,0,0},50000);
    assert(d.state==RecoveryState::Wait && d.symptom==StuckSymptom::Oscillation);
    Replay wide;
    bool detected=false;
    for(int i=0;i<30;++i) {
        d=wide.frame({i%2 ? 10.0f:0.0f,0,0},ExpectedProgress::Walk,true,{i%2 ? 1.0:-1.0,0,0});
        detected|=d.state!=RecoveryState::Monitoring;
    }
    assert(detected); // Repeated measured excursions cannot keep refilling a window.
    Replay alternating; detected=false;
    for(int i=0;i<30;++i) {
        const float start=i%2 ? 10.0f:0.0f,end=i%2 ? 0.0f:10.0f;
        assert(alternating.recovery.report({alternating.b,{alternating.tick},{alternating.tick+1},{start,0,0},
            {i%2 ? -1.0:1.0,0,0},100000,ExpectedProgress::Walk,true}));
        alternating.tick+=2; alternating.now+=100000;
        d=alternating.recovery.observe(alternating.b,{alternating.tick},alternating.now,{end,0,0});
        detected|=d.state!=RecoveryState::Monitoring;
    }
    assert(detected);
    Replay lateral; lateral.recovery.replanned();
    for(int i=0;i<30;++i) {
        d=lateral.frame({0,float(i*5),0});
        assert(d.state==RecoveryState::Monitoring && d.attempts==1);
    }
}
void pausesAndStaleFeedback() {
    for(auto expected:{ExpectedProgress::Pause,ExpectedProgress::Walk}) {
        Replay r;
        for(int i=0;i<30;++i) assert(r.frame({},expected,expected==ExpectedProgress::Pause).state==RecoveryState::Monitoring);
        assert(r.recovery.decision().commandedUs==0);
    }
    Replay r; r.frame(); const auto saved=r.recovery.decision().commandedUs;
    auto wrong=r.b; ++wrong.actor.generation.value;
    assert(!r.recovery.report({wrong,{10},{11},{},{1,0,0},100000,ExpectedProgress::Walk,true}));
    assert(!r.recovery.report({r.b,{1},{2},{},{1,0,0},100000,ExpectedProgress::Walk,true}));
    assert(r.recovery.decision().commandedUs==saved);
    assert(!r.recovery.bindRoute(wrong));
}
void finiteReplanAndReset() {
    Replay r; RecoveryDecision d;
    for(int i=0;i<5;++i) d=r.frame();
    assert(d.state==RecoveryState::Wait);
    for(const auto next:{RecoveryState::Sidestep,RecoveryState::Reverse,RecoveryState::Replan}) {
        r.now=d.deadlineUs; r.tick+=2;
        d=r.recovery.observe(r.b,{r.tick},r.now,{}); assert(d.state==next);
    }
    r.recovery.replanned(); ++r.b.routeGeneration; assert(r.recovery.bindRoute(r.b));
    for(int i=0;i<4;++i) assert(r.frame().state==RecoveryState::Monitoring);
    d=r.frame(); assert(d.state==RecoveryState::Aborted && d.terminalEvent && d.attempts==1);
    for(int i=0;i<10;++i) assert(!r.frame().terminalEvent);
    // Even a further route replacement cannot resurrect a terminal goal.
    ++r.b.routeGeneration; assert(r.recovery.bindRoute(r.b));
    assert(r.frame().state==RecoveryState::Aborted);
    Replay resumed; resumed.recovery.replanned(); ++resumed.b.routeGeneration;
    assert(resumed.recovery.bindRoute(resumed.b)); resumed.frame();
    d=resumed.frame({5,0,0}); assert(d.measuredProgress && d.attempts==0);
    // Unknown stuck reasons replan without inventing a directed-edge fact.
    nav::runtime::ReplanAttempt plan;
    assert(plan.scheduleRecovery(binding(),{1},0));
    const auto policy=plan.consume(binding(),{2},100000); assert(policy && !policy->blocked);
    assert(!plan.scheduleRecovery(binding(),{3},200000));
    assert(plan.state()==nav::runtime::ReplanState::Exhausted);
    for(auto cause:{StuckCause::DoorBlocked,StuckCause::PlayerBlocked,StuckCause::GeometryBlocked,StuckCause::TraversalFailed,StuckCause::Unknown}) {
        Recovery terminal; assert(terminal.abort(cause).terminalEvent);
        assert(terminal.decision().cause==cause && !terminal.abort(cause).terminalEvent);
    }
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    detection(); pausesAndStaleFeedback(); finiteReplanAndReset();
}
