// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/diagnostics/error.hpp"
#include "nav/enrichment/traversal_link.hpp"
#include "nav/model/mesh_snapshot.hpp"
#include "nav/query/geometry.hpp"
#include <memory>
#include <optional>

namespace astrabot::nav::query {
struct NavGraphLimits {
    std::size_t maxAreas{0}, maxEdges{0}, maxGraphBytes{0};
};
struct NavDirectedEdge {
    model::NavAreaId source{}, target{};
    std::uint8_t direction{0};
    model::NavTraversalKind traversal{model::NavTraversalKind::Walk};
    // Owned provenance; cardinal direction is meaningful only when absent.
    std::optional<enrichment::NavTraversalLink> external{};
};
class NavGraph final {
  public:
    static diagnostics::ReadResult<std::shared_ptr<const NavGraph>>
    build(std::shared_ptr<const model::NavMeshSnapshot> snapshot,
          const NavGraphLimits &limits) noexcept;
    // Caller must bind expectedFingerprint to snapshot. No hash computation.
    static diagnostics::ReadResult<std::shared_ptr<const NavGraph>>
    compose(std::shared_ptr<const model::NavMeshSnapshot> snapshot,
            const enrichment::NavMapFingerprint &expectedFingerprint,
            const enrichment::NavTraversalLinkSet &links,
            const NavGraphLimits &limits,
            const enrichment::NavEnrichmentLimits &enrichmentLimits) noexcept;
    std::size_t areaCount() const noexcept { return vertices_.size(); }
    std::size_t edgeCount() const noexcept { return edges_.size(); }
    std::size_t logicalBytes() const noexcept { return logicalBytes_; }
    std::optional<std::size_t> find(model::NavAreaId id) const noexcept;
    // Index accessors require valid indices obtained from this graph.
    const model::NavAreaRecord &area(std::size_t vertex) const noexcept {
        return snapshot_->areas()[vertices_[vertex].snapshotIndex];
    }
    NavQueryPoint center(std::size_t vertex) const noexcept { return vertices_[vertex].center; }
    std::size_t edgeBegin(std::size_t vertex) const noexcept { return vertices_[vertex].begin; }
    std::size_t edgeEnd(std::size_t vertex) const noexcept { return vertices_[vertex].end; }
    const NavDirectedEdge &edge(std::size_t edgeIndex) const noexcept { return edges_[edgeIndex].selected; }
    std::size_t targetIndex(std::size_t edgeIndex) const noexcept { return edges_[edgeIndex].target; }

  private:
    static diagnostics::ReadResult<std::shared_ptr<const NavGraph>>
    buildImpl(std::shared_ptr<const model::NavMeshSnapshot> snapshot,
              const NavGraphLimits &limits,
              const enrichment::NavMapFingerprint *expected,
              const enrichment::NavTraversalLinkSet *links,
              const enrichment::NavEnrichmentLimits &enrichmentLimits) noexcept;
    NavGraph() noexcept = default;
    NavGraph(const NavGraph &) = delete;
    NavGraph &operator=(const NavGraph &) = delete;
    struct Vertex {
        std::size_t snapshotIndex{0};
        NavQueryPoint center{};
        std::size_t begin{0}, end{0};
    };
    struct Edge {
        NavDirectedEdge selected{};
        std::size_t target{0};
    };
    std::shared_ptr<const model::NavMeshSnapshot> snapshot_{};
    std::vector<Vertex> vertices_{};
    std::vector<Edge> edges_{};
    std::size_t logicalBytes_{0};
};
} // namespace astrabot::nav::query
