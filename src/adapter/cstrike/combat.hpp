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
    std::array<std::uint16_t, core::combat::kMaxOwnedWeapons> owned{};
    std::size_t ownedCount{0};
    std::int32_t clipAmmo{0};
    std::int32_t reserveAmmo{0};
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

} // namespace astrabot::adapter::cstrike
