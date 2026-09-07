// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/team_director.hpp"

#include <array>
#include <cassert>

namespace {

namespace t = astrabot::core::team;
using astrabot::core::BotAgentId;
using astrabot::core::Generation;
using astrabot::core::PlayerId;
using astrabot::core::EntityId;

PlayerId player(std::uint16_t slot, std::uint32_t generation = 1) {
    return {slot, Generation{generation}};
}

BotAgentId agent(std::uint32_t value) { return {value}; }

t::ObjectiveTargetId targetId(EntityId value,
                              std::uint32_t generation = 1) {
    return {value, Generation{generation}};
}

t::TeamSnapshot snapshot(
    std::size_t count = 5,
    astrabot::core::perception::Team side =
        astrabot::core::perception::Team::CounterTerrorist) {
    t::TeamSnapshot result{};
    result.map = {2};
    result.round = {3};
    result.tick = {4};
    result.nowMicros = 2'000'000;
    result.objective = {t::ObjectivePhase::Attack, {}, 0, 0, true};
    result.team = side;
    result.memberCount = count;
    for (std::size_t i = 0; i < count; ++i) {
        const auto slot = static_cast<std::uint16_t>(i + 1);
        result.members[i] = {
            player(slot), agent(static_cast<std::uint32_t>(i + 1)),
            {static_cast<double>(i * 32), static_cast<double>(i * 16), 0.0},
            100.0F - static_cast<float>(i), 100 + static_cast<int>(i),
            i == 0 ? t::Personality::Aggressive : t::Personality::Balanced,
            {i == 0 ? t::PlanIntent::Entry : t::PlanIntent::Support, 0},
            false, false, true, true, side};
    }
    return result;
}

std::size_t countKind(const t::TeamDecision& decision,
                      t::ObjectiveAssignmentKind kind) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < decision.shared.objectiveAssignmentCount; ++i) {
        if (decision.shared.objectiveAssignments[i].kind == kind) ++count;
    }
    return count;
}

const t::ObjectiveAssignment* findKind(
    const t::TeamDecision& decision, t::ObjectiveAssignmentKind kind) {
    for (std::size_t i = 0; i < decision.shared.objectiveAssignmentCount; ++i) {
        if (decision.shared.objectiveAssignments[i].kind == kind) {
            return &decision.shared.objectiveAssignments[i];
        }
    }
    return nullptr;
}

bool hasAssignment(const t::TeamDecision& decision,
                   t::ObjectiveAssignmentKind kind, PlayerId playerId,
                   t::ObjectiveTargetId target = {}) {
    for (std::size_t i = 0; i < decision.shared.objectiveAssignmentCount; ++i) {
        const auto& assignment = decision.shared.objectiveAssignments[i];
        if (assignment.kind == kind && assignment.player == playerId &&
            assignment.target == target) {
            return true;
        }
    }
    return false;
}

void assertNoDuplicateExclusive(const t::TeamDecision& decision) {
    for (std::size_t i = 0; i < decision.shared.objectiveAssignmentCount; ++i) {
        const auto& left = decision.shared.objectiveAssignments[i];
        if (!left.exclusive) continue;
        for (std::size_t j = 0; j < i; ++j) {
            const auto& right = decision.shared.objectiveAssignments[j];
            assert(!(right.exclusive && left.kind == right.kind &&
                     left.playerTarget == right.playerTarget &&
                     left.target == right.target));
        }
    }
}

void testObjectiveContractsRejectInvalidAndStaleTargets() {
    auto input = snapshot();
    input.objective.family = t::ObjectiveFamily::VipEscort;
    input.objective.vip = {};
    assert(!input.valid());

    input = snapshot();
    input.objective.family = t::ObjectiveFamily::HostageRescue;
    input.objective.targets[0] = {targetId(7), {}, {}, true, false, false};
    input.objective.targets[1] = input.objective.targets[0];
    input.objective.targetCount = 2;
    assert(!input.valid());

    input = snapshot();
    input.objective.family = t::ObjectiveFamily::Escape;
    input.objective.routeArea = 8;
    t::TeamDirector director;
    const auto decision = director.update(input);
    assert(decision.accepted);
    assert(decision.shared.map == input.map);
    assert(decision.shared.round == input.round);
    assert(decision.shared.valid(input.nowMicros));
    for (std::size_t i = 0; i < decision.shared.objectiveAssignmentCount; ++i) {
        assert(decision.shared.objectiveAssignments[i].valid(
            input.map, input.round, input.nowMicros));
    }
}

void testBombDefuseHasOneExclusiveDefuserAndReassigns() {
    auto input = snapshot();
    input.objective = {t::ObjectivePhase::Planted, {}, 5, 30'000, true};
    input.objective.state = t::ObjectiveState::Planted;
    input.members[0].defuseCapable = true;
    input.members[1].defuseCapable = true;
    t::TeamDirector director;
    const auto initial = director.update(input);
    assert(initial.accepted && initial.strategy == t::Strategy::DefusePriority);
    assert(countKind(initial, t::ObjectiveAssignmentKind::Defuser) == 1);
    assert(countKind(initial, t::ObjectiveAssignmentKind::DefuseCover) == 1);
    assertNoDuplicateExclusive(initial);
    const auto* firstDefuser = findKind(initial, t::ObjectiveAssignmentKind::Defuser);
    assert(firstDefuser != nullptr);
    const auto dead = firstDefuser->player;
    for (std::size_t i = 0; i < input.memberCount; ++i) {
        if (input.members[i].player == dead) {
            input.members[i].alive = false;
            input.members[i].healthPercent = 0.0F;
        }
    }
    t::TeamEvents events{};
    events.defuserDied = true;
    const auto reassigned = director.update(input, events);
    assert(reassigned.accepted && reassigned.reassigned);
    assert(reassigned.reassignment == t::ReassignmentReason::DefuserDied);
    assert(countKind(reassigned, t::ObjectiveAssignmentKind::Defuser) == 1);
    assert(!hasAssignment(reassigned, t::ObjectiveAssignmentKind::Defuser, dead));
    assertNoDuplicateExclusive(reassigned);
}

void testVipEscortAndInterceptAssignments() {
    auto input = snapshot();
    input.objective = {t::ObjectivePhase::Escort, {}, 0, 0, true};
    input.objective.family = t::ObjectiveFamily::VipEscort;
    input.objective.vip = player(1);
    input.objective.escapeArea = 42;
    t::TeamDirector director;
    const auto initial = director.update(input);
    assert(initial.strategy == t::Strategy::EscortPriority);
    assert(countKind(initial, t::ObjectiveAssignmentKind::Vip) == 1);
    assert(countKind(initial, t::ObjectiveAssignmentKind::VipEscort) == 1);
    assert(countKind(initial, t::ObjectiveAssignmentKind::VipPathClear) == 1);
    assertNoDuplicateExclusive(initial);

    const auto* escort = findKind(initial, t::ObjectiveAssignmentKind::VipEscort);
    assert(escort != nullptr);
    const auto deadEscort = escort->player;
    for (std::size_t i = 0; i < input.memberCount; ++i) {
        if (input.members[i].player == deadEscort) input.members[i].alive = false;
    }
    t::TeamEvents events{};
    events.botDied = true;
    const auto replacement = director.update(input, events);
    assert(countKind(replacement, t::ObjectiveAssignmentKind::VipEscort) == 1);
    assert(!hasAssignment(replacement, t::ObjectiveAssignmentKind::VipEscort,
                          deadEscort));

    input.members[0].alive = false;
    input.objective.state = t::ObjectiveState::Failed;
    events = {};
    events.vipDied = true;
    const auto failed = director.update(input, events);
    assert(failed.reassignment == t::ReassignmentReason::VipDied);
    assert(failed.shared.objectiveAssignmentCount == 0);

    auto terrorist = snapshot(5, astrabot::core::perception::Team::Terrorist);
    terrorist.objective = {t::ObjectivePhase::Escort, {}, 0, 0, true};
    terrorist.objective.family = t::ObjectiveFamily::VipEscort;
    terrorist.objective.vip = player(31);
    terrorist.objective.routeArea = 9;
    t::TeamDirector interceptDirector;
    const auto intercept = interceptDirector.update(terrorist);
    assert(countKind(intercept, t::ObjectiveAssignmentKind::VipIntercept) == 5);
    assertNoDuplicateExclusive(intercept);
}

void testHostagesGetDistinctOwnershipAndFollowReassignment() {
    auto input = snapshot();
    input.objective = {t::ObjectivePhase::Escort, {}, 0, 0, true};
    input.objective.family = t::ObjectiveFamily::HostageRescue;
    input.objective.routeArea = 77;
    input.objective.targets[0] = {targetId(20), {}, {}, true, false, false};
    input.objective.targets[1] = {targetId(10), {}, {}, true, false, false};
    input.objective.targetCount = 2;
    t::TeamDirector director;
    const auto initial = director.update(input);
    assert(initial.strategy == t::Strategy::EscortPriority);
    assert(countKind(initial, t::ObjectiveAssignmentKind::HostageRescuer) == 2);
    assertNoDuplicateExclusive(initial);
    const auto* first = findKind(initial, t::ObjectiveAssignmentKind::HostageRescuer);
    assert(first != nullptr);
    const auto deadRescuer = first->player;
    for (std::size_t i = 0; i < input.memberCount; ++i) {
        if (input.members[i].player == deadRescuer) input.members[i].alive = false;
    }
    t::TeamEvents events{};
    events.hostageStateChanged = true;
    const auto replacement = director.update(input, events);
    assert(countKind(replacement, t::ObjectiveAssignmentKind::HostageRescuer) == 2);
    assert(!hasAssignment(replacement, t::ObjectiveAssignmentKind::HostageRescuer,
                          deadRescuer));

    input = snapshot();
    input.objective = {t::ObjectivePhase::Escort, {}, 0, 0, true};
    input.objective.family = t::ObjectiveFamily::HostageRescue;
    input.objective.routeArea = 77;
    input.objective.targets[0] = {
        targetId(10), {}, player(3), true, true, false};
    input.objective.targetCount = 1;
    events = {};
    events.hostageOwnershipChanged = true;
    const auto following = director.update(input, events);
    assert(hasAssignment(following, t::ObjectiveAssignmentKind::HostageEscort,
                         player(3), targetId(10)));
}

void testEscapeAssignmentsReplanForBothSides() {
    auto input = snapshot(8, astrabot::core::perception::Team::Terrorist);
    input.objective = {t::ObjectivePhase::Attack, {}, 0, 0, true};
    input.objective.family = t::ObjectiveFamily::Escape;
    input.objective.routeArea = 8;
    input.objective.escapeArea = 9;
    input.objective.remainingPlayers = 8;
    t::TeamDirector director;
    const auto initial = director.update(input);
    assert(initial.strategy == t::Strategy::EscortPriority);
    assert(countKind(initial, t::ObjectiveAssignmentKind::EscapeRunner) == 1);
    assert(countKind(initial, t::ObjectiveAssignmentKind::EscapeEscort) == 1);
    assert(countKind(initial, t::ObjectiveAssignmentKind::EscapeRouteGuard) >= 1);
    const auto* runner = findKind(initial, t::ObjectiveAssignmentKind::EscapeRunner);
    assert(runner != nullptr);
    const auto deadRunner = runner->player;
    for (std::size_t i = 0; i < input.memberCount; ++i) {
        if (input.members[i].player == deadRunner) input.members[i].alive = false;
    }
    t::TeamEvents events{};
    events.escapeProgressChanged = true;
    const auto replacement = director.update(input, events);
    assert(countKind(replacement, t::ObjectiveAssignmentKind::EscapeRunner) == 1);
    assert(!hasAssignment(replacement, t::ObjectiveAssignmentKind::EscapeRunner,
                          deadRunner));

    auto counter = snapshot(5, astrabot::core::perception::Team::CounterTerrorist);
    counter.objective = {t::ObjectivePhase::Defense, {}, 0, 0, true};
    counter.objective.family = t::ObjectiveFamily::Escape;
    counter.objective.routeArea = 8;
    t::TeamDirector counterDirector;
    const auto defense = counterDirector.update(counter);
    assert(defense.strategy == t::Strategy::DefenseSplit);
    assert(countKind(defense, t::ObjectiveAssignmentKind::EscapeBlocker) == 1);
    assert(countKind(defense, t::ObjectiveAssignmentKind::EscapeIntercept) == 1);
    assert(countKind(defense, t::ObjectiveAssignmentKind::RouteDefense) >= 1);
}

void testMapRoundGenerationAndInputOrderAreSafe() {
    auto input = snapshot(8, astrabot::core::perception::Team::Terrorist);
    input.objective = {t::ObjectivePhase::Attack, {}, 0, 0, true};
    input.objective.family = t::ObjectiveFamily::Escape;
    input.objective.routeArea = 13;
    t::TeamDirector director;
    const auto first = director.update(input);
    input.map = {3};
    input.round = {4};
    t::TeamEvents events{};
    events.mapChanged = true;
    events.roundChanged = true;
    const auto transitioned = director.update(input, events);
    assert(transitioned.reassignment == t::ReassignmentReason::MapChanged);
    assert(transitioned.shared.map == input.map);
    assert(transitioned.shared.round == input.round);
    assert(first.shared.objectiveAssignmentCount ==
           transitioned.shared.objectiveAssignmentCount);

    auto reused = snapshot(5, astrabot::core::perception::Team::Terrorist);
    reused.objective = {t::ObjectivePhase::Attack, {}, 0, 0, true};
    reused.objective.family = t::ObjectiveFamily::Escape;
    reused.objective.routeArea = 13;
    t::TeamDirector reuseDirector;
    const auto beforeReuse = reuseDirector.update(reused);
    reused.members[0].player = player(1, 2);
    events = {};
    events.playerGenerationChanged = true;
    const auto afterReuse = reuseDirector.update(reused, events);
    assert(afterReuse.reassignment == t::ReassignmentReason::ObjectiveInvalidated);
    for (std::size_t i = 0; i < afterReuse.shared.objectiveAssignmentCount; ++i) {
        assert(afterReuse.shared.objectiveAssignments[i].player != player(1));
    }
    assert(beforeReuse.shared.objectiveAssignmentCount ==
           afterReuse.shared.objectiveAssignmentCount);

    auto left = snapshot();
    left.objective = {t::ObjectivePhase::Escort, {}, 0, 0, true};
    left.objective.family = t::ObjectiveFamily::HostageRescue;
    left.objective.routeArea = 15;
    left.objective.targets[0] = {targetId(2), {}, {}, true, false, false};
    left.objective.targets[1] = {targetId(1), {}, {}, true, false, false};
    left.objective.targetCount = 2;
    auto right = left;
    std::swap(right.members[0], right.members[4]);
    t::TeamDirector leftDirector;
    t::TeamDirector rightDirector;
    const auto leftDecision = leftDirector.update(left);
    const auto rightDecision = rightDirector.update(right);
    assert(leftDecision.shared.objectiveAssignmentCount ==
           rightDecision.shared.objectiveAssignmentCount);
    for (std::size_t i = 0;
         i < leftDecision.shared.objectiveAssignmentCount; ++i) {
        const auto& assignment = leftDecision.shared.objectiveAssignments[i];
        bool found = false;
        for (std::size_t j = 0;
             j < rightDecision.shared.objectiveAssignmentCount; ++j) {
            const auto& candidate = rightDecision.shared.objectiveAssignments[j];
            if (candidate.kind == assignment.kind &&
                candidate.player == assignment.player &&
                candidate.target == assignment.target) {
                found = true;
                break;
            }
        }
        assert(found);
    }
}

} // namespace

int main() {
    testObjectiveContractsRejectInvalidAndStaleTargets();
    testBombDefuseHasOneExclusiveDefuserAndReassigns();
    testVipEscortAndInterceptAssignments();
    testHostagesGetDistinctOwnershipAndFollowReassignment();
    testEscapeAssignmentsReplanForBothSides();
    testMapRoundGenerationAndInputOrderAreSafe();
    return 0;
}
