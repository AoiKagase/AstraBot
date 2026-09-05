// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"
#include "nav/runtime/movement_snapshot.hpp"

namespace astrabot::nav::local {
enum class PumpReason { None, InvalidActor, DuplicateFrame, ClockOverflow,
    MissingIntent, StaleIntent, InvalidIntent, StaleDecision, SubmissionRejected };
struct FrameSchedule { bool accepted{}, decisionDue{}; PumpReason reason{PumpReason::None}; std::uint64_t missedDeadlines{}; };
struct PumpOutput {
    bool emit{}, firstFrame{};
    MovementIntent intent{};
    PumpReason reason{PumpReason::None};
    core::TickId tick{};
    std::uint64_t frameUs{}, intentAgeUs{};
};
// Per actor/map/route owner. The host must beginFrame AFTER dispatching the old
// queue, decide at most once when due, then take() and submit for a later tick.
// No wall clock, callbacks, A*, motor call or SDK dependency is retained here.
class IntentPump final {
public:
    static constexpr std::uint64_t decisionPeriodUs=40000, maxIntentAgeUs=120000;
    explicit IntentPump(Binding binding) noexcept : binding_(binding) {}
    FrameSchedule beginFrame(const runtime::MovementSnapshot&) noexcept;
    bool publish(Binding, core::TickId decisionTick, const MovementIntent&) noexcept;
    PumpOutput take() noexcept;
    void stop(PumpReason reason=PumpReason::MissingIntent) noexcept;
    void submissionRejected() noexcept { stop(PumpReason::SubmissionRejected); }
    std::uint64_t timeUs() const noexcept { return timeUs_; }
private:
    Binding binding_{};
    core::TickId tick_{};
    std::uint64_t timeUs_{}, nextDecisionUs_{}, frameUs_{}, intentTimeUs_{};
    MovementIntent intent_{};
    PumpReason reason_{PumpReason::MissingIntent};
    bool started_{}, retired_{}, eligible_{}, due_{}, published_{}, taken_{}, hasIntent_{}, first_{};
};
}
