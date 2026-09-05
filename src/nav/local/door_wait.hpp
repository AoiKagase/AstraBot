// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"
#include "nav/runtime/world_query.hpp"

namespace astrabot::nav::local {
enum class DoorWaitState { Ready, Waiting, Clear, Failed, Aborted };
enum class DoorWaitReason { None, InvalidInput, InvalidBinding, StaleTick,
    InvalidObservation, Unusable, Replaced, TimedOut, Cancelled };
struct DoorWaitFeedback {
    Binding binding{};
    runtime::QueryStamp requested{};
    runtime::WorldQueryResult observed{};
    // Host-owned monotonic elapsed simulation clock, including skipped decision
    // frames. Never a per-frame delta. Nav neither reads nor accumulates a clock.
    std::uint64_t nowUs{};
    // Host proves this view selects the observed usable door within use range.
    // A classname/spawnflag or an unverified geometric direction is insufficient.
    std::optional<core::IntentVector> useView{};
};
struct DoorWaitDecision {
    DoorWaitState state{DoorWaitState::Ready};
    DoorWaitReason reason{DoorWaitReason::None};
    bool accepted{}, terminalEvent{};
    MovementIntent intent{};
};
// One attempt per binding. A blocked usable door emits one Press intent; later
// updates are neutral. Dropped presses are not retried. Clear only authorizes a
// new ground/clearance inspection by Walk, never translation or route arrival.
// The owner must abort on actor death/disconnect or route/map invalidation.
class DoorWait final {
public:
    DoorWait(Binding binding, std::uint64_t timeoutUs) noexcept
        : binding_(binding), timeoutUs_(timeoutUs) {}
    DoorWaitDecision update(const DoorWaitFeedback&) noexcept;
    DoorWaitDecision abort() noexcept;
    DoorWaitState state() const noexcept { return state_; }
private:
    Binding binding_{};
    std::uint64_t timeoutUs_{}, startedUs_{}, lastUs_{}, doorId_{};
    core::TickId tick_{};
    DoorWaitState state_{DoorWaitState::Ready};
    DoorWaitReason reason_{DoorWaitReason::None};
    DoorWaitDecision finish(DoorWaitState, DoorWaitReason) noexcept;
};
}
