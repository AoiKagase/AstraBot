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

bool validFamily(ObjectiveFamily family) noexcept {
    return family <= ObjectiveFamily::Escape;
}

bool validState(ObjectiveState state) noexcept {
    return state != ObjectiveState::Unknown && state <= ObjectiveState::Escaped;
}

bool validIntent(PlanIntent intent) noexcept {
    return intent <= PlanIntent::Defuse;
}

bool sameObjective(const TeamObjective& left,
                  const TeamObjective& right) noexcept {
    return left.phase == right.phase && left.carrier == right.carrier &&
           left.site == right.site &&
           left.remainingMicros == right.remainingMicros &&
           left.known == right.known && left.family == right.family &&
           left.state == right.state && left.vip == right.vip &&
           left.targetCount == right.targetCount &&
           left.routeArea == right.routeArea &&
           left.escapeArea == right.escapeArea &&
           left.remainingPlayers == right.remainingPlayers &&
           left.position.x == right.position.x &&
           left.position.y == right.position.y &&
           left.position.z == right.position.z &&
           left.positionKnown == right.positionKnown &&
           left.vipPosition.x == right.vipPosition.x &&
           left.vipPosition.y == right.vipPosition.y &&
           left.vipPosition.z == right.vipPosition.z &&
           left.vipPositionKnown == right.vipPositionKnown &&
           left.routeAvailable == right.routeAvailable &&
           left.enemyBeliefCount == right.enemyBeliefCount &&
           [&]() noexcept {
               for (std::size_t i = 0; i < left.targetCount; ++i) {
                   const auto& a = left.targets[i];
                   const auto& b = right.targets[i];
                   if (a.id != b.id || a.position.x != b.position.x ||
                       a.position.y != b.position.y || a.position.z != b.position.z ||
                       a.owner != b.owner || a.alive != b.alive ||
                       a.following != b.following || a.completed != b.completed) {
                       return false;
                   }
               }
               return true;
           }();
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

bool sameObjectiveAssignment(const ObjectiveAssignment& left,
                             const ObjectiveAssignment& right) noexcept {
    return left.map == right.map && left.round == right.round &&
           left.player == right.player && left.agent == right.agent &&
           left.kind == right.kind && left.playerTarget == right.playerTarget &&
           left.target == right.target && left.routeArea == right.routeArea &&
           left.exclusive == right.exclusive;
}

bool sameExclusiveTask(const ObjectiveAssignment& left,
                       const ObjectiveAssignment& right) noexcept {
    if (!left.exclusive || !right.exclusive || left.kind != right.kind) {
        return false;
    }
    return left.playerTarget == right.playerTarget && left.target == right.target;
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

std::size_t memberIndex(const TeamSnapshot& snapshot, PlayerId player) noexcept {
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        if (snapshot.members[i].player == player) return i;
    }
    return snapshot.memberCount;
}

bool activeMember(const TeamMemberSnapshot& member) noexcept {
    return member.connected && member.alive;
}

bool objectiveTargetActive(const ObjectiveTarget& target) noexcept {
    return target.alive && !target.completed;
}

bool optionalPlayerId(PlayerId player) noexcept {
    return (!player.slot && !player.generation.isValid()) || player.isValid();
}

bool optionalObjectiveTargetId(ObjectiveTargetId target) noexcept {
    return (target.value == 0 && !target.generation.isValid()) ||
           target.isValid();
}

int objectiveScore(ObjectiveAssignmentKind kind,
                   const TeamMemberSnapshot& member,
                   const TeamSnapshot& snapshot) noexcept {
    int score = static_cast<int>(std::lround(member.healthPercent)) +
        (std::min)(member.weaponValue, 200) / 2;
    if (kind == ObjectiveAssignmentKind::Defuser) {
        // The objective position is an observed value-level hint. A missing
        // position never disqualifies a candidate; it only removes the
        // distance tie-break from the score.
        const auto& objective = snapshot.objective;
        if (objective.positionKnown) {
            const auto distance = frontlineDistance(
                member, objective.position.x, objective.position.y,
                objective.position.z);
            score += (std::max)(0, 200 - static_cast<int>(std::lround(distance / 64.0)));
        }
    }
    switch (kind) {
    case ObjectiveAssignmentKind::Defuser:
        score += member.defuseCapable ? 600 : -600;
        if (member.plan.intent == PlanIntent::Defuse) score += 200;
        break;
    case ObjectiveAssignmentKind::VipEscort:
    case ObjectiveAssignmentKind::HostageEscort:
    case ObjectiveAssignmentKind::EscapeEscort:
        if (member.personality == Personality::Supportive) score += 160;
        if (member.plan.intent == PlanIntent::Escort) score += 180;
        break;
    case ObjectiveAssignmentKind::VipPathClear:
    case ObjectiveAssignmentKind::RescueRouteGuard:
    case ObjectiveAssignmentKind::EscapeRouteGuard:
        if (member.plan.intent == PlanIntent::Entry ||
            member.plan.intent == PlanIntent::Attack) score += 120;
        break;
    case ObjectiveAssignmentKind::VipGuard:
    case ObjectiveAssignmentKind::RescueCover:
    case ObjectiveAssignmentKind::DefuseCover:
    case ObjectiveAssignmentKind::BombGuard:
        if (member.plan.intent == PlanIntent::Hold ||
            member.plan.intent == PlanIntent::Defend) score += 180;
        if (member.personality == Personality::Cautious) score += 100;
        break;
    case ObjectiveAssignmentKind::HostageRescuer:
        if (member.plan.intent == PlanIntent::Entry ||
            member.plan.intent == PlanIntent::Attack) score += 120;
        break;
    case ObjectiveAssignmentKind::RetakeEntry:
    case ObjectiveAssignmentKind::BombEscort:
    case ObjectiveAssignmentKind::BombCarrier:
    case ObjectiveAssignmentKind::Vip:
    case ObjectiveAssignmentKind::VipIntercept:
    case ObjectiveAssignmentKind::HostageIntercept:
    case ObjectiveAssignmentKind::EscapeRunner:
    case ObjectiveAssignmentKind::EscapeBlocker:
    case ObjectiveAssignmentKind::EscapeIntercept:
    case ObjectiveAssignmentKind::RouteDefense:
    case ObjectiveAssignmentKind::None:
        break;
    }
    return score;
}

std::size_t bestObjectiveMember(
    const TeamSnapshot& snapshot,
    const std::array<bool, kMaxTeamMembers>& used,
    ObjectiveAssignmentKind kind) noexcept {
    std::size_t best = snapshot.memberCount;
    int bestScore = -1'000'001;
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        const auto& member = snapshot.members[i];
        if (used[i] || !activeMember(member) ||
            (kind == ObjectiveAssignmentKind::Defuser &&
             !member.defuseCapable)) {
            continue;
        }
        const auto score = objectiveScore(kind, member, snapshot);
        const bool earlier = best == snapshot.memberCount ||
            member.player < snapshot.members[best].player;
        if (score > bestScore || (score == bestScore && earlier)) {
            best = i;
            bestScore = score;
        }
    }
    return best;
}

std::size_t nextObjectiveTarget(
    const TeamObjective& objective,
    const std::array<bool, kMaxObjectiveTargets>& used) noexcept {
    std::size_t best = objective.targetCount;
    for (std::size_t i = 0; i < objective.targetCount; ++i) {
        if (used[i] || !objectiveTargetActive(objective.targets[i])) continue;
        if (best == objective.targetCount ||
            objective.targets[i].id < objective.targets[best].id) {
            best = i;
        }
    }
    return best;
}

ObjectiveAssignmentReason objectiveReasonFor(const TeamEvents& events) noexcept {
    if (events.routeInvalidated) return ObjectiveAssignmentReason::RouteInvalidated;
    if (events.vipDied || events.vipEscaped) return ObjectiveAssignmentReason::TargetCompleted;
    if (events.hostageOwnershipChanged || events.hostageStateChanged) {
        return ObjectiveAssignmentReason::TargetChanged;
    }
    if (events.botDied || events.defuserDied) return ObjectiveAssignmentReason::OwnerDied;
    if (events.botDisconnected) return ObjectiveAssignmentReason::OwnerDisconnected;
    if (events.objectiveTransition || events.bombDropped || events.escapeProgressChanged) {
        return ObjectiveAssignmentReason::ObjectiveTransition;
    }
    return ObjectiveAssignmentReason::Replacement;
}

void appendObjectiveAssignment(
    const TeamSnapshot& snapshot, SharedTeamState& state, std::size_t index,
    ObjectiveAssignmentKind kind, PlayerId playerTarget,
    ObjectiveTargetId target, std::uint32_t routeArea, bool exclusive,
    std::uint64_t assignmentGeneration,
    ObjectiveAssignmentReason reason) noexcept {
    if (index >= snapshot.memberCount ||
        state.objectiveAssignmentCount >= kMaxObjectiveAssignments) return;
    const auto& member = snapshot.members[index];
    auto& assignment =
        state.objectiveAssignments[state.objectiveAssignmentCount++];
    assignment = {snapshot.map, snapshot.round, member.player, member.agent, kind,
                  playerTarget, target, routeArea, snapshot.nowMicros,
                  assignmentGeneration, reason, exclusive};
}

void preserveObjectiveAssignmentHistory(
    SharedTeamState& next, const SharedTeamState& previous) noexcept {
    for (std::size_t i = 0; i < next.objectiveAssignmentCount; ++i) {
        for (std::size_t j = 0; j < previous.objectiveAssignmentCount; ++j) {
            if (!sameObjectiveAssignment(next.objectiveAssignments[i],
                                         previous.objectiveAssignments[j])) {
                continue;
            }
            next.objectiveAssignments[i].assignedMicros =
                previous.objectiveAssignments[j].assignedMicros;
            next.objectiveAssignments[i].assignmentGeneration =
                previous.objectiveAssignments[j].assignmentGeneration;
            next.objectiveAssignments[i].reason =
                previous.objectiveAssignments[j].reason;
            break;
        }
    }
}

} // namespace

bool ObjectiveTarget::valid() const noexcept {
    return id.isValid() && perception::finite(position) &&
           (!following || owner.isValid());
}

bool TeamObjective::valid() const noexcept {
    if (!known || !validPhase(phase) || !validFamily(family) ||
        !validState(state) || targetCount > targets.size() ||
        (positionKnown && !perception::finite(position)) ||
        (vipPositionKnown && !perception::finite(vipPosition))) {
        return false;
    }
    for (std::size_t i = 0; i < targetCount; ++i) {
        if (!targets[i].valid()) return false;
        for (std::size_t j = 0; j < i; ++j) {
            if (targets[i].id == targets[j].id) return false;
        }
    }
    switch (family) {
    case ObjectiveFamily::BombDefusal:
        switch (phase) {
        case ObjectivePhase::None:
        case ObjectivePhase::Attack:
        case ObjectivePhase::Defense:
        case ObjectivePhase::Retake:
        case ObjectivePhase::Escort:
            return !carrier.isValid() && remainingMicros == 0 &&
                   targetCount == 0;
        case ObjectivePhase::Defuse:
        case ObjectivePhase::Planted:
            return site != 0 && remainingMicros != 0 && !carrier.isValid() &&
                   targetCount == 0;
        case ObjectivePhase::Carried:
            return carrier.isValid() && remainingMicros == 0 &&
                   targetCount == 0;
        case ObjectivePhase::Dropped:
            return !carrier.isValid() && targetCount == 0;
        }
        break;
    case ObjectiveFamily::VipEscort:
        return vip.isValid() && targetCount == 0;
    case ObjectiveFamily::HostageRescue:
        return targetCount != 0;
    case ObjectiveFamily::Escape:
        return state == ObjectiveState::Completed ||
               state == ObjectiveState::Failed || routeArea != 0;
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
           botDisconnected || mapChanged || roundChanged ||
           playerGenerationChanged || vipDied || vipEscaped ||
           hostageOwnershipChanged || hostageStateChanged ||
           escapeProgressChanged || routeInvalidated;
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
    if (objective.family == ObjectiveFamily::BombDefusal &&
        objective.phase == ObjectivePhase::Carried &&
        !containsMember(*this, objective.carrier)) {
        return false;
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

bool ObjectiveAssignment::valid(
    MapGeneration expectedMap, perception::RoundGeneration expectedRound,
    std::uint64_t nowMicros) const noexcept {
    return map == expectedMap && round == expectedRound && player.isValid() &&
           agent.isValid() && kind != ObjectiveAssignmentKind::None &&
           assignedMicros <= nowMicros && assignmentGeneration != 0 &&
           optionalPlayerId(playerTarget) && optionalObjectiveTargetId(target);
}

bool SharedTeamState::valid(std::uint64_t nowMicros) const noexcept {
    if (!map.isValid() || !round.isValid() || !tick.isValid() || !objective.valid() ||
        observationCount > observations.size() ||
        proposalCount > proposals.size() || assignmentCount > assignments.size() ||
        objectiveAssignmentCount > objectiveAssignments.size()) {
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
    for (std::size_t i = 0; i < objectiveAssignmentCount; ++i) {
        const auto& assignment = objectiveAssignments[i];
        if (!assignment.valid(map, round, nowMicros)) return false;
        for (std::size_t j = 0; j < i; ++j) {
            const auto& previous = objectiveAssignments[j];
            if (assignment.player == previous.player ||
                assignment.agent == previous.agent ||
                sameExclusiveTask(assignment, previous)) {
                return false;
            }
        }
    }
    return true;
}

Strategy TeamDirector::chooseStrategy(const TeamSnapshot& snapshot) noexcept {
    switch (snapshot.objective.family) {
    case ObjectiveFamily::VipEscort:
    case ObjectiveFamily::HostageRescue:
        return Strategy::EscortPriority;
    case ObjectiveFamily::Escape:
        return snapshot.team == perception::Team::CounterTerrorist
            ? Strategy::DefenseSplit : Strategy::EscortPriority;
    case ObjectiveFamily::BombDefusal:
        break;
    }
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

void TeamDirector::buildObjectiveAssignments(
    const TeamSnapshot& snapshot, SharedTeamState& state,
    std::uint64_t assignmentGeneration,
    ObjectiveAssignmentReason reason) noexcept {
    const auto& objective = snapshot.objective;
    std::array<bool, kMaxTeamMembers> usedMembers{};
    const auto assign = [&](std::size_t index, ObjectiveAssignmentKind kind,
                            PlayerId playerTarget = {},
                            ObjectiveTargetId target = {},
                            std::uint32_t routeArea = 0,
                            bool exclusive = false) noexcept {
        appendObjectiveAssignment(snapshot, state, index, kind, playerTarget,
                                  target, routeArea, exclusive,
                                  assignmentGeneration, reason);
        if (index < usedMembers.size()) usedMembers[index] = true;
    };

    if (objective.family == ObjectiveFamily::BombDefusal) {
        const bool defusePhase = objective.phase == ObjectivePhase::Defuse ||
            objective.phase == ObjectivePhase::Planted ||
            objective.state == ObjectiveState::Planted;
        if (defusePhase) {
            const auto defuser = bestObjectiveMember(
                snapshot, usedMembers, ObjectiveAssignmentKind::Defuser);
            if (defuser != snapshot.memberCount) {
                assign(defuser, ObjectiveAssignmentKind::Defuser, {}, {},
                       objective.site, true);
            }
            while (state.objectiveAssignmentCount < kMaxObjectiveAssignments) {
                const auto kind = state.objectiveAssignmentCount == 1
                    ? ObjectiveAssignmentKind::DefuseCover
                    : ObjectiveAssignmentKind::RetakeEntry;
                const auto candidate =
                    bestObjectiveMember(snapshot, usedMembers, kind);
                if (candidate == snapshot.memberCount) break;
                assign(candidate, kind, {}, {}, objective.site, false);
            }
        } else if (objective.phase == ObjectivePhase::Carried &&
                   objective.carrier.isValid()) {
            const auto carrier = memberIndex(snapshot, objective.carrier);
            if (carrier != snapshot.memberCount && activeMember(snapshot.members[carrier])) {
                assign(carrier, ObjectiveAssignmentKind::BombCarrier, {}, {},
                       objective.site, true);
            }
            while (state.objectiveAssignmentCount < kMaxObjectiveAssignments) {
                const auto candidate = bestObjectiveMember(
                    snapshot, usedMembers, ObjectiveAssignmentKind::BombEscort);
                if (candidate == snapshot.memberCount) break;
                assign(candidate, ObjectiveAssignmentKind::BombEscort, {}, {},
                       objective.site, false);
            }
        } else if (objective.phase == ObjectivePhase::Dropped) {
            bool guardAssigned = false;
            while (state.objectiveAssignmentCount < kMaxObjectiveAssignments) {
                const auto kind = guardAssigned
                    ? ObjectiveAssignmentKind::RetakeEntry
                    : ObjectiveAssignmentKind::BombGuard;
                const auto candidate = bestObjectiveMember(snapshot, usedMembers, kind);
                if (candidate == snapshot.memberCount) break;
                assign(candidate, kind, {}, {}, objective.site, false);
                guardAssigned = true;
            }
        }
        return;
    }

    if (objective.family == ObjectiveFamily::VipEscort) {
        const bool complete = objective.state == ObjectiveState::Completed ||
            objective.state == ObjectiveState::Failed ||
            objective.state == ObjectiveState::Escaped;
        if (complete) return;
        const auto vip = memberIndex(snapshot, objective.vip);
        if (snapshot.team == perception::Team::CounterTerrorist &&
            vip != snapshot.memberCount && activeMember(snapshot.members[vip])) {
            assign(vip, ObjectiveAssignmentKind::Vip, objective.vip, {},
                   objective.escapeArea, true);
            const std::array<ObjectiveAssignmentKind, 3> escortRoles{
                ObjectiveAssignmentKind::VipEscort,
                ObjectiveAssignmentKind::VipGuard,
                ObjectiveAssignmentKind::VipPathClear};
            for (const auto kind : escortRoles) {
                const auto candidate = bestObjectiveMember(snapshot, usedMembers, kind);
                if (candidate == snapshot.memberCount) break;
                assign(candidate, kind, objective.vip, {}, objective.escapeArea,
                       false);
            }
            while (state.objectiveAssignmentCount < kMaxObjectiveAssignments) {
                const auto candidate = bestObjectiveMember(
                    snapshot, usedMembers, ObjectiveAssignmentKind::VipGuard);
                if (candidate == snapshot.memberCount) break;
                assign(candidate, ObjectiveAssignmentKind::VipGuard, objective.vip,
                       {}, objective.escapeArea, false);
            }
        } else if (snapshot.team == perception::Team::Terrorist &&
                   objective.vip.isValid()) {
            while (state.objectiveAssignmentCount < kMaxObjectiveAssignments) {
                const auto candidate = bestObjectiveMember(
                    snapshot, usedMembers, ObjectiveAssignmentKind::VipIntercept);
                if (candidate == snapshot.memberCount) break;
                assign(candidate, ObjectiveAssignmentKind::VipIntercept,
                       objective.vip, {}, objective.routeArea, false);
            }
        }
        return;
    }

    if (objective.family == ObjectiveFamily::HostageRescue) {
        std::array<bool, kMaxObjectiveTargets> usedTargets{};
        const bool counterTerrorist =
            snapshot.team == perception::Team::CounterTerrorist;
        while (state.objectiveAssignmentCount < kMaxObjectiveAssignments) {
            const auto targetIndex = nextObjectiveTarget(objective, usedTargets);
            if (targetIndex == objective.targetCount) break;
            const auto& target = objective.targets[targetIndex];
            auto owner = memberIndex(snapshot, target.owner);
            if (!target.following || owner == snapshot.memberCount ||
                !activeMember(snapshot.members[owner]) || usedMembers[owner]) {
                owner = bestObjectiveMember(
                    snapshot, usedMembers,
                    counterTerrorist ? ObjectiveAssignmentKind::HostageRescuer
                                     : ObjectiveAssignmentKind::HostageIntercept);
            }
            if (owner == snapshot.memberCount) break;
            assign(owner,
                   target.following && target.owner.isValid()
                       ? ObjectiveAssignmentKind::HostageEscort
                       : (counterTerrorist ? ObjectiveAssignmentKind::HostageRescuer
                                           : ObjectiveAssignmentKind::HostageIntercept),
                   {}, target.id, objective.routeArea, true);
            usedTargets[targetIndex] = true;
        }
        std::size_t firstTarget = objective.targetCount;
        for (std::size_t i = 0; i < objective.targetCount; ++i) {
            if (!objectiveTargetActive(objective.targets[i])) continue;
            if (firstTarget == objective.targetCount ||
                objective.targets[i].id < objective.targets[firstTarget].id) {
                firstTarget = i;
            }
        }
        while (state.objectiveAssignmentCount < kMaxObjectiveAssignments) {
            const auto candidate = bestObjectiveMember(
                snapshot, usedMembers,
                counterTerrorist ? ObjectiveAssignmentKind::RescueCover
                                 : ObjectiveAssignmentKind::HostageIntercept);
            if (candidate == snapshot.memberCount) break;
            const auto target = firstTarget == objective.targetCount
                ? ObjectiveTargetId{} : objective.targets[firstTarget].id;
            const auto kind = counterTerrorist
                ? (state.objectiveAssignmentCount == 0
                       ? ObjectiveAssignmentKind::RescueCover
                       : ObjectiveAssignmentKind::RescueRouteGuard)
                : ObjectiveAssignmentKind::HostageIntercept;
            assign(candidate, kind, {}, target, objective.routeArea, false);
        }
        return;
    }

    if (objective.family == ObjectiveFamily::Escape &&
        objective.state != ObjectiveState::Completed &&
        objective.state != ObjectiveState::Failed &&
        objective.state != ObjectiveState::Escaped &&
        objective.routeAvailable) {
        const auto route = objective.escapeArea != 0
            ? objective.escapeArea : objective.routeArea;
        const bool terrorist = snapshot.team == perception::Team::Terrorist;
        const std::array<ObjectiveAssignmentKind, 3> roles = terrorist
            ? std::array<ObjectiveAssignmentKind, 3>{
                  ObjectiveAssignmentKind::EscapeRunner,
                  ObjectiveAssignmentKind::EscapeEscort,
                  ObjectiveAssignmentKind::EscapeRouteGuard}
            : std::array<ObjectiveAssignmentKind, 3>{
                  ObjectiveAssignmentKind::EscapeBlocker,
                  ObjectiveAssignmentKind::EscapeIntercept,
                  ObjectiveAssignmentKind::RouteDefense};
        for (const auto kind : roles) {
            const auto candidate = bestObjectiveMember(snapshot, usedMembers, kind);
            if (candidate == snapshot.memberCount) break;
            assign(candidate, kind, {}, {}, route, kind == roles[0]);
        }
        while (state.objectiveAssignmentCount < kMaxObjectiveAssignments) {
            const auto kind = terrorist ? ObjectiveAssignmentKind::EscapeRouteGuard
                                        : ObjectiveAssignmentKind::RouteDefense;
            const auto candidate = bestObjectiveMember(snapshot, usedMembers, kind);
            if (candidate == snapshot.memberCount) break;
            assign(candidate, kind, {}, {}, route, false);
        }
    }
}

ReassignmentReason TeamDirector::reasonFor(const TeamEvents& events) noexcept {
    if (events.mapChanged) return ReassignmentReason::MapChanged;
    if (events.roundChanged) return ReassignmentReason::RoundChanged;
    if (events.vipDied) return ReassignmentReason::VipDied;
    if (events.vipEscaped) return ReassignmentReason::VipEscaped;
    if (events.hostageOwnershipChanged || events.hostageStateChanged) {
        return ReassignmentReason::HostageStateChanged;
    }
    if (events.escapeProgressChanged) return ReassignmentReason::EscapeProgressChanged;
    if (events.routeInvalidated) return ReassignmentReason::RouteInvalidated;
    if (events.playerGenerationChanged) return ReassignmentReason::ObjectiveInvalidated;
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
    next.map = snapshot.map;
    next.round = snapshot.round;
    next.tick = snapshot.tick;
    next.objective = snapshot.objective;
    next.observationCount = snapshot.observationCount;
    next.proposalCount = snapshot.proposalCount;
    for (std::size_t i = 0; i < next.observationCount; ++i) {
        next.observations[i] = snapshot.observations[i];
    }
    for (std::size_t i = 0; i < next.proposalCount; ++i) {
        next.proposals[i] = snapshot.proposals[i];
    }

    const bool mapChanged = active_ && state_.map != snapshot.map;
    const bool roundChanged = active_ && state_.round != snapshot.round;
    const bool objectiveChanged = active_ &&
        !sameObjective(state_.objective, snapshot.objective);
    auto assignmentReason = !active_ ? ObjectiveAssignmentReason::Initial
                                     : objectiveReasonFor(events);
    if (active_ && objectiveChanged && !events.any()) {
        assignmentReason = ObjectiveAssignmentReason::TargetChanged;
    }
    buildObjectiveAssignments(snapshot, next, generation_ + 1,
                              assignmentReason);
    if (active_) preserveObjectiveAssignmentHistory(next, state_);

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
        !active_ || state_.map != next.map || state_.round != next.round ||
        !sameObjective(state_.objective, next.objective) ||
        state_.observationCount != next.observationCount ||
        state_.proposalCount != next.proposalCount || assignmentsChanged ||
        state_.objectiveAssignmentCount != next.objectiveAssignmentCount ||
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
            for (std::size_t i = 0; i < next.objectiveAssignmentCount; ++i) {
                if (!sameObjectiveAssignment(
                        state_.objectiveAssignments[i],
                        next.objectiveAssignments[i])) {
                    return true;
                }
            }
            return false;
        }();
    const bool strategyChanged = !active_ || strategy_ != selectedStrategy;

    if (sharedChanged || strategyChanged) ++generation_;
    if (!active_) {
        result.reassignment = ReassignmentReason::Initial;
    } else if (mapChanged) {
        result.reassignment = ReassignmentReason::MapChanged;
    } else if (roundChanged) {
        result.reassignment = ReassignmentReason::RoundChanged;
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
    case ReassignmentReason::MapChanged: return "map_changed";
    case ReassignmentReason::RoundChanged: return "round_changed";
    case ReassignmentReason::ObjectiveInvalidated: return "objective_invalidated";
    case ReassignmentReason::VipDied: return "vip_died";
    case ReassignmentReason::VipEscaped: return "vip_escaped";
    case ReassignmentReason::HostageStateChanged: return "hostage_state_changed";
    case ReassignmentReason::EscapeProgressChanged: return "escape_progress_changed";
    case ReassignmentReason::RouteInvalidated: return "route_invalidated";
    case ReassignmentReason::None: break;
    }
    return "none";
}

const char* objectiveFamilyName(ObjectiveFamily family) noexcept {
    switch (family) {
    case ObjectiveFamily::BombDefusal: return "bomb_defusal";
    case ObjectiveFamily::VipEscort: return "vip_escort";
    case ObjectiveFamily::HostageRescue: return "hostage_rescue";
    case ObjectiveFamily::Escape: return "escape";
    }
    return "unknown";
}

const char* objectiveStateName(ObjectiveState state) noexcept {
    switch (state) {
    case ObjectiveState::Unknown: return "unknown";
    case ObjectiveState::Active: return "active";
    case ObjectiveState::Carried: return "carried";
    case ObjectiveState::Dropped: return "dropped";
    case ObjectiveState::Planted: return "planted";
    case ObjectiveState::Completed: return "completed";
    case ObjectiveState::Failed: return "failed";
    case ObjectiveState::Escaped: return "escaped";
    }
    return "unknown";
}

const char* objectiveAssignmentName(ObjectiveAssignmentKind kind) noexcept {
    switch (kind) {
    case ObjectiveAssignmentKind::None: return "none";
    case ObjectiveAssignmentKind::Defuser: return "defuser";
    case ObjectiveAssignmentKind::DefuseCover: return "defuse_cover";
    case ObjectiveAssignmentKind::RetakeEntry: return "retake_entry";
    case ObjectiveAssignmentKind::BombGuard: return "bomb_guard";
    case ObjectiveAssignmentKind::BombCarrier: return "bomb_carrier";
    case ObjectiveAssignmentKind::BombEscort: return "bomb_escort";
    case ObjectiveAssignmentKind::Vip: return "vip";
    case ObjectiveAssignmentKind::VipEscort: return "vip_escort";
    case ObjectiveAssignmentKind::VipGuard: return "vip_guard";
    case ObjectiveAssignmentKind::VipPathClear: return "vip_path_clear";
    case ObjectiveAssignmentKind::VipIntercept: return "vip_intercept";
    case ObjectiveAssignmentKind::HostageRescuer: return "hostage_rescuer";
    case ObjectiveAssignmentKind::HostageEscort: return "hostage_escort";
    case ObjectiveAssignmentKind::RescueCover: return "rescue_cover";
    case ObjectiveAssignmentKind::RescueRouteGuard: return "rescue_route_guard";
    case ObjectiveAssignmentKind::HostageIntercept: return "hostage_intercept";
    case ObjectiveAssignmentKind::EscapeRunner: return "escape_runner";
    case ObjectiveAssignmentKind::EscapeEscort: return "escape_escort";
    case ObjectiveAssignmentKind::EscapeRouteGuard: return "escape_route_guard";
    case ObjectiveAssignmentKind::EscapeBlocker: return "escape_blocker";
    case ObjectiveAssignmentKind::EscapeIntercept: return "escape_intercept";
    case ObjectiveAssignmentKind::RouteDefense: return "route_defense";
    }
    return "unknown";
}

const char* objectiveAssignmentReasonName(
    ObjectiveAssignmentReason reason) noexcept {
    switch (reason) {
    case ObjectiveAssignmentReason::None: return "none";
    case ObjectiveAssignmentReason::Initial: return "initial";
    case ObjectiveAssignmentReason::BestCandidate: return "best_candidate";
    case ObjectiveAssignmentReason::Replacement: return "replacement";
    case ObjectiveAssignmentReason::ObjectiveTransition: return "objective_transition";
    case ObjectiveAssignmentReason::OwnerDied: return "owner_died";
    case ObjectiveAssignmentReason::OwnerDisconnected: return "owner_disconnected";
    case ObjectiveAssignmentReason::TargetChanged: return "target_changed";
    case ObjectiveAssignmentReason::TargetCompleted: return "target_completed";
    case ObjectiveAssignmentReason::RouteInvalidated: return "route_invalidated";
    case ObjectiveAssignmentReason::TacticalIntentMismatch: return "tactical_intent_mismatch";
    }
    return "unknown";
}

} // namespace astrabot::core::team
