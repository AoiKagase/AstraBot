// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"
#include "nav/local/ground_probe.hpp"
#include "nav/local/traversal_constraints.hpp"
namespace astrabot::nav::local {
enum class JumpState { Approach, Align, Accelerate, Takeoff, Airborne, Recover, Complete, Failed, Aborted };
enum class JumpReason { None, InvalidInput, InvalidActor, StaleTick, StaleInspection,
    MissingObservation, MissingSupport, Blocked, OutsideTakeoff, MissingDispatch,
    StaleDispatch, DispatchRejected, TakeoffTimeout, AirborneTimeout, ApproachTimeout,
    WrongLanding, LostSupport, Cancelled };
struct JumpPlan {
    model::NavAreaId source{}, target{};
    model::NavVector3 takeoff{}, landing{}; // Standing-hull origins, not area centers.
    std::uint8_t sourceAttributes{}, targetAttributes{};
};
struct JumpLimits {
    double approachSpeed{}, minimumSpeed{}, maximumSpeed{}, takeoffRadius{}, landingRadius{}, facingDegrees{},
        maximumDistance{}, maximumRise{}, supportTolerance{};
    std::uint32_t maxQueries{};
    std::uint64_t approachTimeoutUs{}, takeoffTimeoutUs{}, airborneTimeoutUs{}, cooldownUs{};
};
// Produced by a trusted, bounded world-query planner, never by NAV hints alone.
// One current-stamped batch owns support and optional approach/flight evidence.
// This controller validates ownership/geometry; it does not manufacture proof.
struct JumpInspection {
    runtime::QueryStamp stamp{}; // Batch ordinal zero.
    std::size_t step{};
    std::uint32_t queries{};
    model::NavVector3 origin{};
    runtime::HullDimensions hull{};
    std::optional<model::NavVector3> velocity{}; // Required for launch clearance.
    std::optional<GroundedTarget> support{}, approach{};
    std::optional<bool> approachClear{}, takeoffClear{}, flightClear{}, landingClear{};
    model::NavVector3 takeoff{}, landing{};
};
struct JumpDispatch {
    Binding binding{};
    core::TickId commandTick{}, dispatchTick{};
    bool dispatched{};
};
struct JumpFeedback {
    Binding binding{};
    runtime::MovementSnapshot movement{};
    std::uint64_t nowUs{};
    std::optional<JumpInspection> inspection{};
    std::optional<JumpDispatch> dispatch{};
};
struct JumpDecision {
    JumpState state{JumpState::Approach};
    JumpReason reason{JumpReason::None};
    bool accepted{}, terminalEvent{};
    MovementIntent intent{};
    core::TickId pressTick{};
};
class SimpleJump final {
public:
    SimpleJump(Binding binding,JumpPlan plan,JumpLimits limits) noexcept : binding_(binding),plan_(plan),limits_(limits) {}
    JumpDecision update(const JumpFeedback&) noexcept;
    JumpDecision abort() noexcept;
    JumpState state() const noexcept { return state_; }
private:
    Binding binding_{};
    JumpPlan plan_{};
    JumpLimits limits_{};
    JumpState state_{JumpState::Approach};
    core::TickId tick_{},pressTick_{};
    std::uint64_t lastUs_{},startedUs_{},phaseUs_{};
    bool started_{},dispatched_{};
    double flightSpeed_{};
    JumpDecision result(JumpReason=JumpReason::None) const noexcept;
    JumpDecision finish(JumpState,JumpReason) noexcept;
};
}
