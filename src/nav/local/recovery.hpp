// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"

namespace astrabot::nav::local {
enum class RecoveryState { Monitoring, Wait, Sidestep, Reverse, Replan, Aborted };
enum class StuckCause { None, DoorBlocked, PlayerBlocked, GeometryBlocked, TraversalFailed, Unknown };
enum class StuckSymptom { None, NoProgress, Oscillation };
enum class ExpectedProgress { Pause, Walk, Crouch };
struct RecoveryDecision {
    RecoveryState state{RecoveryState::Monitoring};
    StuckCause cause{StuckCause::None};
    StuckSymptom symptom{StuckSymptom::None};
    core::IntentVector forward{};
    std::uint64_t commandedUs{}, deadlineUs{};
    double displacement{}, projected{}, travel{};
    unsigned attempts{};
    bool terminalEvent{}, measuredProgress{};
};
struct ProgressDispatch {
    Binding binding{};
    core::TickId commandTick{}, dispatchTick{};
    model::NavVector3 origin{};
    core::IntentVector direction{};
    std::uint64_t durationUs{};
    ExpectedProgress expected{ExpectedProgress::Pause};
    bool dispatched{};
};
// One explicit goal owns this value. Replacing Walk or its route cannot reset it.
// Only trusted, successfully dispatched ground motion contributes detector time.
// Recovery movement never counts as forward progress and cannot refill its budget.
class Recovery final {
public:
    static constexpr std::uint64_t walkWindowUs=500000, crouchWindowUs=1000000, stageUs=250000;
    static constexpr double progressDistance=4;
    bool bindRoute(Binding) noexcept;
    bool report(const ProgressDispatch&) noexcept;
    void pause(Binding) noexcept;
    RecoveryDecision observe(Binding,core::TickId,std::uint64_t nowUs,model::NavVector3) noexcept;
    RecoveryDecision abort(StuckCause) noexcept;
    void replanned() noexcept;
    RecoveryDecision decision() const noexcept { return decision_; }
private:
    Binding binding_{};
    core::TickId dispatchTick_{}, observationTick_{};
    std::uint64_t nowUs_{}, windowUs_{};
    model::NavVector3 anchor_{}, previous_{};
    double furthest_{};
    bool bound_{}, window_{}, credited_{}, reference_{};
    RecoveryDecision decision_{};
    void clearWindow() noexcept;
};
}
