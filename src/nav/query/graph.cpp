// SPDX-License-Identifier: MPL-2.0
#include "nav/query/graph.hpp"
#include "nav/query/detail/route_budget.hpp"
#include "nav/enrichment/detail/validation.hpp"
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
    return buildImpl(std::move(snapshot), limits, nullptr, nullptr, {});
}

diagnostics::ReadResult<std::shared_ptr<const NavGraph>>
NavGraph::compose(std::shared_ptr<const model::NavMeshSnapshot> snapshot,
                  const enrichment::NavMapFingerprint &expected,
                  const enrichment::NavTraversalLinkSet &links,
                  const NavGraphLimits &limits,
                  const enrichment::NavEnrichmentLimits &enrichmentLimits) noexcept {
    return buildImpl(std::move(snapshot), limits, &expected, &links, enrichmentLimits);
}

diagnostics::ReadResult<std::shared_ptr<const NavGraph>>
NavGraph::buildImpl(std::shared_ptr<const model::NavMeshSnapshot> snapshot,
                    const NavGraphLimits &limits,
                    const enrichment::NavMapFingerprint *expected,
                    const enrichment::NavTraversalLinkSet *links,
                    const enrichment::NavEnrichmentLimits &enrichmentLimits) noexcept {
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
    if (links) {
        auto e = enrichment::detail::workingBudget(areaCount, links->links.size(), enrichmentLimits);
        if (!e.isNone()) return Result::failure(e);
        e = detail::charge(edgeCount, links->links.size(), 1, limits.maxEdges);
        if (!e.isNone()) {
            e.field = F::ConnectionCount;
            return Result::failure(e);
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
        if (links) {
            error = enrichment::detail::validate(*snapshot, *expected, *links);
            if (!error.isNone()) return Result::failure(error);
        }
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
        }
        if (links) {
            for (const auto &link : links->links) {
                const auto target = graph->find(link.to); // validated above
                graph->edges_.push_back({{link.from, link.to, 0, link.traversal, link}, *target});
            }
        }
        std::sort(graph->edges_.begin(), graph->edges_.end(), [](const Edge &a, const Edge &b) {
            const auto &x = a.selected; const auto &y = b.selected;
            if (x.source != y.source) return x.source < y.source;
            if (x.external.has_value() != y.external.has_value()) return !x.external;
            if (x.external) return enrichment::detail::identity(*x.external) <
                                   enrichment::detail::identity(*y.external);
            return std::tie(x.direction, x.target, x.traversal) <
                   std::tie(y.direction, y.target, y.traversal);
        });
        std::size_t cursor = 0;
        for (auto &vertex : graph->vertices_) {
            vertex.begin = cursor;
            const auto id = graph->snapshot_->areas()[vertex.snapshotIndex].id;
            while (cursor < graph->edges_.size() && graph->edges_[cursor].selected.source == id) ++cursor;
            vertex.end = cursor;
        }
        return Result::success(std::move(graph));
    } catch (const std::bad_alloc &) {
        return Result::failure(detail::graphError(K::AllocationFailure));
    } catch (const std::length_error &) {
        return Result::failure(detail::graphError(K::AllocationFailure));
    }
}
} // namespace astrabot::nav::query
