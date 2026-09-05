// SPDX-License-Identifier: MPL-2.0
#include "evidence/scene.hpp"
#include "nav/query/spatial_index.hpp"
#include "nav/query/route_search.hpp"
#include <cstdio>
using namespace evidence;
namespace q = astrabot::nav::query;
void compare(const Scene& s, const q::NavSpatialIndex& index, nav::model::NavVector3 point) {
    for (double vertical : {0., 1., 20.}) for (double radius : {0., 2., 100.}) {
        for (bool containing : {true, false}) {
            const auto expected = linear(s, point, radius, vertical, containing);
            const auto actual = containing ? index.containing(point, vertical) :
                index.nearestGeometry(point, {radius, vertical});
            check(bool(actual) && actual.value->has_value() == expected.has_value(), "query presence");
            if (expected) {
                const auto& a = **actual.value;
                check(a.areaId.value == expected->id && close(a.distanceSquared, expected->squared), "query rank");
                check(close(a.projectedPoint.x, expected->point[0]) &&
                      close(a.projectedPoint.y, expected->point[1]) &&
                      close(a.projectedPoint.z, expected->point[2]), "projection");
            }
        }
    }
}
int main() {
    evidence::configureErrors();
    auto s = evidence::scene(32);
    // Identical geometry/stacked layers and a disconnected last vertex.
    s.patches[1] = s.patches[0]; s.patches[1].id = 2;
    s.arcs.erase(std::remove_if(s.arcs.begin(), s.arcs.end(),
        [](const Arc& a) { return a.from == 32 || a.to == 32; }), s.arcs.end());
    const auto loaded = evidence::load(evidence::encode(s).bytes);
    evidence::check(bool(loaded), "scene loads");
    const auto index = astrabot::nav::query::NavSpatialIndex::build(*loaded.value, {128, 255, 4194304});
    evidence::check(bool(index), "scene index");
    for (const auto& p : s.patches) {
        for (float x : {p.x, p.x + 1, p.x + 2, p.x - .5f})
            for (float y : {p.y, p.y + 1, p.y + 2})
                compare(s, **index.value, {x, y, p.z00});
    }
    for (unsigned i = 0; i < 128; ++i)
        compare(s, **index.value, {float(i % 17) - 3, float((i * 7) % 19) - 4,
                                  float((i * 11) % 23) - 5});
    const auto graph = q::NavGraph::build(*loaded.value, {128, 4096, 4194304});
    check(bool(graph), "scene graph");
    std::size_t complete = 0, unreachable = 0;
    for (std::size_t start = 0; start < s.patches.size(); ++start) {
        const auto expected = dijkstra(s, start);
        for (std::size_t goal = 0; goal < s.patches.size(); ++goal) {
            const auto actual = q::NavRouteSearch::search(**graph.value,
                {{s.patches[start].id}, {s.patches[goal].id}, {128, 4194304}, false});
            check(bool(actual), "route result");
            const auto& route = *actual.value;
            if (!std::isfinite(expected[goal])) {
                check(route.status == q::NavRouteStatus::Unreachable &&
                      route.areas.empty() && route.steps.empty(), "unreachable");
                ++unreachable; continue;
            }
            check(route.status == q::NavRouteStatus::Complete && close(route.total, expected[goal]), "Dijkstra cost");
            check(!route.areas.empty() && route.areas.front().value == s.patches[start].id &&
                  route.areas.back().value == s.patches[goal].id &&
                  route.steps.size() + 1 == route.areas.size(), "corridor");
            double sum = 0;
            for (std::size_t i = 0; i < route.steps.size(); ++i) {
                const auto& step = route.steps[i];
                const auto arc = std::find_if(s.arcs.begin(), s.arcs.end(), [&](const Arc& a) {
                    return a.from == route.areas[i].value && a.to == route.areas[i+1].value &&
                           a.direction == step.edge.direction;
                });
                check(arc != s.arcs.end() && !step.edge.external &&
                      step.edge.source == route.areas[i] && step.edge.target == route.areas[i+1], "route edge");
                const double cost = arcCost(s.patches[arc->from - 1], s.patches[arc->to - 1]);
                check(close(cost, step.total) && close(cost, step.components.distance) &&
                      step.components.traversal == 0 && step.components.danger == 0 &&
                      step.components.experience == 0, "step components");
                sum += cost;
            }
            check(close(sum, route.total) && close(route.components.distance, sum) &&
                  route.components.traversal == 0 && route.components.danger == 0 &&
                  route.components.experience == 0, "total components");
            ++complete;
        }
    }
    std::printf("linear queries=9216 Dijkstra pairs=1024 complete=%zu unreachable=%zu OK\n", complete, unreachable);
}
