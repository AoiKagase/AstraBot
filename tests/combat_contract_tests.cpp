// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/cstrike/combat.hpp"
#include "debug/host_trace.hpp"

#include <cassert>
#include <limits>

namespace {

namespace c = astrabot::core::combat;
namespace p = astrabot::core::perception;
namespace w = astrabot::core::world;
namespace a = astrabot::adapter::cstrike;
namespace d = astrabot::debug;

d::CombatTrace gCombatTrace{};
bool gCombatTraceSeen = false;

void captureCombatTrace(const d::CombatTrace& trace) noexcept {
    gCombatTrace = trace;
    gCombatTraceSeen = true;
}

constexpr astrabot::core::PlayerId player{1, {1}};
constexpr astrabot::core::BotAgentId agent{1};

a::WeaponObservation weaponObservation() {
    a::WeaponObservation observation{};
    observation.map = {3};
    observation.round = {7};
    observation.tick = {11};
    observation.observedMicros = 1100000;
    observation.activeWeapon = 5;
    observation.activeClass = c::WeaponSnapshot::WeaponClass::Rifle;
    observation.owned[0] = 5;
    observation.owned[1] = 7;
    observation.ownedCount = 2;
    observation.clipAmmo = 12;
    observation.reserveAmmo = 48;
    observation.reloadClipThreshold = 3;
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
    assert(converted.snapshot.activeClass == c::WeaponSnapshot::WeaponClass::Rifle);
    assert(converted.snapshot.reloadClipThreshold == 3);
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
    invalid.reloadClipThreshold = c::kMaxAmmo + 1;
    const auto invalidThreshold = a::toWeaponSnapshot(invalid);
    assert(!invalidThreshold);
    assert(invalidThreshold.error ==
           a::WeaponConversionError::InvalidReloadThreshold);

    invalid = weaponObservation();
    invalid.activeWeapon = 0;
    const auto noActive = a::toWeaponSnapshot(invalid);
    assert(!noActive);
    assert(noActive.error == a::WeaponConversionError::InvalidActiveWeapon);

    invalid = weaponObservation();
    invalid.activeClass = static_cast<c::WeaponSnapshot::WeaponClass>(255);
    const auto invalidClass = a::toWeaponSnapshot(invalid);
    assert(!invalidClass);
    assert(invalidClass.error == a::WeaponConversionError::InvalidWeaponClass);
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

void testCombatObservationConversion() {
    const auto expected = validInput();
    a::CombatObservation observation{};
    observation.map = expected.map;
    observation.round = expected.round;
    observation.tick = expected.tick;
    observation.timeMicros = expected.timeMicros;
    observation.player = expected.player;
    observation.agent = expected.agent;
    observation.alive = expected.alive;
    observation.team = expected.team;
    observation.eye = expected.eye;
    observation.view = expected.view;
    observation.world = expected.world;
    observation.difficulty = expected.difficulty;
    observation.weapon = weaponObservation();

    const auto converted = a::toCombatInput(observation);
    assert(converted);
    assert(converted.input.map == expected.map);
    assert(converted.input.round == expected.round);
    assert(converted.input.tick == expected.tick);
    assert(converted.input.timeMicros == expected.timeMicros);
    assert(converted.input.player == expected.player);
    assert(converted.input.agent == expected.agent);
    assert(converted.input.alive == expected.alive);
    assert(converted.input.team == expected.team);
    assert(converted.input.eye.x == expected.eye.x);
    assert(converted.input.eye.y == expected.eye.y);
    assert(converted.input.eye.z == expected.eye.z);
    assert(converted.input.view == expected.view);
    assert(converted.input.weapon.active == c::WeaponId{5});
    assert(converted.input.weapon.owns(c::WeaponId{7}));

    observation.weapon.owned[1] = observation.weapon.owned[0];
    const auto rejected = a::toCombatInput(observation);
    assert(!rejected);
    assert(rejected.error == a::CombatConversionError::InvalidWeaponObservation);
}

void testStructuredCombatTrace() {
    d::CombatTrace expected{};
    expected.map = {3};
    expected.round = {7};
    expected.player = player;
    expected.agent = agent;
    expected.target = {2, {4}};
    expected.source = p::ObservationSource::Vision;
    expected.targetAgeMicros = 500;
    expected.action = c::CombatAction::Fire;
    expected.reason = c::CombatReason::Accepted;
    expected.activeWeapon = {5};
    expected.clipAmmo = 12;
    expected.reserveAmmo = 48;
    expected.cooldownReady = true;
    expected.inputTick = {11};
    expected.sequence = 42;
    expected.transportError = d::MovementTraceError::WeaponSelectionRejected;
    expected.hostError = astrabot::host::HostError::Rejected;
    expected.commandBuilt = true;
    expected.commandAccepted = false;

    gCombatTrace = {};
    gCombatTraceSeen = false;
    d::emitCombat(expected, &captureCombatTrace);
    assert(gCombatTraceSeen);
    assert(gCombatTrace.map == expected.map);
    assert(gCombatTrace.round == expected.round);
    assert(gCombatTrace.player == expected.player);
    assert(gCombatTrace.agent == expected.agent);
    assert(gCombatTrace.target == expected.target);
    assert(gCombatTrace.source == expected.source);
    assert(gCombatTrace.targetAgeMicros == expected.targetAgeMicros);
    assert(gCombatTrace.action == expected.action);
    assert(gCombatTrace.reason == expected.reason);
    assert(gCombatTrace.activeWeapon == expected.activeWeapon);
    assert(gCombatTrace.clipAmmo == expected.clipAmmo);
    assert(gCombatTrace.reserveAmmo == expected.reserveAmmo);
    assert(gCombatTrace.cooldownReady == expected.cooldownReady);
    assert(gCombatTrace.inputTick == expected.inputTick);
    assert(gCombatTrace.sequence == expected.sequence);
    assert(gCombatTrace.transportError == expected.transportError);
    assert(gCombatTrace.hostError == expected.hostError);
    assert(gCombatTrace.commandBuilt == expected.commandBuilt);
    assert(gCombatTrace.commandAccepted == expected.commandAccepted);
}

void testFireModeExtensionAndDecisionValidation() {
    assert(c::FirePlan::tap().valid());
    assert(c::FirePlan::burst(3).valid());
    assert(c::FirePlan::fullAuto().valid());
    assert(!c::FirePlan::burst(1).valid());
    assert(!c::FirePlan::burst(c::kMaxBurstShots + 1).valid());
    assert(!(c::FirePlan{c::FirePattern::FullAuto, 1}).valid());

    const auto noOp = c::CombatDecision::noOp({11}, c::CombatReason::NoTarget);
    assert(noOp.validateForP5());
    assert(!noOp.hasAttackInput());

    auto trackWithPlan = noOp;
    trackWithPlan.firePlan = c::FirePlan::tap();
    assert(!trackWithPlan.validate());
    assert(trackWithPlan.validate().error == c::DecisionValidation::Error::UnexpectedFirePlan);

    c::CombatDecision direct{};
    direct.action = c::CombatAction::Fire;
    direct.target = {2, {1}};
    direct.fireMode = c::FireMode::DirectFire;
    direct.firePlan = c::FirePlan::tap();
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
    testCombatObservationConversion();
    testStructuredCombatTrace();
    return 0;
}
