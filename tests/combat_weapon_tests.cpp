// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/combat.hpp"

#include <cassert>

namespace {

namespace c = astrabot::core::combat;
namespace p = astrabot::core::perception;
namespace w = astrabot::core::world;

constexpr astrabot::core::PlayerId kBot{1, {1}};
constexpr astrabot::core::PlayerId kTarget{2, {1}};
constexpr astrabot::core::BotAgentId kAgent{1};

struct Fixture {
    c::CombatInput input{};
    w::MemorySnapshot visual{};
    w::SoundSnapshot sounds{};
    c::CombatDecision aim{};

    Fixture() {
        input.map = {3};
        input.round = {7};
        input.tick = {11};
        input.timeMicros = 1100000;
        input.player = kBot;
        input.agent = kAgent;
        input.alive = true;
        input.team = p::Team::CounterTerrorist;
        input.eye = {10.0, 20.0, 30.0};
        input.view = {0.0F, 0.0F, 0.0F};

        visual.count = 1;
        visual.memories[0].target = kTarget;
        visual.memories[0].lastKnownPosition = {100.0, 20.0, 30.0};
        visual.memories[0].lastSeenMicros = input.timeMicros;
        visual.memories[0].confidence = 1.0;
        input.world.roster[kTarget.slot - 1U] = {kTarget, p::Team::Terrorist};

        input.weapon.map = input.map;
        input.weapon.round = input.round;
        input.weapon.tick = input.tick;
        input.weapon.observedMicros = input.timeMicros;
        input.weapon.active = {5};
        input.weapon.owned[0] = input.weapon.active;
        input.weapon.ownedCount = 1;
        input.weapon.clipAmmo = 12;
        input.weapon.reserveAmmo = 48;

        aim.action = c::CombatAction::Track;
        aim.target = kTarget;
        aim.view = input.view;
        aim.source = p::ObservationSource::Vision;
        aim.confidence = 1.0;
        aim.reason = c::CombatReason::Accepted;
        aim.inputTick = input.tick;
        aim.validUntilMicros = input.timeMicros;
        refreshStamp();
    }

    void refreshStamp() {
        const p::Stamp stamp{kAgent, kBot, input.map, input.tick,
                             input.timeMicros, input.round};
        visual.stamp = stamp;
        sounds.stamp = stamp;
        input.world.stamp = stamp;
        input.world.visual = &visual;
        input.world.sounds = &sounds;
        visual.memories[0].identity = {
            input.map, input.round, p::ObservationSource::Vision, 1,
            input.timeMicros, input.timeMicros};
        visual.memories[0].lastSeenMicros = input.timeMicros;
    }

    void advance(std::uint64_t deltaMicros = 1000) {
        ++input.tick.value;
        input.timeMicros += deltaMicros;
        input.weapon.tick = input.tick;
        input.weapon.observedMicros = input.timeMicros;
        aim.inputTick = input.tick;
        aim.validUntilMicros = input.timeMicros;
        refreshStamp();
    }

    void advanceTo(std::uint64_t timeMicros) {
        ++input.tick.value;
        input.timeMicros = timeMicros;
        input.weapon.tick = input.tick;
        input.weapon.observedMicros = input.timeMicros;
        aim.inputTick = input.tick;
        aim.validUntilMicros = input.timeMicros;
        refreshStamp();
    }
};

void assertNoAttack(const c::CombatDecision& decision, c::CombatReason reason) {
    assert(decision.action != c::CombatAction::Fire);
    assert(!decision.hasAttackInput());
    assert(!decision.fireMode.has_value());
    assert(!decision.firePlan.has_value());
    assert(decision.reason == reason);
}

void assertReload(const c::FireAuthorization& result) {
    assert(result.decision.action == c::CombatAction::Reload);
    assert(result.decision.buttons ==
           static_cast<astrabot::core::ButtonMask>(astrabot::core::Button::Reload));
    assert(!result.decision.fireMode.has_value());
    assert(!result.decision.firePlan.has_value());
    assert(result.decision.validateForP5());
    assert(result.decision.reason == c::CombatReason::Accepted);
}

void assertSwitch(const c::FireAuthorization& result, c::WeaponId expected) {
    assert(result.decision.action == c::CombatAction::SwitchWeapon);
    assert(result.decision.selectedWeapon == expected);
    assert(result.decision.buttons == 0U);
    assert(result.decision.validateForP5());
    assert(result.decision.reason == c::CombatReason::Accepted);
}

void assertFirePattern(const c::FireAuthorization& result, c::FirePattern pattern,
                       std::uint8_t burstShots) {
    assert(result.decision.action == c::CombatAction::Fire);
    assert(result.decision.fireMode.has_value());
    assert(*result.decision.fireMode == c::FireMode::DirectFire);
    assert(result.decision.firePlan.has_value());
    assert(result.decision.firePlan->pattern == pattern);
    assert(result.decision.firePlan->burstShots == burstShots);
    assert(result.decision.hasAttackInput());
    assert(result.decision.validateForP5());
}

void testReloadPolicyAndCompletionBoundary() {
    {
        Fixture fixture;
        fixture.input.weapon.clipAmmo = 0;
        fixture.input.weapon.reserveAmmo = 24;
        fixture.input.weapon.canReload = true;
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertReload(result);
        assert(result.nextState.lastAction == c::CombatAction::Reload);

        fixture.advance();
        fixture.input.weapon.reloading = true;
        const auto pending =
            c::authorizeFire(fixture.input, fixture.aim, result.nextState);
        assertNoAttack(pending.decision, c::CombatReason::Reloading);
        assert(!pending.nextState.cadenceActive);

        fixture.advance();
        fixture.input.weapon.reloading = false;
        fixture.input.weapon.clipAmmo = 12;
        fixture.input.weapon.reserveAmmo = 12;
        const auto completed =
            c::authorizeFire(fixture.input, fixture.aim, pending.nextState);
        assert(completed.decision.action == c::CombatAction::Fire);
    }
    {
        Fixture fixture;
        fixture.visual.count = 0;
        fixture.input.weapon.clipAmmo = 3;
        fixture.input.weapon.reloadClipThreshold = 3;
        fixture.input.weapon.reserveAmmo = 24;
        fixture.input.weapon.canReload = true;
        fixture.aim = c::CombatDecision::noOp(fixture.input.tick,
                                               c::CombatReason::NoTarget);
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertReload(result);
    }
    {
        Fixture fixture;
        fixture.input.weapon.clipAmmo = 3;
        fixture.input.weapon.reloadClipThreshold = 3;
        fixture.input.weapon.reserveAmmo = 24;
        fixture.input.weapon.canReload = true;
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assert(result.decision.action == c::CombatAction::Fire);
        assert(result.decision.action != c::CombatAction::Reload);
    }
}

void testWeaponSwitchPolicy() {
    {
        Fixture fixture;
        fixture.input.weapon.clipAmmo = 0;
        fixture.input.weapon.reserveAmmo = 0;
        fixture.input.weapon.canSwitch = true;
        fixture.input.weapon.owned[1] = {9};
        fixture.input.weapon.owned[2] = {3};
        fixture.input.weapon.ownedCount = 3;
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertSwitch(result, {3});

        const auto duplicate =
            c::authorizeFire(fixture.input, fixture.aim, result.nextState);
        assertNoAttack(duplicate.decision, c::CombatReason::DuplicateAction);
    }
    {
        Fixture fixture;
        fixture.input.weapon.clipAmmo = 0;
        fixture.input.weapon.reserveAmmo = 0;
        fixture.input.weapon.canSwitch = true;
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertNoAttack(result.decision, c::CombatReason::NoUsableWeapon);
    }
    {
        Fixture fixture;
        fixture.input.weapon.clipAmmo = 0;
        fixture.input.weapon.reserveAmmo = 12;
        fixture.input.weapon.canSwitch = true;
        fixture.input.weapon.owned[1] = {3};
        fixture.input.weapon.ownedCount = 2;
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertSwitch(result, {3});
    }
}

void testCadencePolicyByWeaponAndRange() {
    {
        Fixture fixture;
        fixture.input.weapon.activeClass = c::WeaponSnapshot::WeaponClass::Rifle;
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertFirePattern(result, c::FirePattern::FullAuto, 0);
    }
    {
        Fixture fixture;
        fixture.visual.memories[0].lastKnownPosition = {610.0, 20.0, 30.0};
        fixture.input.weapon.activeClass = c::WeaponSnapshot::WeaponClass::Rifle;
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertFirePattern(result, c::FirePattern::Burst, 3);
    }
    {
        Fixture fixture;
        fixture.visual.memories[0].lastKnownPosition = {1100.0, 20.0, 30.0};
        fixture.input.weapon.activeClass = c::WeaponSnapshot::WeaponClass::Rifle;
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertFirePattern(result, c::FirePattern::Tap, 1);
    }
    {
        Fixture fixture;
        fixture.input.weapon.activeClass = c::WeaponSnapshot::WeaponClass::Pistol;
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertFirePattern(result, c::FirePattern::Tap, 1);
    }
    {
        Fixture fixture;
        fixture.input.weapon.activeClass = c::WeaponSnapshot::WeaponClass::SMG;
        fixture.visual.memories[0].lastKnownPosition = {610.0, 20.0, 30.0};
        const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
        assertFirePattern(result, c::FirePattern::Burst, 5);
    }
}

void testCadenceIsBoundedAndReevaluated() {
    Fixture fixture;
    fixture.input.weapon.activeClass = c::WeaponSnapshot::WeaponClass::Rifle;
    c::AttackLifecycleState state{};

    auto result = c::authorizeFire(fixture.input, fixture.aim, state);
    assertFirePattern(result, c::FirePattern::FullAuto, 0);
    state = result.nextState;
    assert(state.cadenceShotsFired == 1);

    for (std::uint8_t shot = 2; shot <= c::kMaxBurstShots; ++shot) {
        fixture.advance();
        result = c::authorizeFire(fixture.input, fixture.aim, state);
        assertFirePattern(result, c::FirePattern::FullAuto, 0);
        state = result.nextState;
        assert(state.cadenceShotsFired == shot);
    }
    assert(state.cadencePauseUntilMicros > fixture.input.timeMicros);

    fixture.advance();
    const auto paused = c::authorizeFire(fixture.input, fixture.aim, state);
    assertNoAttack(paused.decision, c::CombatReason::Cooldown);
    assert(paused.nextState.cadenceActive);

    fixture.advanceTo(state.cadencePauseUntilMicros);
    const auto resumed =
        c::authorizeFire(fixture.input, fixture.aim, paused.nextState);
    assertFirePattern(resumed, c::FirePattern::FullAuto, 0);
    assert(resumed.nextState.cadenceShotsFired == 1);
}

void testCadenceInterruptionAndDeterminism() {
    Fixture first;
    Fixture second;
    first.input.weapon.activeClass = c::WeaponSnapshot::WeaponClass::Rifle;
    second.input.weapon.activeClass = c::WeaponSnapshot::WeaponClass::Rifle;

    const auto firstResult = c::authorizeFire(first.input, first.aim, {});
    const auto secondResult = c::authorizeFire(second.input, second.aim, {});
    assert(firstResult.decision.firePlan == secondResult.decision.firePlan);
    assert(firstResult.nextState.cadencePlan == secondResult.nextState.cadencePlan);
    assert(firstResult.nextState.cadenceShotsFired ==
           secondResult.nextState.cadenceShotsFired);

    first.advance();
    first.visual.count = 0;
    const auto interrupted =
        c::authorizeFire(first.input, first.aim, firstResult.nextState);
    assertNoAttack(interrupted.decision, c::CombatReason::InvalidVisibility);
    assert(!interrupted.nextState.cadenceActive);
}

} // namespace

int main() {
    testReloadPolicyAndCompletionBoundary();
    testWeaponSwitchPolicy();
    testCadencePolicyByWeaponAndRange();
    testCadenceIsBoundedAndReevaluated();
    testCadenceInterruptionAndDeterminism();
    return 0;
}
