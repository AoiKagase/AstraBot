// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"
#include "nav/local/ground_probe.hpp"
namespace astrabot::nav::local {
enum class LadderState { Approach, Align, Contact, ClimbUp, ClimbDown, Exit, Support,
    Reacquire, Complete, Failed, Aborted };
enum class LadderReason { None, InvalidInput, InvalidActor, StaleTick, MissingObservation,
    StaleInspection, WrongContact, MissingSupport, Blocked, Timeout, Fall,
    ReacquireExhausted, WrongLanding, Cancelled };
struct LadderPlan {
    enrichment::NavTraversalLink link{};
    model::NavVector3 start{},end{},mount{},dismount{},normal{};
};
struct LadderLimits {
    double approachSpeed{80},positionTolerance{1},heightTolerance{4},facingDegrees{5},
        shaftTolerance{8},maximumHeight{4096},maximumApproach{96},maximumFallSpeed{260};
    std::uint32_t maxQueries{21};
    std::uint64_t approachTimeoutUs{3000000},contactTimeoutUs{1000000},climbTimeoutUs{30000000},
        exitTimeoutUs{2000000},supportTimeoutUs{500000},reacquireTimeoutUs{500000};
};
struct LadderInspection {
    runtime::QueryStamp stamp{};
    std::size_t step{};
    std::uint64_t sourceId{},generation{},linkId{};
    std::uint32_t queries{};
    model::NavVector3 origin{},velocity{},target{};
    runtime::HullDimensions hull{};
    std::optional<bool> pathClear{};
    std::optional<GroundedTarget> support{};
    // Fresh clearance-approved mode-specific exit control from the host planner.
    // Analog walking must not be substituted while ladder mode is active.
    std::optional<MovementIntent> exitIntent{};
};
struct LadderFeedback {
    Binding binding{};
    runtime::MovementSnapshot movement{};
    std::uint64_t nowUs{};
    std::optional<LadderInspection> inspection{};
    // Independently observed engine ladder movement mode, not model overlap.
    std::optional<bool> climbing{};
};
struct LadderDecision {
    LadderState state{LadderState::Approach};
    LadderReason reason{LadderReason::None};
    bool accepted{},terminalEvent{};
    MovementIntent intent{};
    enrichment::NavTraversalLink link{};
    unsigned reacquires{};
};
class Ladder final {
public:
    Ladder(Binding binding,LadderPlan plan,LadderLimits limits={}) noexcept : binding_(binding),plan_(plan),limits_(limits) {}
    LadderDecision update(const LadderFeedback&) noexcept;
    LadderDecision abort() noexcept;
    LadderState state() const noexcept { return state_; }
    // Exact endpoint to inspect from the current actor origin for this state.
    model::NavVector3 target(model::NavVector3 origin) const noexcept;
private:
    Binding binding_{}; LadderPlan plan_{}; LadderLimits limits_{};
    LadderState state_{LadderState::Approach};
    core::TickId tick_{};
    std::uint64_t lastUs_{},startedUs_{},phaseUs_{},climbUs_{};
    bool started_{},climbStarted_{}; unsigned reacquires_{};
    LadderDecision result(LadderReason=LadderReason::None) const noexcept;
    LadderDecision finish(LadderState,LadderReason) noexcept;
};
}
