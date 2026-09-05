// SPDX-License-Identifier: MPL-2.0
#include "nav/query/graph.hpp"
#include "nav/query/detail/route_budget.hpp"
#include "route_fixture.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>

namespace {
std::size_t failAfter = std::numeric_limits<std::size_t>::max();
void *allocate(std::size_t size) {
    if (failAfter != std::numeric_limits<std::size_t>::max()) {
        if (failAfter == 0)
            throw std::bad_alloc{};
        --failAfter;
    }
    void *p = std::malloc(size ? size : 1);
    if (!p)
        throw std::bad_alloc{};
    return p;
}
} // namespace
_Ret_notnull_ _Post_writable_byte_size_(size) void *operator new(std::size_t size) {
    return allocate(size);
}
_Ret_notnull_ _Post_writable_byte_size_(size) void *operator new[](std::size_t size) {
    return allocate(size);
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

using namespace astrabot::nav;
using query::NavGraph;
using K = diagnostics::NavErrorKind;
int main() {
    static_assert(!std::is_default_constructible_v<NavGraph>);
    static_assert(std::is_same_v<decltype(NavGraph::build(nullptr, {})),
                  diagnostics::ReadResult<std::shared_ptr<const NavGraph>>>);
    const model::NavExtent square{{0, 0, 0}, {1, 1, 0}, 0, 0};
    route_test::Area area10{10, square}, area20{20, square};
    area10.targets[0] = {20};
    area10.targets[1] = {20};
    auto snap = route_test::snapshot({area20, area10});
    auto r = NavGraph::build(snap, {2, 2, 100000});
    assert(r);
    auto graph = *r.value;
    assert(graph->areaCount() == 2 && graph->edgeCount() == 2);
    assert(graph->area(0).id == model::NavAreaId{10});
    assert(graph->edge(0).source == model::NavAreaId{10});
    assert(graph->edge(0).target == model::NavAreaId{20});
    assert(graph->edge(0).direction == 0 && graph->edge(1).direction == 1);
    assert(graph->edge(0).traversal == model::NavTraversalKind::Walk);
    assert(graph->edgeBegin(0) == 0 && graph->edgeEnd(0) == 2);
    assert(graph->edgeBegin(1) == graph->edgeEnd(1));
    assert(graph->targetIndex(0) == 1 && graph->targetIndex(1) == 1);
    assert(graph->find({10}) == 0 && graph->find({20}) == 1);
    assert(!graph->find({0}) && !graph->find({11}) && !graph->find({30}));
    assert(snap->areas()[0].id == model::NavAreaId{20});
    assert(&graph->area(0) == &snap->areas()[1]);
    const auto bytes = graph->logicalBytes();
    assert(NavGraph::build(snap, {2, 2, bytes}));
    for (query::NavGraphLimits limits : {query::NavGraphLimits{1, 2, bytes},
          {2, 1, bytes}, {2, 2, bytes - 1}, {0, 2, bytes}, {2, 0, bytes}, {2, 2, 0}}) {
        auto failure = NavGraph::build(snap, limits);
        assert(!failure.value && failure.error.kind == K::CountLimitExceeded);
        assert(failure.error.offset == 0 && failure.error.record == diagnostics::NavRecord::Graph);
    }
    assert(NavGraph::build(nullptr, {}).error.kind == K::InvalidInput);
    std::weak_ptr<const model::NavMeshSnapshot> weak = snap;
    snap.reset();
    assert(!weak.expired() && graph->area(0).id == model::NavAreaId{10});
    auto singleton = NavGraph::build(route_test::snapshot({area20}), {1, 0, 100000});
    assert(singleton && (*singleton.value)->edgeCount() == 0);
    assert((*singleton.value)->edgeBegin(0) == (*singleton.value)->edgeEnd(0));
    assert((*singleton.value)->center(0).x == 0.5);

    // Midpoints between adjacent floats must not narrow back to float; Z uses all corners.
    route_test::Area slope{7, {{16777216, 0, 4}, {16777218, 2, 12}, 8, 16}};
    auto sloped = NavGraph::build(route_test::snapshot({slope}), {1, 0, 100000});
    assert(sloped);
    auto center = (*sloped.value)->center(0);
    assert(center.x == 16777217 && center.y == 1 && center.z == 10);
    const float maximum = std::numeric_limits<float>::max();
    route_test::Area huge{8, {{-maximum, -maximum, maximum},
                            {maximum, maximum, maximum}, maximum, maximum}};
    auto big = NavGraph::build(route_test::snapshot({huge}), {1, 0, 100000});
    assert(big);
    center = (*big.value)->center(0);
    assert(center.x == 0 && center.y == 0 && center.z == double(maximum));

    // Wire targets are unsorted; graph order and lookup are canonical.
    route_test::Area area30{30, square};
    area10.targets[0] = {30, 20};
    auto unsorted = route_test::snapshot({area30, area10, area20});
    auto sorted = NavGraph::build(unsorted, {3, 3, 100000});
    assert(sorted);
    assert((*sorted.value)->edge(0).target == model::NavAreaId{20});
    assert((*sorted.value)->edge(1).target == model::NavAreaId{30});
    assert((*sorted.value)->edge(2).direction == 1);
    assert((*sorted.value)->targetIndex(1) == 2);
    assert(unsorted->areas()[1].connections[0][0].target == model::NavAreaId{30});

    bool built = false;
    for (std::size_t n = 0; n < 64; ++n) {
        failAfter = n;
        auto attempt = NavGraph::build(unsorted, {3, 3, 100000});
        failAfter = std::numeric_limits<std::size_t>::max();
        assert(graph->area(0).id == model::NavAreaId{10});
        assert(unsorted->areas()[0].id == model::NavAreaId{30});
        if (attempt) {
            built = true;
            break;
        }
        assert(!attempt.value && attempt.error.kind == K::AllocationFailure);
    }
    assert(built);
    failAfter = 0;
    auto capped = NavGraph::build(unsorted, {3, 3, 0});
    failAfter = std::numeric_limits<std::size_t>::max();
    assert(!capped && capped.error.kind == K::CountLimitExceeded);

    const auto sizeMax = std::numeric_limits<std::size_t>::max();
    std::size_t used = 3;
    assert(query::detail::charge(used, 2, 4, 11).isNone() && used == 11);
    assert(query::detail::charge(used, 1, 1, 11).kind == K::CountLimitExceeded && used == 11);
    assert(query::detail::charge(used, sizeMax, 2, 0).kind == K::OffsetOverflow && used == 11);
    used = sizeMax;
    assert(query::detail::charge(used, 1, 1, sizeMax).kind == K::OffsetOverflow);
    assert(query::detail::charge(used, 0, 8, sizeMax).isNone());
    for (unsigned kind = 0; kind <= 255; ++kind) {
        auto error = query::detail::validateEdge(static_cast<model::NavTraversalKind>(kind));
        assert(error.kind == (kind <= 4 ? K::None : K::UnsupportedValue));
        assert(error.offset == 0);
    }
}
