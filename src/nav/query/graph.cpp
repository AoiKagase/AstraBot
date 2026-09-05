// SPDX-License-Identifier: MPL-2.0
#include "nav/query/graph.hpp"
#include "nav/query/detail/route_budget.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <tuple>

namespace astrabot::nav::query {
std::optional<std::size_t> NavGraph::find(model::NavAreaId id) const noexcept {
    const auto it = std::lower_bound(vertices_.begin(), vertices_.end(), id,
        [this](const Vertex &vertex, model::NavAreaId key) {
            return snapshot_->areas()[vertex.snapshotIndex].id < key;
        });
    if (it == vertices_.end() || snapshot_->areas()[it->snapshotIndex].id != id)
        return std::nullopt;
    return static_cast<std::size_t>(it - vertices_.begin());
}

diagnostics::ReadResult<std::shared_ptr<const NavGraph>>
NavGraph::build(std::shared_ptr<const model::NavMeshSnapshot> snapshot,
                const NavGraphLimits &limits) noexcept {
    using Result = diagnostics::ReadResult<std::shared_ptr<const NavGraph>>;
    using K = diagnostics::NavErrorKind;
    using F = diagnostics::NavField;
    if (!snapshot)
        return Result::failure(detail::graphError(K::InvalidInput));
    const auto areaCount = snapshot->areas().size();
    if (areaCount > limits.maxAreas)
        return Result::failure(detail::graphError(K::CountLimitExceeded, F::AreaCount));
    std::size_t edgeCount = 0;
    for (const auto &area : snapshot->areas()) {
        for (const auto &direction : area.connections) {
            auto error = detail::charge(edgeCount, direction.size(), 1, limits.maxEdges);
            if (!error.isNone()) {
                error.field = F::ConnectionCount;
                return Result::failure(error);
            }
            for (const auto &connection : direction) {
                error = detail::validateEdge(connection.traversal);
                if (!error.isNone())
                    return Result::failure(error);
            }
        }
    }
    // Preflight the complete logical total before any allocation. Arithmetic
    // overflow takes precedence over the final byte cap.
    std::size_t bytes = sizeof(NavGraph);
    const auto maximum = std::numeric_limits<std::size_t>::max();
    auto error = detail::charge(bytes, areaCount, sizeof(Vertex), maximum);
    if (error.isNone())
        error = detail::charge(bytes, edgeCount, sizeof(Edge), maximum);
    if (!error.isNone())
        return Result::failure(error);
    if (bytes > limits.maxGraphBytes)
        return Result::failure(detail::graphError(K::CountLimitExceeded));
    try {
        std::shared_ptr<NavGraph> graph(new NavGraph);
        graph->snapshot_ = std::move(snapshot);
        graph->logicalBytes_ = bytes;
        graph->vertices_.resize(areaCount);
        for (std::size_t i = 0; i < areaCount; ++i) {
            auto &vertex = graph->vertices_[i];
            vertex.snapshotIndex = i;
            const auto &e = graph->snapshot_->areas()[i].extent;
            vertex.center = {(double(e.northWest.x) + double(e.southEast.x)) * 0.5,
                             (double(e.northWest.y) + double(e.southEast.y)) * 0.5,
                             double(e.northWest.z) * 0.25 + double(e.northEastZ) * 0.25 +
                             double(e.southWestZ) * 0.25 + double(e.southEast.z) * 0.25};
        }
        std::sort(graph->vertices_.begin(), graph->vertices_.end(),
                  [&graph](const Vertex &a, const Vertex &b) {
                      return graph->snapshot_->areas()[a.snapshotIndex].id <
                             graph->snapshot_->areas()[b.snapshotIndex].id;
                  });
        graph->edges_.reserve(edgeCount);
        for (auto &vertex : graph->vertices_) {
            const auto &area = graph->snapshot_->areas()[vertex.snapshotIndex];
            vertex.begin = graph->edges_.size();
            for (std::uint8_t direction = 0; direction < 4; ++direction) {
                for (const auto &connection : area.connections[direction]) {
                    const auto target = graph->find(connection.target);
                    if (!target)
                        return Result::failure(detail::graphError(K::DanglingReference,
                                                                  F::ConnectionAreaId));
                    graph->edges_.push_back({{area.id, connection.target, direction,
                                             connection.traversal}, *target});
                }
            }
            vertex.end = graph->edges_.size();
            std::sort(graph->edges_.begin() + static_cast<std::ptrdiff_t>(vertex.begin),
                      graph->edges_.end(), [](const Edge &a, const Edge &b) {
                          return std::tie(a.selected.direction, a.selected.target, a.selected.traversal) <
                                 std::tie(b.selected.direction, b.selected.target, b.selected.traversal);
                      });
        }
        return Result::success(std::move(graph));
    } catch (const std::bad_alloc &) {
        return Result::failure(detail::graphError(K::AllocationFailure));
    } catch (const std::length_error &) {
        return Result::failure(detail::graphError(K::AllocationFailure));
    }
}
} // namespace astrabot::nav::query
