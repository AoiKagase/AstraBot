// SPDX-License-Identifier: MPL-2.0
#include "nav/io/mesh_loader.hpp"
#include "nav/query/spatial_index.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <thread>

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
namespace {
struct Area {
    std::uint32_t id;
    model::NavExtent extent;
};
void append(std::vector<std::uint8_t> &b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
}
void scalar(std::vector<std::uint8_t> &b, float f) {
    std::uint32_t n;
    std::memcpy(&n, &f, 4);
    append(b, n);
}
auto snapshot(const std::vector<Area> &areas) {
    std::vector<std::uint8_t> bytes;
    append(bytes, 0xFEEDFACE);
    append(bytes, 1);
    append(bytes, static_cast<std::uint32_t>(areas.size()));
    for (const auto &a : areas) {
        const auto &e = a.extent;
        append(bytes, a.id);
        bytes.push_back(0);
        for (float f : {e.northWest.x, e.northWest.y, e.northWest.z, e.southEast.x, e.southEast.y,
                        e.southEast.z, e.northEastZ, e.southWestZ})
            scalar(bytes, f);
        for (unsigned i = 0; i < 4; ++i)
            append(bytes, 0);
        bytes.push_back(0);
        bytes.push_back(0);
        append(bytes, 0);
    }
    auto r = io::NavMeshLoader::load(
        {bytes.data(), bytes.size()},
        {1000000, {1000, 0, 0, 0}, {1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 1000000});
    assert(r);
    return *r.value;
}
query::NavSpatialIndexLimits caps() { return {1000, 1999, 1000000}; }
auto index(const std::vector<Area> &a) {
    auto r = query::NavSpatialIndex::build(snapshot(a), caps());
    assert(r);
    return *r.value;
}
std::optional<query::NavAreaMatch> oracle(const std::vector<Area> &areas, model::NavVector3 p,
                                          query::NavQueryLimits limits, bool containing) {
    std::optional<query::NavAreaMatch> best;
    for (const auto &a : areas) {
        const auto &e = a.extent;
        double x = std::max(double(e.northWest.x), std::min(double(p.x), double(e.southEast.x)));
        double y = std::max(double(e.northWest.y), std::min(double(p.y), double(e.southEast.y)));
        if (containing && (x != p.x || y != p.y))
            continue;
        const double u = (x - e.northWest.x) / (double(e.southEast.x) - e.northWest.x);
        const double v = (y - e.northWest.y) / (double(e.southEast.y) - e.northWest.y);
        const double z = (1 - u) * (1 - v) * e.northWest.z + u * (1 - v) * e.northEastZ +
                         (1 - u) * v * e.southWestZ + u * v * e.southEast.z;
        double dx = x - p.x, dy = y - p.y, dz = z - p.z;
        if (std::abs(dz) > limits.maxVerticalDistance ||
            (!containing && std::hypot(dx, dy, dz) > limits.maxRadius))
            continue;
        double d = dx * dx + dy * dy + dz * dz;
        if (!best || d < best->distanceSquared ||
            (d == best->distanceSquared && a.id < best->areaId.value))
            best = query::NavAreaMatch{model::NavAreaId{a.id}, {x, y, z}, d};
    }
    return best;
}
void same(const std::optional<query::NavAreaMatch> &got,
          const std::optional<query::NavAreaMatch> &want) {
    assert(bool(got) == bool(want));
    if (!got)
        return;
    assert(got->areaId == want->areaId);
    assert(std::abs(got->projectedPoint.x - want->projectedPoint.x) < 1e-9);
    assert(std::abs(got->projectedPoint.y - want->projectedPoint.y) < 1e-9);
    assert(std::abs(got->projectedPoint.z - want->projectedPoint.z) < 1e-9);
    assert(std::abs(got->distanceSquared - want->distanceSquared) < 1e-8);
}
} // namespace
int main() {
    using K = diagnostics::NavErrorKind;
    std::vector<Area> areas{{9, {{0, 0, 0}, {10, 10, 0}, 0, 0}},
                            {3, {{0, 0, 20}, {10, 10, 20}, 20, 20}}};
    auto idx = index(areas);
    auto q = idx->containing({5, 5, 10}, 10);
    assert(q && *q.value && (**q.value).areaId.value == 3);
    q = idx->containing({5, 5, 1}, 1);
    assert(q && *q.value && (**q.value).areaId.value == 9);
    q = idx->containing({5, 5, 10}, 9);
    assert(q && !*q.value);
    q = idx->containing({10, 10, 0}, 0);
    assert(q && *q.value);
    q = idx->nearestGeometry({13, 14, 0}, {5, 0});
    assert(q && *q.value && (**q.value).distanceSquared == 25);
    q = idx->nearestGeometry({13, 14, 0}, {4.99, 0});
    assert(q && !*q.value);
    q = idx->nearestGeometry({5, 5, 0}, {0, 0});
    assert(q && *q.value);
    query::NavSpatialIndex empty;
    auto no = empty.nearestGeometry({0, 0, 0}, {0, 0});
    assert(no && !*no.value);
    assert(empty.containing({0, 0, 0}, -1).error.kind == K::InvalidInput);
    assert(!query::NavSpatialIndex::build(nullptr, caps()));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    assert(!idx->containing({nan, 0, 0}, 1));
    assert(!idx->nearestGeometry({0, 0, 0}, {-1, 1}));
    assert(!idx->nearestGeometry({0, 0, 0}, {1, std::numeric_limits<double>::infinity()}));
    auto snap = snapshot(areas);
    auto limits = caps();
    limits.maxNodes = 2;
    assert(query::NavSpatialIndex::build(snap, limits).error.kind == K::CountLimitExceeded);
    limits = caps();
    limits.maxAreas = 1;
    assert(query::NavSpatialIndex::build(snap, limits).error.kind == K::CountLimitExceeded);
    limits = caps();
    limits.maxIndexBytes = 0;
    assert(query::NavSpatialIndex::build(snap, limits).error.kind == K::CountLimitExceeded);
    bool success = false;
    for (std::size_t n = 0; n < 128; ++n) {
        failAfter = n;
        auto r = query::NavSpatialIndex::build(snap, caps());
        failAfter = std::numeric_limits<std::size_t>::max();
        if (r) {
            success = true;
            break;
        }
        assert(!r.value && r.error.kind == K::AllocationFailure);
    }
    assert(success);
    failAfter = 0;
    q = idx->nearestGeometry({5, 5, 0}, {100, 100});
    failAfter = std::numeric_limits<std::size_t>::max();
    assert(q && *q.value);
    // Limits reject before any index allocation, even under forced OOM.
    limits = caps();
    limits.maxIndexBytes = 1;
    failAfter = 0;
    auto capped = query::NavSpatialIndex::build(snap, limits);
    failAfter = std::numeric_limits<std::size_t>::max();
    assert(!capped && capped.error.kind == K::CountLimitExceeded);
    assert(empty.containing({0, 0, 0}, 0) && !*empty.containing({0, 0, 0}, 0).value);
    const float maximum = std::numeric_limits<float>::max();
    auto huge = index({{1, {{-maximum, -maximum, 0}, {maximum, maximum, 0}, 0, 0}}});
    auto far =
        huge->nearestGeometry({maximum, maximum, maximum}, {std::numeric_limits<double>::max(),
                                                            std::numeric_limits<double>::max()});
    assert(far && *far.value && std::isfinite((**far.value).distanceSquared));
    // Independent varied, overlapping/sloped patches, deterministic query stream.
    areas.clear();
    for (std::uint32_t i = 0; i < 64; ++i) {
        const float x = float(i % 8) * 8, y = float(i / 8) * 8, z = float(i % 3) * 12;
        areas.push_back({i + 1, {{x, y, z}, {x + 10, y + 10, z + 4}, z + 2, z - 2}});
    }
    idx = index(areas);
    auto reversed = areas;
    std::reverse(reversed.begin(), reversed.end());
    auto other = index(reversed);
    std::uint32_t state = 12345;
    for (unsigned i = 0; i < 1000; ++i) {
        auto next = [&state] {
            state = state * 1664525U + 1013904223U;
            return float(state % 100) - 20;
        };
        const model::NavVector3 point{next(), next(), next()};
        const query::NavQueryLimits filter{30, 15};
        auto a = idx->nearestGeometry(point, filter), b = other->nearestGeometry(point, filter);
        assert(a && b);
        same(*a.value, oracle(areas, point, filter, false));
        same(*a.value, *b.value);
        a = idx->containing(point, 15);
        b = other->containing(point, 15);
        assert(a && b);
        same(*a.value, oracle(areas, point, filter, true));
        same(*a.value, *b.value);
    }
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < 4; ++i)
        workers.emplace_back([idx] {
            for (unsigned j = 0; j < 100; ++j) {
                auto r = idx->nearestGeometry({5, 5, 1}, {100, 100});
                assert(r && *r.value && (**r.value).areaId.value == 1);
            }
        });
    for (auto &t : workers)
        t.join();
}
