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
        input.view = {0.0F, 90.0F, 0.0F};

        const p::Stamp stamp{kAgent, kBot, input.map, input.tick,
                             input.timeMicros, input.round};
        visual.stamp = stamp;
        visual.count = 1;
        visual.memories[0].target = kTarget;
        visual.memories[0].lastKnownPosition = {100.0, 20.0, 30.0};
        visual.memories[0].lastSeenMicros = input.timeMicros;
        visual.memories[0].confidence = 1.0;
        visual.memories[0].identity = {
            input.map, input.round, p::ObservationSource::Vision, 1,
            input.timeMicros, input.timeMicros};
        sounds.stamp = stamp;

        input.world.stamp = stamp;
        input.world.visual = &visual;
        input.world.sounds = &sounds;
        input.world.roster[kTarget.slot - 1U] =
            {kTarget, p::Team::Terrorist};

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
    }

    c::CombatDecision aim{};
};

void assertFire(const c::FireAuthorization& result) {
    assert(result.decision.action == c::CombatAction::Fire);
    assert(result.decision.target == kTarget);
    assert(result.decision.fireMode.has_value());
    assert(*result.decision.fireMode == c::FireMode::DirectFire);
    assert(result.decision.firePlan.has_value());
    assert(result.decision.firePlan->pattern == c::FirePattern::Tap);
    assert(result.decision.firePlan->burstShots == 1);
    assert(result.decision.hasAttackInput());
    assert(result.decision.selectedWeapon == c::WeaponId{5});
    assert(result.decision.reason == c::CombatReason::Accepted);
}

void assertSuppressed(const c::FireAuthorization& result,
                      c::CombatReason reason) {
    assert(result.decision.action != c::CombatAction::Fire);
    assert(!result.decision.fireMode.has_value());
    assert(!result.decision.firePlan.has_value());
    assert(!result.decision.hasAttackInput());
    assert(result.decision.reason == reason);
}

void testCurrentDirectVisionAuthorizesFire() {
    Fixture fixture;
    const auto result = c::authorizeFire(fixture.input, fixture.aim, {});
    assertFire(result);
    assert(result.nextState.initialized);
    assert(result.nextState.lastFireTick == fixture.input.tick);
    assert(result.nextState.lastFireMicros == fixture.input.timeMicros);
    assert(!result.nextState.attackHeld);
}

void testVisibilityAndRelationGates() {
    {
        Fixture fixture;
        fixture.visual.memories[0].identity.source =
            p::ObservationSource::TeamReport;
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::InvalidVisibility);
    }
    {
        Fixture fixture;
        --fixture.visual.memories[0].identity.observedMicros;
        --fixture.visual.memories[0].lastSeenMicros;
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::InvalidVisibility);
    }
    {
        Fixture fixture;
        fixture.input.world.roster[kTarget.slot - 1U] = {};
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::UnknownRelation);
    }
    {
        Fixture fixture;
        fixture.input.world.roster[kTarget.slot - 1U] =
            {kTarget, p::Team::CounterTerrorist};
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::Ally);
    }
}

void testReactionAndWeaponGates() {
    {
        Fixture fixture;
        fixture.input.difficulty.reactionDelayMicros = 100;
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::ReactionDelay);
    }
    {
        Fixture fixture;
        fixture.input.weapon.reloading = true;
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::Reloading);
    }
    {
        Fixture fixture;
        fixture.input.weapon.clipAmmo = 0;
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::EmptyClip);
    }
    {
        Fixture fixture;
        fixture.input.weapon.primaryAttackReadyMicros =
            fixture.input.timeMicros + 1;
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::Cooldown);
    }
}

void testInputAndAimRejection() {
    {
        Fixture fixture;
        fixture.input.tick = {12};
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::StaleInput);
    }
    {
        Fixture fixture;
        fixture.input.player.generation.value = 2;
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::StaleInput);
    }
    {
        Fixture fixture;
        fixture.input.weapon.tick = {12};
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::StaleWeapon);
    }
    {
        Fixture fixture;
        fixture.input.weapon.active = {};
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::InvalidWeapon);
    }
    {
        Fixture fixture;
        fixture.aim.source = p::ObservationSource::TeamReport;
        assertSuppressed(
            c::authorizeFire(fixture.input, fixture.aim, {}),
            c::CombatReason::InvalidVisibility);
    }
}

void testLifecycleAndGenerationReset() {
    Fixture fixture;
    const auto first = c::authorizeFire(fixture.input, fixture.aim, {});
    assertFire(first);

    const auto duplicate =
        c::authorizeFire(fixture.input, fixture.aim, first.nextState);
    assertSuppressed(duplicate, c::CombatReason::DuplicateAttack);

    fixture.input.map = {4};
    fixture.input.tick = {12};
    fixture.input.timeMicros = 1200000;
    const p::Stamp stamp{kAgent, kBot, fixture.input.map, fixture.input.tick,
                         fixture.input.timeMicros, fixture.input.round};
    fixture.visual.stamp = stamp;
    fixture.sounds.stamp = stamp;
    fixture.input.world.stamp = stamp;
    fixture.visual.memories[0].lastSeenMicros = fixture.input.timeMicros;
    fixture.visual.memories[0].identity.map = fixture.input.map;
    fixture.visual.memories[0].identity.observedMicros = fixture.input.timeMicros;
    fixture.visual.memories[0].identity.receivedMicros = fixture.input.timeMicros;
    fixture.input.weapon.map = fixture.input.map;
    fixture.input.weapon.tick = fixture.input.tick;
    fixture.input.weapon.observedMicros = fixture.input.timeMicros;
    fixture.aim.inputTick = fixture.input.tick;
    fixture.aim.validUntilMicros = fixture.input.timeMicros;

    assertFire(c::authorizeFire(fixture.input, fixture.aim, first.nextState));
}

void testDeterministicDecision() {
    Fixture left;
    Fixture right;
    const auto first = c::authorizeFire(left.input, left.aim, {});
    const auto second = c::authorizeFire(right.input, right.aim, {});
    assert(first.decision.action == second.decision.action);
    assert(first.decision.target == second.decision.target);
    assert(first.decision.fireMode == second.decision.fireMode);
    assert(first.decision.firePlan->pattern == second.decision.firePlan->pattern);
    assert(first.decision.buttons == second.decision.buttons);
    assert(first.decision.reason == second.decision.reason);
    assert(first.nextState.lastFireTick == second.nextState.lastFireTick);
    assert(first.nextState.lastFireMicros == second.nextState.lastFireMicros);
}

} // namespace

int main() {
    testCurrentDirectVisionAuthorizesFire();
    testVisibilityAndRelationGates();
    testReactionAndWeaponGates();
    testInputAndAimRejection();
    testLifecycleAndGenerationReset();
    testDeterministicDecision();
    return 0;
}
