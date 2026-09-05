// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/query/graph.hpp"
#include <vector>

namespace astrabot::nav::query {
struct NavCostComponents { double distance{0}, traversal{0}, danger{0}, experience{0}; };
struct NavCostDecision { bool blocked{false}; NavCostComponents components{}; };
struct NavCostContext {
    const NavDirectedEdge &edge;
    const model::NavAreaRecord &source;
    const model::NavAreaRecord &target;
    double geometricDistance;
};
struct NavHeuristicContext {
    const model::NavAreaRecord &area;
    const model::NavAreaRecord &goal;
    double geometricDistance;
};
struct NavRoutePolicy {
    // Callbacks must be pure. A custom cost defaults to h=0; an explicit
    // heuristic must be admissible, finite, nonnegative and zero at the goal.
    // Inconsistent admissible heuristics are supported by reopening vertices.
    const void *context{nullptr}; // Borrowed for one synchronous search.
    NavCostDecision (*cost)(const NavCostContext &, const void *){nullptr};
    double (*heuristic)(const NavHeuristicContext &, const void *){nullptr};
};
// Zero permits no expansions/bytes. Logical bytes include query controls,
// per-vertex records and heap indices, and worst-case owned result storage.
struct NavRouteLimits { std::size_t maxExpansions{0}, maxWorkingBytes{0}; };
struct NavRouteRequest {
    model::NavAreaId start{}, goal{};
    NavRouteLimits limits{};
    bool allowPartial{false};
};
enum class NavRouteStatus { Complete, Unreachable, ExpansionLimit };
struct NavRouteMetrics {
    std::size_t expansions{0}, examinedEdges{0}, relaxations{0}, reopens{0}, peakOpen{0};
};
struct NavRouteStep { NavDirectedEdge edge{}; NavCostComponents components{}; double total{0}; };
struct NavRouteResult {
    // Only opt-in ExpansionLimit results may contain a partial corridor.
    // Unreachable results have no corridor. Complete includes both endpoints.
    NavRouteStatus status{NavRouteStatus::Unreachable};
    std::vector<model::NavAreaId> areas;
    std::vector<NavRouteStep> steps;
    NavCostComponents components{};
    double total{0};
    NavRouteMetrics metrics{};
};
} // namespace astrabot::nav::query
