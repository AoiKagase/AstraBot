// SPDX-License-Identifier: MPL-2.0
#include "nav/query/graph.hpp"
#include "nav/query/detail/route_budget.hpp"
#include "route_fixture.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

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
#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(size)
#endif
void *operator new(std::size_t size) {
    return allocate(size);
}
#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(size)
#endif
void *operator new[](std::size_t size) {
    return allocate(size);
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

using namespace astrabot::nav;
using query::NavGraph;
using K = diagnostics::NavErrorKind;
namespace {
void chargeBoundaries() {
    const auto maximum = std::numeric_limits<std::size_t>::max();
    const auto checkFailure = [](std::size_t before, std::size_t count,
                                 std::size_t size, std::size_t cap, K kind) {
        auto used = before;
        const auto e = query::detail::charge(used, count, size, cap);
        assert((e == diagnostics::NavError{kind, 0, diagnostics::NavRecord::Graph,
                                         diagnostics::NavField::GraphBytes}));
        assert(used == before);
    };
    // Addition and multiplication boundaries, including arithmetic precedence
    // over a zero cap. None of these tests allocates count-sized storage.
    checkFailure(0, maximum / 2 + 1, 2, maximum, K::OffsetOverflow);
    checkFailure(0, maximum / 2 + 1, 2, 0, K::OffsetOverflow);
    checkFailure(maximum - 3, 2, 2, maximum, K::OffsetOverflow);
    checkFailure(maximum, 1, 1, 0, K::OffsetOverflow);
    checkFailure(7, 0, 8, 6, K::CountLimitExceeded);
    checkFailure(7, maximum, 0, 6, K::CountLimitExceeded);
    checkFailure(7, 2, 4, 14, K::CountLimitExceeded);
    std::size_t used = 1;
    assert(query::detail::charge(used, maximum / 2, 2, maximum).isNone());
    assert(used == maximum);
    assert(query::detail::charge(used, 0, maximum, maximum).isNone());
    assert(query::detail::charge(used, maximum, 0, maximum).isNone() && used == maximum);
    used = 7;
    assert(query::detail::charge(used, 2, 4, 15).isNone() && used == 15);
}
void constructionShapeSweeps() {
    const model::NavExtent extent{{0, 0, 0}, {2, 2, 0}, 0, 0};
    std::vector<route_test::Area> dense;
    for (std::uint32_t id = 1; id <= 16; ++id) {
        route_test::Area a{id, extent};
        for (auto &direction : a.targets)
            for (std::uint32_t target = 16; target != 0; --target)
                if (target != id) direction.push_back(target);
        dense.push_back(std::move(a));
    }
    // The independent loader requires at least one area; exercise no-edge
    // construction with a valid disconnected snapshot instead of zero areas.
    for (const auto &areas : {std::vector<route_test::Area>{{1, extent}, {2, extent}},
                              std::vector<route_test::Area>{{1, extent}}, dense}) {
        auto snap = route_test::snapshot(areas);
        const auto published = NavGraph::build(snap, {16, 1024, 1000000});
        assert(published);
        const auto previous = *published.value;
        const auto exact = previous->logicalBytes();
        const auto count = previous->areaCount(), edges = previous->edgeCount();
        const auto checkRetained = [&] {
            assert(failAfter == std::numeric_limits<std::size_t>::max());
            assert(previous->logicalBytes() == exact && previous->areaCount() == count);
            assert(previous->edgeCount() == edges && snap->areas().size() == count);
            for (std::size_t i = 0; i < count; ++i) {
                assert(&previous->area(i) == &snap->areas()[i]);
                assert(previous->find(snap->areas()[i].id) == i);
                assert(previous->center(i).x == 1 && previous->center(i).y == 1);
                for (auto edge = previous->edgeBegin(i); edge < previous->edgeEnd(i); ++edge) {
                    const auto &e = previous->edge(edge);
                    assert(e.source == snap->areas()[i].id);
                    assert(previous->area(previous->targetIndex(edge)).id == e.target);
                    assert(e.direction == (edge - previous->edgeBegin(i)) / (count - 1));
                    auto target = (edge - previous->edgeBegin(i)) % (count - 1) + 1;
                    if (target >= e.source.value) ++target; // loader forbids self edges
                    assert(e.target.value == target);
                }
            }
            if (edges != 0) assert(snap->areas()[0].connections[0][0].target == model::NavAreaId{16});
        };
        assert(NavGraph::build(snap, {count, edges, exact}));
        failAfter = 0;
        const auto under = NavGraph::build(snap, {count, edges, exact - 1});
        // Preflight must reject without consuming the first allocation.
        assert(failAfter == 0);
        failAfter = std::numeric_limits<std::size_t>::max();
        assert(!under && !under.value);
        assert((under.error == diagnostics::NavError{K::CountLimitExceeded, 0,
                diagnostics::NavRecord::Graph, diagnostics::NavField::GraphBytes}));
        bool succeeded = false;
        for (std::size_t n = 0; n < 64; ++n) {
            failAfter = n;
            const auto attempt = NavGraph::build(snap, {count, edges, exact});
            const auto remaining = failAfter;
            failAfter = std::numeric_limits<std::size_t>::max();
            checkRetained();
            if (attempt) {
                assert(remaining == 0 && n >= 2);
                assert((*attempt.value)->logicalBytes() == exact);
                assert((*attempt.value)->areaCount() == count && (*attempt.value)->edgeCount() == edges);
                std::printf("graph shape %zu areas/%zu edges: exact bytes %zu; %zu OOM sites then success\n",
                            count, edges, exact, n);
                succeeded = true; break;
            }
            assert(!attempt.value);
            assert((attempt.error == diagnostics::NavError{K::AllocationFailure, 0,
                    diagnostics::NavRecord::Graph, diagnostics::NavField::GraphBytes}));
        }
        assert(succeeded);
    }
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
    assert(graph->find({10}) == std::size_t{0} && graph->find({20}) == std::size_t{1});
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
    chargeBoundaries();
    constructionShapeSweeps();
}
