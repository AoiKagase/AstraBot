// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/combat.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot::adapter::cstrike {

// Adapter-owned values intentionally contain no edict_t, entvars_t, SDK
// weapon object, or raw message buffer. A future SDK reader fills this DTO.
struct WeaponObservation {
    core::MapGeneration map{};
    core::perception::RoundGeneration round{};
    core::TickId tick{};
    std::uint64_t observedMicros{0};
    std::uint16_t activeWeapon{0};
    core::combat::WeaponSnapshot::WeaponClass activeClass{
        core::combat::WeaponSnapshot::WeaponClass::Unknown};
    std::array<std::uint16_t, core::combat::kMaxOwnedWeapons> owned{};
    std::size_t ownedCount{0};
    std::int32_t clipAmmo{0};
    std::int32_t reserveAmmo{0};
    std::int32_t reloadClipThreshold{0};
    bool reloading{false};
    bool canReload{false};
    bool canSwitch{false};
    std::uint64_t primaryAttackReadyMicros{0};
};

enum class WeaponConversionError : std::uint8_t {
    None = 0,
    InvalidIdentity,
    InvalidInventory,
    DuplicateWeapon,
    InvalidActiveWeapon,
    ImpossibleAmmo,
    InvalidReloadThreshold,
};

struct WeaponConversionResult {
    core::combat::WeaponSnapshot snapshot{};
    WeaponConversionError error{WeaponConversionError::None};
    bool accepted{false};

    constexpr explicit operator bool() const noexcept {
        return accepted && error == WeaponConversionError::None;
    }
};

WeaponConversionResult toWeaponSnapshot(const WeaponObservation& observation) noexcept;

// This DTO is the adapter's boundary for one observer frame. It intentionally
// contains only value contracts; the adapter may fill it from edict_t, CS
// private data, or decoded messages without exposing those sources to Core.
struct CombatObservation {
    core::MapGeneration map{};
    core::perception::RoundGeneration round{};
    core::TickId tick{};
    std::uint64_t timeMicros{0};
    core::PlayerId player{};
    core::BotAgentId agent{};
    bool alive{false};
    core::perception::Team team{core::perception::Team::Unknown};
    core::perception::Point eye{};
    core::ViewAngles view{};
    core::world::WorldSnapshot world{};
    core::combat::DifficultySettings difficulty{};
    WeaponObservation weapon{};
};

enum class CombatConversionError : std::uint8_t {
    None = 0,
    InvalidWeaponObservation,
    InvalidCombatInput,
};

struct CombatInputConversionResult {
    core::combat::CombatInput input{};
    CombatConversionError error{CombatConversionError::None};
    bool accepted{false};

    constexpr explicit operator bool() const noexcept {
        return accepted && error == CombatConversionError::None;
    }
};

CombatInputConversionResult toCombatInput(
    const CombatObservation& observation) noexcept;

} // namespace astrabot::adapter::cstrike
