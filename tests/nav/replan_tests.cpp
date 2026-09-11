// SPDX-License-Identifier: MPL-2.0
#include "nav/runtime/replan.hpp"
#include "nav/runtime/execution.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
using namespace astrabot;
using namespace astrabot::nav;
int main() {
    route_test::Area a{1,{{0,0,0},{100,100,0},0,0}},b{2,{{100,0,0},{200,100,0},0,0}},
        c{3,{{0,100,0},{100,200,0},0,0}},d{4,{{100,100,0},{200,200,0},0,0}};
    a.targets[1]={2}; a.targets[2]={3}; b.targets[3]={1}; c.targets[1]={4}; d.targets[0]={2};
    const auto mesh=route_test::snapshot({a,b,c,d});
    const auto graph=query::NavGraph::build(mesh,{4,5,1000000}); assert(graph);
    const auto search=[&](query::NavRoutePolicy policy) {
        return query::NavRouteSearch::search(**graph.value,{{1},{2},{4,1000000},false},policy);
    };
    const auto original=search({}); assert(original && original.value->steps.size()==1);
    const auto edge=original.value->steps[0].edge;
    runtime::Execution execution;
    execution.begin();
    assert(execution.state==runtime::ExecutionState::Planning);
    execution.fail({2},runtime::ExecutionFailure::Corridor,100,edge,true);
    assert(execution.state==runtime::ExecutionState::Failed && execution.blocked(edge));
    assert(execution.cooling({2},101) && !execution.cooling({2},execution.retryAtUs));
    assert(!execution.canSearch(101) && execution.canSearch(250'100));
    assert(!execution.cooling({4},101)); // another goal remains eligible
    const runtime::ExecutionPolicy excluded{&execution,{}};
    const auto executableDetour=search(excluded.policy());
    assert(executableDetour && executableDetour.value->areas==
        std::vector<model::NavAreaId>({{1},{3},{4},{2}}));
    execution.begin(); // new route does not forget structurally invalid edges
    assert(execution.blocked(edge));
    const auto executionReverse=query::NavRouteSearch::search(**graph.value,
        {{2},{1},{4,1000000},false},excluded.policy());
    assert(executionReverse && executionReverse.value->steps.size()==1);
    runtime::Execution otherActor;
    const runtime::ExecutionPolicy otherPolicy{&otherActor,{}};
    assert(search(otherPolicy.policy()).value->steps.size()==1);
    execution={}; // a new actor/map generation starts with no inherited failures
    assert(!execution.blocked(edge) && !execution.cooling({2},101));
    execution.fail({2},runtime::ExecutionFailure::Observation,100,edge,false);
    assert(!execution.blocked(edge)); // transient failures never poison topology
    execution.setSearchTime(101);
    assert(execution.edgeCooling(edge));
    assert(search(excluded.policy()).value->steps.size()==3);
    execution.begin(); // A new goal retains the bounded temporary edge cooldown.
    assert(execution.edgeCooling(edge));
    assert(search(otherPolicy.policy()).value->steps.size()==1);
    execution.setSearchTime(100+runtime::Execution::edgeRetryDelayUs);
    assert(!execution.edgeCooling(edge) && !execution.blocked(edge));
    assert(search(excluded.policy()).value->steps.size()==1);
    execution.fail({2},runtime::ExecutionFailure::Observation,3'000'000,edge,false);
    execution.setSearchTime(3'000'001);
    assert(execution.edgeCooling(edge));
    execution.clearEdgeCooldown(edge);
    assert(!execution.edgeCooling(edge) && !execution.blocked(edge));
    local::Binding binding{{1},{1,{1}},{1},1,0};
    runtime::ReplanAttempt attempt;
    assert(attempt.schedule(binding,edge,{1},100));
    assert(!attempt.schedule(binding,edge,{2},999999)); // cannot refresh a pending fact
    assert(!attempt.consume(binding,{1},101));
    const auto policy=attempt.consume(binding,{2},102); assert(policy && attempt.attempts()==1);
    const auto detour=search(policy->policy()); assert(detour && detour.value->areas==std::vector<model::NavAreaId>({{1},{3},{4},{2}}));
    assert(detour.value->total==300 && original.value->total==100);
    const auto reverse=query::NavRouteSearch::search(**graph.value,{{2},{1},{4,1000000},false},policy->policy());
    assert(reverse && reverse.value->steps.size()==1); // directional fact
    auto later=binding; ++later.routeGeneration;
    assert(!attempt.schedule(later,edge,{3},200) && attempt.state()==runtime::ReplanState::Exhausted);
    const auto expired=attempt.snapshot(binding.map,100+runtime::ReplanAttempt::factLifetimeUs);
    assert(!expired.blocked && search(expired.policy()).value->steps.size()==1);
    assert(!attempt.snapshot({2},102).blocked && !attempt.snapshot(binding.map,99).blocked);
    // Reset is explicit; a failed/expired consume cannot smuggle stale facts into search.
    attempt={}; assert(attempt.schedule(binding,edge,{1},100));
    assert(!attempt.consume(binding,{2},100+runtime::ReplanAttempt::factLifetimeUs));
    assert(attempt.state()==runtime::ReplanState::Expired);
    attempt={}; assert(attempt.schedule(binding,edge,{1},100));
    later=binding; ++later.actor.generation.value;
    assert(!attempt.consume(later,{2},101) && attempt.state()==runtime::ReplanState::Invalid);
    // Immutable graph remains usable after overlay expiry/invalidation.
    assert(search({}).value->areas==original.value->areas && (*graph.value)->edgeCount()==5);
    // External edge identity and added traversal cost are preserved by policy.
    auto external=edge; external.external=enrichment::NavTraversalLink{};
    external.external->sourceId=5; external.external->generation=7; external.external->linkId=9;
    external.external->additionalCost=13;
    runtime::ReplanAttempt::PolicySnapshot ext{external};
    const auto& source=(*graph.value)->area(*(*graph.value)->find({1}));
    const auto& target=(*graph.value)->area(*(*graph.value)->find({2}));
    assert(ext.cost({external,source,target,100},&ext).blocked);
    ++external.external->generation;
    const auto cost=ext.cost({external,source,target,100},&ext);
    assert(!cost.blocked && cost.components.distance==100 && cost.components.traversal==13);
}
