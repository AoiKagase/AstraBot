// SPDX-License-Identifier: MPL-2.0
#include "route_fixture.hpp"
#include "nav/enrichment/traversal_link.hpp"
#include "nav/enrichment/detail/validation.hpp"
#include "nav/query/route_search.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <atomic>
#include <thread>
#include <type_traits>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {
thread_local std::size_t failAfter = std::numeric_limits<std::size_t>::max();
void *allocate(std::size_t size) {
    if (failAfter != std::numeric_limits<std::size_t>::max()) {
        if (failAfter == 0) throw std::bad_alloc{};
        --failAfter;
    }
    void *p = std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc{};
    return p;
}
}
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
using namespace astrabot::nav::enrichment;
using K = diagnostics::NavErrorKind;
using F = diagnostics::NavField;
using GraphResult = diagnostics::ReadResult<std::shared_ptr<const NavGraph>>;
namespace {
const NavGraphLimits graphLimits{10, 100, 1000000};
const NavEnrichmentLimits linkLimits{100, 1000000};
NavMapFingerprint fingerprint() { NavMapFingerprint f{}; f[0] = 0x42; return f; }
route_test::Area area(std::uint32_t id, float z) {
    return {id, {{0, 0, z}, {2, 2, z}, z, z}};
}
auto snapshot() { return route_test::snapshot({area(2, 10), area(1, 0), area(3, 20)}); }
NavTraversalLink up(std::uint64_t id = 1) {
    return {1, 1, id, {1}, {2}, {1, 1, 0}, {1, 1, 10},
            model::NavTraversalKind::Ladder, NavLinkDirection::Up, 2};
}
NavTraversalLinkSet links() { return {fingerprint(), {up()}}; }
GraphResult compose(const NavTraversalLinkSet &set) {
    return NavGraph::compose(snapshot(), fingerprint(), set, graphLimits, linkLimits);
}
void failure(const GraphResult &r, K kind, F field,
             diagnostics::NavRecord record = diagnostics::NavRecord::TraversalLink) {
    assert(!r && !r.value);
    assert((r.error == diagnostics::NavError{kind, 0, record, field}));
}
void shapeAndIdentity() {
    auto set = links();
    set.links.push_back(up(2));
    auto r = compose(set);
    assert(r && (*r.value)->areaCount() == 3 && (*r.value)->edgeCount() == 2);
    const auto &g = **r.value;
    assert(g.area(0).id == model::NavAreaId{1});
    assert(g.edge(0).external && g.edge(0).external->linkId == 1);
    assert(g.edge(1).external && g.edge(1).external->linkId == 2);
    assert(g.edge(0).external->entry.z == 0 && g.edge(0).external->exit.z == 10);
    assert(g.edge(0).traversal == model::NavTraversalKind::Ladder);
    assert(g.edgeBegin(1) == g.edgeEnd(1)); // no inferred reverse edge
    std::reverse(set.links.begin(), set.links.end());
    r = compose(set);
    assert(r && (*r.value)->edge(0).external->linkId == 1);
    set.links[0].sourceId = 2; set.links[0].generation = 9;
    assert(compose(set)); // different sources may use different generations
}
void individualValidation() {
    auto set = links();
    set.fingerprint[31] = 1;
    failure(compose(set), K::InvalidValue, F::LinkFingerprint);
    const auto check = [](auto mutate, K kind, F field) {
        auto candidate = links(); mutate(candidate.links[0]); failure(compose(candidate), kind, field);
    };
    check([](auto &l) { l.sourceId = 0; }, K::InvalidValue, F::LinkSourceId);
    check([](auto &l) { l.generation = 0; }, K::InvalidValue, F::LinkGeneration);
    check([](auto &l) { l.linkId = 0; }, K::InvalidValue, F::LinkId);
    check([](auto &l) { l.from = {0}; }, K::InvalidValue, F::LinkFrom);
    check([](auto &l) { l.to = {0}; }, K::InvalidValue, F::LinkTo);
    check([](auto &l) { l.from = {99}; }, K::DanglingReference, F::LinkFrom);
    check([](auto &l) { l.to = {99}; }, K::DanglingReference, F::LinkTo);
    check([](auto &l) { l.to = l.from; }, K::InvalidValue, F::LinkTo);
    check([](auto &l) { l.traversal = static_cast<model::NavTraversalKind>(255); },
          K::UnsupportedValue, F::LinkTraversal);
    check([](auto &l) { l.direction = static_cast<NavLinkDirection>(255); },
          K::UnsupportedValue, F::LinkDirection);
    for (auto number : {std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()}) {
        check([number](auto &l) { l.entry.x = number; }, K::InvalidGeometry, F::LinkEntry);
        check([number](auto &l) { l.exit.y = number; }, K::InvalidGeometry, F::LinkExit);
        check([number](auto &l) { l.additionalCost = number; }, K::InvalidValue, F::LinkCost);
    }
    check([](auto &l) { l.exit.z = 0; }, K::InvalidGeometry, F::LinkDirection);
    check([](auto &l) { l.direction = NavLinkDirection::Down; }, K::InvalidGeometry, F::LinkDirection);
    check([](auto &l) { l.additionalCost = -1; }, K::InvalidValue, F::LinkCost);
    for (auto kind : {model::NavTraversalKind::Walk, model::NavTraversalKind::Crouch,
                      model::NavTraversalKind::Jump, model::NavTraversalKind::Ladder,
                      model::NavTraversalKind::Drop}) {
        set = links(); set.links[0].traversal = kind;
        set.links[0].direction = NavLinkDirection::Forward;
        set.links[0].entry = {200, -300, 10}; // no invented area containment constraint
        set.links[0].additionalCost = 0;
        assert(compose(set));
    }
}
void relationalValidation() {
    auto set = links(); set.links.push_back(up());
    failure(compose(set), K::DuplicateId, F::LinkId);
    set.links[1].additionalCost = 3;
    failure(compose(set), K::InvalidValue, F::LinkConflict);
    set.links.push_back(up());
    for (int i = 0; i < 3; ++i) {
        std::rotate(set.links.begin(), set.links.begin() + 1, set.links.end());
        failure(compose(set), K::InvalidValue, F::LinkConflict);
    }
    set.links[1].generation = 2;
    failure(compose(set), K::InvalidValue, F::LinkGenerationConflict);
    set.links[2].linkId = 0; // individual errors before relational errors
    failure(compose(set), K::InvalidValue, F::LinkId);
    set.fingerprint[0] = 1;
    failure(compose(set), K::InvalidValue, F::LinkFingerprint);
    set = links(); auto invalid = up(2); invalid.additionalCost = -1;
    set.links.push_back(invalid); set.links[0].to = {99};
    failure(compose(set), K::DanglingReference, F::LinkTo);
    std::reverse(set.links.begin(), set.links.end());
    failure(compose(set), K::InvalidValue, F::LinkCost);
}
void budgets() {
    auto snap = snapshot(); auto set = links();
    failure(NavGraph::compose({}, fingerprint(), set, graphLimits, linkLimits),
            K::InvalidInput, F::GraphBytes, diagnostics::NavRecord::Graph);
    failure(NavGraph::compose(snap, fingerprint(), set, graphLimits, {0, 1000000}),
            K::CountLimitExceeded, F::LinkCount);
    const auto working = (snap->areas().size() + set.links.size()) * sizeof(std::size_t);
    failure(NavGraph::compose(snap, fingerprint(), set, graphLimits, {1, working - 1}),
            K::CountLimitExceeded, F::LinkWorkingBytes);
    auto r = NavGraph::compose(snap, fingerprint(), set, graphLimits, {1, working});
    assert(r);
    const auto bytes = (*r.value)->logicalBytes();
    assert(NavGraph::compose(snap, fingerprint(), set, {3, 1, bytes}, {1, working}));
    failure(NavGraph::compose(snap, fingerprint(), set, {3, 1, bytes - 1}, linkLimits),
            K::CountLimitExceeded, F::GraphBytes, diagnostics::NavRecord::Graph);
    failure(NavGraph::compose(snap, fingerprint(), set, {3, 0, 1000000}, linkLimits),
            K::CountLimitExceeded, F::ConnectionCount, diagnostics::NavRecord::Graph);
    failure(NavGraph::compose(snap, fingerprint(), set, {2, 10, 1000000}, linkLimits),
            K::CountLimitExceeded, F::AreaCount, diagnostics::NavRecord::Graph);
    set.links.clear();
    r = NavGraph::compose(snap, fingerprint(), set, graphLimits, {0, 0});
    assert(r && (*r.value)->edgeCount() == 0);
}
NavRouteRequest request(std::uint32_t from, std::uint32_t to) {
    return {{from}, {to}, {10, 1000000}, false};
}
auto search(const NavGraph &g, std::uint32_t from, std::uint32_t to) {
    return NavRouteSearch::search(g, request(from, to));
}
void ladderRoutes() {
    auto snap = snapshot(); auto set = links();
    auto base = NavGraph::build(snap, graphLimits); assert(base);
    auto r = search(**base.value, 1, 2);
    assert(r && r.value->status == NavRouteStatus::Unreachable && r.value->steps.empty());
    auto g = compose(set); assert(g);
    r = search(**g.value, 1, 2);
    assert(r && r.value->status == NavRouteStatus::Complete);
    assert(r.value->total == 12 && r.value->components.distance == 10 &&
           r.value->components.traversal == 2);
    assert((r.value->areas == std::vector<model::NavAreaId>{{1}, {2}}));
    assert(r.value->steps.size() == 1 && r.value->steps[0].edge.external);
    assert(r.value->steps[0].edge.external->sourceId == 1 &&
           r.value->steps[0].edge.external->generation == 1 &&
           r.value->steps[0].edge.external->linkId == 1 &&
           r.value->steps[0].edge.external->direction == NavLinkDirection::Up);
    r = search(**g.value, 2, 1);
    assert(r && r.value->status == NavRouteStatus::Unreachable);
    auto down = up(2); down.from = {2}; down.to = {1};
    std::swap(down.entry, down.exit); down.direction = NavLinkDirection::Down;
    down.additionalCost = 5; set.links.push_back(down);
    g = compose(set); assert(g);
    r = search(**g.value, 2, 1);
    assert(r && r.value->total == 15 && r.value->components.traversal == 5);
    assert(r.value->steps[0].edge.external->linkId == 2);
    assert(r.value->steps[0].edge.external->direction == NavLinkDirection::Down);
    assert(snap->areas()[0].connections[0].empty()); // snapshot unchanged
}
void parallelLinksAndPolicies() {
    auto set = links(); set.links.push_back(up(2));
    set.links[0].additionalCost = 4; // cheaper parallel edge must win
    auto g = compose(set); assert(g);
    auto r = search(**g.value, 1, 2);
    assert(r && r.value->total == 12 && r.value->steps[0].edge.external->linkId == 2);
    set.links[0].additionalCost = 2;
    for (int n = 0; n < 2; ++n) {
        std::reverse(set.links.begin(), set.links.end());
        g = compose(set); assert(g);
        r = search(**g.value, 1, 2);
        assert(r && r.value->total == 12 && r.value->steps[0].edge.external->linkId == 1);
    }
    NavRoutePolicy policy{};
    policy.cost = [](const NavCostContext &c, const void *) -> NavCostDecision {
        assert(c.edge.external && c.geometricDistance == 10);
        // Full replacement, deliberately not geometric + link cost.
        return {c.edge.external->linkId == 1, {1, 3, 4, 5}};
    };
    r = NavRouteSearch::search(**g.value, request(1, 2), policy);
    assert(r && r.value->total == 13 && r.value->components.distance == 1 &&
           r.value->components.traversal == 3 && r.value->components.danger == 4 &&
           r.value->components.experience == 5);
    assert(r.value->steps[0].edge.external->linkId == 2);
    auto a = area(1, 0); a.targets[3] = {2};
    auto snap = route_test::snapshot({a, area(2, 10)});
    set.links[0].additionalCost = set.links[1].additionalCost = 0;
    g = NavGraph::compose(snap, fingerprint(), set, graphLimits, linkLimits); assert(g);
    assert((*g.value)->edgeCount() == 3);
    assert(!(*g.value)->edge(0).external && (*g.value)->edge(0).direction == 3);
    r = search(**g.value, 1, 2);
    assert(r && r.value->total == 10 && !r.value->steps[0].edge.external);
    auto base = NavGraph::build(snap, graphLimits); assert(base);
    const auto before = search(**base.value, 1, 2); assert(before);
    assert(before.value->total == r.value->total &&
           before.value->areas == r.value->areas &&
           before.value->steps[0].edge.direction == r.value->steps[0].edge.direction);
    set.links.clear();
    g = NavGraph::compose(snap, fingerprint(), set, graphLimits, {0, 0}); assert(g);
    r = search(**g.value, 1, 2);
    assert(r && r.value->total == before.value->total &&
           r.value->metrics.examinedEdges == before.value->metrics.examinedEdges);
}
void partialAndOverflow() {
    auto set = links(); auto next = up(2);
    next.from = {2}; next.to = {3}; next.entry.z = 10; next.exit.z = 20;
    set.links.push_back(next);
    auto g = compose(set); assert(g);
    auto req = request(1, 3); req.limits.maxExpansions = 1;
    auto r = NavRouteSearch::search(**g.value, req);
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit && r.value->steps.empty());
    req.allowPartial = true;
    r = NavRouteSearch::search(**g.value, req);
    assert(r && r.value->status == NavRouteStatus::ExpansionLimit && r.value->total == 12);
    assert((r.value->areas == std::vector<model::NavAreaId>{{1}, {2}}));
    assert(r.value->steps[0].edge.external->linkId == 1);
    r = search(**g.value, 1, 3);
    assert(r && r.value->total == 24 && r.value->steps.size() == 2);
    for (auto &l : set.links) l.additionalCost = std::numeric_limits<double>::max();
    g = compose(set); assert(g); // individually finite; accumulated route must reject
    r = search(**g.value, 1, 3);
    assert(!r && !r.value && r.error.kind == K::InvalidValue && r.error.field == F::RouteCost);
}
void sameLink(const NavTraversalLink &a, const NavTraversalLink &b) {
    assert(a.sourceId == b.sourceId && a.generation == b.generation && a.linkId == b.linkId);
    assert(a.from == b.from && a.to == b.to && a.traversal == b.traversal);
    assert(a.direction == b.direction && a.additionalCost == b.additionalCost);
    assert(a.entry.x == b.entry.x && a.entry.y == b.entry.y && a.entry.z == b.entry.z);
    assert(a.exit.x == b.exit.x && a.exit.y == b.exit.y && a.exit.z == b.exit.z);
}
void sameRoute(const NavRouteResult &a, const NavRouteResult &b) {
    const auto components = [](const auto &x, const auto &y) {
        assert(x.distance == y.distance && x.traversal == y.traversal &&
               x.danger == y.danger && x.experience == y.experience);
    };
    assert(a.status == b.status && a.areas == b.areas && a.steps.size() == b.steps.size());
    assert(a.total == b.total); components(a.components, b.components);
    assert(a.metrics.expansions == b.metrics.expansions &&
           a.metrics.examinedEdges == b.metrics.examinedEdges &&
           a.metrics.relaxations == b.metrics.relaxations &&
           a.metrics.reopens == b.metrics.reopens && a.metrics.peakOpen == b.metrics.peakOpen);
    for (std::size_t i = 0; i < a.steps.size(); ++i) {
        const auto &x = a.steps[i]; const auto &y = b.steps[i];
        assert(x.total == y.total); components(x.components, y.components);
        assert(x.edge.source == y.edge.source && x.edge.target == y.edge.target &&
               x.edge.direction == y.edge.direction && x.edge.traversal == y.edge.traversal);
        assert(x.edge.external.has_value() == y.edge.external.has_value());
        if (x.edge.external) sameLink(*x.edge.external, *y.edge.external);
    }
}
void workingOverflowAndPreflight() {
    using enrichment::detail::workingBudget;
    const auto maximum = std::numeric_limits<std::size_t>::max();
    for (auto cap : {std::size_t{0}, maximum}) {
        const auto mult = workingBudget(maximum / sizeof(std::size_t) + 1, 1, {maximum, cap});
        const auto add = workingBudget(maximum / sizeof(std::size_t), 1, {maximum, cap});
        assert((mult == diagnostics::NavError{K::OffsetOverflow, 0,
                    diagnostics::NavRecord::TraversalLink, F::LinkWorkingBytes}));
        assert(add == mult);
    }
    auto snap = snapshot(); auto set = links();
    auto previous = compose(set); assert(previous);
    const auto exact = (*previous.value)->logicalBytes();
    const auto check = [&](NavGraphLimits gl, NavEnrichmentLimits el, K kind, F field,
                           diagnostics::NavRecord record) {
        failAfter = 0;
        auto r = NavGraph::compose(snap, fingerprint(), set, gl, el);
        assert(failAfter == 0); // no target allocation before cap/fingerprint rejection
        failAfter = maximum;
        failure(r, kind, field, record);
    };
    check(graphLimits, {0, maximum}, K::CountLimitExceeded, F::LinkCount,
          diagnostics::NavRecord::TraversalLink);
    check(graphLimits, {1, 0}, K::CountLimitExceeded, F::LinkWorkingBytes,
          diagnostics::NavRecord::TraversalLink);
    check({3, 1, exact - 1}, linkLimits, K::CountLimitExceeded, F::GraphBytes,
          diagnostics::NavRecord::Graph);
    set.fingerprint[0] = 0;
    check(graphLimits, linkLimits, K::InvalidValue, F::LinkFingerprint,
          diagnostics::NavRecord::TraversalLink);
    // Caps precede fingerprint and all semantic errors.
    check(graphLimits, {0, maximum}, K::CountLimitExceeded, F::LinkCount,
          diagnostics::NavRecord::TraversalLink);
}
void failureSweeps() {
    const auto maximum = std::numeric_limits<std::size_t>::max();
    auto snap = snapshot(); auto set = links();
    auto built = compose(set); assert(built);
    const auto previous = *built.value;
    const auto saved = search(*previous, 1, 2); assert(saved);
    bool success = false;
    for (std::size_t n = 0; n < 32; ++n) {
        failAfter = n;
        auto r = NavGraph::compose(snap, fingerprint(), set, graphLimits, linkLimits);
        const auto remaining = failAfter; failAfter = maximum;
        auto retained = search(*previous, 1, 2); assert(retained);
        sameRoute(*retained.value, *saved.value);
        assert(snap->areas()[0].connections[0].empty());
        if (r) {
            assert(n >= 6 && remaining == 0);
            auto route = search(**r.value, 1, 2); assert(route);
            sameRoute(*route.value, *saved.value);
            std::printf("compose: %zu allocation failures then success\n", n);
            success = true; break;
        }
        assert(!r.value && r.error.kind == K::AllocationFailure);
    }
    assert(success);
    for (bool partial : {false, true}) {
        success = false;
        auto req = request(1, partial ? 3 : 2);
        if (partial) { req.limits.maxExpansions = 1; req.allowPartial = true; }
        const auto expected = NavRouteSearch::search(*previous, req); assert(expected);
        for (std::size_t n = 0; n < 32; ++n) {
            failAfter = n;
            auto r = NavRouteSearch::search(*previous, req);
            const auto remaining = failAfter; failAfter = maximum;
            auto retained = search(*previous, 1, 2); assert(retained);
            sameRoute(*retained.value, *saved.value);
            if (r) {
                assert(n >= 4 && remaining == 0);
                sameRoute(*r.value, *expected.value);
                std::printf("route partial=%d: %zu allocation failures then success\n", partial, n);
                success = true; break;
            }
            assert(!r.value && r.error.kind == K::AllocationFailure);
        }
        assert(success);
    }
}
void lifetimeAndConcurrentReads() {
    static_assert(std::is_same_v<decltype(std::declval<const NavGraph &>().edge(0)),
                                 const NavDirectedEdge &>);
    static_assert(std::is_same_v<decltype(std::declval<GraphResult>().value)::value_type,
                                 std::shared_ptr<const NavGraph>>);
    std::shared_ptr<const NavGraph> graph;
    std::weak_ptr<const model::NavMeshSnapshot> weak;
    NavRouteResult saved;
    {
        auto snap = snapshot(); weak = snap;
        auto set = links();
        auto r = NavGraph::compose(snap, fingerprint(), set, graphLimits, linkLimits); assert(r);
        graph = *r.value;
        set.links[0].sourceId = 999; set.links[0].additionalCost = 999;
        auto route = search(*graph, 1, 2); assert(route);
        saved = *route.value;
        assert(saved.total == 12 && saved.steps[0].edge.external->sourceId == 1);
    }
    assert(!weak.expired()); // graph retains snapshot; input bytes/set are gone
    std::atomic<unsigned> ready{0};
    std::atomic<bool> start{false};
    const auto run = [&] {
        ready.fetch_add(1);
        while (!start.load()) std::this_thread::yield();
        for (int i = 0; i < 100; ++i) {
            auto r = search(*graph, 1, 2); assert(r);
            sameRoute(*r.value, saved);
        }
    };
    std::thread first(run), second(run);
    while (ready.load() != 2) std::this_thread::yield();
    start.store(true);
    first.join(); second.join();
    graph.reset(); assert(weak.expired());
    assert(saved.total == 12);
    sameLink(*saved.steps[0].edge.external, up());
}
void validationMatrix() {
    static_assert(sizeof(NavMapFingerprint) == 32);
    // Every coordinate participates in finite validation, not just the axes
    // used by direction checks.
    for (auto member : {&NavLinkPoint::x, &NavLinkPoint::y, &NavLinkPoint::z}) {
        for (auto point : {&NavTraversalLink::entry, &NavTraversalLink::exit}) {
            auto set = links();
            (set.links[0].*point).*member = std::numeric_limits<double>::quiet_NaN();
            failure(compose(set), K::InvalidGeometry,
                    point == &NavTraversalLink::entry ? F::LinkEntry : F::LinkExit);
        }
    }
    const auto conflict = [](auto change) {
        auto set = links(); set.links.push_back(up());
        change(set.links[1]);
        failure(compose(set), K::InvalidValue, F::LinkConflict);
    };
    conflict([](auto &l) { l.from = {3}; });
    conflict([](auto &l) { l.to = {3}; });
    conflict([](auto &l) { l.entry.x = 9; });
    conflict([](auto &l) { l.entry.y = 9; });
    conflict([](auto &l) { l.entry.z = -1; });
    conflict([](auto &l) { l.exit.x = 9; });
    conflict([](auto &l) { l.exit.y = 9; });
    conflict([](auto &l) { l.exit.z = 11; });
    conflict([](auto &l) { l.traversal = model::NavTraversalKind::Jump; });
    conflict([](auto &l) { l.direction = NavLinkDirection::Forward; });
    auto set = links();
    set.links[0].additionalCost = -0.0;
    auto other = set.links[0]; other.additionalCost = 0.0;
    set.links.push_back(other);
    failure(compose(set), K::DuplicateId, F::LinkId); // numeric, not padding/bit equality
    set = links();
    set.links.push_back(up()); // source1 duplicate precedes source2 mixed generations
    auto secondSource = up(); secondSource.sourceId = 2;
    set.links.push_back(secondSource); ++secondSource.generation;
    set.links.push_back(secondSource);
    for (int n = 0; n < 4; ++n) {
        std::rotate(set.links.begin(), set.links.begin() + 1, set.links.end());
        failure(compose(set), K::DuplicateId, F::LinkId);
    }
    set.links.clear(); set.fingerprint[0] = 0;
    failure(compose(set), K::InvalidValue, F::LinkFingerprint);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    shapeAndIdentity(); individualValidation(); relationalValidation(); budgets();
    ladderRoutes(); parallelLinksAndPolicies(); partialAndOverflow();
    workingOverflowAndPreflight(); failureSweeps(); lifetimeAndConcurrentReads();
    validationMatrix();
    std::puts("enrichment validation and composition passed");
}
