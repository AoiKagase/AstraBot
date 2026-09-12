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
    MissingUpdateClientData,
    MissingWeaponData,
    InvalidWeaponObservation,
    MissingTeam,
    TeamGenerationMismatch,
    UnknownTeam,
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
    core::MapGeneration map{};
    core::perception::RoundGeneration round{};
    core::TickId tick{};
    std::uint64_t nowMicros{0};
    core::PlayerId player{};
    core::BotAgentId agent{};
    std::uint16_t activeWeapon{0};
    core::combat::WeaponSnapshot::WeaponClass activeClass{
        core::combat::WeaponSnapshot::WeaponClass::Unknown};
    bool currentAreaHeld{false};
    bool updateClientDataAvailable{false};
    bool weaponDataAvailable{false};
    bool updateClientDataCalled{false};
    bool weaponDataCalled{false};
    // A failed actor is deliberately omitted from the orchestrator input
    // array. These fields let the transport distinguish that omission from a
    // valid actor which simply produced no command this frame.
    bool inputIncluded{false};
    bool idleDispatchSuppressed{false};
};

std::size_t buildRuntimeInputs(const LifecycleCoordinator&, const RuntimeFrame&,
    DLL_FUNCTIONS*, RuntimeActorInput*, std::size_t,
    RuntimeInputBuildStatus* = nullptr,
    std::size_t statusCapacity = 1) noexcept;
bool runtimeActorReady(const LifecycleCoordinator&, const RuntimeFrame&,
    DLL_FUNCTIONS*, core::PlayerId, core::combat::WeaponId, bool attack) noexcept;
}
