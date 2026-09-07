// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/action_planner.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

namespace {

namespace a = astrabot::core::action;
namespace c = astrabot::core::combat;
namespace p = astrabot::core::perception;

constexpr astrabot::core::PlayerId kTarget{32, {1}};

a::ActionPlannerInput input() {
    a::ActionPlannerInput result{};
    result.map = {3};
    result.round = {7};
    result.tick = {11};
    result.nowMicros = 1'100'000;
    result.player = {1, {1}};
    result.agent = {1};
    result.alive = true;
    result.healthPercent = 100.0F;
    result.role = a::TacticalRole::Entry;
    result.personality = a::Personality::Balanced;
    result.weapon.active = {5};
    result.weapon.activeClass = c::WeaponSnapshot::WeaponClass::Rifle;
    result.weapon.clipAmmo = 30;
    result.weapon.reserveAmmo = 90;
    return result;
}

a::NavigationCandidate safeArea(std::uint32_t id = 1) {
    return {{id + 100}, {100.0, 20.0, 0.0}, 100.0, 12.0, 0.05, true, id};
}

a::WeaponPickupCandidate dropped(std::uint32_t reference,
                                  c::WeaponSnapshot::WeaponClass weaponClass) {
    a::WeaponPickupCandidate result{};
    result.observation.reference = reference;
    result.observation.weapon = {static_cast<std::uint16_t>(reference + 10)};
    result.observation.weaponClass = weaponClass;
    result.observation.estimatedClipAmmo = 30;
    result.observation.estimatedReserveAmmo = 60;
    result.observation.position = {100.0, 20.0, 0.0};
    result.observation.area = {reference + 100};
    result.observation.map = {3};
    result.observation.round = {7};
    result.observation.observedTick = {11};
    result.observation.observedMicros = 1'100'000;
    result.observation.confidence = 1.0;
    result.distance = 100.0;
    result.routeCost = 12.0;
    result.routeDanger = 0.05;
    result.exposure = 0.05;
    result.timeRequiredMicros = 100'000;
    result.safeRoute = true;
    return result;
}

void testMinimumActionSetAndObjectives() {
    {
        auto state = input();
        state.enemy = {kTarget, {100.0, 0.0, 0.0}, state.nowMicros, 1.0, true, true, true};
        const auto decision = a::ActionPlanner{}.decide(state);
        assert(decision.accepted);
        assert(decision.intent.action == a::ActionType::Engage);
        assert(decision.intent.target == kTarget);
    }
    {
        auto state = input();
        state.weapon.clipAmmo = 0;
        state.weapon.reserveAmmo = 30;
        state.weapon.canReload = true;
        const auto decision = a::ActionPlanner{}.decide(state);
        assert(decision.intent.action == a::ActionType::Reload);
        assert(decision.precondition.validFor(a::ActionType::Reload));
    }
    {
        auto state = input();
        state.healthPercent = 20.0F;
        state.enemy = {kTarget, {100.0, 0.0, 0.0}, state.nowMicros, 1.0, true, true, true};
        state.navigation[0] = safeArea();
        state.navigationCount = 1;
        const auto decision = a::ActionPlanner{}.decide(state);
        assert(decision.intent.action == a::ActionType::Retreat);
        assert(decision.intent.targetArea == astrabot::nav::model::NavAreaId{101});
    }
    {
        auto state = input();
        state.objective.kind = a::ObjectiveKind::Bomb;
        state.objective.bomb = a::BombState::Planted;
        state.objective.hasDefuseKit = true;
        state.objective.remainingMicros = 20'000'000;
        state.objective.estimatedActionMicros = 5'000'000;
        const auto decision = a::ActionPlanner{}.decide(state);
        assert(decision.intent.action == a::ActionType::Defuse);
    }
    {
        auto state = input();
        state.objective.kind = a::ObjectiveKind::Bomb;
        state.objective.bomb = a::BombState::Dropped;
        state.objective.canRecoverBomb = true;
        const auto decision = a::ActionPlanner{}.decide(state);
        assert(decision.intent.action == a::ActionType::RecoverBomb);
    }
}

void testWeaponAcquisitionUtilityAndLifetime() {
    auto state = input();
    state.enemy = {kTarget, {100.0, 0.0, 0.0}, state.nowMicros, 0.6, true, false, false};
    state.weapons[0] = dropped(7, c::WeaponSnapshot::WeaponClass::SMG);
    state.weaponCount = 1;
    const auto evaluation = a::evaluateWeaponPickup(state, state.weapons[0]);
    assert(evaluation.actionable);
    assert(evaluation.totalUtility > 0);
    assert(evaluation.ammoNeed >= 0);
    const auto selection = a::selectWeaponPickup(state);
    assert(selection.found && selection.candidateIndex == 0);
    const auto decision = a::ActionPlanner{}.decide(state);
    assert(decision.intent.action == a::ActionType::AcquireWeapon);
    assert(decision.hasPickupEvaluation && decision.hasAcquisition);
    assert(decision.acquisition.reference == 7);
    assert(decision.acquisition.targetArea == astrabot::nav::model::NavAreaId{107});

    auto stale = state.weapons[0];
    stale.observation.map = {99};
    assert(a::evaluateWeaponPickup(state, stale).failure ==
           a::WeaponAcquisitionFailure::StaleObservation);
    stale = state.weapons[0];
    stale.observation.observedMicros = state.nowMicros - 3'000'000;
    assert(a::evaluateWeaponPickup(state, stale).failure ==
           a::WeaponAcquisitionFailure::StaleObservation);
    stale = state.weapons[0];
    stale.observation.position.x = (std::numeric_limits<double>::quiet_NaN)();
    assert(a::evaluateWeaponPickup(state, stale).failure ==
           a::WeaponAcquisitionFailure::InvalidObservation);

    auto urgent = state;
    urgent.objective.kind = a::ObjectiveKind::Bomb;
    urgent.objective.bomb = a::BombState::Planted;
    urgent.objective.remainingMicros = 5'000'000;
    assert(a::evaluateWeaponPickup(urgent, urgent.weapons[0]).failure ==
           a::WeaponAcquisitionFailure::ObjectiveUrgent);
}

void testArbitrationAndAbort() {
    auto state = input();
    state.weapon.clipAmmo = 0;
    state.weapon.reserveAmmo = 30;
    state.weapon.canReload = true;
    a::ActionPlanner planner;
    const auto reload = planner.decide(state);
    assert(reload.intent.action == a::ActionType::Reload);
    state.tick.value++;
    state.nowMicros += 1'000;
    state.enemy = {kTarget, {100.0, 0.0, 0.0}, state.nowMicros, 1.0, true, true, true};
    const auto engage = planner.decide(state);
    assert(engage.intent.action == a::ActionType::Engage);
    assert(engage.changed);
    assert(engage.transitionReason == a::ActionAbortReason::BetterActionCritical);

    auto observation = a::ActionObservation{};
    const auto running = planner.update(state, observation);
    assert(running.state == a::ActionResultState::Running);
    observation.enemyAppeared = true;
    const auto aborted = planner.update(state, observation);
    assert(aborted.state == a::ActionResultState::Aborted);
    assert(aborted.reason == a::ActionAbortReason::EnemyAppeared);
    assert(!planner.active());

    a::ActionPlannerSettings settings{};
    settings.actionTimeoutMicros = 100;
    a::ActionPlanner timeoutPlanner(settings);
    auto hold = input();
    const auto first = timeoutPlanner.decide(hold);
    assert(first.intent.action == a::ActionType::Hold);
    hold.tick.value++;
    hold.nowMicros += 100;
    const auto timed = timeoutPlanner.decide(hold);
    assert(timed.transitionReason == a::ActionAbortReason::TimedOut);
}

std::array<a::ActionType, 16> replay(std::size_t load) {
    std::array<a::ActionType, 16> result{};
    for (std::size_t i = 0; i < load; ++i) {
        auto state = input();
        state.player = {static_cast<std::uint16_t>(i + 1), {1}};
        state.agent = {static_cast<std::uint32_t>(i + 1)};
        state.enemy = {kTarget, {100.0, 0.0, 0.0}, state.nowMicros, 1.0, true, true, true};
        result[i] = a::ActionPlanner{}.decide(state).intent.action;
    }
    return result;
}

void testDeterministicLoads() {
    for (const std::size_t load : {std::size_t{1}, std::size_t{8}, std::size_t{16}}) {
        assert(replay(load) == replay(load));
    }
}

} // namespace

int main() {
    testMinimumActionSetAndObjectives();
    testWeaponAcquisitionUtilityAndLifetime();
    testArbitrationAndAbort();
    testDeterministicLoads();
    return 0;
}
