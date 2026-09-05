// SPDX-License-Identifier: MPL-2.0
#include "nav/local/primitive.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <limits>
using namespace astrabot::nav;
using namespace astrabot::nav::local;
namespace {
Binding binding() { return {{1},{2,{3}},{4},5,0}; }
corridor::Transition transition(model::NavTraversalKind kind=model::NavTraversalKind::Walk) {
    corridor::Transition t;
    t.edge={{1},{2},1,kind,{}};
    t.sourceExtent={{0,0,0},{100,100,0},0,0};
    t.targetExtent={{100,0,0},{200,100,0},0,0};
    t.sourceLow=t.targetLow={100,16,0}; t.sourceHigh=t.targetHigh={100,84,0};
    return t;
}
bool neutral(const MovementIntent& i) {
    return i.direction.x==0 && i.direction.y==0 && i.direction.z==0 && i.speed==0 &&
        i.lateralCorrection==0 && !i.view && i.duck==ActionRequest::None &&
        i.jump==ActionRequest::None && i.use==ActionRequest::None;
}
Feedback feedback() { Feedback f; f.binding=binding(); f.tick={11}; return f; }
void lifecycle() {
    Primitive p; assert(p.abort().event==PrimitiveEvent::None);
    auto t=transition(); auto r=p.enter(binding(),t,{10});
    assert(r.accepted && r.event==PrimitiveEvent::Entered && neutral(r.intent));
    assert(!p.enter(binding(),t,{10}).accepted);
    auto f=feedback(); f.intent.direction={1,0,0}; f.intent.speed=100; f.intent.jump=ActionRequest::Press;
    r=p.update(f); assert(r.accepted && r.event==PrimitiveEvent::None && r.intent.jump==ActionRequest::Press);
    r=p.update(f); assert(!r.accepted && r.reason==PrimitiveReason::StaleUpdate && neutral(r.intent));
    f.tick={12}; f.progress=Progress::Complete; f.supportedArea=model::NavAreaId{2}; f.supportVerified=true;
    r=p.update(f); assert(r.accepted && r.event==PrimitiveEvent::Complete && neutral(r.intent));
    f.tick={13}; assert(p.update(f).event==PrimitiveEvent::None);
    assert(p.abort().event==PrimitiveEvent::None && !p.enter(binding(),t,{14}).accepted);
    assert(p.state()==PrimitiveState::Complete);
}
void staleAndTerminal() {
    for(int field=0;field<6;++field) {
        Primitive p; assert(p.enter(binding(),transition(),{10}).accepted);
        auto f=feedback();
        switch(field) {
        case 0: ++f.binding.agent.value; break;
        case 1: ++f.binding.actor.generation.value; break;
        case 2: ++f.binding.map.value; break;
        case 3: ++f.binding.routeGeneration; break;
        case 4: ++f.binding.step; break;
        case 5: f.tick={9}; break;
        }
        auto r=p.update(f); assert(!r.accepted && r.reason==PrimitiveReason::StaleUpdate && neutral(r.intent));
        assert(p.state()==PrimitiveState::Running);
        r=p.abort(); assert(r.accepted && r.event==PrimitiveEvent::Aborted && neutral(r.intent));
        assert(p.abort().event==PrimitiveEvent::None);
    }
    for(int mode=0;mode<4;++mode) {
        Primitive p; assert(p.enter(binding(),transition(),{10}).accepted);
        auto f=feedback(); f.progress=Progress::Complete;
        if(mode==1) { f.supportedArea=model::NavAreaId{1}; f.supportVerified=true; }
        if(mode==2) f.supportedArea=model::NavAreaId{2};
        if(mode==3) f.progress=Progress::Failed;
        auto r=p.update(f); assert(r.accepted && r.event==PrimitiveEvent::Failed && neutral(r.intent));
        assert(r.reason==(mode==3 ? PrimitiveReason::ControllerFailure:PrimitiveReason::MissingSupport));
        assert(p.update(f).event==PrimitiveEvent::None);
    }
}
void dispatchAndOwnership() {
    for(auto kind : {model::NavTraversalKind::Walk,model::NavTraversalKind::Crouch,
                    model::NavTraversalKind::Jump,model::NavTraversalKind::Ladder}) {
        Primitive p; auto t=transition(kind);
        t.edge.external=enrichment::NavTraversalLink{7,8,9,{1},{2},{25,25,0},{125,25,0},kind,
                                                     enrichment::NavLinkDirection::Forward,0};
        assert(p.enter(binding(),t,{10}).event==PrimitiveEvent::Entered);
        t.edge.external->linkId=99;
        assert(p.transition()->edge.external->linkId==9 && p.transition()->edge.traversal==kind);
        assert(p.abort().event==PrimitiveEvent::Aborted);
        assert(p.transition()->edge.external->sourceId==7 && p.transition()->edge.external->generation==8);
    }
    for(auto kind : {model::NavTraversalKind::Drop,static_cast<model::NavTraversalKind>(255)}) {
        Primitive p; auto r=p.enter(binding(),transition(kind),{10});
        assert(r.accepted && r.event==PrimitiveEvent::Failed && r.reason==PrimitiveReason::UnsupportedTraversal);
        assert(p.transition()->edge.traversal==kind);
        assert(p.abort().event==PrimitiveEvent::None);
    }
}
void badInputs() {
    Primitive p; auto b=binding(); b.routeGeneration=0;
    assert(p.enter(b,transition(),{10}).reason==PrimitiveReason::InvalidBinding);
    assert(p.state()==PrimitiveState::Idle);
    auto t=transition(); t.edge.target={};
    assert(p.enter(binding(),t,{10}).reason==PrimitiveReason::InvalidTransition);
    for(int mode=0;mode<9;++mode) {
        Primitive q; assert(q.enter(binding(),transition(),{10}).accepted);
        auto f=feedback();
        if(mode==0) f.intent.speed=std::numeric_limits<double>::quiet_NaN();
        if(mode==1) f.intent.direction.x=std::numeric_limits<double>::infinity();
        if(mode==2) f.intent.speed=-1;
        if(mode==3) f.intent.use=static_cast<ActionRequest>(255);
        if(mode==4) f.progress=static_cast<Progress>(255);
        if(mode==5) f.intent.lateralCorrection=1.1;
        if(mode==6) f.intent.speed=401;
        if(mode==7) f.intent.view=astrabot::core::IntentVector{0,0,std::numeric_limits<double>::quiet_NaN()};
        if(mode==8) f.intent.speed=1; // nonzero speed needs a direction
        const auto r=q.update(f); assert(r.event==PrimitiveEvent::Failed && neutral(r.intent));
        assert(r.reason==(mode==4 ? PrimitiveReason::InvalidFeedback:PrimitiveReason::InvalidIntent));
    }
}
void corridorReplay() {
    route_test::Area a{1,{{0,0,0},{100,100,0},0,0},{}},
                     b{2,{{100,0,0},{200,100,0},0,0},{}},
                     c{3,{{100,100,0},{200,200,0},0,0},{}};
    a.targets[1]={2}; b.targets[2]={3};
    auto g=query::NavGraph::build(route_test::snapshot({a,b,c}),{3,2,1000000}); assert(g);
    auto r=query::NavRouteSearch::search(**g.value,{{1},{3},{3,1000000},false}); assert(r);
    auto corridor=corridor::Corridor::build(**g.value,*r.value,{16,16},{2,1000000,2}); assert(corridor);
    // Replay the same controller outcomes through owned constraints and lifecycle.
    for(int replay=0;replay<2;++replay) {
        corridor::Cursor cursor(corridor.value);
        for(std::size_t step=0;step<2;++step) {
            Primitive p; auto bind=binding(); bind.step=step;
            const auto& t=corridor.value->transitions()[step];
            assert(p.enter(bind,t,{10}).event==PrimitiveEvent::Entered);
            auto f=feedback(); f.binding=bind;
            assert(p.update(f).state==PrimitiveState::Running && cursor.index()==step);
            f.tick={12}; f.progress=Progress::Complete; f.supportedArea=t.edge.target; f.supportVerified=true;
            auto terminal=p.update(f); assert(terminal.event==PrimitiveEvent::Complete);
            assert(cursor.advance(step,*f.supportedArea,f.supportVerified));
            assert(p.update(f).event==PrimitiveEvent::None);
        }
        assert(cursor.exhausted());
    }
}
}
int main() { lifecycle(); staleAndTerminal(); dispatchAndOwnership(); badInputs(); corridorReplay(); }
