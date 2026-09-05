// SPDX-License-Identifier: MPL-2.0
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>
#endif

using namespace astrabot::nav;
using namespace astrabot::nav::query;
using K = diagnostics::NavErrorKind;
using F = diagnostics::NavField;
namespace {
route_test::Area area(std::uint32_t id, float x = 0, float z = 0) {
    return {id, {{x, 0, z}, {x + 2, 2, z}, z, z}};
}
std::shared_ptr<const NavGraph> graph(const std::vector<route_test::Area> &areas) {
    auto r = NavGraph::build(route_test::snapshot(areas), {1000, 4000, 1000000});
    assert(r);
    return *r.value;
}
NavRouteRequest request(std::uint32_t start = 1, std::uint32_t goal = 4,
                        std::size_t cap = 100, bool partial = false) {
    return {{start}, {goal}, {cap, 1000000}, partial};
}
void corridor(const NavRouteResult &r, std::initializer_list<model::NavAreaId> ids,
              double total) {
    assert(r.areas == std::vector<model::NavAreaId>(ids));
    assert(r.steps.size() + 1 == r.areas.size());
    assert(r.total == total);
    for (std::size_t i = 0; i < r.steps.size(); ++i) {
        assert(r.steps[i].edge.source == r.areas[i]);
        assert(r.steps[i].edge.target == r.areas[i + 1]);
        assert(r.steps[i].edge.traversal == model::NavTraversalKind::Walk);
    }
}
void failure(const diagnostics::ReadResult<NavRouteResult> &r, K kind, F field) {
    assert(!r && !r.value);
    assert((r.error == diagnostics::NavError{kind, 0, diagnostics::NavRecord::Route, field}));
}
NavCostDecision unit(const NavCostContext &, const void *) { return {false, {1, 0, 0, 0}}; }
std::vector<route_test::Area> diamond() {
    auto a = area(1), b = area(2), c = area(3), d = area(4);
    a.targets[0] = {3, 2}; b.targets[0] = {4}; c.targets[0] = {4};
    return {a, b, c, d};
}
void trivialAndDirected() {
    auto a = area(1), b = area(2, 3, 4), c = area(3, 6, 8), d = area(4);
    a.targets[0] = {2}; b.targets[1] = {3};
    auto g = graph({d, c, b, a});
    auto r = NavRouteSearch::search(*g, {{1}, {1}, {0, 100000}, false});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}}, 0);
    assert(r.value->metrics.expansions == 0 && r.value->metrics.examinedEdges == 0);
    r = NavRouteSearch::search(*g, request(1, 3));
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {2}, {3}}, 10);
    assert(r.value->components.distance == 10 && r.value->components.traversal == 0);
    assert(r.value->steps[0].edge.direction == 0 && r.value->steps[1].edge.direction == 1);
    for (auto invalid : {0U, 99U}) {
        failure(NavRouteSearch::search(*g, request(invalid, 1)), K::InvalidInput, F::RouteStart);
        failure(NavRouteSearch::search(*g, request(1, invalid)), K::InvalidInput, F::RouteGoal);
    }
    for (auto req : {request(3, 1), request(1, 4, 100, true)}) {
        r = NavRouteSearch::search(*g, req);
        assert(r && r.value->status == NavRouteStatus::Unreachable);
        assert(r.value->areas.empty() && r.value->steps.empty() && r.value->total == 0);
    }
}
void deterministicDiamond() {
    auto areas = diamond();
    for (int permutation = 0; permutation < 2; ++permutation) {
        auto g = graph(areas);
        for (int run = 0; run < 100; ++run) {
            auto r = NavRouteSearch::search(*g, request(), {nullptr, unit, nullptr});
            assert(r && r.value->status == NavRouteStatus::Complete);
            corridor(*r.value, {{1}, {2}, {4}}, 2);
            const auto &m = r.value->metrics;
            assert(m.expansions == 3 && m.examinedEdges == 4 && m.relaxations == 3);
            assert(m.reopens == 0 && m.peakOpen == 2);
            for (const auto &s : r.value->steps) {
                assert(s.edge.direction == 0 && s.components.distance == 1 && s.total == 1);
                assert(s.components.traversal == 0 && s.components.danger == 0 && s.components.experience == 0);
            }
        }
        std::reverse(areas[0].targets[0].begin(), areas[0].targets[0].end());
        std::reverse(areas.begin(), areas.end());
    }
}
struct EvidenceContext { const NavGraph &graph; mutable std::size_t calls{0}; };
NavCostDecision evidence(const NavCostContext &c, const void *opaque) {
    const auto &state = *static_cast<const EvidenceContext *>(opaque);
    ++state.calls;
    assert(&c.source == &state.graph.area(*state.graph.find(c.edge.source)));
    assert(&c.target == &state.graph.area(*state.graph.find(c.edge.target)));
    assert(c.geometricDistance == 5);
    assert(c.edge.traversal == model::NavTraversalKind::Walk);
    if (c.edge.direction == 0)
        return {false, {100, 0, 0, 0}};
    assert(c.edge.direction == 1);
    return {false, {1, 2, 4, 8}};
}
void selectedEvidence() {
    auto a = area(1), b = area(2, 3, 4), c = area(3, 6, 8);
    a.targets[0] = {2}; a.targets[1] = {2}; b.targets[1] = {3};
    auto g = graph({a, b, c});
    EvidenceContext state{*g};
    auto r = NavRouteSearch::search(*g, request(1, 3), {&state, evidence, nullptr});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {2}, {3}}, 30);
    assert(state.calls == 3 && r.value->metrics.examinedEdges == 3);
    const auto &sum = r.value->components;
    assert(sum.distance == 2 && sum.traversal == 4 && sum.danger == 8 && sum.experience == 16);
    for (const auto &s : r.value->steps) {
        assert(s.edge.direction == 1 && s.total == 15);
        assert(s.components.distance == 1 && s.components.traversal == 2);
        assert(s.components.danger == 4 && s.components.experience == 8);
    }
}
// Catches an ID tie-break applied before h, and loss of a queued decrease-key
// beneath multiple heap levels (the four-node diamond alone cannot cover this).
void heapOrderingAndDecreaseKey() {
    auto g = graph(diamond());
    const auto cost = [](const NavCostContext &c, const void *) -> NavCostDecision {
        return {false, {c.edge.source.value == 1 ? (c.edge.target.value == 2 ? 1.0 : 2.0)
                                                                : (c.edge.source.value == 2 ? 2.0 : 1.0),
                        0, 0, 0}};
    };
    const auto h = [](const NavHeuristicContext &c, const void *) -> double {
        return c.area.id.value == 2 ? 2 : (c.area.id.value == 3 ? 1 : 0);
    };
    auto r = NavRouteSearch::search(*g, request(), {nullptr, cost, h});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {3}, {4}}, 3);
    assert(r.value->metrics.expansions == 2);

    std::vector<route_test::Area> areas;
    for (std::uint32_t id = 1; id <= 10; ++id) areas.push_back(area(id));
    areas[0].targets[0] = {2, 3, 4, 5, 6, 7, 8, 9};
    areas[7].targets[0] = {2};
    areas[1].targets[0] = {10};
    g = graph(areas);
    const auto varied = [](const NavCostContext &c, const void *) -> NavCostDecision {
        constexpr std::array<double, 8> costs{8, 3, 7, 4, 2, 6, 1, 5};
        return {false, {c.edge.source.value == 1 ? costs[c.edge.target.value - 2]
                                                                : (c.edge.source.value == 8 ? 0.0 : 2.0),
                        0, 0, 0}};
    };
    r = NavRouteSearch::search(*g, request(1, 10), {nullptr, varied, nullptr});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {8}, {2}, {10}}, 3);
    const auto &m = r.value->metrics;
    assert(m.expansions == 5 && m.examinedEdges == 10 && m.relaxations == 10);
    assert(m.reopens == 0 && m.peakOpen == 8);
}
struct ReopenContext { bool block{false}; mutable std::array<unsigned, 5> heuristicCalls{}; };
NavCostDecision reopenCost(const NavCostContext &c, const void *opaque) {
    const auto &state = *static_cast<const ReopenContext *>(opaque);
    if (c.edge.source.value == 1)
        return {false, {c.edge.target.value == 2 ? 3.0 : 1.0, 0, 0, 0}};
    if (c.edge.source.value == 3)
        return {state.block, {1, 0, 0, 0}};
    return {false, {10, 0, 0, 0}};
}
double inconsistent(const NavHeuristicContext &c, const void *opaque) {
    const auto &state = *static_cast<const ReopenContext *>(opaque);
    ++state.heuristicCalls[c.area.id.value];
    assert(c.goal.id.value == 4 && c.geometricDistance >= 0);
    return c.area.id.value == 3 ? 5 : 0;
}
void reopeningAndBlocking() {
    auto a = diamond(); a[2].targets[0] = {2};
    auto g = graph(a);
    ReopenContext state;
    auto r = NavRouteSearch::search(*g, request(), {&state, reopenCost, inconsistent});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {3}, {2}, {4}}, 12);
    const auto &m = r.value->metrics;
    assert(m.reopens == 1 && m.expansions == 4 && m.examinedEdges == 5);
    assert(m.relaxations == 5 && m.peakOpen == 2);
    for (std::size_t i = 1; i <= 4; ++i) assert(state.heuristicCalls[i] == 1);
    state = {true, {}};
    r = NavRouteSearch::search(*g, request(), {&state, reopenCost, inconsistent});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {2}, {4}}, 13);
    assert(r.value->metrics.reopens == 0);
}
NavCostDecision zero(const NavCostContext &, const void *) { return {}; }
void zeroCyclesAndCustomDefault() {
    auto a = area(1), b = area(2, 1000), c = area(3, 1);
    a.targets[0] = {2, 3}; b.targets[0] = {1, 3}; c.targets[0] = {1};
    auto g = graph({a, b, c});
    auto r = NavRouteSearch::search(*g, request(1, 3), {nullptr, zero, nullptr});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {3}}, 0);
    assert(r.value->metrics.expansions == 2 && r.value->metrics.relaxations == 2);
    const auto cost = [](const NavCostContext &ctx, const void *) -> NavCostDecision {
        return {false, {ctx.edge.source.value == 1 && ctx.edge.target.value == 3 ? 5.0 : 1.0, 0, 0, 0}};
    };
    r = NavRouteSearch::search(*g, request(1, 3), {nullptr, cost, nullptr});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {2}, {3}}, 2); // Euclidean h would incorrectly choose direct cost 5.
}
NavCostDecision numericCost(const NavCostContext &, const void *p) {
    return {false, *static_cast<const NavCostComponents *>(p)};
}
double numericHeuristic(const NavHeuristicContext &, const void *p) {
    return *static_cast<const double *>(p);
}
void numericAndExceptionBaseline() {
    auto a = area(1), b = area(2), c = area(3);
    a.targets[0] = {2}; b.targets[0] = {3};
    auto g = graph({a, b, c});
    for (double bad : {-1.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        for (auto components : {NavCostComponents{bad, 0, 0, 0}, {0, bad, 0, 0},
                                {0, 0, bad, 0}, {0, 0, 0, bad}})
            failure(NavRouteSearch::search(*g, request(1, 3), {&components, numericCost, nullptr}),
                    K::InvalidValue, F::RouteCost);
        failure(NavRouteSearch::search(*g, request(1, 1), {&bad, nullptr, numericHeuristic}),
                K::InvalidValue, F::RouteHeuristic);
    }
    double nonzeroGoal = 1;
    failure(NavRouteSearch::search(*g, request(1, 1), {&nonzeroGoal, nullptr, numericHeuristic}),
            K::InvalidValue, F::RouteHeuristic);
    const double huge = std::numeric_limits<double>::max();
    for (auto components : {NavCostComponents{huge, huge, 0, 0}, {huge, 0, 0, 0}})
        failure(NavRouteSearch::search(*g, request(1, 3), {&components, numericCost, nullptr}),
                K::InvalidValue, F::RouteCost);
    // Finite g and h can still overflow the queue priority before goal output.
    const NavCostComponents large{huge * 0.75, 0, 0, 0};
    const auto largeH = [](const NavHeuristicContext &c, const void *) -> double {
        return c.area.id.value == 2 ? std::numeric_limits<double>::max() * 0.75 : 0;
    };
    failure(NavRouteSearch::search(*g, request(1, 3), {&large, numericCost, largeH}),
            K::InvalidValue, F::RouteHeuristic);
    const auto badDiscoveredH = [](const NavHeuristicContext &c, const void *) -> double {
        return c.area.id.value == 2 ? -1 : 0;
    };
    failure(NavRouteSearch::search(*g, request(1, 3), {nullptr, unit, badDiscoveredH}),
            K::InvalidValue, F::RouteHeuristic);
    // Opt-in partial output must not turn a numeric error into a usable route.
    const NavCostComponents overflow{huge, huge, 0, 0};
    failure(NavRouteSearch::search(*g, request(1, 3, 1, true), {&overflow, numericCost, nullptr}),
            K::InvalidValue, F::RouteCost);
    const auto throwsCost = [](const NavCostContext &, const void *) -> NavCostDecision {
        throw std::runtime_error("cost");
    };
    failure(NavRouteSearch::search(*g, request(1, 3), {nullptr, throwsCost, nullptr}),
            K::PolicyFailure, F::RouteCost);
    const auto throwsHeuristic = [](const NavHeuristicContext &, const void *) -> double {
        throw std::length_error("heuristic");
    };
    failure(NavRouteSearch::search(*g, request(1, 1), {nullptr, nullptr, throwsHeuristic}),
            K::PolicyFailure, F::RouteHeuristic);
    const auto allocation = [](const NavCostContext &, const void *) -> NavCostDecision {
        throw std::bad_alloc{};
    };
    failure(NavRouteSearch::search(*g, request(1, 3), {nullptr, allocation, nullptr}),
            K::AllocationFailure, F::RouteCost);
    const auto goalAllocation = [](const NavHeuristicContext &, const void *) -> double {
        throw std::bad_alloc{};
    };
    failure(NavRouteSearch::search(*g, request(1, 1), {nullptr, nullptr, goalAllocation}),
            K::AllocationFailure, F::RouteHeuristic);
    const auto validAgain = NavRouteSearch::search(*g, request(1, 3), {nullptr, unit, nullptr});
    assert(validAgain && validAgain.value->status == NavRouteStatus::Complete);
    corridor(*validAgain.value, {{1}, {2}, {3}}, 2);
}
void capAndBudgetBaseline() {
    auto a = area(1), b = area(2, 3, 4); a.targets[0] = {2};
    auto g = graph({a, b});
    auto r = NavRouteSearch::search(*g, request(1, 2, 0));
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit && r.value->areas.empty());
    assert(r.value->metrics.expansions == 0);
    r = NavRouteSearch::search(*g, request(1, 2, 0, true));
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
    corridor(*r.value, {{1}}, 0);
    r = NavRouteSearch::search(*g, request(1, 2, 1));
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {2}}, 5);
    failure(NavRouteSearch::search(*g, {{1}, {2}, {1, 0}, true}),
            K::CountLimitExceeded, F::RouteBytes);
}
} // namespace
int main() {
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    static_assert(noexcept(NavRouteSearch::search(std::declval<const NavGraph &>(), {})));
    static_assert(std::is_same_v<decltype(NavCostContext::source), const model::NavAreaRecord &>);
    trivialAndDirected();
    deterministicDiamond();
    selectedEvidence();
    heapOrderingAndDecreaseKey();
    reopeningAndBlocking();
    zeroCyclesAndCustomDefault();
    numericAndExceptionBaseline();
    capAndBudgetBaseline();
}
