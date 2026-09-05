// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/ground_probe.hpp"
#include "nav/local/primitive.hpp"

namespace astrabot::nav::local {
enum class WalkState { Running, Arrived, Failed, Aborted };
enum class WalkReason { None, InvalidInput, StaleTick, InvalidActor, StaleNavigation,
    UnsupportedTraversal, InvalidGoal, OffCorridor, InvalidPortal, ProbeFailed, Cancelled };
struct WalkLimits {
    GroundProbeLimits probe{};
    double speed{}, arrivalTolerance{}, crossingMargin{};
    std::size_t lookAhead{};
};
struct WalkDecision {
    WalkState state{WalkState::Running};
    WalkReason reason{WalkReason::None};
    bool accepted{}, terminalEvent{};
    Binding binding{};
    core::TickId tick{};
    MovementIntent intent{};
    PrimitiveEvent primitiveEvent{PrimitiveEvent::None};
    std::optional<GroundedTarget> support{}, target{};
    ProbeReason probeReason{ProbeReason::None};
    std::uint32_t queries{}, samples{}, steps{};
};
// One owned route, synchronous decision seam. Caller schedules decisions and
// invalidates on route replacement; this class never submits a host command.
// A projected endpoint is not arrival: only later measured support advances.
class Walk final {
public:
    Walk(Binding, std::shared_ptr<const corridor::Corridor>, model::NavVector3 goal,
         WalkLimits) noexcept;
    WalkDecision update(const runtime::MovementSnapshot&, const query::NavSpatialIndex&,
                        core::MapGeneration indexMap, runtime::IWorldQueries&) noexcept;
    WalkDecision abort() noexcept;
    WalkState state() const noexcept { return state_; }
    std::size_t step() const noexcept { return cursor_.index(); }
private:
    Binding binding_{};
    std::shared_ptr<const corridor::Corridor> corridor_{};
    corridor::Cursor cursor_;
    model::NavVector3 goal_{};
    WalkLimits limits_{};
    core::TickId tick_{};
    WalkState state_{WalkState::Running};
    WalkReason reason_{WalkReason::None};
    Primitive primitive_{};
    WalkDecision finish(WalkDecision, WalkState, WalkReason) noexcept;
};
}
