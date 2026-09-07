// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/cstrike/combat.hpp"

#include <cassert>
#include <limits>

namespace {

namespace c = astrabot::core::combat;
namespace p = astrabot::core::perception;
namespace w = astrabot::core::world;
namespace a = astrabot::adapter::cstrike;

constexpr astrabot::core::PlayerId player{1, {1}};
constexpr astrabot::core::BotAgentId agent{1};

a::WeaponObservation weaponObservation() {
    a::WeaponObservation observation{};
    observation.map = {3};
    observation.round = {7};
    observation.tick = {11};
    observation.observedMicros = 1100000;
    observation.activeWeapon = 5;
    observation.owned[0] = 5;
    observation.owned[1] = 7;
    observation.ownedCount = 2;
    observation.clipAmmo = 12;
    observation.reserveAmmo = 48;
    observation.canReload = true;
    observation.canSwitch = true;
    observation.primaryAttackReadyMicros = 1100100;
    return observation;
}

c::CombatInput validInput() {
    static w::MemorySnapshot visual{};
    static w::SoundSnapshot sounds{};
    c::CombatInput input{};
    input.map = {3};
    input.round = {7};
    input.tick = {11};
    input.timeMicros = 1100000;
    input.player = player;
    input.agent = agent;
    input.alive = true;
    input.team = p::Team::CounterTerrorist;
    input.eye = {10.0, 20.0, 30.0};
    input.view = {0.0F, 90.0F, 0.0F};
    visual.stamp = {agent, player, {3}, {11}, 1100000, {7}};
    sounds.stamp = visual.stamp;
    input.world.stamp = visual.stamp;
    input.world.visual = &visual;
    input.world.sounds = &sounds;
    input.weapon = a::toWeaponSnapshot(weaponObservation()).snapshot;
    assert(input.weapon.validate());
    return input;
}

void testAdapterConversion() {
    const auto converted = a::toWeaponSnapshot(weaponObservation());
    assert(converted);
    assert(converted.snapshot.active == c::WeaponId{5});
    assert(converted.snapshot.owns(c::WeaponId{7}));
    assert(converted.snapshot.clipAmmo == 12);
    assert(converted.snapshot.reserveAmmo == 48);
    assert(converted.snapshot.canReload);
    assert(converted.snapshot.canSwitch);

    auto invalid = weaponObservation();
    invalid.owned[1] = invalid.owned[0];
    const auto duplicate = a::toWeaponSnapshot(invalid);
    assert(!duplicate);
    assert(duplicate.error == a::WeaponConversionError::DuplicateWeapon);

    invalid = weaponObservation();
    invalid.clipAmmo = -1;
    const auto impossible = a::toWeaponSnapshot(invalid);
    assert(!impossible);
    assert(impossible.error == a::WeaponConversionError::ImpossibleAmmo);

    invalid = weaponObservation();
    invalid.activeWeapon = 0;
    const auto noActive = a::toWeaponSnapshot(invalid);
    assert(!noActive);
    assert(noActive.error == a::WeaponConversionError::InvalidActiveWeapon);
}

void testInputValidationAndSafeRejection() {
    auto input = validInput();
    assert(input.validate());

    auto invalid = input;
    invalid.tick.value = 12;
    const auto stale = invalid.validate();
    assert(!stale);
    assert(stale.error == c::CombatInputError::StaleWorldSnapshot);
    assert(invalid.reject().reason == c::CombatReason::StaleInput);
    assert(!invalid.reject().hasAttackInput());

    invalid = input;
    invalid.map.value = 4;
    assert(invalid.validate().error == c::CombatInputError::StaleWorldSnapshot);

    invalid = input;
    invalid.round.value = 8;
    assert(invalid.validate().error == c::CombatInputError::StaleWorldSnapshot);

    invalid = input;
    invalid.player = {2, {1}};
    assert(invalid.validate().error == c::CombatInputError::StaleWorldSnapshot);

    invalid = input;
    invalid.eye.x = (std::numeric_limits<double>::quiet_NaN)();
    assert(invalid.validate().error == c::CombatInputError::NonFinitePose);
    assert(invalid.reject().reason == c::CombatReason::NonFinitePose);

    invalid = input;
    invalid.weapon.clipAmmo = c::kMaxAmmo + 1;
    assert(invalid.validate().error == c::CombatInputError::ImpossibleAmmo);
    assert(!invalid.reject().hasAttackInput());

    invalid = input;
    invalid.weapon.tick.value = 10;
    assert(invalid.validate().error == c::CombatInputError::StaleWeapon);
    assert(invalid.reject().reason == c::CombatReason::StaleWeapon);
}

void testFireModeExtensionAndDecisionValidation() {
    const auto noOp = c::CombatDecision::noOp({11}, c::CombatReason::NoTarget);
    assert(noOp.validateForP5());
    assert(!noOp.hasAttackInput());

    c::CombatDecision direct{};
    direct.action = c::CombatAction::Fire;
    direct.target = {2, {1}};
    direct.fireMode = c::FireMode::DirectFire;
    direct.buttons = static_cast<astrabot::core::ButtonMask>(astrabot::core::Button::Attack);
    direct.inputTick = {11};
    assert(direct.validateForP5());
    assert(direct.hasAttackInput());

    auto future = direct;
    future.fireMode = c::FireMode::Wallbang;
    assert(future.validate());
    assert(!future.validateForP5());
    assert(future.validateForP5().error == c::DecisionValidation::Error::UnsupportedFireMode);
}

} // namespace

int main() {
    testAdapterConversion();
    testInputValidationAndSafeRejection();
    testFireModeExtensionAndDecisionValidation();
    return 0;
}
