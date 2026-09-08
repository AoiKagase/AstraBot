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

enum class RuntimeInputBuildReason : std::uint8_t {
    None,
    InvalidFrame,
    MissingPrimary,
    StaleActor,
    MissingWorld,
    MissingNav,
    MissingCurrentArea,
    MissingPosition,
    WeaponUnavailable,
    MissingTeam,
    CombatConversionFailed,
};

enum class RuntimeActorStaleReason : std::uint8_t {
    None,
    MapInactive,
    MapGenerationMismatch,
    TickMismatch,
    RoundMismatch,
    PlayerGenerationMismatch,
    BindingInvalid,
    BindingAgentMismatch,
    BindingMapMismatch,
    MissingEntity,
    EntityFree,
    RemovalPending,
    NotJoined,
    Dead,
    InvalidHealth,
    SpectatorState,
    SpectatorFlag,
};

struct RuntimeInputBuildStatus final {
    RuntimeInputBuildReason reason{RuntimeInputBuildReason::None};
    RuntimeActorStaleReason staleReason{RuntimeActorStaleReason::None};
    core::PlayerId player{};
    std::uint16_t activeWeapon{0};
};

std::size_t buildRuntimeInputs(const LifecycleCoordinator&, const RuntimeFrame&,
    DLL_FUNCTIONS*, RuntimeActorInput*, std::size_t,
    RuntimeInputBuildStatus* = nullptr) noexcept;
bool runtimeActorReady(const LifecycleCoordinator&, const RuntimeFrame&,
    DLL_FUNCTIONS*, core::PlayerId, core::combat::WeaponId, bool attack) noexcept;
}
