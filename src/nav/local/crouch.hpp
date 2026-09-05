// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"
#include "nav/runtime/world_query.hpp"
namespace astrabot::nav::local {
enum class CrouchState { Standing, Lowering, Crouched, Raising, Failed, Aborted };
enum class CrouchReason { None, InvalidInput, InvalidActor, StaleTick, MissingObservation,
    QueryFailed, StaleQuery, BudgetExceeded, Blocked, TimedOut, Cancelled };
struct CrouchLimits {
    runtime::HullDimensions standing{}, crouched{};
    std::uint64_t transitionTimeoutUs{};
};
struct CrouchDecision {
    CrouchState state{CrouchState::Standing};
    CrouchReason reason{CrouchReason::None};
    bool accepted{}, terminalEvent{}, movementAllowed{};
    MovementIntent intent{}; // Posture only. Owner separately proves ground/path clearance.
    std::uint32_t queries{};
};
// Route-owned posture gate. It never advances a portal or infers arrival.
// Position/hull/duck observations must agree before movement can be enabled.
// Releasing duck requires a fresh stationary standing-hull clearance query at
// the same foot position. A blocked/failed abort stays ducked, with zero motion.
class Crouch final {
public:
    Crouch(Binding binding,CrouchLimits limits) noexcept : binding_(binding),limits_(limits) {}
    CrouchDecision update(const runtime::MovementSnapshot&,bool required,std::uint64_t nowUs,
        runtime::IWorldQueries&,std::uint32_t reservedQueries,std::uint32_t maxQueries) noexcept;
    CrouchDecision abort() noexcept;
    CrouchState state() const noexcept { return state_; }
private:
    Binding binding_{};
    CrouchLimits limits_{};
    CrouchState state_{CrouchState::Standing};
    core::TickId tick_{};
    std::uint64_t lastUs_{}, startedUs_{};
    bool observedDuck_{}, waiting_{};
    CrouchDecision result(CrouchReason reason=CrouchReason::None) const noexcept;
    CrouchDecision fail(CrouchReason) noexcept;
};
}
