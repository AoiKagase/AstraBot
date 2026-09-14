// SPDX-License-Identifier: MPL-2.0
#include "nav/runtime/execution.hpp"
#include "nav/runtime/replan.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"

#include <cassert>

using namespace astrabot;
using namespace astrabot::nav;

namespace
{
struct BasePolicyContext
{
	const void* expected{};
	unsigned costCalls{};
	unsigned heuristicCalls{};
};

query::NavCostDecision customCost(const query::NavCostContext& c, const void* context)
{
	auto& state = *const_cast<BasePolicyContext*>(static_cast<const BasePolicyContext*>(context));
	assert(context == state.expected);
	++state.costCalls;
	return {false, {c.geometricDistance + 7, 3, 0, 0, 0}};
}

double customHeuristic(const query::NavHeuristicContext& c, const void* context)
{
	auto& state = *const_cast<BasePolicyContext*>(static_cast<const BasePolicyContext*>(context));
	assert(context == state.expected);
	++state.heuristicCalls;
	return c.geometricDistance / 2;
}
} // namespace

int main()
{
	route_test::Area a{1, {{0, 0, 0}, {100, 100, 0}, 0, 0}}, b{2, {{100, 0, 0}, {200, 100, 0}, 0, 0}},
		c{3, {{0, 100, 0}, {100, 200, 0}, 0, 0}};
	a.targets[1] = {2};
	a.targets[2] = {3};
	const auto graph = query::NavGraph::build(route_test::snapshot({a, b, c}), {3, 2, 1'000'000});
	assert(graph);
	const auto& source = (*graph.value)->area(*(*graph.value)->find({1}));
	const auto& target = (*graph.value)->area(*(*graph.value)->find({2}));
	const auto& siblingTarget = (*graph.value)->area(*(*graph.value)->find({3}));
	const query::NavDirectedEdge failed{{1}, {2}, 1, model::NavTraversalKind::Walk, {}};
	const query::NavDirectedEdge sibling{{1}, {3}, 2, model::NavTraversalKind::Walk, {}};

	runtime::Execution execution;
	execution.fail({2}, runtime::ExecutionFailure::Motion, 1'000'000, failed);
	execution.fail({2}, runtime::ExecutionFailure::Motion, 1'100'000, failed);
	execution.setSearchTime(1'100'001);
	assert(execution.edgeCooling(failed));
	assert(!execution.edgeCooling(sibling));
	assert(!execution.sourceCooling(failed));
	assert(!execution.sourceCooling(sibling));

	runtime::Execution oneFailure;
	oneFailure.fail({2}, runtime::ExecutionFailure::Motion, 5'000'000, failed);
	assert(oneFailure.searchWaiting(5'249'999));
	assert(!oneFailure.canSearch(5'249'999));
	assert(!oneFailure.searchWaiting(5'250'000));
	assert(oneFailure.canSearch(5'250'000));
	assert(oneFailure.goalCooling({2}, 6'999'999));
	assert(!oneFailure.goalCooling({2}, 7'000'000));
	assert(oneFailure.edgeCooling(failed, 6'999'999));
	assert(!oneFailure.edgeCooling(failed, 7'000'000));
	oneFailure.setSearchTime(6'999'999);
	assert(oneFailure.edgeCooling(failed));
	assert(!oneFailure.edgeCooling(sibling));
	oneFailure.setSearchTime(7'000'000);
	assert(!oneFailure.edgeCooling(failed));

	BasePolicyContext baseContext{};
	baseContext.expected = &baseContext;
	const query::NavRoutePolicy base{&baseContext, &customCost, &customHeuristic};
	const runtime::ExecutionPolicy composed{nullptr, base};
	const auto composedPolicy = composed.policy();
	const auto decision = composedPolicy.cost({failed, source, target, 100}, composedPolicy.context);
	assert(decision.components.distance == 107 && decision.components.traversal == 3);
	assert(composedPolicy.heuristic && composedPolicy.heuristic({source, target, 100}, composedPolicy.context) == 50);
	assert(baseContext.costCalls == 1 && baseContext.heuristicCalls == 1);

	const runtime::ExecutionPolicy defaults{};
	const auto defaultPolicy = defaults.policy();
	const auto defaultCost = defaultPolicy.cost({sibling, source, siblingTarget, 125}, defaultPolicy.context);
	assert(defaultCost.components.distance == 125);
	assert(defaultPolicy.heuristic &&
		   defaultPolicy.heuristic({source, siblingTarget, 125}, defaultPolicy.context) == 125);

	const query::NavRoutePolicy arbitraryCost{&baseContext, &customCost, nullptr};
	const runtime::ExecutionPolicy safeZero{nullptr, arbitraryCost};
	assert(!safeZero.policy().heuristic);

	runtime::RouteGoalLease lease;
	const runtime::RouteGoalIdentity identity{{4}, {3, {2}}, {7}, {9}, 11};
	assert(lease.acquire(identity, {2}));
	assert(lease.holds(identity) && lease.goal() == model::NavAreaId{2});
	auto changed = identity;
	++changed.routeGeneration;
	assert(!lease.holds(changed));
	changed = identity;
	++changed.round.value;
	assert(!lease.holds(changed));
	lease.release();
	assert(!lease.holds(identity) && !lease.goal().isValid());

	runtime::ReplanAttempt::PolicySnapshot snapshot{failed, base};
	const auto replanPolicy = snapshot.policy();
	assert(replanPolicy.cost({sibling, source, siblingTarget, 80}, replanPolicy.context).components.distance == 87);
	assert(replanPolicy.heuristic && replanPolicy.heuristic({source, siblingTarget, 80}, replanPolicy.context) == 40);
	assert(baseContext.costCalls == 2 && baseContext.heuristicCalls == 2);
}
