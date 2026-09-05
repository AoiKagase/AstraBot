// SPDX-License-Identifier: MPL-2.0
#include "nav/query/spatial_index.hpp"
#include <algorithm>
#include <cmath>
#include <new>
#include <numeric>
#include <stdexcept>
namespace astrabot::nav::query {
namespace {
diagnostics::NavError error(diagnostics::NavErrorKind kind) noexcept {
    return {kind, 0, diagnostics::NavRecord::RawInput, diagnostics::NavField::RawBytes};
}
} // namespace
diagnostics::ReadResult<std::shared_ptr<const NavSpatialIndex>>
NavSpatialIndex::build(std::shared_ptr<const model::NavMeshSnapshot> snapshot,
                       const NavSpatialIndexLimits &limits) noexcept {
    using Result = diagnostics::ReadResult<std::shared_ptr<const NavSpatialIndex>>;
    using K = diagnostics::NavErrorKind;
    if (!snapshot)
        return Result::failure(error(K::InvalidInput));
    const auto n = snapshot->areas().size();
    if (n > limits.maxAreas)
        return Result::failure(error(K::CountLimitExceeded));
    const auto maximum = std::numeric_limits<std::size_t>::max();
    if (n > maximum / 2 + 1)
        return Result::failure(error(K::OffsetOverflow));
    const auto nodeCount = n == 0 ? 0 : n + (n - 1);
    if (nodeCount > limits.maxNodes)
        return Result::failure(error(K::CountLimitExceeded));
    std::size_t bytes = sizeof(NavSpatialIndex);
    if (n > (maximum - bytes) / sizeof(std::size_t))
        return Result::failure(error(K::OffsetOverflow));
    bytes += n * sizeof(std::size_t);
    if (nodeCount > (maximum - bytes) / sizeof(Node))
        return Result::failure(error(K::OffsetOverflow));
    bytes += nodeCount * sizeof(Node);
    if (bytes > limits.maxIndexBytes)
        return Result::failure(error(K::CountLimitExceeded));
    try {
        auto candidate = std::make_shared<NavSpatialIndex>();
        candidate->snapshot_ = std::move(snapshot);
        candidate->order_.resize(n);
        std::iota(candidate->order_.begin(), candidate->order_.end(), std::size_t{0});
        candidate->nodes_.reserve(nodeCount);
        if (n != 0)
            candidate->buildRange(0, n);
        return Result::success(std::move(candidate));
    } catch (const std::bad_alloc &) {
        return Result::failure(error(K::AllocationFailure));
    } catch (const std::length_error &) {
        return Result::failure(error(K::AllocationFailure));
    }
}
NavSpatialIndex::Node NavSpatialIndex::bounds(std::size_t area) const noexcept {
    const auto &e = snapshot_->areas()[area].extent;
    Node result;
    result.area = area;
    result.low = {e.northWest.x, e.northWest.y,
                  std::min({double(e.northWest.z), double(e.northEastZ), double(e.southWestZ),
                            double(e.southEast.z)})};
    result.high = {e.southEast.x, e.southEast.y,
                   std::max({double(e.northWest.z), double(e.northEastZ), double(e.southWestZ),
                             double(e.southEast.z)})};
    return result;
}
std::size_t NavSpatialIndex::buildRange(std::size_t first, std::size_t last) {
    Node node = bounds(order_[first]);
    for (auto i = first + 1; i < last; ++i) {
        const auto b = bounds(order_[i]);
        for (std::size_t axis = 0; axis < 3; ++axis) {
            node.low[axis] = std::min(node.low[axis], b.low[axis]);
            node.high[axis] = std::max(node.high[axis], b.high[axis]);
        }
    }
    const auto location = nodes_.size();
    nodes_.push_back(node);
    if (last - first == 1)
        return location;
    std::size_t axis = 0;
    for (std::size_t i = 1; i < 3; ++i)
        if (node.high[i] - node.low[i] > node.high[axis] - node.low[axis])
            axis = i;
    std::sort(order_.begin() + static_cast<std::ptrdiff_t>(first),
              order_.begin() + static_cast<std::ptrdiff_t>(last),
              [this, axis](std::size_t a, std::size_t b) {
                  const auto left = bounds(a), right = bounds(b);
                  const double x = left.low[axis] + left.high[axis],
                               y = right.low[axis] + right.high[axis];
                  return x < y || (x == y && snapshot_->areas()[a].id < snapshot_->areas()[b].id);
              });
    const auto middle = first + (last - first) / 2;
    const auto left = buildRange(first, middle), right = buildRange(middle, last);
    nodes_[location].left = left;
    nodes_[location].right = right;
    nodes_[location].area = absent;
    return location;
}
void NavSpatialIndex::visit(std::size_t index, model::NavVector3 point, NavQueryLimits limits,
                            bool containment, std::optional<NavAreaMatch> &best) const noexcept {
    const auto &node = nodes_[index];
    const double x = double(point.x), y = double(point.y), z = double(point.z);
    if (containment && (x < node.low[0] || x > node.high[0] || y < node.low[1] || y > node.high[1]))
        return;
    const double dx = std::clamp(x, node.low[0], node.high[0]) - x;
    const double dy = std::clamp(y, node.low[1], node.high[1]) - y;
    const double dz = std::clamp(z, node.low[2], node.high[2]) - z;
    if (std::abs(dz) > limits.maxVerticalDistance)
        return;
    if (!containment && std::hypot(dx, dy, dz) > limits.maxRadius)
        return;
    if (best && dx * dx + dy * dy + dz * dz > best->distanceSquared)
        return;
    if (node.area != absent) {
        const auto &area = snapshot_->areas()[node.area];
        if (containment && !containsXY(area.extent, point))
            return;
        const auto projected = projectToArea(area.extent, point);
        if (std::abs(projected.z - z) > limits.maxVerticalDistance)
            return;
        if (!containment &&
            std::hypot(projected.x - x, projected.y - y, projected.z - z) > limits.maxRadius)
            return;
        const double distance = squaredDistance(projected, point);
        if (!best || distance < best->distanceSquared ||
            (distance == best->distanceSquared && area.id < best->areaId))
            best = NavAreaMatch{area.id, projected, distance};
        return;
    }
    // Stored child order is deterministic; strict pruning preserves distance/ID ties.
    visit(node.left, point, limits, containment, best);
    visit(node.right, point, limits, containment, best);
}
NavQueryResult NavSpatialIndex::query(model::NavVector3 point, NavQueryLimits limits,
                                      bool containment) const noexcept {
    if (!point.isFinite() || !std::isfinite(limits.maxVerticalDistance) ||
        limits.maxVerticalDistance < 0 || !std::isfinite(limits.maxRadius) || limits.maxRadius < 0)
        return NavQueryResult::failure(error(diagnostics::NavErrorKind::InvalidInput));
    std::optional<NavAreaMatch> best;
    if (!nodes_.empty())
        visit(0, point, limits, containment, best);
    return NavQueryResult::success(best);
}
NavQueryResult NavSpatialIndex::containing(model::NavVector3 point,
                                           double maxVerticalDistance) const noexcept {
    return query(point, {0, maxVerticalDistance}, true);
}
NavQueryResult NavSpatialIndex::nearestGeometry(model::NavVector3 point,
                                                NavQueryLimits limits) const noexcept {
    return query(point, limits, false);
}
} // namespace astrabot::nav::query
