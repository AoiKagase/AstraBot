// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/diagnostics/error.hpp"
#include "nav/model/mesh_snapshot.hpp"
#include "nav/query/geometry.hpp"
#include <array>
#include <limits>
#include <memory>
#include <optional>
namespace astrabot::nav::query {
struct NavSpatialIndexLimits final {
    std::size_t maxAreas{0}, maxNodes{0}, maxIndexBytes{0};
};
struct NavQueryLimits final {
    double maxRadius{0}, maxVerticalDistance{0};
};
struct NavAreaMatch final {
    model::NavAreaId areaId{};
    NavQueryPoint projectedPoint{};
    double distanceSquared{0};
};
using NavQueryResult = diagnostics::ReadResult<std::optional<NavAreaMatch>>;
class NavSpatialIndex final {
  public:
    // An empty index is queryable; it does not represent a loadable empty NAV file.
    NavSpatialIndex() noexcept = default;
    static diagnostics::ReadResult<std::shared_ptr<const NavSpatialIndex>>
    build(std::shared_ptr<const model::NavMeshSnapshot> snapshot,
          const NavSpatialIndexLimits &limits) noexcept;
    NavQueryResult containing(model::NavVector3 point, double maxVerticalDistance) const noexcept;
    // Rank XY-clamped, bilinearly interpolated points, not exact surface minima.
    NavQueryResult nearestGeometry(model::NavVector3 point, NavQueryLimits limits) const noexcept;

  private:
    static constexpr std::size_t absent = (std::numeric_limits<std::size_t>::max)();
    struct Node final {
        std::array<double, 3> low{}, high{};
        std::size_t left{absent}, right{absent}, area{absent};
    };
    std::shared_ptr<const model::NavMeshSnapshot> snapshot_{};
    std::vector<Node> nodes_{};
    std::vector<std::size_t> order_{};
    Node bounds(std::size_t area) const noexcept;
    std::size_t buildRange(std::size_t first, std::size_t last);
    void visit(std::size_t node, model::NavVector3 point, NavQueryLimits limits, bool containment,
               std::optional<NavAreaMatch> &best) const noexcept;
    NavQueryResult query(model::NavVector3 point, NavQueryLimits limits,
                         bool containment) const noexcept;
};
} // namespace astrabot::nav::query
