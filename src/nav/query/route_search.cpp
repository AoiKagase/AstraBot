// SPDX-License-Identifier: MPL-2.0
#include "nav/query/route_search.hpp"
#include "nav/query/detail/indexed_heap.hpp"
#include "nav/query/detail/route_budget.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <tuple>

namespace astrabot::nav::query {
namespace {
using Result = diagnostics::ReadResult<NavRouteResult>;
using Error = diagnostics::NavError;
using K = diagnostics::NavErrorKind;
using F = diagnostics::NavField;
constexpr auto absent = std::numeric_limits<std::size_t>::max();

Error routeError(K kind, F field) noexcept {
    return {kind, 0, diagnostics::NavRecord::Route, field};
}
bool valid(double value) noexcept { return std::isfinite(value) && value >= 0; }
bool add(double &total, double value) noexcept {
    total += value;
    return valid(total);
}
bool costTotal(const NavCostComponents &c, double &total) noexcept {
    total = c.distance;
    return valid(c.distance) && valid(c.traversal) && valid(c.danger) && valid(c.experience) &&
           add(total, c.traversal) && add(total, c.danger) && add(total, c.experience);
}
double distance(NavQueryPoint a, NavQueryPoint b) noexcept {
    return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}
struct Record {
    double g{std::numeric_limits<double>::infinity()}, h{0}, f{0}, goalDistance{0};
    std::size_t parentEdge{absent}, heapPosition{absent};
    NavCostComponents components{};
    double edgeTotal{0};
    bool closed{false}, heuristicReady{false};
};
struct Query {
    std::vector<Record> records;
    detail::IndexedHeap<Record> open{records};
};

Error preflight(const NavGraph &graph, const NavRouteLimits &limits,
                const Query &query, const NavRouteResult &result) noexcept {
    const auto count = graph.areaCount();
    // Both control objects are charged once; their vectors' elements are
    // charged separately. The graph and borrowed policy context are not owned.
    std::size_t bytes = 0;
    const auto charge = [&bytes](std::size_t n, std::size_t size) {
        auto error = detail::charge(bytes, n, size, std::numeric_limits<std::size_t>::max());
        error.record = diagnostics::NavRecord::Route;
        error.field = F::RouteBytes;
        return error;
    };
    for (const auto &part : {std::pair<std::size_t, std::size_t>{1, sizeof(Query)},
                            {1, sizeof(NavRouteResult)}, {count, sizeof(Record)},
                            {count, sizeof(std::size_t)}, {count, sizeof(model::NavAreaId)},
                            {count == 0 ? 0 : count - 1, sizeof(NavRouteStep)}}) {
        auto error = charge(part.first, part.second);
        if (!error.isNone())
            return error;
    }
    if (bytes > limits.maxWorkingBytes)
        return routeError(K::CountLimitExceeded, F::RouteBytes);
    if (count > query.records.max_size() || count > query.open.max_size() ||
        count > result.areas.max_size() || (count != 0 && count - 1 > result.steps.max_size()))
        return routeError(K::AllocationFailure, F::RouteBytes);
    return {};
}

Error heuristic(const NavGraph &graph, Query &query, std::size_t vertex,
                std::size_t goal, NavRoutePolicy policy, F &activeField) {
    auto &record = query.records[vertex];
    if (record.heuristicReady)
        return {};
    record.goalDistance = distance(graph.center(vertex), graph.center(goal));
    activeField = F::RouteHeuristic;
    record.h = policy.heuristic
        ? policy.heuristic({graph.area(vertex), graph.area(goal), record.goalDistance}, policy.context)
        : (policy.cost ? 0 : record.goalDistance);
    if (!valid(record.goalDistance) || !valid(record.h) || (vertex == goal && record.h != 0))
        return routeError(K::InvalidValue, F::RouteHeuristic);
    record.heuristicReady = true;
    return {};
}

Error reconstruct(const NavGraph &graph, const Query &query, std::size_t start,
                  std::size_t target, NavRouteResult &result) {
    // All maximum output storage was budgeted before query allocation. Reserve
    // only when publishing a corridor; a failure still discards the whole result.
    try {
        result.areas.reserve(graph.areaCount());
        result.steps.reserve(graph.areaCount() - 1);
    } catch (const std::length_error &) {
        return routeError(K::AllocationFailure, F::RouteBytes);
    }
    for (std::size_t count = 0; count < graph.areaCount(); ++count) {
        result.areas.push_back(graph.area(target).id);
        if (target == start)
            break;
        const auto &record = query.records[target];
        if (record.parentEdge == absent || count + 1 == graph.areaCount())
            return routeError(K::InvalidValue, F::RouteCost);
        const auto &edge = graph.edge(record.parentEdge);
        result.steps.push_back({edge, record.components, record.edgeTotal});
        const auto parent = graph.find(edge.source);
        if (!parent)
            return routeError(K::InvalidValue, F::RouteCost);
        target = *parent;
    }
    std::reverse(result.areas.begin(), result.areas.end());
    std::reverse(result.steps.begin(), result.steps.end());
    // Forward evidence, not a candidate's possibly stale g, defines published
    // totals. No callback is invoked during reconstruction.
    for (const auto &step : result.steps) {
        if (!add(result.components.distance, step.components.distance) ||
            !add(result.components.traversal, step.components.traversal) ||
            !add(result.components.danger, step.components.danger) ||
            !add(result.components.experience, step.components.experience) ||
            !add(result.total, step.total))
            return routeError(K::InvalidValue, F::RouteCost);
    }
    return {};
}
} // namespace

diagnostics::ReadResult<NavRouteResult>
NavRouteSearch::search(const NavGraph &graph, const NavRouteRequest &request,
                       NavRoutePolicy policy) noexcept {
    const auto start = graph.find(request.start), goal = graph.find(request.goal);
    if (!request.start.isValid() || !start)
        return Result::failure(routeError(K::InvalidInput, F::RouteStart));
    if (!request.goal.isValid() || !goal)
        return Result::failure(routeError(K::InvalidInput, F::RouteGoal));
    F activeField = F::RouteBytes;
    try {
        Query query;
        NavRouteResult result;
        auto error = preflight(graph, request.limits, query, result);
        if (!error.isNone())
            return Result::failure(error);
        try {
            query.records.resize(graph.areaCount());
            query.open.reserve(graph.areaCount());
        } catch (const std::length_error &) {
            return Result::failure(routeError(K::AllocationFailure, F::RouteBytes));
        }
        // Validate goal zero even for start==goal or an undiscovered goal. Cache
        // this value, so discovering the goal never invokes its callback twice.
        error = heuristic(graph, query, *goal, *goal, policy, activeField);
        if (error.isNone())
            error = heuristic(graph, query, *start, *goal, policy, activeField);
        if (!error.isNone())
            return Result::failure(error);
        auto &initial = query.records[*start];
        initial.g = 0;
        initial.f = initial.h;
        activeField = F::RouteBytes;
        query.open.improve(*start);
        result.metrics.peakOpen = 1;
        std::size_t target = absent;
        while (!query.open.empty()) {
            const auto vertex = query.open.top();
            if (vertex == *goal) {
                result.status = NavRouteStatus::Complete;
                target = vertex;
                break;
            }
            if (result.metrics.expansions == request.limits.maxExpansions) {
                result.status = NavRouteStatus::ExpansionLimit;
                if (request.allowPartial) {
                    for (std::size_t i = 0; i < graph.areaCount(); ++i) {
                        const auto &candidate = query.records[i];
                        if (!std::isfinite(candidate.g))
                            continue;
                        if (target == absent ||
                            std::tie(candidate.goalDistance, candidate.g, i) <
                            std::tie(query.records[target].goalDistance, query.records[target].g, target))
                            target = i;
                    }
                }
                break;
            }
            query.open.pop();
            auto &current = query.records[vertex];
            current.closed = true;
            error = detail::incrementRouteMetric(result.metrics.expansions);
            if (!error.isNone())
                return Result::failure(error);
            for (auto edgeIndex = graph.edgeBegin(vertex); edgeIndex < graph.edgeEnd(vertex); ++edgeIndex) {
                error = detail::incrementRouteMetric(result.metrics.examinedEdges);
                if (!error.isNone())
                    return Result::failure(error);
                const auto &edge = graph.edge(edgeIndex);
                const auto next = graph.targetIndex(edgeIndex);
                const auto geometric = distance(graph.center(vertex), graph.center(next));
                activeField = F::RouteCost;
                const auto decision = policy.cost
                    ? policy.cost({edge, graph.area(vertex), graph.area(next), geometric}, policy.context)
                    : NavCostDecision{false, {geometric,
                        edge.external ? edge.external->additionalCost : 0, 0, 0}};
                if (decision.blocked)
                    continue;
                double total = 0;
                if (!costTotal(decision.components, total))
                    return Result::failure(routeError(K::InvalidValue, F::RouteCost));
                double g = current.g;
                if (!add(g, total))
                    return Result::failure(routeError(K::InvalidValue, F::RouteCost));
                auto &neighbor = query.records[next];
                if (g >= neighbor.g)
                    continue;
                error = heuristic(graph, query, next, *goal, policy, activeField);
                if (!error.isNone())
                    return Result::failure(error);
                double f = g;
                if (!add(f, neighbor.h))
                    return Result::failure(routeError(K::InvalidValue, F::RouteHeuristic));
                neighbor.g = g;
                neighbor.f = f;
                neighbor.parentEdge = edgeIndex;
                neighbor.components = decision.components;
                neighbor.edgeTotal = total;
                error = detail::incrementRouteMetric(result.metrics.relaxations);
                if (!error.isNone())
                    return Result::failure(error);
                if (neighbor.closed) {
                    neighbor.closed = false;
                    error = detail::incrementRouteMetric(result.metrics.reopens);
                    if (!error.isNone())
                        return Result::failure(error);
                }
                activeField = F::RouteBytes;
                query.open.improve(next);
                result.metrics.peakOpen = std::max(result.metrics.peakOpen, query.open.size());
            }
        }
        if (target != absent) {
            activeField = F::RouteBytes;
            error = reconstruct(graph, query, *start, target, result);
            if (!error.isNone())
                return Result::failure(error);
        }
        return Result::success(std::move(result));
    } catch (const std::bad_alloc &) {
        return Result::failure(routeError(K::AllocationFailure, activeField));
    } catch (...) {
        return Result::failure(routeError(K::PolicyFailure, activeField));
    }
}
} // namespace astrabot::nav::query
