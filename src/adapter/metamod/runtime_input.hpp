// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "adapter/metamod/runtime_orchestrator.hpp"
#include "adapter/metamod/plugin_entry.hpp"

namespace astrabot::adapter::metamod {
class LifecycleCoordinator;

// Synchronous, uncached observations. Unknown objective/economy observations
// never authorize objective actions or purchases.
struct RuntimeObjectiveObservation final {
    core::team::TeamObjective team{};
    core::tactical::ObjectiveState tactical{};
    core::action::ObjectiveSnapshot action{};
    bool available{false};
};
struct RuntimeEconomyObservation final {
    core::tactical::EconomySummary tactical{};
};

std::size_t buildRuntimeInputs(const LifecycleCoordinator&, const RuntimeFrame&,
    DLL_FUNCTIONS*, RuntimeActorInput*, std::size_t) noexcept;
bool runtimeActorReady(const LifecycleCoordinator&, const RuntimeFrame&,
    DLL_FUNCTIONS*, core::PlayerId, core::combat::WeaponId, bool attack) noexcept;
}
