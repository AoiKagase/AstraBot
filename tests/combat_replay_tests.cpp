// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/combat.hpp"

#include <array>
#include <cassert>
#include <cstdint>

namespace {

namespace c = astrabot::core::combat;
namespace p = astrabot::core::perception;
namespace w = astrabot::core::world;

struct Scenario final {
    c::CombatInput input{};
    w::MemorySnapshot visual{};
    w::SoundSnapshot sounds{};
    c::AttackLifecycleState state{};
    astrabot::core::PlayerId target{};

    Scenario(
        astrabot::core::PlayerId player = {1, {1}},
        astrabot::core::PlayerId targetPlayer = {2, {1}},
        astrabot::core::BotAgentId agent = {1})
        : target(targetPlayer) {
        input.map = {1};
        input.round = {1};
        input.tick = {1};
        input.timeMicros = 1'000'000;
        input.player = player;
        input.agent = agent;
        input.alive = true;
        input.team = p::Team::CounterTerrorist;
        input.eye = {0.0, 0.0, 36.0};
        input.view = {0.0F, 0.0F, 0.0F};
        input.weapon.active = {5};
        input.weapon.activeClass = c::WeaponSnapshot::WeaponClass::Rifle;
        input.weapon.owned[0] = input.weapon.active;
        input.weapon.ownedCount = 1;
        input.weapon.clipAmmo = 12;
        input.weapon.reserveAmmo = 48;
        input.weapon.canReload = true;
        input.weapon.canSwitch = true;
        input.world.roster[target.slot - 1U] = {target, p::Team::Terrorist};
        visible(true);
    }

    void visible(bool value) {
        visual.count = value ? 1U : 0U;
        if (value) {
            visual.memories[0].target = target;
            visual.memories[0].lastKnownPosition = {100.0, 0.0, 36.0};
            visual.memories[0].lastSeenMicros = input.timeMicros;
            visual.memories[0].confidence = 1.0;
        }
        refresh();
    }

    void refresh() {
        const p::Stamp stamp{
            input.agent, input.player, input.map, input.tick,
            input.timeMicros, input.round};
        visual.stamp = stamp;
        if (visual.count != 0) {
            visual.memories[0].target = target;
            visual.memories[0].lastSeenMicros = input.timeMicros;
            visual.memories[0].identity = {
                input.map, input.round, p::ObservationSource::Vision, 1,
                input.timeMicros, input.timeMicros};
        }
        sounds.stamp = stamp;
        input.world.stamp = stamp;
        input.world.visual = &visual;
        input.world.sounds = &sounds;
        input.weapon.map = input.map;
        input.weapon.round = input.round;
        input.weapon.tick = input.tick;
        input.weapon.observedMicros = input.timeMicros;
    }

    c::FireAuthorization authorize() {
        const auto aim = c::aimTarget(input);
        const auto result = c::authorizeFire(input, aim, state);
        state = result.nextState;
        return result;
    }

    void next(std::uint64_t elapsedMicros = 1'000) {
        ++input.tick.value;
        input.timeMicros += elapsedMicros;
        refresh();
    }
};

void testMinimumScenarioReplay() {
    Scenario scenario;
    scenario.input.difficulty.reactionDelayMicros = 100'000;

    const auto acquired = c::aimTarget(scenario.input);
    assert(acquired.action == c::CombatAction::Track);
    assert(acquired.reason == c::CombatReason::ReactionDelay);
    assert(scenario.authorize().decision.reason == c::CombatReason::ReactionDelay);

    scenario.next(100'000);
    const auto first = scenario.authorize();
    assert(first.decision.action == c::CombatAction::Fire);
    assert(first.decision.fireMode == c::FireMode::DirectFire);
    assert(first.decision.firePlan->pattern == c::FirePattern::FullAuto);
    assert(first.nextState.cadenceActive);

    for (std::uint8_t shot = 1; shot < c::kMaxBurstShots; ++shot) {
        --scenario.input.weapon.clipAmmo;
        scenario.next();
        const auto result = scenario.authorize();
        assert(result.decision.action == c::CombatAction::Fire);
        assert(result.decision.firePlan->pattern == c::FirePattern::FullAuto);
    }
    assert(scenario.state.cadencePauseUntilMicros > scenario.input.timeMicros);

    scenario.next();
    const auto paused = scenario.authorize();
    assert(paused.decision.action != c::CombatAction::Fire);
    assert(paused.decision.reason == c::CombatReason::Cooldown);

    scenario.visible(false);
    scenario.next(100'000);
    const auto hidden = scenario.authorize();
    assert(hidden.decision.action == c::CombatAction::NoOp);
    assert(!hidden.decision.hasAttackInput());
    assert(!scenario.state.cadenceActive);
}

void testNoTargetStillScansView() {
    Scenario scenario;
    scenario.visible(false);

    const auto first = c::aimTarget(scenario.input);
    assert(first.action == c::CombatAction::NoOp);
    assert(first.reason == c::CombatReason::NoTarget);

    scenario.next(100'000);
    const auto second = c::aimTarget(scenario.input);
    assert(second.action == c::CombatAction::NoOp);
    assert(second.view.pitch == scenario.input.view.pitch);
    assert(second.view.yaw != first.view.yaw);
}

void testBurstPauseAndReevaluation() {
    Scenario scenario;
    scenario.input.view = {0.0F, 0.0F, 0.0F};
    scenario.visual.memories[0].lastKnownPosition = {700.0, 0.0, 36.0};
    scenario.refresh();

    const auto first = scenario.authorize();
    assert(first.decision.action == c::CombatAction::Fire);
    assert(first.decision.firePlan->pattern == c::FirePattern::Burst);
    assert(first.decision.firePlan->burstShots == 3);
    --scenario.input.weapon.clipAmmo;
    scenario.next();
    assert(scenario.authorize().decision.action == c::CombatAction::Fire);
    --scenario.input.weapon.clipAmmo;
    scenario.next();
    assert(scenario.authorize().decision.action == c::CombatAction::Fire);
    assert(scenario.state.cadencePauseUntilMicros > scenario.input.timeMicros);

    scenario.next();
    assert(scenario.authorize().decision.reason == c::CombatReason::Cooldown);
    scenario.next(c::kMaxReactionDelayMicros / 100); // bounded and beyond the pause
    const auto reevaluated = scenario.authorize();
    assert(reevaluated.decision.action == c::CombatAction::Fire);
    assert(reevaluated.decision.firePlan->pattern == c::FirePattern::Burst);
}

void testReloadSwitchAndStaleLifecycle() {
    Scenario scenario;
    scenario.visible(false);
    scenario.input.weapon.clipAmmo = 2;
    scenario.input.weapon.reserveAmmo = 10;
    scenario.input.weapon.reloadClipThreshold = 3;
    scenario.refresh();
    auto reload = scenario.authorize();
    assert(reload.decision.action == c::CombatAction::Reload);
    assert((reload.decision.buttons & static_cast<astrabot::core::ButtonMask>(
        astrabot::core::Button::Reload)) != 0U);

    scenario.next();
    scenario.input.weapon.reloading = true;
    scenario.visible(true);
    const auto duringReload = scenario.authorize();
    assert(duringReload.decision.action != c::CombatAction::Fire);
    assert(duringReload.decision.reason == c::CombatReason::Reloading);

    scenario.input.weapon.reloading = false;
    scenario.input.weapon.clipAmmo = 0;
    scenario.input.weapon.reserveAmmo = 0;
    scenario.input.weapon.owned[1] = {7};
    scenario.input.weapon.ownedCount = 2;
    scenario.next();
    const auto switched = scenario.authorize();
    assert(switched.decision.action == c::CombatAction::SwitchWeapon);
    assert(switched.decision.selectedWeapon == c::WeaponId{7});

    scenario.input.weapon.canSwitch = false;
    scenario.next();
    assert(scenario.authorize().decision.reason == c::CombatReason::NoUsableWeapon);

    scenario.input.weapon.clipAmmo = 12;
    scenario.input.weapon.reserveAmmo = 48;
    scenario.input.weapon.ownedCount = 1;
    scenario.input.weapon.canSwitch = true;
    scenario.visible(true);
    scenario.next();
    const auto fired = scenario.authorize();
    assert(fired.decision.action == c::CombatAction::Fire);
    const auto duplicate = scenario.authorize();
    assert(duplicate.decision.reason == c::CombatReason::DuplicateAttack);

    scenario.input.alive = false;
    scenario.next();
    const auto dead = scenario.authorize();
    assert(dead.decision.reason == c::CombatReason::Dead);
    assert(!scenario.state.cadenceActive);
}

void testTargetReplacementAndAllyGate() {
    Scenario scenario;
    const auto first = scenario.authorize();
    assert(first.decision.action == c::CombatAction::Fire);

    const astrabot::core::PlayerId replacement{3, {1}};
    scenario.target = replacement;
    scenario.visual.memories[0].target = replacement;
    scenario.input.world.roster[1] = {};
    scenario.input.world.roster[replacement.slot - 1U] = {
        replacement, p::Team::Terrorist};
    scenario.next();
    const auto replaced = scenario.authorize();
    assert(replaced.decision.action == c::CombatAction::Fire);
    assert(replaced.decision.target == replacement);
    assert(scenario.state.cadenceTarget == replacement);

    scenario.input.world.roster[replacement.slot - 1U] = {
        replacement, p::Team::CounterTerrorist};
    scenario.next();
    const auto ally = scenario.authorize();
    assert(ally.decision.action != c::CombatAction::Fire);
    assert(ally.decision.reason == c::CombatReason::Ally);
    assert(!scenario.state.cadenceActive);
}

void testMapRoundAndLoadDeterminism() {
    for (const auto load : {1U, 8U, 16U}) {
        for (const auto intervalMs : {8U, 16U, 100U}) {
            std::array<c::CombatAction, 16> firstActions{};
            std::array<c::CombatAction, 16> secondActions{};
            for (unsigned pass = 0; pass < 2; ++pass) {
                for (unsigned index = 0; index < load; ++index) {
                    const auto bot = astrabot::core::PlayerId{
                        static_cast<std::uint16_t>(index * 2U + 1U), {1}};
                    const auto enemy = astrabot::core::PlayerId{
                        static_cast<std::uint16_t>(index * 2U + 2U), {1}};
                    Scenario scenario{bot, enemy, {index + 1U}};
                    scenario.input.weapon.activeClass =
                        c::WeaponSnapshot::WeaponClass::Pistol;
                    scenario.input.difficulty.reactionDelayMicros = 0;
                    scenario.refresh();
                    const auto result = scenario.authorize();
                    assert(result.decision.action == c::CombatAction::Fire);
                    const auto duplicate = scenario.authorize();
                    assert(duplicate.decision.reason == c::CombatReason::DuplicateAttack);
                    scenario.next(static_cast<std::uint64_t>(intervalMs) * 1000U);
                    const auto next = scenario.authorize();
                    assert(next.decision.action == c::CombatAction::Fire);
                    (pass == 0 ? firstActions : secondActions)[index] =
                        result.decision.action;
                }
            }
            for (unsigned index = 0; index < load; ++index) {
                assert(firstActions[index] == secondActions[index]);
            }
        }
    }

    Scenario transition;
    assert(transition.authorize().decision.action == c::CombatAction::Fire);
    transition.input.map = {2};
    transition.input.round = {2};
    transition.next();
    const auto afterTransition = transition.authorize();
    assert(afterTransition.decision.action == c::CombatAction::Fire);
    assert(afterTransition.nextState.map == transition.input.map);
    assert(afterTransition.nextState.round == transition.input.round);
}

} // namespace

int main() {
    testMinimumScenarioReplay();
    testNoTargetStillScansView();
    testBurstPauseAndReevaluation();
    testReloadSwitchAndStaleLifecycle();
    testTargetReplacementAndAllyGate();
    testMapRoundAndLoadDeterminism();
    return 0;
}
