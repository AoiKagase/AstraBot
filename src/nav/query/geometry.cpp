// SPDX-License-Identifier: MPL-2.0
#include "nav/query/geometry.hpp"
#include <algorithm>
namespace astrabot::nav::query {
NavQueryPoint projectToArea(const model::NavExtent &e, model::NavVector3 p) noexcept {
    const double x = std::clamp(double(p.x), double(e.northWest.x), double(e.southEast.x));
    const double y = std::clamp(double(p.y), double(e.northWest.y), double(e.southEast.y));
    const double u = (x - double(e.northWest.x)) / (double(e.southEast.x) - double(e.northWest.x));
    const double v = (y - double(e.northWest.y)) / (double(e.southEast.y) - double(e.northWest.y));
    const double north = (1 - u) * double(e.northWest.z) + u * double(e.northEastZ);
    const double south = (1 - u) * double(e.southWestZ) + u * double(e.southEast.z);
    const double low = std::min(
        {double(e.northWest.z), double(e.northEastZ), double(e.southWestZ), double(e.southEast.z)});
    const double high = std::max(
        {double(e.northWest.z), double(e.northEastZ), double(e.southWestZ), double(e.southEast.z)});
    return {x, y, std::clamp((1 - v) * north + v * south, low, high)};
}
bool containsXY(const model::NavExtent &e, model::NavVector3 p) noexcept {
    return p.x >= e.northWest.x && p.x <= e.southEast.x && p.y >= e.northWest.y &&
           p.y <= e.southEast.y;
}
double squaredDistance(NavQueryPoint a, model::NavVector3 b) noexcept {
    const double x = a.x - double(b.x), y = a.y - double(b.y), z = a.z - double(b.z);
    return x * x + y * y + z * z;
}
} // namespace astrabot::nav::query
