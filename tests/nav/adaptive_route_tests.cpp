// SPDX-License-Identifier: MPL-2.0
#include "nav/query/adaptive_route.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

namespace {
namespace e = astrabot::core::experience;
namespace p = astrabot::core::perception;
using namespace astrabot::nav;
using namespace astrabot::nav::query;

e::MapIdentity map() {
    e::MapIdentity value{};
    value.name = "de_adaptive";
    return value;
}

std::shared_ptr<const NavGraph> diamond() {
    auto a = route_test::Area{1, {{0, 0, 0}, {2, 2, 0}, 0, 0}};
    auto b = route_test::Area{2, {{3, 0, 0}, {5, 2, 0}, 0, 0}};
    auto c = route_test::Area{3, {{3, 0, 0}, {5, 2, 0}, 0, 0}};
    auto d = route_test::Area{4, {{6, 0, 0}, {8, 2, 0}, 0, 0}};
    a.targets[0] = {2, 3}; b.targets[0] = {4}; c.targets[0] = {4};
    auto result = NavGraph::build(route_test::snapshot({a, b, c, d}), {100, 1000, 100000});
    assert(result);
    return *result.value;
}

e::ExperienceSnapshot snapshot(double area2Visits, double area3Visits,
                               double area2Danger, double area3Danger) {
    e::ExperienceSnapshot result{};
    result.map = map(); result.round = {1};
    e::AreaExperience first{}; first.area = 2; first.visits = area2Visits;
    first.dangerT = area2Danger;
    e::AreaExperience second{}; second.area = 3; second.visits = area3Visits;
    second.dangerT = area3Danger;
    result.areas = {first, second};
    assert(result.valid());
    return result;
}

NavRouteResult route(const std::shared_ptr<const NavGraph>& graph,
                     const AdaptiveRouteContext& context) {
    const auto result = NavRouteSearch::search(*graph, {{1}, {4}, {100, 100000}, false},
                                                adaptiveRoutePolicy(context));
    assert(result && result.value->status == NavRouteStatus::Complete);
    return *result.value;
}

void expectMiddle(const NavRouteResult& result, std::uint32_t middle) {
    assert(result.areas.size() == 3U);
    assert(result.areas[0] == model::NavAreaId{1});
    assert(result.areas[1] == model::NavAreaId{middle});
    assert(result.areas[2] == model::NavAreaId{4});
}

void experienceChangesTheRoute() {
    e::ExperienceModel experience;
    assert(experience.load(snapshot(100.0, 0.0, 0.0, 0.0)));
    AdaptiveRouteContext context{};
    context.experience = &experience;
    context.settings.experienceWeight = 10.0;
    const auto graph = diamond();
    auto result = route(graph, context);
    expectMiddle(result, 2);

    assert(experience.load(snapshot(0.0, 100.0, 0.0, 0.0)));
    result = route(graph, context);
    expectMiddle(result, 3);
}

void personalityAndStylesChangeRiskWeighting() {
    e::ExperienceModel experience;
    assert(experience.load(snapshot(0.0, 0.0, 10.0, 0.0)));
    AdaptiveRouteContext context{};
    context.experience = &experience;
    context.settings.team = p::Team::Terrorist;
    context.settings.experienceWeight = 0.0;
    context.settings.style = AdaptiveRouteStyle::Safe;
    context.settings.personality = AdaptiveRoutePersonality::Cautious;
    const auto graph = diamond();
    auto result = route(graph, context);
    expectMiddle(result, 3);
    assert(std::string(adaptiveRouteStyleName(AdaptiveRouteStyle::LowExposure)) == "LOW_EXPOSURE");
    assert(std::string(adaptiveRoutePersonalityName(AdaptiveRoutePersonality::Aggressive)) == "Aggressive");
}

void traversalEvidenceAddsRisk() {
    auto a = route_test::Area{1, {{0, 0, 0}, {2, 2, 0}, 0, 0}};
    auto b = route_test::Area{2, {{3, 0, 0}, {5, 2, 0}, 0, 0}};
    auto c = route_test::Area{3, {{3, 0, 0}, {5, 2, 0}, 0, 0}};
    auto d = route_test::Area{4, {{6, 0, 0}, {8, 2, 0}, 0, 0}};
    a.targets[0] = {3}; b.targets[0] = {4}; c.targets[0] = {4};
    enrichment::NavTraversalLink link{};
    link.sourceId = 1; link.generation = 1; link.linkId = 7; link.from = {1}; link.to = {2};
    link.entry = {1, 1, 0}; link.exit = {4, 1, 0};
    enrichment::NavTraversalLinkSet links{}; links.links.push_back(link);
    const auto graph = NavGraph::compose(route_test::snapshot({a, b, c, d}), {}, links,
                                          {100, 1000, 100000}, {100, 100000});
    assert(graph);
    AdaptiveTraversalExperience evidence{7, 10.0, 10.0, 0.0, 0.0, 10.0};
    AdaptiveRouteContext context{};
    context.traversalExperience = &evidence;
    context.traversalExperienceCount = 1;
    context.settings.experienceWeight = 0.0;
    auto result = route(*graph.value, context);
    expectMiddle(result, 3);

    evidence = {7, 10.0, 10.0, 0.0, 0.0, 0.0};
    result = route(*graph.value, context);
    expectMiddle(result, 2);
}

void invalidContextIsRejected() {
    AdaptiveRouteContext context{};
    context.settings.distanceWeight = -1.0;
    assert(!context.valid());
    const auto result = NavRouteSearch::search(*diamond(), {{1}, {4}, {100, 100000}, false},
                                               adaptiveRoutePolicy(context));
    assert(!result && result.error.field == diagnostics::NavField::RouteCost);
}

} // namespace

int main() {
    experienceChangesTheRoute();
    personalityAndStylesChangeRiskWeighting();
    traversalEvidenceAddsRisk();
    invalidContextIsRejected();
}
