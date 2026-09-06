// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/ladder.hpp"
#include "nav/local/ladder_physics.hpp"
#include <array>
namespace astrabot::nav::local {
struct LadderClearanceColumn { model::NavVector3 bottom{},top{}; };
enum class LadderExitReason { None, InvalidInput, StaleNavigation, Unsupported,
    NoLanding, BudgetExceeded };
struct LadderExitCandidate {
    MovementIntent intent{};
    core::BotCommand command{};
    model::NavVector3 landing{};
    std::array<LadderClearanceColumn,18> columns{};
    std::uint32_t columnCount{},simulatedFrames{};
};
struct LadderExitResult {
    LadderExitReason reason{LadderExitReason::InvalidInput};
    std::optional<LadderExitCandidate> value{};
    explicit operator bool() const noexcept { return reason==LadderExitReason::None && value.has_value(); }
};
// Candidate geometry, NOT world clearance or an approved exit intent. Standard
// standing CS, upper exit only; releaseZ is the measured model's upper hull
// contact boundary (model maxZ+36), not the destination support height. Simulates
// the current rounded frame duration for
// up to2 seconds/256 frames. A caller must sweep every column, prove actual
// landing floor and revalidate physics/ownership before dispatching anything.
LadderExitResult planUpperLadderExit(const LadderPlan&,const runtime::MovementSnapshot&,
    bool touching,double releaseZ,LadderAirPhysics,std::uint8_t commandMsec,
    const query::NavSpatialIndex&,core::MapGeneration indexMap) noexcept;
// Airborne-only outward jump (when touching) followed by monotonic outward air
// flight. Uses the same forecast bounds. Host must verify first-frame model
// release and every world column/landing; this function cannot authorize input.
LadderExitResult planJumpLadderExit(const LadderPlan&,const runtime::MovementSnapshot&,
    bool touching,LadderAirPhysics,std::uint8_t commandMsec,
    const query::NavSpatialIndex&,core::MapGeneration indexMap) noexcept;
}
