// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/team_director.hpp"

#include <array>
#include <cassert>

namespace {

namespace t = astrabot::core::team;
using astrabot::core::BotAgentId;
using astrabot::core::PlayerId;

PlayerId player(std::uint16_t slot) { return {slot, {1}}; }
BotAgentId agent(std::uint32_t value) { return {value}; }

t::TeamSnapshot snapshot(std::size_t count = 5) {
    t::TeamSnapshot result{};
    result.map = {1};
    result.round = {1};
    result.tick = {1};
    result.nowMicros = 1'000'000;
    result.objective = {t::ObjectivePhase::Attack, {}, 0, 0, true};
    result.memberCount = count;
    for (std::size_t i = 0; i < count; ++i) {
        const auto slot = static_cast<std::uint16_t>(i + 1);
        result.members[i] = {
            player(slot), agent(static_cast<std::uint32_t>(i + 1)),
            {static_cast<double>(i * 100), static_cast<double>(i * 25), 0.0},
            100.0F - static_cast<float>(i), 100 + static_cast<int>(i * 10),
            i == 0 ? t::Personality::Aggressive
                   : (i == 1 ? t::Personality::Supportive
                              : t::Personality::Balanced),
            {i == 0 ? t::PlanIntent::Entry
                    : (i == 1 ? t::PlanIntent::Support
                               : t::PlanIntent::Attack),
             0},
            false, false, true, true};
    }
    return result;
}

bool hasRole(const t::TeamDecision& decision, t::Role role) {
    for (std::size_t i = 0; i < decision.shared.assignmentCount; ++i) {
        if (decision.shared.assignments[i].role == role) return true;
    }
    return false;
}

std::size_t roleCount(const t::TeamDecision& decision, t::Role role) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < decision.shared.assignmentCount; ++i) {
        if (decision.shared.assignments[i].role == role) ++count;
    }
    return count;
}

void testFiveBotsReceiveUniqueAttackRoles() {
    const auto input = snapshot();
    t::TeamDirector director;
    const auto first = director.update(input);
    assert(first.accepted && first.changed && first.reassigned);
    assert(first.strategy == t::Strategy::AttackSplit);
    assert(first.reassignment == t::ReassignmentReason::Initial);
    assert(first.shared.assignmentCount == 5);
    assert(first.shared.valid(input.nowMicros));
    assert(hasRole(first, t::Role::Entry));
    assert(hasRole(first, t::Role::Trade));
    assert(hasRole(first, t::Role::Support));
    assert(hasRole(first, t::Role::Lurk));
    assert(hasRole(first, t::Role::FlankWatch));

    std::array<bool, 10> seen{};
    for (std::size_t i = 0; i < first.shared.assignmentCount; ++i) {
        const auto role = static_cast<std::size_t>(first.shared.assignments[i].role);
        assert(role < seen.size());
        assert(!seen[role]);
        seen[role] = true;
    }

    const auto stable = director.update(input);
    assert(stable.accepted && !stable.changed && !stable.reassigned);
    assert(stable.generation == first.generation);
}

void testDeterministicInputsAndBoundedTeamSize() {
    auto input = snapshot(16);
    input.members[0].defuseCapable = true;
    t::TeamDirector left;
    t::TeamDirector right;
    const auto a = left.update(input);
    const auto b = right.update(input);
    assert(a.accepted && b.accepted);
    assert(a.shared.assignmentCount == 16);
    assert(a.generation == 1 && b.generation == 1);
    assert(a.shared.assignmentCount == b.shared.assignmentCount);
    for (std::size_t i = 0; i < a.shared.assignmentCount; ++i) {
        assert(a.shared.assignments[i].player == b.shared.assignments[i].player);
        assert(a.shared.assignments[i].role == b.shared.assignments[i].role);
    }
    assert(roleCount(a, t::Role::Defuser) == 1);
}

void testObjectiveStrategies() {
    struct Case {
        t::ObjectivePhase phase;
        t::Strategy strategy;
        std::uint32_t site;
        std::uint64_t remaining;
        bool defuser;
    };
    const std::array cases{
        Case{t::ObjectivePhase::Attack, t::Strategy::AttackSplit, 0, 0, false},
        Case{t::ObjectivePhase::Defense, t::Strategy::DefenseSplit, 0, 0, false},
        Case{t::ObjectivePhase::Retake, t::Strategy::RetakeGroup, 0, 0, false},
        Case{t::ObjectivePhase::Escort, t::Strategy::EscortPriority, 0, 0, false},
        Case{t::ObjectivePhase::Defuse, t::Strategy::DefusePriority, 7, 20'000, true},
        Case{t::ObjectivePhase::Planted, t::Strategy::RetakeGroup, 7, 20'000, false},
        Case{t::ObjectivePhase::Dropped, t::Strategy::RetakeGroup, 0, 0, false},
    };
    for (const auto& item : cases) {
        auto input = snapshot();
        input.objective = {item.phase, {}, item.site, item.remaining, true};
        for (std::size_t i = 0; i < input.memberCount; ++i) {
            input.members[i].defuseCapable = item.defuser && i < 2;
        }
        t::TeamDirector director;
        const auto decision = director.update(input);
        assert(decision.accepted && decision.strategy == item.strategy);
        assert(decision.shared.objective.phase == item.phase);
    }
}

void testDefuserDeathAndBombDropReassign() {
    auto input = snapshot();
    input.objective = {t::ObjectivePhase::Planted, {}, 3, 15'000, true};
    input.members[0].defuseCapable = true;
    input.members[1].defuseCapable = true;
    t::TeamDirector director;
    const auto initial = director.update(input);
    assert(initial.strategy == t::Strategy::DefusePriority);
    assert(roleCount(initial, t::Role::Defuser) == 1);

    PlayerId defuser{};
    for (std::size_t i = 0; i < initial.shared.assignmentCount; ++i) {
        if (initial.shared.assignments[i].role == t::Role::Defuser) {
            defuser = initial.shared.assignments[i].player;
        }
    }
    assert(defuser.isValid());
    for (std::size_t i = 0; i < input.memberCount; ++i) {
        if (input.members[i].player == defuser) {
            input.members[i].alive = false;
            input.members[i].healthPercent = 0.0F;
        }
    }
    const auto reassigned = director.update(input, {false, false, true, false, false});
    assert(reassigned.accepted && reassigned.reassigned);
    assert(reassigned.reassignment == t::ReassignmentReason::DefuserDied);
    assert(roleCount(reassigned, t::Role::Defuser) == 1);
    for (std::size_t i = 0; i < reassigned.shared.assignmentCount; ++i) {
        assert(reassigned.shared.assignments[i].player != defuser);
    }

    input.objective = {t::ObjectivePhase::Dropped, {}, 0, 0, true};
    const auto dropped = director.update(input, {false, true, false, true, false});
    assert(dropped.strategy == t::Strategy::RetakeGroup);
    assert(dropped.reassignment == t::ReassignmentReason::BombDropped);
}

void testCommunicationBoundaryAndValidation() {
    auto input = snapshot();
    input.observations[0] = {
        input.members[0].player, player(31), {400.0, 20.0, 0.0},
        input.nowMicros, 0.8F, t::SharedObservationSource::DirectVision};
    input.observationCount = 1;
    input.proposals[0] = {input.members[1].agent, t::PlanIntent::Support,
                          42, 3, true};
    input.proposalCount = 1;

    t::TeamDirector director;
    const auto decision = director.update(input);
    assert(decision.accepted && decision.shared.observationCount == 1);
    assert(decision.shared.proposalCount == 1);
    assert(decision.shared.observations[0].position.x == 400.0);
    assert(decision.shared.proposals[0].targetArea == 42);

    input.observations[0].source = t::SharedObservationSource::Unknown;
    const auto rejected = director.update(input);
    assert(!rejected.accepted);
    assert(rejected.reassignment == t::ReassignmentReason::InvalidInput);

    input = snapshot();
    input.members[0].alive = true;
    input.members[0].connected = false;
    assert(!input.valid());
}

} // namespace

int main() {
    testFiveBotsReceiveUniqueAttackRoles();
    testDeterministicInputsAndBoundedTeamSize();
    testObjectiveStrategies();
    testDefuserDeathAndBombDropReassign();
    testCommunicationBoundaryAndValidation();
    return 0;
}
