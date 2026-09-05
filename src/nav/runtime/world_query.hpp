// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/runtime/movement_snapshot.hpp"

namespace astrabot::nav::runtime {
struct QueryStamp {
    core::BotAgentId agent{};
    core::PlayerId actor{};
    core::MapGeneration map{};
    core::TickId tick{};
    std::uint64_t routeGeneration{};
    std::uint32_t ordinal{};
    friend bool operator==(const QueryStamp& a, const QueryStamp& b) noexcept {
        return a.agent==b.agent && a.actor==b.actor && a.map==b.map && a.tick==b.tick &&
            a.routeGeneration==b.routeGeneration && a.ordinal==b.ordinal;
    }
};
enum class QueryKind { GroundedArea, SweptHull, Floor, Clearance, Door, Blocker };
enum class QueryError { None, Unavailable, BudgetExceeded, InvalidResult };
struct QueryRequest {
    QueryStamp stamp{};
    QueryKind kind{QueryKind::GroundedArea};
    model::NavVector3 start{}, end{};
    std::optional<HullDimensions> hull{};
};
struct FloorObservation { float height{}; model::NavVector3 normal{}; bool supported{}; };
struct GroundedAreaObservation {
    std::optional<model::NavAreaId> area{};
    std::optional<FloorObservation> floor{};
};
struct HullObservation { float fraction{}; model::NavVector3 end{}, normal{}; bool startSolid{}; };
struct ClearanceObservation { bool clear{}; };
struct DoorObservation { std::uint64_t id{}; bool open{}, canUse{}; };
enum class BlockerKind { Unknown, Teammate, Enemy, Geometry, Other };
struct BlockerObservation { std::uint64_t id{}; BlockerKind kind{BlockerKind::Unknown}; };
struct WorldQueryResult {
    QueryStamp stamp{};
    QueryKind kind{QueryKind::GroundedArea};
    QueryError error{QueryError::Unavailable};
    std::optional<GroundedAreaObservation> ground{};
    std::optional<HullObservation> hull{};
    std::optional<FloorObservation> floor{};
    std::optional<ClearanceObservation> clearance{};
    std::optional<DoorObservation> door{};
    std::optional<BlockerObservation> blocker{};
};
// Synchronous, borrowed for one call. Session never retains this interface or SDK pointers.
class IWorldQueries {
public:
    virtual ~IWorldQueries() = default;
    virtual WorldQueryResult query(const QueryRequest& request) = 0;
};
} // namespace astrabot::nav::runtime
