// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/query/route_types.hpp"

namespace astrabot::nav::corridor {
enum class Error { None, InvalidRoute, InvalidHull, InvalidPortal, LimitExceeded,
                   AllocationFailure, InvalidCursor, InvalidPosition };
struct HullClearance { double halfX{}, halfY{}; };
// No implicit allowance. Logical bytes exclude allocator overhead/control blocks.
struct Limits { std::size_t maxTransitions{}, maxBytes{}, maxEdgeChecks{}; };
struct Transition {
    query::NavDirectedEdge edge{};
    // Walk: endpoints ordered by increasing tangent coordinate, with independent
    // source/target floor Z. External: degenerate entry/exit segments.
    query::NavQueryPoint sourceLow{}, sourceHigh{}, targetLow{}, targetHigh{};
    model::NavExtent sourceExtent{}, targetExtent{};
    std::uint8_t sourceAttributes{}, targetAttributes{};
    // NAV geometry is a constraint, never proof of world clearance/support.
    bool requiresWorldProbe{true};
};
class Corridor;
struct BuildResult {
    std::shared_ptr<const Corridor> value{};
    Error error{Error::None};
    std::size_t transition{};
    explicit operator bool() const noexcept { return value && error == Error::None; }
};
struct TargetResult {
    std::optional<query::NavQueryPoint> value{};
    Error error{Error::None};
    explicit operator bool() const noexcept { return value.has_value() && error == Error::None; }
};
class Corridor final {
public:
    static BuildResult build(const query::NavGraph&, const query::NavRouteResult&,
                             HullClearance, Limits) noexcept;
    const std::vector<Transition>& transitions() const noexcept { return transitions_; }
    model::NavAreaId start() const noexcept { return start_; }
    model::NavAreaId goal() const noexcept { return goal_; }
    std::uint8_t startAttributes() const noexcept { return startAttributes_; }
    std::size_t logicalBytes() const noexcept { return logicalBytes_; }
    // Returns a source-side target on the active portal, never a shortcut beyond
    // it. Reverse projection of at most lookAhead gates biases the tangent.
    // External traversal is a barrier; its target is its exact entry point.
    TargetResult target(std::size_t cursor, query::NavQueryPoint position,
                        std::size_t lookAhead) const noexcept;
private:
    Corridor() = default;
    std::vector<Transition> transitions_{};
    model::NavAreaId start_{}, goal_{};
    std::uint8_t startAttributes_{}; // Also preserves constraints on a same-area route.
    std::size_t logicalBytes_{};
};
// Single owner. Advancement requires caller-validated support in the target
// area and the exact active step. No nearest-area/jitter-driven advancement.
// Exhaustion means no remaining transitions, not actor arrival at the goal.
class Cursor final {
public:
    explicit Cursor(std::shared_ptr<const Corridor> corridor) noexcept : corridor_(std::move(corridor)) {}
    bool advance(std::size_t expected, model::NavAreaId supportedArea, bool supportVerified) noexcept;
    std::size_t index() const noexcept { return index_; }
    bool exhausted() const noexcept { return corridor_ && index_ == corridor_->transitions().size(); }
    TargetResult target(query::NavQueryPoint p, std::size_t lookAhead) const noexcept;
private:
    std::shared_ptr<const Corridor> corridor_{};
    std::size_t index_{};
};
} // namespace astrabot::nav::corridor
