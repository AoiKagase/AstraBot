// SPDX-License-Identifier: MPL-2.0
#include "nav/query/geometry.hpp"
#include <cassert>
#include <cmath>
#include <limits>
using namespace astrabot::nav;
int main() {
    model::NavExtent e{{0, 0, 0}, {10, 10, 10}, 10, 0};
    auto p = query::projectToArea(e, {5, 5, 0});
    assert(p.x == 5 && p.y == 5 && p.z == 5);
    // z=x: vertical XY projection is not the true nearest point (2.5,5,2.5).
    assert(query::squaredDistance(p, {5, 5, 0}) == 25);
    p = query::projectToArea(e, {-5, 20, 123});
    assert(p.x == 0 && p.y == 10 && p.z == 0);
    assert(query::containsXY(e, {0, 10, 999}));
    assert(!query::containsXY(e, {-0.01F, 10, 0}));
    e = {{0, 0, 0}, {10, 10, 0}, 10, 10};
    p = query::projectToArea(e, {5, 5, 0});
    assert(p.z == 5);
    p = query::projectToArea(e, {2.5F, 7.5F, 0});
    assert(p.z == 6.25);
    const float m = std::numeric_limits<float>::max();
    e = {{-m, -m, -m}, {m, m, m}, m, -m};
    p = query::projectToArea(e, {0, 0, 0});
    assert(p.x == 0 && p.y == 0 && p.z == 0);
    assert(std::isfinite(query::squaredDistance(p, {m, m, m})));
}
