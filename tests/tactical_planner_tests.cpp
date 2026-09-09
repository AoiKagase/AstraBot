// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/tactical_planner.hpp"

#include <array>
#include <cassert>

namespace {

namespace t = astrabot::core::tactical;
namespace p = astrabot::core::perception;
namespace w = astrabot::core::world;

constexpr astrabot::core::PlayerId kSelf{1, {1}};
constexpr astrabot::core::PlayerId kEntry{2, {1}};
constexpr astrabot::core::PlayerId kEnemyA{10, {1}};
constexpr astrabot::core::PlayerId kEnemyB{11, {1}};

t::TargetArea area(std::uint32_t id) {
    return {{id}, {static_cast<double>(id), 20.0, 0.0}, id};
}

t::TacticalRoute route(t::RouteStyle style, std::uint32_t id,
                       std::uint64_t eta = 1'000'000) {
    return {area(id), style, eta, 0.1, true};
}

t::TacticalContext context() {
    t::TacticalContext result{};
    result.map = {1};
    result.round = {1};
    result.tick = {1};
    result.nowMicros = 1'000'000;
    result.self = {kSelf, {1}, p::Team::CounterTerrorist,
                   t::RolePreference::Any, {0.0, 0.0, 0.0}, {100},
                   100.0F, 100, true};
    result.objective = {};
    result.economy = {true, 100, false};
    result.navigation.routes[0] = route(t::RouteStyle::Hold, 100);
    result.navigation.routeCount = 1;
    return result;
}

void addRoute(t::TacticalContext& state, t::RouteStyle style, std::uint32_t id,
              std::uint64_t eta = 1'000'000) {
    assert(state.navigation.routeCount < state.navigation.routes.size());
    state.navigation.routes[state.navigation.routeCount++] = route(style, id, eta);
}

t::EnemyBelief enemy(astrabot::core::PlayerId player, std::uint32_t areaId) {
    return {player, {areaId}, {static_cast<double>(areaId), 0.0, 0.0},
            0.95, 0, true, false};
}

void testContractsAndMinimumChoices() {
    {
        auto state = context();
        state.objective.canAttack = true;
        state.objective.target = area(200);
        addRoute(state, t::RouteStyle::Direct, 200);
        t::TacticalPlanner planner;
        const auto decision = planner.plan(state);
        assert(decision.accepted && decision.replanned);
        assert(decision.intent.type == t::IntentType::AttackSite);
        assert(decision.intent.reason == t::Reason::AttackObjective);
        assert(decision.intent.validity == t::Validity::Valid);
        assert(decision.intent.target.area == astrabot::nav::model::NavAreaId{200});
    }
    {
        auto state = context();
        state.objective.canDefend = true;
        addRoute(state, t::RouteStyle::Safe, 201);
        const auto decision = t::TacticalPlanner{}.plan(state);
        assert(decision.intent.type == t::IntentType::DefendSite);
    }
    {
        const auto decision = t::TacticalPlanner{}.plan(context());
        assert(decision.intent.type == t::IntentType::Hold);
        assert(decision.intent.route == t::RouteStyle::Hold);
    }
}

void testRotateFromConfirmedEnemyConcentration() {
    auto state = context();
    addRoute(state, t::RouteStyle::Rotate, 300);
    state.enemies[0] = enemy(kEnemyA, 210);
    state.enemies[1] = enemy(kEnemyB, 210);
    state.enemyCount = 2;

    t::TacticalPlanner planner;
    const auto decision = planner.plan(state);
    assert(decision.intent.type == t::IntentType::Rotate);
    assert(decision.intent.reason == t::Reason::EnemyConcentration);
    assert(decision.evaluatedEnemies == 2);
}

void testRetakeAndSaveBranches() {
    auto state = context();
    state.objective.kind = t::ObjectiveKind::Bomb;
    state.objective.bomb = t::BombState::Planted;
    state.objective.target = area(400);
    state.objective.remainingMicros = 20'000'000;
    state.objective.retakeTimeMicros = 5'000'000;
    state.objective.retakeFeasible = true;
    addRoute(state, t::RouteStyle::Retake, 400, 5'000'000);

    t::TacticalPlanner planner;
    const auto retake = planner.plan(state);
    assert(retake.intent.type == t::IntentType::Retake);
    assert(retake.intent.target.area == astrabot::nav::model::NavAreaId{400});

    state.nowMicros += 1000;
    state.objective.retakeFeasible = false;
    state.navigation.routes[1].available = false;
    const auto save = planner.plan(state, {});
    assert(save.intent.type == t::IntentType::Save);
    assert(save.intent.reason == t::Reason::RetakeUnavailable);
}

void testEntryDeathSupportReevaluation() {
    auto state = context();
    state.self.role = t::RolePreference::Entry;
    state.teammates[0] = {kEntry, t::RolePreference::Entry, {210}, true, false};
    state.teammateCount = 1;
    addRoute(state, t::RouteStyle::Support, 500);

    t::TacticalPlanner planner;
    const auto initial = planner.plan(state);
    assert(initial.intent.type == t::IntentType::Hold);

    state.nowMicros += 1000;
    state.teammates[0].alive = false;
    t::ReplanEvents events{};
    events.teammateDeath = true;
    events.entryPlayerDied = true;
    const auto support = planner.plan(state, events);
    assert(support.replanned && support.changed);
    assert(support.trigger == t::ReplanTrigger::TeammateDeath);
    assert(support.intent.type == t::IntentType::Support);
    assert(support.intent.reason == t::Reason::EntryLost);
}

void testPeriodicAndBlockedReplanning() {
    auto state = context();
    t::TacticalPlanner planner;
    const auto first = planner.plan(state);
    assert(first.replanned);
    state.nowMicros += 500'000;
    const auto stable = planner.plan(state);
    assert(!stable.replanned && !stable.changed);
    state.nowMicros += 500'000;
    const auto periodic = planner.plan(state);
    assert(periodic.replanned && periodic.trigger == t::ReplanTrigger::Periodic);
    state.nowMicros += 1;
    t::ReplanEvents events{};
    events.routeBlocked = true;
    const auto blocked = planner.plan(state, events);
    assert(blocked.replanned && blocked.trigger == t::ReplanTrigger::RouteBlocked);
}

void testWorldModelContextUsesOnlyKnownBeliefs() {
    w::MemorySnapshot visual{};
    w::SoundSnapshot sounds{};
    w::WorldSnapshot snapshot{};
    snapshot.stamp = {{1}, kSelf, {1}, {4}, 2'000'000, {1}};
    snapshot.visual = &visual;
    snapshot.sounds = &sounds;
    snapshot.roster[0] = {kSelf, p::Team::CounterTerrorist};
    snapshot.roster[1] = {kEntry, p::Team::CounterTerrorist};
    snapshot.roster[9] = {kEnemyA, p::Team::Terrorist};
    visual.stamp = snapshot.stamp;
    visual.count = 1;
    visual.memories[0] = {kEnemyA, {900.0, 10.0, 0.0}, 1'900'000, 0.9, {}};
    sounds.stamp = snapshot.stamp;

    auto seed = t::TacticalContextSeed{};
    seed.self = context().self;
    seed.self.team = p::Team::CounterTerrorist;
    seed.objective = {};
    seed.economy = {true, 50, false};
    seed.navigation = context().navigation;
    const auto result = t::buildTacticalContext(snapshot, seed);
    assert(result.valid());
    assert(result.teammateCount == 1);
    assert(result.enemyCount == 1);
    assert(result.enemies[0].target == kEnemyA);
    assert(result.enemies[0].confirmed);
    assert(result.enemies[0].area == astrabot::nav::model::NavAreaId{});
}

void testAutonomousRoamAndPriorityContracts() {
    auto state = context();
    state.navigation.routes[0].available = false;
    state.navigation.routeCount = 0;
    state.navigation.roamCandidates[0] = area(101);
    state.navigation.roamCandidates[1] = area(102);
    state.navigation.roamCandidateCount = 2;

    t::TacticalPlanner planner;
    const auto first = planner.plan(state);
    assert(first.accepted);
    assert(first.intent.type == t::IntentType::Roam);
    assert(first.intent.route == t::RouteStyle::Roam);
    assert(first.intent.reason == t::Reason::AutonomousRoam);
    assert(first.intent.target.area != state.self.currentArea);

    state.nowMicros += 1;
    t::ReplanEvents arrived{};
    arrived.intentInvalidated = true;
    const auto second = planner.plan(state, arrived);
    assert(second.replanned);
    assert(second.intent.type == t::IntentType::Roam);
    assert(second.intent.target.area != first.intent.target.area);

    auto explicitRoute = context();
    explicitRoute.navigation.routes[0].available = false;
    explicitRoute.navigation.routeCount = 0;
    explicitRoute.navigation.explicitRouteAvailable = true;
    explicitRoute.navigation.explicitRoute = route(t::RouteStyle::Hold, 500);
    explicitRoute.navigation.roamCandidates[0] = area(501);
    explicitRoute.navigation.roamCandidateCount = 1;
    const auto explicitDecision = t::TacticalPlanner{}.plan(explicitRoute);
    assert(explicitDecision.intent.type == t::IntentType::Hold);
    assert(explicitDecision.intent.target.area ==
           astrabot::nav::model::NavAreaId{500});
}
} // namespace

int main() {
    testContractsAndMinimumChoices();
    testRotateFromConfirmedEnemyConcentration();
    testRetakeAndSaveBranches();
    testEntryDeathSupportReevaluation();
    testPeriodicAndBlockedReplanning();
    testWorldModelContextUsesOnlyKnownBeliefs();
    testAutonomousRoamAndPriorityContracts();
    return 0;
}
