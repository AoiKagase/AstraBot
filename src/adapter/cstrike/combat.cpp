// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/cstrike/combat.hpp"

namespace astrabot::adapter::cstrike {
namespace {

WeaponConversionError mapError(core::combat::WeaponValidationError error) noexcept {
    using Error = core::combat::WeaponValidationError;
    switch (error) {
    case Error::None:
        return WeaponConversionError::None;
    case Error::InvalidIdentity:
    case Error::InvalidTimestamp:
        return WeaponConversionError::InvalidIdentity;
    case Error::InvalidActiveWeapon:
        return WeaponConversionError::InvalidActiveWeapon;
    case Error::InvalidInventory:
        return WeaponConversionError::InvalidInventory;
    case Error::DuplicateWeapon:
        return WeaponConversionError::DuplicateWeapon;
    case Error::ImpossibleAmmo:
        return WeaponConversionError::ImpossibleAmmo;
    }
    return WeaponConversionError::InvalidInventory;
}

} // namespace

WeaponConversionResult toWeaponSnapshot(const WeaponObservation& observation) noexcept {
    WeaponConversionResult result{};
    auto& snapshot = result.snapshot;
    snapshot.map = observation.map;
    snapshot.round = observation.round;
    snapshot.tick = observation.tick;
    snapshot.observedMicros = observation.observedMicros;
    snapshot.active = {observation.activeWeapon};
    snapshot.ownedCount = observation.ownedCount;
    snapshot.clipAmmo = observation.clipAmmo;
    snapshot.reserveAmmo = observation.reserveAmmo;
    snapshot.reloading = observation.reloading;
    snapshot.canReload = observation.canReload;
    snapshot.canSwitch = observation.canSwitch;
    snapshot.primaryAttackReadyMicros = observation.primaryAttackReadyMicros;
    for (std::size_t i = 0; i < snapshot.owned.size(); ++i) {
        snapshot.owned[i] = {observation.owned[i]};
    }

    const auto validation = snapshot.validate();
    if (!validation) {
        result.error = mapError(validation.error);
        return result;
    }
    result.accepted = true;
    return result;
}

} // namespace astrabot::adapter::cstrike
