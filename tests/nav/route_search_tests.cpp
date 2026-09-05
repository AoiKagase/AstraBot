// SPDX-License-Identifier: MPL-2.0
#include "nav/query/route_search.hpp"
#include "nav/query/detail/route_budget.hpp"
#include "route_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {
constexpr auto noFailure = std::numeric_limits<std::size_t>::max();
// Enabled only synchronously around the tested call, never during a thread test.
std::size_t failAfter = noFailure;
std::size_t allocationCalls = 0;
bool probeAllocations = false;
void *allocate(std::size_t size) {
    if (probeAllocations) ++allocationCalls;
    if (failAfter != noFailure) {
        if (failAfter == 0) throw std::bad_alloc{};
        --failAfter;
    }
    void *p = std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc{};
    return p;
}
struct AllocationProbe {
    explicit AllocationProbe(std::size_t after) {
        assert(!probeAllocations && failAfter == noFailure);
        allocationCalls = 0;
        probeAllocations = true;
        failAfter = after;
    }
    ~AllocationProbe() { failAfter = noFailure; probeAllocations = false; }
    AllocationProbe(const AllocationProbe &) = delete;
    AllocationProbe &operator=(const AllocationProbe &) = delete;
};
} // namespace
#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(size)
#endif
void *operator new(std::size_t size) { return allocate(size); }
#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(size)
#endif
void *operator new[](std::size_t size) { return allocate(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

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
void metrics(const NavRouteResult &r, std::size_t expansions, std::size_t edges,
             std::size_t relaxations, std::size_t reopens, std::size_t peak) {
    const auto &m = r.metrics;
    assert(m.expansions == expansions && m.examinedEdges == edges);
    assert(m.relaxations == relaxations && m.reopens == reopens && m.peakOpen == peak);
}
void emptyRoute(const NavRouteResult &r) {
    assert(r.areas.empty() && r.steps.empty() && r.total == 0);
    const auto &c = r.components;
    assert(c.distance == 0 && c.traversal == 0 && c.danger == 0 && c.experience == 0);
}
void capPrecedence() {
    auto a = area(1, 100), b = area(2, 20), goal = area(4);
    a.targets[0] = {2, 4};
    auto g = graph({goal, b, a});
    for (bool partial : {false, true}) {
        auto r = NavRouteSearch::search(*g, request(1, 4, 0, partial), {nullptr, unit, nullptr});
        assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
        metrics(*r.value, 0, 0, 0, 0, 1);
        if (partial) corridor(*r.value, {{1}}, 0); else emptyRoute(*r.value);
        // Goal has been discovered, but lower-ID equal-priority B is on top.
        r = NavRouteSearch::search(*g, request(1, 4, 1, partial), {nullptr, unit, nullptr});
        assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
        metrics(*r.value, 1, 2, 2, 0, 2);
        // Goal participates in partial ranking even before it is popped.
        if (partial) corridor(*r.value, {{1}, {4}}, 1); else emptyRoute(*r.value);
        r = NavRouteSearch::search(*g, request(1, 4, 2, partial), {nullptr, unit, nullptr});
        assert(r && r.value->status == NavRouteStatus::Complete);
        corridor(*r.value, {{1}, {4}}, 1);
        metrics(*r.value, 2, 2, 2, 0, 2);
        r = NavRouteSearch::search(*g, request(1, 1, 0, partial));
        assert(r && r.value->status == NavRouteStatus::Complete);
        corridor(*r.value, {{1}}, 0);
        metrics(*r.value, 0, 0, 0, 0, 1);
    }
    // A one-edge goal beats the exact cap. Empty frontier beats the cap too.
    a.targets[0] = {2};
    g = graph({a, b, goal});
    for (bool partial : {false, true}) {
        auto r = NavRouteSearch::search(*g, request(1, 2, 1, partial), {nullptr, unit, nullptr});
        assert(r && r.value->status == NavRouteStatus::Complete);
        corridor(*r.value, {{1}, {2}}, 1);
        metrics(*r.value, 1, 1, 1, 0, 1);
        for (auto cap : {std::size_t{2}, noFailure}) {
            r = NavRouteSearch::search(*g, request(1, 4, cap, partial), {nullptr, unit, nullptr});
            assert(r && r.value->status == NavRouteStatus::Unreachable);
            emptyRoute(*r.value);
            metrics(*r.value, 2, 1, 1, 0, 1);
        }
        r = NavRouteSearch::search(*g, request(4, 1, 1, partial));
        assert(r && r.value->status == NavRouteStatus::Unreachable);
        emptyRoute(*r.value);
        metrics(*r.value, 1, 0, 0, 0, 1);
    }
}
struct RankingPolicy {
    std::array<double, 5> costs{0, 0, 2, 3, 0};
    std::array<double, 5> h{};
};
NavCostDecision rankingCost(const NavCostContext &c, const void *p) {
    return {false, {static_cast<const RankingPolicy *>(p)->costs[c.edge.target.value], 0, 0, 0}};
}
double rankingH(const NavHeuristicContext &c, const void *p) {
    return static_cast<const RankingPolicy *>(p)->h[c.area.id.value];
}
void partialRanking() {
    auto s = area(1, 100), a = area(2, 1, 10), b = area(3, 3, 4), goal = area(4);
    s.targets[0] = {3, 2};
    RankingPolicy p;
    p.h[3] = 20; // B wins geometry despite worse g, h, ID and XY distance.
    auto g = graph({b, goal, s, a});
    auto r = NavRouteSearch::search(*g, request(1, 4, 1, true), {&p, rankingCost, rankingH});
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
    corridor(*r.value, {{1}, {3}}, 3);
    metrics(*r.value, 1, 2, 2, 0, 2);
    // Equal 3D distance: g beats ID. Both vertices are still open.
    a = area(2, -3, 4); b = area(3, 3, 4);
    p.costs[2] = 9; p.costs[3] = 3;
    g = graph({b, goal, s, a});
    r = NavRouteSearch::search(*g, request(1, 4, 1, true), {&p, rankingCost, rankingH});
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
    corridor(*r.value, {{1}, {3}}, 3);
    // Equal geometry and g: lower ID wins even if its h is larger.
    p.costs[2] = 3; p.h[2] = 20; p.h[3] = 0;
    r = NavRouteSearch::search(*g, request(1, 4, 1, true), {&p, rankingCost, rankingH});
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
    corridor(*r.value, {{1}, {2}}, 3);
    // Close the winning candidate while a worse geometric candidate stays open.
    a = area(2, 1); b = area(3, 20); p.h[2] = 0; p.h[3] = 20;
    g = graph({b, goal, s, a});
    r = NavRouteSearch::search(*g, request(1, 4, 2, true), {&p, rankingCost, rankingH});
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
    corridor(*r.value, {{1}, {2}}, 3);
    metrics(*r.value, 2, 2, 2, 0, 2);
    // Discovered start also participates after being closed.
    s = area(1); s.targets[0] = {2, 3};
    g = graph({b, goal, s, a});
    r = NavRouteSearch::search(*g, request(1, 4, 1, true), {&p, rankingCost, rankingH});
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
    corridor(*r.value, {{1}}, 0);
}
struct AncestorPolicy { bool blockGoal{true}; mutable std::size_t calls{0}; };
NavCostDecision ancestorCost(const NavCostContext &c, const void *p) {
    const auto &state = *static_cast<const AncestorPolicy *>(p);
    ++state.calls;
    const auto from = c.edge.source.value, to = c.edge.target.value;
    if (from == 1) return {false, {to == 2 ? 3.0 : 1.0, 0, 0, 0}};
    if (from == 4) return {state.blockGoal, {100, 0, 0, 0}};
    return {false, {1, 0, 0, 0}};
}
double ancestorH(const NavHeuristicContext &c, const void *) { return c.area.id.value == 3 ? 5 : 0; }
void staleAncestorEvidence() {
    auto s = area(1, 100), a = area(2, 20), b = area(3, 30), d = area(4, 1), goal = area(5);
    s.targets[0] = {2, 3}; a.targets[0] = {4}; b.targets[0] = {2}; d.targets[0] = {5};
    auto g = graph({goal, d, b, a, s});
    AncestorPolicy state;
    auto r = NavRouteSearch::search(*g, request(1, 5, 4, true), {&state, ancestorCost, ancestorH});
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
    corridor(*r.value, {{1}, {3}, {2}, {4}}, 3); // D still has stored g=4.
    assert(r.value->components.distance == 3 && state.calls == 5);
    for (const auto &step : r.value->steps) assert(step.total == 1 && step.components.distance == 1);
    metrics(*r.value, 4, 5, 4, 1, 2);
    // Tie D's geometry with an open E whose stored g=3.5: E must outrank D's
    // stored 4, even though D's reconstructed current chain would cost only 3.
    auto e = area(6, -1); s.targets[0].push_back(6);
    auto tied = graph({goal, e, d, b, a, s});
    const auto tiedCost = [](const NavCostContext &c, const void *p) -> NavCostDecision {
        if (c.edge.target.value == 6) return {false, {3.5, 0, 0, 0}};
        return ancestorCost(c, p);
    };
    const auto tiedH = [](const NavHeuristicContext &c, const void *p) -> double {
        return c.area.id.value == 6 ? 10 : ancestorH(c, p);
    };
    r = NavRouteSearch::search(*tied, request(1, 5, 4, true), {&state, tiedCost, tiedH});
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit);
    corridor(*r.value, {{1}, {6}}, 3.5);
    metrics(*r.value, 4, 6, 5, 1, 3);
    state = {false, 0};
    r = NavRouteSearch::search(*g, request(1, 5), {&state, ancestorCost, ancestorH});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {3}, {2}, {4}, {5}}, 103);
    assert(r.value->components.distance == 103 && state.calls == 7);
    metrics(*r.value, 6, 7, 7, 2, 2);
}
struct CallCounts { mutable std::size_t costs{0}, heuristics{0}; };
NavCostDecision countedCost(const NavCostContext &, const void *p) {
    ++static_cast<const CallCounts *>(p)->costs;
    return {false, {1, 2, 4, 8}};
}
double countedH(const NavHeuristicContext &, const void *p) {
    ++static_cast<const CallCounts *>(p)->heuristics;
    return 0;
}
void sameComponents(const NavCostComponents &a, const NavCostComponents &b) {
    assert(a.distance == b.distance && a.traversal == b.traversal);
    assert(a.danger == b.danger && a.experience == b.experience);
}
void sameRoute(const NavRouteResult &a, const NavRouteResult &b) {
    assert(a.status == b.status && a.areas == b.areas && a.total == b.total);
    sameComponents(a.components, b.components);
    metrics(a, b.metrics.expansions, b.metrics.examinedEdges, b.metrics.relaxations,
            b.metrics.reopens, b.metrics.peakOpen);
    assert(a.steps.size() == b.steps.size());
    for (std::size_t i = 0; i < a.steps.size(); ++i) {
        const auto &x = a.steps[i], &y = b.steps[i];
        assert(x.edge.source == y.edge.source && x.edge.target == y.edge.target);
        assert(x.edge.direction == y.edge.direction && x.edge.traversal == y.edge.traversal);
        assert(x.total == y.total);
        sameComponents(x.components, y.components);
    }
}
void queryByteBoundaries() {
    // Locate the exact logical threshold without coupling tests to private
    // Record padding. It is invariant under route length/status and policy.
    auto g = graph(diamond());
    std::size_t low = 0, high = 1000000;
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        auto req = request(); req.limits.maxWorkingBytes = mid;
        auto r = NavRouteSearch::search(*g, req, {nullptr, unit, nullptr});
        if (r) high = mid;
        else { failure(r, K::CountLimitExceeded, F::RouteBytes); low = mid + 1; }
    }
    const auto exact = low;
    assert(exact > sizeof(NavRouteResult) + 4 * sizeof(model::NavAreaId) + 3 * sizeof(NavRouteStep));
    for (auto req : {request(), request(1, 4, 0, true), request(1, 4, 0),
                     request(1, 4, 1, true), request(1, 1, 0), request(4, 1)}) {
        CallCounts calls;
        req.limits.maxWorkingBytes = exact;
        auto r = NavRouteSearch::search(*g, req, {&calls, countedCost, countedH});
        assert(r && calls.heuristics != 0);
        for (auto budget : {exact - 1, std::size_t{0}}) {
            req.limits.maxWorkingBytes = budget; calls = {};
            auto under = [&] {
                AllocationProbe probe(0);
                return NavRouteSearch::search(*g, req, {&calls, countedCost, countedH});
            }();
            failure(under, K::CountLimitExceeded, F::RouteBytes);
            assert(allocationCalls == 0 && calls.costs == 0 && calls.heuristics == 0);
        }
    }
    // Endpoint errors precede byte rejection and require no allocation/policy.
    CallCounts calls;
    auto bad = request(0, 4); bad.limits.maxWorkingBytes = 0;
    auto r = [&] { AllocationProbe probe(0);
        return NavRouteSearch::search(*g, bad, {&calls, countedCost, countedH}); }();
    failure(r, K::InvalidInput, F::RouteStart);
    assert(allocationCalls == 0 && calls.costs == 0 && calls.heuristics == 0);
    std::printf("query exact logical bytes (4 vertices): %zu; exact passes, one-under rejects before allocation\n", exact);
}
void allocationFailureSweeps() {
    auto areas = diamond();
    // Nontrivial partial path and snapshot wire order are independently visible.
    areas[0].extent = area(1, 100).extent;
    areas[1].extent = area(2, 1).extent;
    areas[2].extent = area(3, 20).extent;
    std::reverse(areas.begin(), areas.end());
    auto snap = route_test::snapshot(areas);
    auto built = NavGraph::build(snap, {4, 4, 1000000});
    assert(built);
    const auto g = *built.value;
    CallCounts counts;
    const NavRoutePolicy policy{&counts, countedCost, countedH};
    auto previous = NavRouteSearch::search(*g, request(), policy);
    assert(previous && previous.value->status == NavRouteStatus::Complete);
    const auto saved = *previous.value;
    const auto *const snapshotArea = &g->area(0);
    const auto checkRetained = [&] {
        assert(failAfter == noFailure && !probeAllocations);
        sameRoute(*previous.value, saved);
        assert(g->areaCount() == 4 && g->edgeCount() == 4);
        assert(&g->area(0) == snapshotArea && &g->area(0) == &snap->areas()[3]);
        assert(snap->areas()[0].id == model::NavAreaId{4});
        assert(snap->areas()[3].connections[0][0].target == model::NavAreaId{3});
        assert(g->edge(0).source == model::NavAreaId{1} && g->edge(0).target == model::NavAreaId{2});
        auto again = NavRouteSearch::search(*g, request(), policy);
        assert(again); sameRoute(*again.value, saved);
    };
    bool success = false;
    for (std::size_t n = 0; n < 64; ++n) {
        auto r = [&] { AllocationProbe probe(n);
            return NavGraph::build(snap, {4, 4, g->logicalBytes()}); }();
        const auto allocations = allocationCalls;
        checkRetained();
        if (r) {
            assert(allocations == n && n >= 4);
            auto route = NavRouteSearch::search(**r.value, request(), policy);
            assert(route); sameRoute(*route.value, saved);
            std::printf("graph OOM sweep: %zu failed allocation sites then success; previous route/graph/snapshot retained\n", n);
            success = true; break;
        }
        assert(!r.value && allocations == n + 1);
        assert((r.error == diagnostics::NavError{K::AllocationFailure, 0,
                diagnostics::NavRecord::Graph, F::GraphBytes}));
    }
    assert(success);
    unsigned scenario = 0;
    for (auto req : {request(), request(1, 4, 1, true), request(1, 4, 0, true),
                     request(1, 1, 0), request(4, 1), request(1, 4, 0)}) {
        const auto expected = NavRouteSearch::search(*g, req, policy);
        assert(expected);
        success = false;
        std::size_t preparationFailures = 0, reconstructionFailures = 0;
        for (std::size_t n = 0; n < 64; ++n) {
            counts = {};
            auto r = [&] { AllocationProbe probe(n);
                return NavRouteSearch::search(*g, req, policy); }();
            const auto allocations = allocationCalls;
            const auto observedCalls = counts;
            checkRetained();
            if (r) {
                assert(allocations == n && n >= 2);
                sameRoute(*r.value, *expected.value);
                assert(preparationFailures >= 2);
                if (!r.value->areas.empty()) assert(reconstructionFailures >= 2);
                else assert(reconstructionFailures == 0);
                std::printf("query OOM scenario %u: %zu preparation + %zu reconstruction failures then success\n",
                            scenario, preparationFailures, reconstructionFailures);
                success = true; break;
            }
            failure(r, K::AllocationFailure, F::RouteBytes);
            assert(allocations == n + 1);
            if (observedCalls.heuristics == 0) {
                assert(observedCalls.costs == 0); ++preparationFailures;
            } else {
                assert(observedCalls.costs == expected.value->metrics.examinedEdges);
                ++reconstructionFailures;
            }
        }
        assert(success); ++scenario;
    }
    assert(failAfter == noFailure && !probeAllocations);
}
struct ThrowPolicy {
    bool allocation{false}, fromCost{false};
    std::uint32_t at{0};
    mutable std::size_t costs{0}, heuristics{0};
};
struct PolicyException {};
void throwAt(const ThrowPolicy &p) {
    if (p.allocation) throw std::bad_alloc{};
    throw PolicyException{};
}
NavCostDecision throwingCost(const NavCostContext &c, const void *p) {
    const auto &state = *static_cast<const ThrowPolicy *>(p);
    ++state.costs;
    if (state.fromCost && c.source.id.value == state.at) throwAt(state);
    return {false, {1, 0, 0, 0}};
}
double throwingH(const NavHeuristicContext &c, const void *p) {
    const auto &state = *static_cast<const ThrowPolicy *>(p);
    ++state.heuristics;
    if (!state.fromCost && c.area.id.value == state.at) throwAt(state);
    return 0;
}
void callbackFailureStages() {
    auto a = area(1), b = area(2), c = area(3), goal = area(4);
    a.targets[0] = {2}; b.targets[0] = {3}; c.targets[0] = {4};
    auto g = graph({a, b, c, goal});
    const auto retained = NavRouteSearch::search(*g, request(), {nullptr, unit, nullptr});
    assert(retained);
    const auto saved = *retained.value;
    for (bool allocation : {false, true}) {
        for (bool cost : {false, true}) {
            for (std::uint32_t at : {1U, 2U, 3U, 4U}) {
                if (cost && at == 4) continue;
                ThrowPolicy p{allocation, cost, at};
                auto r = NavRouteSearch::search(*g, request(1, 4, 100, true),
                                              {&p, throwingCost, throwingH});
                failure(r, allocation ? K::AllocationFailure : K::PolicyFailure,
                        cost ? F::RouteCost : F::RouteHeuristic);
                if (cost) assert(p.costs == at);
                else assert(p.costs == (at == 4 ? 0U : at - 1));
                sameRoute(*retained.value, saved);
                const auto again = NavRouteSearch::search(*g, request(), {nullptr, unit, nullptr});
                assert(again); sameRoute(*again.value, saved);
            }
        }
    }
}
void numericFailureStages() {
    auto a = area(1), b = area(2), c = area(3), goal = area(4);
    a.targets[0] = {2}; b.targets[0] = {3}; c.targets[0] = {4};
    auto g = graph({a, b, c, goal});
    struct InvalidPolicy { NavCostComponents components; double h; std::uint32_t at; };
    const auto cost = [](const NavCostContext &ctx, const void *p) -> NavCostDecision {
        return {false, ctx.source.id.value == 2 ? static_cast<const InvalidPolicy *>(p)->components
                                               : NavCostComponents{1, 0, 0, 0}};
    };
    const auto h = [](const NavHeuristicContext &ctx, const void *p) -> double {
        const auto &state = *static_cast<const InvalidPolicy *>(p);
        return ctx.area.id.value == state.at ? state.h : 0;
    };
    for (double bad : {-1.0, -std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        for (auto components : {NavCostComponents{bad, 0, 0, 0}, {0, bad, 0, 0},
                                {0, 0, bad, 0}, {0, 0, 0, bad}}) {
            InvalidPolicy p{components, 0, 0};
            failure(NavRouteSearch::search(*g, request(1, 4, 100, true), {&p, cost, h}),
                    K::InvalidValue, F::RouteCost);
        }
        for (std::uint32_t at : {1U, 3U, 4U}) {
            InvalidPolicy p{{1, 0, 0, 0}, bad, at};
            failure(NavRouteSearch::search(*g, request(1, 4, 100, true), {&p, cost, h}),
                    K::InvalidValue, F::RouteHeuristic);
        }
    }
    InvalidPolicy nonzeroGoal{{1, 0, 0, 0}, 1, 4};
    failure(NavRouteSearch::search(*g, request(1, 4, 0, true), {&nonzeroGoal, cost, h}),
            K::InvalidValue, F::RouteHeuristic);
    // A finite maximum is valid until actual addition overflows; it must not
    // be rejected solely because it is large.
    const NavCostComponents huge{std::numeric_limits<double>::max(), 0, 0, 0};
    auto r = NavRouteSearch::search(*g, request(1, 2), {&huge, numericCost, nullptr});
    assert(r && r.value->status == NavRouteStatus::Complete);
    corridor(*r.value, {{1}, {2}}, huge.distance);
}
void metricSizeBoundaries() {
    // Repeated expansions can examine/relax more edges than size_t can count.
    // Exercise the production guard directly instead of allocating a huge graph
    // or running SIZE_MAX expansions. Errors must not wrap or use Graph context.
    auto value = noFailure - 1;
    assert(query::detail::incrementRouteMetric(value).isNone() && value == noFailure);
    const auto overflow = query::detail::incrementRouteMetric(value);
    assert((overflow == diagnostics::NavError{K::OffsetOverflow, 0,
            diagnostics::NavRecord::Route, F::None}));
    assert(value == noFailure);
    value = 0;
    assert(query::detail::incrementRouteMetric(value).isNone() && value == 1);
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
    capPrecedence();
    partialRanking();
    staleAncestorEvidence();
    queryByteBoundaries();
    allocationFailureSweeps();
    callbackFailureStages();
    numericFailureStages();
    metricSizeBoundaries();
}
