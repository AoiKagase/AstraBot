// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/team_director.hpp"

#include <algorithm>
#include <cmath>

namespace astrabot::core::team {
namespace {

bool validPhase(ObjectivePhase phase) noexcept {
    return phase <= ObjectivePhase::Planted;
}

bool validIntent(PlanIntent intent) noexcept {
    return intent <= PlanIntent::Defuse;
}

bool sameObjective(const TeamObjective& left,
                  const TeamObjective& right) noexcept {
    return left.phase == right.phase && left.carrier == right.carrier &&
           left.site == right.site &&
           left.remainingMicros == right.remainingMicros &&
           left.known == right.known;
}

bool sameObservation(const SharedObservation& left,
                     const SharedObservation& right) noexcept {
    return left.observer == right.observer && left.subject == right.subject &&
           left.position.x == right.position.x &&
           left.position.y == right.position.y &&
           left.position.z == right.position.z &&
           left.observedMicros == right.observedMicros &&
           left.confidence == right.confidence && left.source == right.source;
}

bool sameProposal(const TacticalProposal& left,
                  const TacticalProposal& right) noexcept {
    return left.proposer == right.proposer && left.intent == right.intent &&
           left.targetArea == right.targetArea &&
           left.priority == right.priority &&
           left.basedOnSharedEvidence == right.basedOnSharedEvidence;
}

bool sameAssignment(const RoleAssignment& left,
                    const RoleAssignment& right) noexcept {
    return left.player == right.player && left.agent == right.agent &&
           left.role == right.role;
}

bool containsMember(const TeamSnapshot& snapshot, PlayerId player) noexcept {
    const auto count = (std::min)(snapshot.memberCount, snapshot.members.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (snapshot.members[i].player == player) return true;
    }
    return false;
}

bool containsAgent(const TeamSnapshot& snapshot, BotAgentId agent) noexcept {
    const auto count = (std::min)(snapshot.memberCount, snapshot.members.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (snapshot.members[i].agent == agent) return true;
    }
    return false;
}

double frontlineDistance(const TeamMemberSnapshot& member,
                         double centerX, double centerY,
                         double centerZ) noexcept {
    const auto dx = member.position.x - centerX;
    const auto dy = member.position.y - centerY;
    const auto dz = member.position.z - centerZ;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

bool TeamObjective::valid() const noexcept {
    if (!known || !validPhase(phase)) return false;
    switch (phase) {
    case ObjectivePhase::None:
    case ObjectivePhase::Attack:
    case ObjectivePhase::Defense:
    case ObjectivePhase::Retake:
    case ObjectivePhase::Escort:
        return !carrier.isValid() && remainingMicros == 0;
    case ObjectivePhase::Defuse:
    case ObjectivePhase::Planted:
        return site != 0 && remainingMicros != 0 && !carrier.isValid();
    case ObjectivePhase::Carried:
        return carrier.isValid() && remainingMicros == 0;
    case ObjectivePhase::Dropped:
        return !carrier.isValid();
    }
    return false;
}

bool TacticalPlan::valid() const noexcept {
    return validIntent(intent);
}

bool TeamMemberSnapshot::valid() const noexcept {
    if (!player.isValid() || !agent.isValid() || !std::isfinite(healthPercent) ||
        healthPercent < 0.0F || healthPercent > 100.0F || weaponValue < 0 ||
        !plan.valid() || (!connected && alive)) {
        return false;
    }
    return !connected || perception::finite(position);
}

bool SharedObservation::valid(std::uint64_t nowMicros) const noexcept {
    return observer.isValid() && subject.isValid() &&
           perception::finite(position) && observedMicros <= nowMicros &&
           std::isfinite(confidence) && confidence >= 0.0F &&
           confidence <= 1.0F &&
           (source == SharedObservationSource::DirectVision ||
            source == SharedObservationSource::TeammateReport);
}

bool TacticalProposal::valid() const noexcept {
    return proposer.isValid() && validIntent(intent) &&
           intent != PlanIntent::None && priority != 0 &&
           basedOnSharedEvidence;
}

bool TeamEvents::any() const noexcept {
    return botDied || bombDropped || defuserDied || objectiveTransition ||
           botDisconnected;
}

bool TeamSnapshot::valid() const noexcept {
    if (!map.isValid() || !round.isValid() || !tick.isValid() ||
        !objective.valid() || memberCount == 0 ||
        memberCount > members.size() || observationCount > observations.size() ||
        proposalCount > proposals.size()) {
        return false;
    }

    for (std::size_t i = 0; i < memberCount; ++i) {
        if (!members[i].valid()) return false;
        for (std::size_t j = 0; j < i; ++j) {
            if (members[i].player == members[j].player ||
                members[i].agent == members[j].agent) {
                return false;
            }
        }
    }
    for (std::size_t i = 0; i < observationCount; ++i) {
        if (!observations[i].valid(nowMicros) ||
            !containsMember(*this, observations[i].observer)) {
            return false;
        }
    }
    for (std::size_t i = 0; i < proposalCount; ++i) {
        if (!proposals[i].valid() ||
            !containsAgent(*this, proposals[i].proposer)) {
            return false;
        }
    }
    return true;
}

bool RoleAssignment::valid() const noexcept {
    return player.isValid() && agent.isValid() && role != Role::None;
}

bool SharedTeamState::valid(std::uint64_t nowMicros) const noexcept {
    if (!objective.valid() || observationCount > observations.size() ||
        proposalCount > proposals.size() || assignmentCount > assignments.size()) {
        return false;
    }
    for (std::size_t i = 0; i < observationCount; ++i) {
        if (!observations[i].valid(nowMicros)) return false;
    }
    for (std::size_t i = 0; i < proposalCount; ++i) {
        if (!proposals[i].valid()) return false;
    }
    for (std::size_t i = 0; i < assignmentCount; ++i) {
        if (!assignments[i].valid()) return false;
        for (std::size_t j = 0; j < i; ++j) {
            if (assignments[i].player == assignments[j].player ||
                assignments[i].agent == assignments[j].agent) {
                return false;
            }
        }
    }
    return true;
}

Strategy TeamDirector::chooseStrategy(const TeamSnapshot& snapshot) noexcept {
    switch (snapshot.objective.phase) {
    case ObjectivePhase::Defuse:
    case ObjectivePhase::Planted: {
        for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
            const auto& member = snapshot.members[i];
            if (member.connected && member.alive && member.defuseCapable) {
                return Strategy::DefusePriority;
            }
        }
        return Strategy::RetakeGroup;
    }
    case ObjectivePhase::Retake:
    case ObjectivePhase::Dropped:
        return Strategy::RetakeGroup;
    case ObjectivePhase::Escort:
        return Strategy::EscortPriority;
    case ObjectivePhase::Defense:
        return Strategy::DefenseSplit;
    case ObjectivePhase::Attack:
    case ObjectivePhase::Carried:
        return Strategy::AttackSplit;
    case ObjectivePhase::None:
        break;
    }

    std::size_t defensive = 0;
    std::size_t offensive = 0;
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        if (!snapshot.members[i].connected || !snapshot.members[i].alive) continue;
        switch (snapshot.members[i].plan.intent) {
        case PlanIntent::Defend:
        case PlanIntent::Hold:
            ++defensive;
            break;
        case PlanIntent::Attack:
        case PlanIntent::Entry:
        case PlanIntent::Lurk:
            ++offensive;
            break;
        default:
            break;
        }
    }
    return defensive > offensive ? Strategy::DefenseSplit : Strategy::AttackSplit;
}

Role TeamDirector::roleForSlot(Strategy strategy, std::size_t slot) noexcept {
    constexpr std::array<Role, 9> attack{
        Role::Entry, Role::Trade, Role::Support, Role::Lurk,
        Role::FlankWatch, Role::Rotator, Role::Anchor, Role::Defuser,
        Role::Escort};
    constexpr std::array<Role, 9> defense{
        Role::Anchor, Role::FlankWatch, Role::Rotator, Role::Support,
        Role::Lurk, Role::Entry, Role::Trade, Role::Defuser, Role::Escort};
    constexpr std::array<Role, 9> retake{
        Role::Entry, Role::Trade, Role::Support, Role::Rotator,
        Role::Defuser, Role::FlankWatch, Role::Anchor, Role::Lurk,
        Role::Escort};
    constexpr std::array<Role, 9> escort{
        Role::Escort, Role::Entry, Role::Trade, Role::Support,
        Role::FlankWatch, Role::Anchor, Role::Rotator, Role::Defuser,
        Role::Lurk};
    constexpr std::array<Role, 9> defuse{
        Role::Defuser, Role::Entry, Role::Trade, Role::Support,
        Role::FlankWatch, Role::Rotator, Role::Anchor, Role::Lurk,
        Role::Escort};

    const auto* roles = &attack;
    switch (strategy) {
    case Strategy::DefenseSplit:
        roles = &defense;
        break;
    case Strategy::RetakeGroup:
        roles = &retake;
        break;
    case Strategy::EscortPriority:
        roles = &escort;
        break;
    case Strategy::DefusePriority:
        roles = &defuse;
        break;
    case Strategy::AttackSplit:
    case Strategy::None:
        break;
    }
    return slot < roles->size() ? (*roles)[slot] : Role::Support;
}

int TeamDirector::roleScore(Role role, const TeamMemberSnapshot& member,
                            const TeamSnapshot& snapshot,
                            double frontline) noexcept {
    const auto health = static_cast<int>(std::lround(member.healthPercent));
    const auto weapon = (std::min)(member.weaponValue, 200);
    const auto position = (std::min)(static_cast<int>(std::lround(frontline / 64.0)),
                                     100);
    int score = health + weapon / 2;

    switch (role) {
    case Role::Entry:
        score += position * 2;
        if (member.personality == Personality::Aggressive) score += 140;
        if (member.plan.intent == PlanIntent::Entry ||
            member.plan.intent == PlanIntent::Attack) score += 180;
        break;
    case Role::Trade:
        score += position;
        if (member.plan.intent == PlanIntent::Support ||
            member.plan.intent == PlanIntent::Entry) score += 100;
        if (member.personality == Personality::Aggressive) score += 30;
        break;
    case Role::Support:
        if (member.personality == Personality::Supportive) score += 160;
        if (member.plan.intent == PlanIntent::Support) score += 180;
        if (member.carryingObjective) score += 50;
        break;
    case Role::Lurk:
        score += position * 2;
        if (member.personality == Personality::Cautious ||
            member.personality == Personality::Aggressive) score += 80;
        if (member.plan.intent == PlanIntent::Lurk) score += 220;
        break;
    case Role::FlankWatch:
        score += position;
        if (member.personality == Personality::Cautious) score += 180;
        if (member.plan.intent == PlanIntent::Hold ||
            member.plan.intent == PlanIntent::Defend) score += 80;
        break;
    case Role::Anchor:
        if (member.personality == Personality::Cautious) score += 120;
        if (member.plan.intent == PlanIntent::Defend ||
            member.plan.intent == PlanIntent::Hold) score += 220;
        score -= position;
        break;
    case Role::Rotator:
        if (member.plan.intent == PlanIntent::Rotate ||
            member.plan.intent == PlanIntent::Retake) score += 220;
        score += health;
        break;
    case Role::Defuser:
        score += member.defuseCapable ? 500 : -500;
        if (member.carryingObjective) score -= 150;
        if (member.plan.intent == PlanIntent::Defuse) score += 200;
        break;
    case Role::Escort:
        score += member.carryingObjective ? 180 : 0;
        if (member.plan.intent == PlanIntent::Escort) score += 220;
        if (member.personality == Personality::Supportive) score += 80;
        break;
    case Role::None:
        return -1'000'000;
    }

    if (snapshot.objective.phase == ObjectivePhase::Attack &&
        member.carryingObjective && role == Role::Support) {
        score += 100;
    }
    return score;
}

ReassignmentReason TeamDirector::reasonFor(const TeamEvents& events) noexcept {
    if (events.defuserDied) return ReassignmentReason::DefuserDied;
    if (events.bombDropped) return ReassignmentReason::BombDropped;
    if (events.objectiveTransition) return ReassignmentReason::ObjectiveTransition;
    if (events.botDisconnected) return ReassignmentReason::BotDisconnected;
    if (events.botDied) return ReassignmentReason::BotDied;
    return ReassignmentReason::InputChanged;
}

TeamDecision TeamDirector::update(const TeamSnapshot& snapshot,
                                  const TeamEvents& events) noexcept {
    TeamDecision result{};
    result.generation = generation_;
    if (!snapshot.valid()) {
        result.reassignment = ReassignmentReason::InvalidInput;
        return result;
    }

    const auto selectedStrategy = chooseStrategy(snapshot);
    SharedTeamState next{};
    next.objective = snapshot.objective;
    next.observationCount = snapshot.observationCount;
    next.proposalCount = snapshot.proposalCount;
    for (std::size_t i = 0; i < next.observationCount; ++i) {
        next.observations[i] = snapshot.observations[i];
    }
    for (std::size_t i = 0; i < next.proposalCount; ++i) {
        next.proposals[i] = snapshot.proposals[i];
    }

    double centerX = 0.0;
    double centerY = 0.0;
    double centerZ = 0.0;
    std::size_t activeMembers = 0;
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        const auto& member = snapshot.members[i];
        if (!member.connected || !member.alive) continue;
        centerX += member.position.x;
        centerY += member.position.y;
        centerZ += member.position.z;
        ++activeMembers;
    }
    if (activeMembers != 0) {
        const auto divisor = static_cast<double>(activeMembers);
        centerX /= divisor;
        centerY /= divisor;
        centerZ /= divisor;
    }

    bool hasDefuseCapableMember = false;
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        const auto& member = snapshot.members[i];
        if (member.connected && member.alive && member.defuseCapable) {
            hasDefuseCapableMember = true;
            break;
        }
    }

    std::array<bool, kMaxTeamMembers> assigned{};
    std::size_t roleSlot = 0;
    for (std::size_t slot = 0; slot < activeMembers; ++slot) {
        auto role = roleForSlot(selectedStrategy, roleSlot++);
        if (role == Role::Defuser && !hasDefuseCapableMember) {
            role = roleForSlot(selectedStrategy, roleSlot++);
        }
        std::size_t best = snapshot.memberCount;
        int bestScore = -1'000'001;
        for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
            const auto& member = snapshot.members[i];
            if (assigned[i] || !member.connected || !member.alive) continue;
            const auto distance = frontlineDistance(member, centerX, centerY, centerZ);
            const auto score = roleScore(role, member, snapshot, distance);
            const bool earlier = best == snapshot.memberCount ||
                member.player.slot < snapshot.members[best].player.slot ||
                (member.player.slot == snapshot.members[best].player.slot &&
                 member.agent.value < snapshot.members[best].agent.value);
            if (score > bestScore || (score == bestScore && earlier)) {
                best = i;
                bestScore = score;
            }
        }
        if (best == snapshot.memberCount) break;
        assigned[best] = true;
        next.assignments[next.assignmentCount++] = {
            snapshot.members[best].player, snapshot.members[best].agent, role};
    }

    const bool assignmentsChanged =
        !active_ || state_.assignmentCount != next.assignmentCount ||
        [&]() noexcept {
            for (std::size_t i = 0; i < next.assignmentCount; ++i) {
                if (!sameAssignment(state_.assignments[i], next.assignments[i])) {
                    return true;
                }
            }
            return false;
        }();
    const bool sharedChanged =
        !active_ || !sameObjective(state_.objective, next.objective) ||
        state_.observationCount != next.observationCount ||
        state_.proposalCount != next.proposalCount || assignmentsChanged ||
        [&]() noexcept {
            for (std::size_t i = 0; i < next.observationCount; ++i) {
                if (!sameObservation(state_.observations[i], next.observations[i])) {
                    return true;
                }
            }
            for (std::size_t i = 0; i < next.proposalCount; ++i) {
                if (!sameProposal(state_.proposals[i], next.proposals[i])) {
                    return true;
                }
            }
            return false;
        }();
    const bool strategyChanged = !active_ || strategy_ != selectedStrategy;

    if (sharedChanged || strategyChanged) ++generation_;
    if (!active_) {
        result.reassignment = ReassignmentReason::Initial;
    } else if (events.any()) {
        result.reassignment = reasonFor(events);
    } else if (sharedChanged || strategyChanged) {
        result.reassignment = ReassignmentReason::InputChanged;
    }

    result.strategy = selectedStrategy;
    result.shared = next;
    result.generation = generation_;
    result.evaluatedMembers = snapshot.memberCount;
    result.accepted = true;
    result.changed = sharedChanged || strategyChanged;
    result.reassigned = !active_ || events.any() || assignmentsChanged || strategyChanged;

    strategy_ = selectedStrategy;
    state_ = next;
    active_ = true;
    return result;
}

void TeamDirector::reset() noexcept {
    strategy_ = Strategy::None;
    state_ = {};
    generation_ = 0;
    active_ = false;
}

const char* roleName(Role role) noexcept {
    switch (role) {
    case Role::Entry: return "entry";
    case Role::Trade: return "trade";
    case Role::Support: return "support";
    case Role::Lurk: return "lurk";
    case Role::FlankWatch: return "flank_watch";
    case Role::Anchor: return "anchor";
    case Role::Rotator: return "rotator";
    case Role::Defuser: return "defuser";
    case Role::Escort: return "escort";
    case Role::None: break;
    }
    return "none";
}

const char* strategyName(Strategy strategy) noexcept {
    switch (strategy) {
    case Strategy::AttackSplit: return "attack_split";
    case Strategy::DefenseSplit: return "defense_split";
    case Strategy::RetakeGroup: return "retake_group";
    case Strategy::EscortPriority: return "escort_priority";
    case Strategy::DefusePriority: return "defuse_priority";
    case Strategy::None: break;
    }
    return "none";
}

const char* objectivePhaseName(ObjectivePhase phase) noexcept {
    switch (phase) {
    case ObjectivePhase::Attack: return "attack";
    case ObjectivePhase::Defense: return "defense";
    case ObjectivePhase::Retake: return "retake";
    case ObjectivePhase::Escort: return "escort";
    case ObjectivePhase::Defuse: return "defuse";
    case ObjectivePhase::Carried: return "carried";
    case ObjectivePhase::Dropped: return "dropped";
    case ObjectivePhase::Planted: return "planted";
    case ObjectivePhase::None: break;
    }
    return "none";
}

const char* planIntentName(PlanIntent intent) noexcept {
    switch (intent) {
    case PlanIntent::Attack: return "attack";
    case PlanIntent::Defend: return "defend";
    case PlanIntent::Rotate: return "rotate";
    case PlanIntent::Retake: return "retake";
    case PlanIntent::Lurk: return "lurk";
    case PlanIntent::Support: return "support";
    case PlanIntent::Entry: return "entry";
    case PlanIntent::Hold: return "hold";
    case PlanIntent::Escort: return "escort";
    case PlanIntent::Defuse: return "defuse";
    case PlanIntent::None: break;
    }
    return "none";
}

const char* reassignmentReasonName(ReassignmentReason reason) noexcept {
    switch (reason) {
    case ReassignmentReason::Initial: return "initial";
    case ReassignmentReason::BotDied: return "bot_died";
    case ReassignmentReason::BombDropped: return "bomb_dropped";
    case ReassignmentReason::DefuserDied: return "defuser_died";
    case ReassignmentReason::ObjectiveTransition: return "objective_transition";
    case ReassignmentReason::BotDisconnected: return "bot_disconnected";
    case ReassignmentReason::InputChanged: return "input_changed";
    case ReassignmentReason::InvalidInput: return "invalid_input";
    case ReassignmentReason::None: break;
    }
    return "none";
}

} // namespace astrabot::core::team
