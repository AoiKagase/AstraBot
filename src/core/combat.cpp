// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/combat.hpp"

#include <cmath>

namespace astrabot::core::combat {
namespace {

bool isFinitePoint(const perception::Point& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool isFiniteView(const ViewAngles& view) noexcept {
    return std::isfinite(view.pitch) && std::isfinite(view.yaw) && std::isfinite(view.roll);
}

bool inRange(const ViewAngles& view) noexcept {
    return view.pitch >= kMinPitch && view.pitch <= kMaxPitch &&
           view.yaw >= kMinYaw && view.yaw <= kMaxYaw &&
           view.roll >= kMinRoll && view.roll <= kMaxRoll;
}

bool validPlayer(PlayerId player) noexcept {
    return player.isValid() && player.slot <= 32;
}

bool sameStamp(const perception::Stamp& left, const perception::Stamp& right) noexcept {
    return left.agent == right.agent && left.observer == right.observer &&
           left.map == right.map && left.round == right.round &&
           left.tick == right.tick && left.timeMicros == right.timeMicros;
}

bool sameStamp(const world::WorldSnapshot& snapshot, const CombatInput& input) noexcept {
    const perception::Stamp expected{input.agent, input.player, input.map, input.tick,
                                    input.timeMicros, input.round};
    return sameStamp(snapshot.stamp, expected);
}

CombatReason rejectionReason(CombatInputError error) noexcept {
    switch (error) {
    case CombatInputError::None:
        return CombatReason::None;
    case CombatInputError::InvalidActor:
        return CombatReason::InvalidActor;
    case CombatInputError::InvalidMap:
        return CombatReason::InvalidMap;
    case CombatInputError::InvalidRound:
        return CombatReason::InvalidRound;
    case CombatInputError::InvalidTick:
        return CombatReason::InvalidTick;
    case CombatInputError::InvalidWorldSnapshot:
        return CombatReason::InvalidWorldSnapshot;
    case CombatInputError::StaleWorldSnapshot:
        return CombatReason::StaleInput;
    case CombatInputError::NonFinitePose:
        return CombatReason::NonFinitePose;
    case CombatInputError::ViewOutOfRange:
        return CombatReason::ViewOutOfRange;
    case CombatInputError::StaleWeapon:
        return CombatReason::StaleWeapon;
    case CombatInputError::InvalidWeapon:
        return CombatReason::InvalidWeapon;
    case CombatInputError::ImpossibleAmmo:
        return CombatReason::ImpossibleAmmo;
    case CombatInputError::InvalidDifficulty:
        return CombatReason::InvalidDifficulty;
    }
    return CombatReason::InvalidInput;
}

} // namespace

WeaponValidation WeaponSnapshot::validate() const noexcept {
    if (!map.isValid() || !round.isValid() || !tick.isValid()) {
        return {WeaponValidationError::InvalidIdentity};
    }
    if (!active.isValid()) {
        return {WeaponValidationError::InvalidActiveWeapon};
    }
    if (ownedCount == 0 || ownedCount > owned.size()) {
        return {WeaponValidationError::InvalidInventory};
    }
    if (clipAmmo < 0 || clipAmmo > kMaxAmmo || reserveAmmo < 0 || reserveAmmo > kMaxAmmo) {
        return {WeaponValidationError::ImpossibleAmmo};
    }

    bool activeOwned = false;
    for (std::size_t i = 0; i < ownedCount; ++i) {
        if (!owned[i].isValid()) {
            return {WeaponValidationError::InvalidInventory};
        }
        if (owned[i] == active) {
            activeOwned = true;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (owned[j] == owned[i]) {
                return {WeaponValidationError::DuplicateWeapon};
            }
        }
    }
    if (!activeOwned) {
        return {WeaponValidationError::InvalidActiveWeapon};
    }
    return {};
}

bool WeaponSnapshot::owns(WeaponId weapon) const noexcept {
    if (!weapon.isValid() || ownedCount > owned.size()) {
        return false;
    }
    for (std::size_t i = 0; i < ownedCount; ++i) {
        if (owned[i] == weapon) {
            return true;
        }
    }
    return false;
}

bool operator==(const WeaponSnapshot& left, const WeaponSnapshot& right) noexcept {
    if (left.map != right.map || left.round != right.round || left.tick != right.tick ||
        left.observedMicros != right.observedMicros || left.active != right.active ||
        left.ownedCount != right.ownedCount || left.clipAmmo != right.clipAmmo ||
        left.reserveAmmo != right.reserveAmmo || left.reloading != right.reloading ||
        left.canReload != right.canReload || left.canSwitch != right.canSwitch ||
        left.primaryAttackReadyMicros != right.primaryAttackReadyMicros) {
        return false;
    }
    for (std::size_t i = 0; i < left.ownedCount && i < left.owned.size(); ++i) {
        if (left.owned[i] != right.owned[i]) {
            return false;
        }
    }
    return true;
}

bool DifficultySettings::valid() const noexcept {
    return reactionDelayMicros <= kMaxReactionDelayMicros &&
           std::isfinite(observationErrorDegrees) &&
           std::isfinite(predictionErrorDegrees) &&
           std::isfinite(aimNoiseDegrees) &&
           observationErrorDegrees >= 0.0F &&
           observationErrorDegrees <= kMaxDifficultyErrorDegrees &&
           predictionErrorDegrees >= 0.0F &&
           predictionErrorDegrees <= kMaxDifficultyErrorDegrees &&
           aimNoiseDegrees >= 0.0F && aimNoiseDegrees <= kMaxDifficultyErrorDegrees &&
           decisionQuality <= kMaxDecisionQuality;
}

CombatDecision CombatDecision::noOp(TickId tick, CombatReason decisionReason) noexcept {
    CombatDecision decision{};
    decision.reason = decisionReason;
    decision.inputTick = tick;
    return decision;
}

DecisionValidation CombatDecision::validate() const noexcept {
    if (!inputTick.isValid()) {
        return {DecisionValidation::Error::InvalidTick};
    }
    if (!isFiniteView(view)) {
        return {DecisionValidation::Error::NonFiniteView};
    }
    if (!inRange(view)) {
        return {DecisionValidation::Error::ViewOutOfRange};
    }
    if ((buttons & ~kKnownButtonMask) != 0U) {
        return {DecisionValidation::Error::UnknownButtons};
    }
    if (!std::isfinite(confidence) || confidence < 0.0 || confidence > 1.0) {
        return {DecisionValidation::Error::InvalidKnowledge};
    }
    if (action == CombatAction::Fire) {
        if (!target.isValid()) {
            return {DecisionValidation::Error::InvalidTarget};
        }
        if (!fireMode.has_value()) {
            return {DecisionValidation::Error::MissingFireMode};
        }
        if ((buttons & static_cast<ButtonMask>(Button::Attack)) == 0U) {
            return {DecisionValidation::Error::MissingAttackButton};
        }
    } else if (fireMode.has_value()) {
        return {DecisionValidation::Error::UnexpectedFireMode};
    }
    if (action == CombatAction::SwitchWeapon && !selectedWeapon.isValid()) {
        return {DecisionValidation::Error::InvalidSelectedWeapon};
    }
    return {};
}

DecisionValidation CombatDecision::validateForP5() const noexcept {
    const auto structural = validate();
    if (!structural || (action == CombatAction::Fire && *fireMode != FireMode::DirectFire)) {
        if (!structural) {
            return structural;
        }
        return {DecisionValidation::Error::UnsupportedFireMode};
    }
    return {};
}

bool CombatDecision::hasAttackInput() const noexcept {
    return action == CombatAction::Fire &&
           (buttons & static_cast<ButtonMask>(Button::Attack)) != 0U;
}

CombatInputValidation CombatInput::validate() const noexcept {
    if (!validPlayer(player) || !agent.isValid()) {
        return {CombatInputError::InvalidActor};
    }
    if (!map.isValid()) {
        return {CombatInputError::InvalidMap};
    }
    if (!round.isValid()) {
        return {CombatInputError::InvalidRound};
    }
    if (!tick.isValid()) {
        return {CombatInputError::InvalidTick};
    }
    if (world.visual == nullptr || world.sounds == nullptr) {
        return {CombatInputError::InvalidWorldSnapshot};
    }
    if (!sameStamp(world, *this)) {
        return {CombatInputError::StaleWorldSnapshot};
    }
    if (!sameStamp(world.visual->stamp, world.stamp) ||
        !sameStamp(world.sounds->stamp, world.stamp)) {
        return {CombatInputError::StaleWorldSnapshot};
    }
    if (world.visual->count > world.visual->memories.size() ||
        world.sounds->count > world.sounds->sounds.size()) {
        return {CombatInputError::InvalidWorldSnapshot};
    }
    if (!isFinitePoint(eye) || !isFiniteView(view)) {
        return {CombatInputError::NonFinitePose};
    }
    if (!inRange(view)) {
        return {CombatInputError::ViewOutOfRange};
    }
    const auto weaponValidation = weapon.validate();
    if (!weaponValidation) {
        switch (weaponValidation.error) {
        case WeaponValidationError::InvalidIdentity:
            return {CombatInputError::StaleWeapon};
        case WeaponValidationError::ImpossibleAmmo:
            return {CombatInputError::ImpossibleAmmo};
        case WeaponValidationError::None:
            break;
        default:
            return {CombatInputError::InvalidWeapon};
        }
    }
    if (weapon.map != map || weapon.round != round || weapon.tick != tick ||
        weapon.observedMicros != timeMicros) {
        return {CombatInputError::StaleWeapon};
    }
    if (!difficulty.valid()) {
        return {CombatInputError::InvalidDifficulty};
    }
    return {};
}

CombatDecision CombatInput::reject() const noexcept {
    const auto validation = validate();
    return CombatDecision::noOp(tick, rejectionReason(validation.error));
}

} // namespace astrabot::core::combat
