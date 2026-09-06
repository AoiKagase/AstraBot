// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/cstrike/nav/ladder_discovery.hpp"
namespace astrabot::adapter::cstrike {
struct LadderFrameWorld {
    LadderWorld ladder{};
    const void* context{};
    // Host verifies registry actor/agent, joined state, route, step and tick.
    bool (*current)(const void*,nav::local::Binding,core::TickId) noexcept = nullptr;
};
enum class LadderFrameReason { None, InvalidInput, Unavailable, StaleWorld,
    StaleActor, BudgetExceeded, InvalidTrace, WrongFace, NoSupport, Blocked };
struct LadderFrameObservation {
    nav::local::LadderInspection inspection{};
    nav::runtime::LadderContact contact{};
    bool climbing{}; // Observed MOVETYPE_FLY, not inferred from overlap.
};
struct LadderFrameResult {
    LadderFrameReason reason{LadderFrameReason::InvalidInput};
    std::optional<LadderFrameObservation> value{};
    std::uint32_t queries{};
    explicit operator bool() const noexcept { return reason==LadderFrameReason::None && value.has_value(); }
};
// Synchronous current-frame observation only. Four traces maximum, no allocation.
// Caller supplies a just-bound plan and controller target. Does not produce an
// exit intent, predict a dismount, dispatch movement or advance the route.
LadderFrameResult inspectLadderFrame(LadderFrameWorld,edict_t*,nav::local::Binding,
    const nav::runtime::MovementSnapshot&,const BoundLadderPlan&,nav::model::NavVector3 target,
    const nav::query::NavSpatialIndex&,core::MapGeneration indexMap,int maxEntities,
    std::uint32_t maxQueries=4) noexcept;
}
