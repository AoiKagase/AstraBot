// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/cstrike/nav/ladder_discovery.hpp"
#include "nav/local/ladder_physics.hpp"
#include "nav/local/ladder_exit.hpp"
namespace astrabot::adapter::cstrike {
struct LadderFrameWorld {
    LadderWorld ladder{};
    const void* context{};
    // Host verifies registry actor/agent, joined state, route, step and tick.
    bool (*current)(const void*,nav::local::Binding,core::TickId) noexcept = nullptr;
};
enum class LadderFrameReason { None, InvalidInput, Unavailable, StaleWorld,
    StaleActor, BudgetExceeded, InvalidTrace, WrongFace, NoSupport, Blocked, NoExit };
struct LadderCommandPrediction {
    core::BotCommand command{};
    nav::model::NavVector3 endpoint{},velocity{};
    bool floorCollision{}; // Predicted, never an observed route-completion fact.
};
struct LadderFrameObservation {
    nav::local::LadderInspection inspection{};
    nav::runtime::LadderContact contact{};
    bool climbing{}; // Observed MOVETYPE_FLY, not inferred from overlap.
    nav::local::LadderAirPhysics physics{};
    std::optional<bool> floorPointSolid{};
    std::optional<LadderCommandPrediction> prediction{};
    std::optional<nav::local::LadderExitCandidate> upperExit{};
    std::optional<nav::local::LadderExitCandidate> jumpExit{};
};
struct LadderFrameResult {
    LadderFrameReason reason{LadderFrameReason::InvalidInput};
    std::optional<LadderFrameObservation> value{};
    std::uint32_t queries{};
    nav::local::LadderExitReason exitReason{nav::local::LadderExitReason::None};
    explicit operator bool() const noexcept { return reason==LadderFrameReason::None && value.has_value(); }
};
// Synchronous current-frame observation only. Four traces maximum, no allocation.
// Caller supplies a just-bound plan and controller target. Does not produce an
// exit intent unless explicitly requested, dispatch movement or advance the
// route. Optional command proof
// adds floor-point contents and actual-frame displacement/flat-floor slide
// sweeps; caller explicitly supplies up to7 queries instead of observation's4.
// Alternatively exitMsec requests an upper rise candidate, a lower grounded
// floor kick, or a lower airborne jump/air candidate. Checks every clearance
// column, first-command release and landing floor within21 total queries. Only a
// fully verified result supplies exitIntent. Explicit command and exit request
// are mutually exclusive. Future frames still require fresh dispatch guards.
LadderFrameResult inspectLadderFrame(LadderFrameWorld,edict_t*,nav::local::Binding,
    const nav::runtime::MovementSnapshot&,const BoundLadderPlan&,nav::model::NavVector3 target,
    const nav::query::NavSpatialIndex&,core::MapGeneration indexMap,int maxEntities,
    std::uint32_t maxQueries=4,std::optional<core::BotCommand> command={},
    std::optional<std::uint8_t> exitMsec={}) noexcept;
}
