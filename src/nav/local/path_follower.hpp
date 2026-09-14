// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "nav/corridor/corridor.hpp"

namespace astrabot::nav::local {
enum class FollowStatus { Moving, Arrived, SpecialTransition, MissingSupport, OutsideCorridor, Invalid };
struct FollowResult {
    FollowStatus status{FollowStatus::Invalid};
    std::optional<query::NavQueryPoint> target{};
    std::size_t step{};
    bool arrived{}, specialTransition{}, changed{};
    // Advisory return toward NAV after bounded physical support outside its XY
    // boundary. This neither advances ownership nor proves a movement passage.
    bool boundaryRecovery{};
    // XY route axis and signed distances in world units from the active portal.
    // routeProgress is a monotonic step + [0,1] measured projection diagnostic.
    query::NavQueryPoint routeForward{};
    double routeProjection{}, targetProjection{}, routeProgress{};
    bool retainedTarget{};
};

// NAV guidance only. Every returned target requires a world hull/support check.
// Ordinary progress is measured and monotonic; special traversal is caller-owned.
class PathFollower final {
public:
    PathFollower(std::shared_ptr<const corridor::Corridor> corridor,
                 query::NavQueryPoint goal) noexcept;
    FollowResult update(query::NavQueryPoint position, model::NavAreaId supportedArea,
                        bool supportVerified, double lookAhead,
                        double goalTolerance = 12.0) noexcept;
    bool completeSpecial(std::size_t expectedStep, model::NavAreaId supportedArea,
                         bool supportVerified, std::uint8_t transitions = 1) noexcept;
    std::size_t step() const noexcept { return step_; }
    const std::shared_ptr<const corridor::Corridor>& corridor() const noexcept { return corridor_; }
    const corridor::Transition* activeTransition() const noexcept;
    // Last world-validated forward aim. Callers may retain it through a
    // transient observation gap, subject to a fresh envelope/physics check.
    const std::optional<query::NavQueryPoint>& confirmedTarget() const noexcept {
        return confirmedTarget_;
    }
private:
    std::shared_ptr<const corridor::Corridor> corridor_{};
    query::NavQueryPoint goal_{};
    std::size_t step_{};
    std::optional<query::NavQueryPoint> confirmedTarget_{};
    std::size_t confirmedStep_{};
    std::size_t confirmedThrough_{};
    double routeProgress_{};
};
} // namespace astrabot::nav::local
