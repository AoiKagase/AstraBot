// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/runtime/world_query.hpp"
#include "nav/query/spatial_index.hpp"

namespace astrabot::nav::local {
struct GroundProbeLimits {
    std::uint32_t maxQueries{}, maxSamples{};
    double maxDistance{}, sampleSpacing{}, maxStepUp{}, maxDrop{}, probeDepth{};
    double supportTolerance{}, navTolerance{}, minNormalZ{};
};
enum class ProbeReason { None, InvalidInput, StaleNavigation, BudgetExceeded, StaleQuery, QueryFailed,
    InvalidResult, NoSupport, NoArea, WrongStartArea, UnsafeDrop, Blocked };
struct GroundedTarget {
    model::NavVector3 origin{};
    model::NavAreaId area{};
    runtime::FloorObservation floor{};
};
struct ProbeResult {
    runtime::QueryStamp stamp{}; // Batch identity; ordinal 0, queries counts issued ordinals.
    ProbeReason reason{ProbeReason::None};
    std::optional<GroundedTarget> target{};
    std::uint32_t queries{}, samples{}, steps{};
    explicit operator bool() const noexcept { return reason==ProbeReason::None && target.has_value(); }
};
// Offline value seam; synchronous, no retained host pointer and no motor command.
// Only target XY is requested; origin Z is derived from observed floor + hull.
// Discrete floor samples are bounded by sampleSpacing. This does not prove
// continuous support between samples; swept hulls only prove collision clearance.
class GroundProbe final {
public:
    // One measured ground query, validated against containing NAV at the floor.
    // No expected-area assumption: the actor may just have crossed a portal.
    static ProbeResult locate(const runtime::MovementSnapshot&, std::uint64_t routeGeneration,
        const query::NavSpatialIndex&, core::MapGeneration indexMap,
        runtime::IWorldQueries&, GroundProbeLimits) noexcept;
    static ProbeResult inspect(const runtime::MovementSnapshot&, std::uint64_t routeGeneration,
        model::NavAreaId currentArea, float targetX, float targetY,
        const query::NavSpatialIndex&, core::MapGeneration indexMap,
        runtime::IWorldQueries&, GroundProbeLimits) noexcept;
};
}
