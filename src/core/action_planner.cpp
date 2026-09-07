// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/action_planner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::core::action {
namespace {

using WeaponClass = combat::WeaponSnapshot::WeaponClass;

constexpr std::int32_t kMaxScore = 100'000;
constexpr double kMaxConfidence = 1.0;

bool finitePoint(const perception::Point& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
}

std::int32_t clampScore(std::int64_t value) noexcept {
    const auto lower = (std::max)(value, -static_cast<std::int64_t>(kMaxScore));
    return static_cast<std::int32_t>(
        (std::min)(lower, static_cast<std::int64_t>(kMaxScore)));
}

std::int32_t roundedScore(double value) noexcept {
    if (!std::isfinite(value)) return 0;
    return clampScore(static_cast<std::int64_t>(std::llround(value)));
}

std::uint64_t saturatingAdd(std::uint64_t left, std::uint64_t right) noexcept {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    return left > maximum - right ? maximum : left + right;
}

int actionPriority(ActionType action) noexcept {
    switch (action) {
    case ActionType::Defuse:
    case ActionType::Plant:
    case ActionType::RecoverBomb:
    case ActionType::RescueHostage:
    case ActionType::EscortHostage:
        return 100;
    case ActionType::Retreat:
        return 90;
    case ActionType::Engage:
        return 80;
    case ActionType::TakeCover:
        return 70;
    case ActionType::Peek:
        return 60;
    case ActionType::Reload:
        return 50;
    case ActionType::AcquireWeapon:
        return 45;
    case ActionType::FollowCurrentIntent:
        return 20;
    case ActionType::Hold:
    case ActionType::NoAction:
        return 0;
    }
    return 0;
}

bool sameIntent(const ActionIntent& left, const ActionIntent& right) noexcept {
    return left.action == right.action && left.target == right.target &&
           left.targetArea == right.targetArea &&
           left.weapon == right.weapon &&
           left.candidateReference == right.candidateReference;
}

std::int32_t classFit(WeaponClass weapon, double range) noexcept {
    if (!std::isfinite(range) || range < 0.0) return 0;
    switch (weapon) {
    case WeaponClass::Rifle:
        return range < 300.0 ? 62 : range > 1'200.0 ? 70 : 82;
    case WeaponClass::SMG:
        return range < 450.0 ? 82 : range > 1'000.0 ? 32 : 65;
    case WeaponClass::Pistol:
        return range < 350.0 ? 48 : 24;
    case WeaponClass::Sniper:
        return range > 700.0 ? 88 : 36;
    case WeaponClass::Unknown:
        return 20;
    }
    return 0;
}

double expectedRange(const ActionPlannerInput& input) noexcept {
    if (input.enemy.known && finitePoint(input.enemy.lastKnownPosition)) {
        // This is a value-level distance between two World Model locations,
        // never a lookup into an engine entity.
        const auto& point = input.enemy.lastKnownPosition;
        const auto& origin = input.currentPosition;
        const double dx = point.x - origin.x;
        const double dy = point.y - origin.y;
        const double dz = point.z - origin.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return 512.0;
}

std::int32_t roleFit(TacticalRole role, WeaponClass weapon) noexcept {
    switch (role) {
    case TacticalRole::Entry:
        return weapon == WeaponClass::Rifle || weapon == WeaponClass::SMG ? 16 : 0;
    case TacticalRole::Support:
        return weapon == WeaponClass::Rifle ? 16 : 5;
    case TacticalRole::Sniper:
        return weapon == WeaponClass::Sniper ? 24 : weapon == WeaponClass::Rifle ? 8 : 0;
    case TacticalRole::Anchor:
        return weapon == WeaponClass::Rifle || weapon == WeaponClass::Sniper ? 14 : 0;
    case TacticalRole::Escort:
        return weapon == WeaponClass::Rifle || weapon == WeaponClass::SMG ? 14 : 0;
    case TacticalRole::Unknown:
        return 0;
    }
    return 0;
}

bool closeThreat(const ActionPlannerInput& input) noexcept {
    return input.enemy.directVision && input.enemy.confidence >= 0.5;
}

bool secondarySolvesImmediateProblem(const ActionPlannerInput& input) noexcept {
    return input.weapon.secondaryUsable && closeThreat(input) &&
           input.weapon.activeClass == WeaponClass::Sniper;
}

std::int32_t riskFrom(double value, double scale) noexcept {
    if (!std::isfinite(value) || value < 0.0) return kMaxScore;
    return roundedScore(value * scale);
}

NavigationCandidate bestLocation(const ActionPlannerInput& input) noexcept {
    NavigationCandidate best{};
    bool found = false;
    const auto limit = (std::min)(input.navigationCount,
                                  input.navigation.size());
    for (std::size_t i = 0; i < limit; ++i) {
        const auto& candidate = input.navigation[i];
        if (!candidate.valid() || !candidate.safe) continue;
        if (!found || candidate.exposure < best.exposure ||
            (candidate.exposure == best.exposure &&
             (candidate.routeCost < best.routeCost ||
              (candidate.routeCost == best.routeCost &&
               candidate.stableId < best.stableId)))) {
            best = candidate;
            found = true;
        }
    }
    return best;
}

bool hasLocation(const NavigationCandidate& candidate) noexcept {
    return candidate.area.isValid() && candidate.safe;
}

struct CandidateDecision final {
    ActionIntent intent{};
    ActionScore score{};
    ActionPrecondition precondition{};
    WeaponPickupEvaluation pickup{};
    WeaponAcquisitionIntent acquisition{};
    bool hasPickup{false};
    bool hasAcquisition{false};
};

bool betterCandidate(const CandidateDecision& left,
                     const CandidateDecision& right) noexcept {
    if (left.score.total != right.score.total) {
        return left.score.total > right.score.total;
    }
    if (actionPriority(left.intent.action) != actionPriority(right.intent.action)) {
        return actionPriority(left.intent.action) > actionPriority(right.intent.action);
    }
    if (left.intent.targetArea != right.intent.targetArea) {
        return left.intent.targetArea < right.intent.targetArea;
    }
    if (left.intent.candidateReference != right.intent.candidateReference) {
        return left.intent.candidateReference < right.intent.candidateReference;
    }
    return left.intent.action < right.intent.action;
}

void setExpiry(ActionIntent& intent, const ActionPlannerInput& input,
               const ActionPlannerSettings& settings,
               std::uint64_t duration = 0) noexcept {
    intent.createdMicros = input.nowMicros;
    const auto selected = duration == 0 ? settings.actionTimeoutMicros : duration;
    intent.expiresMicros = saturatingAdd(input.nowMicros, selected);
}

CandidateDecision basicCandidate(ActionType action, const ActionPlannerInput& input,
                                 const ActionPlannerSettings& settings,
                                 std::int32_t base) noexcept {
    CandidateDecision candidate{};
    candidate.intent.action = action;
    setExpiry(candidate.intent, input, settings);
    candidate.precondition.action = action;
    candidate.precondition.validSituation = true;
    candidate.precondition.alive = input.alive;
    candidate.precondition.knownEnemy = input.enemy.known;
    candidate.precondition.directThreat = input.enemy.directVision;
    candidate.precondition.hasReserveAmmo = input.weapon.reserveAmmo > 0;
    candidate.precondition.canReload = input.weapon.canReload;
    candidate.precondition.objectiveAvailable = input.objective.kind != ObjectiveKind::None;
    candidate.precondition.satisfied = input.alive;
    candidate.score.base = base;
    return candidate;
}

} // namespace

bool ObjectiveSnapshot::valid() const noexcept {
    if (kind == ObjectiveKind::None && bomb != BombState::None) return false;
    if (bomb == BombState::Planted && kind != ObjectiveKind::Bomb) return false;
    return true;
}

bool ObjectiveSnapshot::urgent(std::uint64_t thresholdMicros) const noexcept {
    return bomb == BombState::Planted && remainingMicros <= thresholdMicros;
}

bool EnemyBelief::valid(std::uint64_t nowMicros) const noexcept {
    if (!std::isfinite(confidence) || confidence < 0.0 || confidence > kMaxConfidence) {
        return false;
    }
    if (!known) return !directVision && !target.isValid();
    return target.isValid() && finitePoint(lastKnownPosition) &&
           observedMicros <= nowMicros;
}

bool WeaponState::valid() const noexcept {
    return clipAmmo >= 0 && clipAmmo <= combat::kMaxAmmo && reserveAmmo >= 0 &&
           reserveAmmo <= combat::kMaxAmmo;
}

std::int32_t WeaponState::totalAmmo() const noexcept {
    return clipAmmo > combat::kMaxAmmo - reserveAmmo ? combat::kMaxAmmo
                                                     : clipAmmo + reserveAmmo;
}

bool NavigationCandidate::valid() const noexcept {
    return area.isValid() && finitePoint(position) && std::isfinite(distance) &&
           std::isfinite(routeCost) && std::isfinite(exposure) && distance >= 0.0 &&
           routeCost >= 0.0 && exposure >= 0.0 && exposure <= 1.0;
}

WeaponObservationError DroppedWeaponObservation::validate(
    MapGeneration currentMap, perception::RoundGeneration currentRound,
    TickId currentTick, std::uint64_t nowMicros,
    std::uint64_t maxAgeMicros) const noexcept {
    if (reference == 0) return WeaponObservationError::InvalidReference;
    if (!weapon.isValid()) return WeaponObservationError::InvalidWeapon;
    if (!map.isValid() || !round.isValid() || !observedTick.isValid()) {
        return WeaponObservationError::InvalidIdentity;
    }
    if (map != currentMap) return WeaponObservationError::StaleMap;
    if (round != currentRound) return WeaponObservationError::StaleRound;
    if (currentTick.isBefore(observedTick)) return WeaponObservationError::StaleTick;
    if (observedMicros > nowMicros || nowMicros - observedMicros > maxAgeMicros) {
        return WeaponObservationError::StaleTime;
    }
    if (!finitePoint(position)) return WeaponObservationError::NonFinitePosition;
    if (!area.isValid()) return WeaponObservationError::InvalidArea;
    if (estimatedClipAmmo < 0 || estimatedClipAmmo > combat::kMaxAmmo ||
        estimatedReserveAmmo < 0 || estimatedReserveAmmo > combat::kMaxAmmo) {
        return WeaponObservationError::InvalidAmmo;
    }
    if (!std::isfinite(confidence) || confidence <= 0.0 || confidence > 1.0) {
        return WeaponObservationError::InvalidConfidence;
    }
    return WeaponObservationError::None;
}

bool operator==(const WeaponPickupEvaluation& left,
                const WeaponPickupEvaluation& right) noexcept {
    return left.reference == right.reference &&
           left.candidateWeapon == right.candidateWeapon &&
           left.candidateClass == right.candidateClass &&
           left.upgradeValue == right.upgradeValue && left.ammoNeed == right.ammoNeed &&
           left.tacticalFit == right.tacticalFit && left.roleFit == right.roleFit &&
           left.travelRisk == right.travelRisk && left.exposureRisk == right.exposureRisk &&
           left.objectiveDelay == right.objectiveDelay && left.swapCost == right.swapCost &&
           left.totalUtility == right.totalUtility && left.failure == right.failure &&
           left.actionable == right.actionable;
}

bool ActionPrecondition::validFor(ActionType requested) const noexcept {
    if (action != requested || !validSituation || !alive) return false;
    switch (requested) {
    case ActionType::Reload:
        return canReload && hasReserveAmmo;
    case ActionType::AcquireWeapon:
        return candidateAvailable && safeRoute && enoughTime && !secondarySufficient;
    case ActionType::TakeCover:
    case ActionType::Retreat:
    case ActionType::Peek:
        return safeRoute;
    case ActionType::NoAction:
        return true;
    default:
        return satisfied;
    }
}

void ActionScore::recompute() noexcept {
    total = clampScore(static_cast<std::int64_t>(base) + threat + ammunition +
                       objective + navigation + role + urgency - risk);
}

bool ActionPlannerSettings::valid() const noexcept {
    return maxCandidateAgeMicros != 0 && actionTimeoutMicros != 0 &&
           peekExposureMicros != 0 && objectiveUrgencyMicros != 0 &&
           maxWeaponCandidates <= kMaxWeaponPickupCandidates &&
           maxNavigationCandidates <= kMaxNavigationCandidates && maxEvaluations != 0 &&
           actionSwitchMargin >= 0 && std::isfinite(retreatHealthPercent) &&
           retreatHealthPercent >= 0.0F && retreatHealthPercent <= 100.0F &&
           reloadClipThreshold >= 0 && reloadClipThreshold <= combat::kMaxAmmo;
}

bool ActionPlannerInput::valid(const ActionPlannerSettings& settings) const noexcept {
    if (!settings.valid() || !map.isValid() || !round.isValid() || !tick.isValid() ||
        !player.isValid() || player.slot > perception::kPlayerCapacity || !agent.isValid()) {
        return false;
    }
    if (!std::isfinite(healthPercent) || healthPercent < 0.0F || healthPercent > 100.0F ||
        !finitePoint(currentPosition) ||
        !enemy.valid(nowMicros) || !weapon.valid() || !objective.valid()) {
        return false;
    }
    if (navigationCount > navigation.size() || weaponCount > weapons.size()) return false;
    return true;
}

std::int32_t estimateCurrentWeaponUtility(const ActionPlannerInput& input) noexcept {
    if (!input.weapon.valid()) return 0;
    const auto fit = classFit(input.weapon.activeClass, expectedRange(input));
    const auto ammunition = (std::min)(input.weapon.totalAmmo(), 32) / 2;
    return clampScore(static_cast<std::int64_t>(fit) + ammunition);
}

WeaponPickupEvaluation evaluateWeaponPickup(
    const ActionPlannerInput& input, const WeaponPickupCandidate& candidate,
    const ActionPlannerSettings& settings) noexcept {
    WeaponPickupEvaluation result{};
    result.reference = candidate.observation.reference;
    result.candidateWeapon = candidate.observation.weapon;
    result.candidateClass = candidate.observation.weaponClass;
    const auto observationError = candidate.observation.validate(
        input.map, input.round, input.tick, input.nowMicros, settings.maxCandidateAgeMicros);
    if (observationError != WeaponObservationError::None) {
        result.failure = observationError == WeaponObservationError::StaleMap ||
                                 observationError == WeaponObservationError::StaleRound ||
                                 observationError == WeaponObservationError::StaleTime ||
                                 observationError == WeaponObservationError::StaleTick
                             ? WeaponAcquisitionFailure::StaleObservation
                             : WeaponAcquisitionFailure::InvalidObservation;
        return result;
    }
    if (!candidate.observation.present) {
        result.failure = WeaponAcquisitionFailure::CandidateUnavailable;
        return result;
    }
    if (!std::isfinite(candidate.distance) || !std::isfinite(candidate.routeCost) ||
        !std::isfinite(candidate.routeDanger) || !std::isfinite(candidate.exposure) ||
        candidate.distance < 0.0 || candidate.routeCost < 0.0 || candidate.routeDanger < 0.0 ||
        candidate.routeDanger > 1.0 || candidate.exposure < 0.0 || candidate.exposure > 1.0 ||
        !candidate.safeRoute) {
        result.failure = WeaponAcquisitionFailure::NoRoute;
        return result;
    }
    if (candidate.timeRequiredMicros > input.objective.remainingMicros &&
        input.objective.remainingMicros != 0) {
        result.failure = WeaponAcquisitionFailure::NoTime;
        return result;
    }
    if (input.objective.urgent(settings.objectiveUrgencyMicros)) {
        result.failure = WeaponAcquisitionFailure::ObjectiveUrgent;
        return result;
    }
    if (secondarySolvesImmediateProblem(input)) {
        result.failure = WeaponAcquisitionFailure::SecondarySufficient;
        return result;
    }

    const auto range = expectedRange(input);
    const auto currentFit = classFit(input.weapon.activeClass, range);
    const auto candidateFit = classFit(candidate.observation.weaponClass, range);
    result.upgradeValue = candidateFit - currentFit;
    const auto currentAmmo = input.weapon.totalAmmo();
    result.ammoNeed = currentAmmo <= 8 ? 40 : currentAmmo <= 20 ? 24 : 6;
    if (candidate.observation.estimatedClipAmmo + candidate.observation.estimatedReserveAmmo == 0) {
        result.ammoNeed = (std::max)(0, result.ammoNeed - 15);
    }
    result.tacticalFit = candidateFit >= 70 ? 18 : candidateFit >= 50 ? 8 : 0;
    if (input.enemy.directVision && candidate.observation.weaponClass == WeaponClass::SMG) {
        result.tacticalFit += 8;
    }
    result.roleFit = roleFit(input.role, candidate.observation.weaponClass);
    result.travelRisk = riskFrom(candidate.routeCost / 128.0, 25.0);
    result.exposureRisk = riskFrom(candidate.routeDanger * 0.7 + candidate.exposure * 0.3, 35.0);
    result.objectiveDelay = input.objective.kind == ObjectiveKind::None
                                ? 0
                                : riskFrom(static_cast<double>(candidate.timeRequiredMicros) /
                                               1'000'000.0,
                                           4.0);
    result.swapCost = input.weapon.active.isValid() &&
                              input.weapon.activeClass != candidate.observation.weaponClass
                          ? 4
                          : 1;
    result.totalUtility = clampScore(static_cast<std::int64_t>(result.upgradeValue) +
                                     result.ammoNeed + result.tacticalFit + result.roleFit -
                                     result.travelRisk - result.exposureRisk -
                                     result.objectiveDelay - result.swapCost);
    if (result.totalUtility < settings.minimumPickupUtility) {
        result.failure = WeaponAcquisitionFailure::InsufficientUtility;
        return result;
    }
    result.actionable = true;
    return result;
}

WeaponSelectionResult selectWeaponPickup(const ActionPlannerInput& input,
                                          const ActionPlannerSettings& settings) noexcept {
    WeaponSelectionResult result{};
    const auto limit = (std::min)({input.weaponCount,
                                   static_cast<std::size_t>(settings.maxWeaponCandidates),
                                   input.weapons.size()});
    const auto evaluationLimit = (std::min)(limit,
                                            static_cast<std::size_t>(settings.maxEvaluations));
    for (std::size_t i = 0; i < evaluationLimit; ++i) {
        const auto evaluation = evaluateWeaponPickup(input, input.weapons[i], settings);
        ++result.evaluatedCandidates;
        if (!evaluation.actionable ||
            (result.found &&
             (evaluation.totalUtility < result.evaluation.totalUtility ||
              (evaluation.totalUtility == result.evaluation.totalUtility &&
               (input.weapons[i].routeCost > input.weapons[result.candidateIndex].routeCost ||
                (input.weapons[i].routeCost == input.weapons[result.candidateIndex].routeCost &&
                  (result.evaluation.candidateWeapon < evaluation.candidateWeapon ||
                  (evaluation.candidateWeapon == result.evaluation.candidateWeapon &&
                   evaluation.reference >= result.evaluation.reference)))))))) {
            continue;
        }
        result.found = true;
        result.candidateIndex = i;
        result.evaluation = evaluation;
    }
    return result;
}

void ActionPlanner::reset() noexcept {
    current_ = {};
    currentScore_ = {};
    currentPrecondition_ = {};
    activeSinceMicros_ = 0;
    lastAbortReason_ = ActionAbortReason::None;
    active_ = false;
}

ActionDecision ActionPlanner::invalidDecision(const ActionPlannerInput& input,
                                              ActionAbortReason reason) noexcept {
    clear(reason);
    ActionDecision decision{};
    decision.intent.action = ActionType::NoAction;
    decision.intent.createdMicros = input.nowMicros;
    decision.intent.expiresMicros = input.nowMicros;
    decision.precondition.action = ActionType::NoAction;
    decision.precondition.validSituation = false;
    decision.transitionReason = reason;
    return decision;
}

ActionDecision ActionPlanner::holdDecision(const ActionPlannerInput& input) noexcept {
    ActionDecision decision{};
    decision.intent.action = ActionType::Hold;
    setExpiry(decision.intent, input, settings_);
    decision.score.base = 10;
    decision.score.recompute();
    decision.precondition = {ActionType::Hold, true, true, input.enemy.known,
                             input.enemy.directVision, false, true,
                             input.weapon.reserveAmmo > 0, input.weapon.canReload,
                             input.objective.kind != ObjectiveKind::None, false,
                             secondarySolvesImmediateProblem(input), true};
    decision.accepted = true;
    return decision;
}

bool ActionPlanner::currentCandidatePresent(const ActionPlannerInput& input) const noexcept {
    if (!active_ || current_.action != ActionType::AcquireWeapon) return true;
    const auto limit = (std::min)(input.weaponCount, input.weapons.size());
    for (std::size_t i = 0; i < limit; ++i) {
        if (input.weapons[i].observation.reference == current_.candidateReference &&
            input.weapons[i].observation.present &&
            input.weapons[i].observation.validate(
                input.map, input.round, input.tick, input.nowMicros,
                settings_.maxCandidateAgeMicros) == WeaponObservationError::None) {
            return true;
        }
    }
    return false;
}

bool ActionPlanner::currentStillValid(const ActionPlannerInput& input) const noexcept {
    if (!active_) return false;
    switch (current_.action) {
    case ActionType::Engage:
        return input.enemy.directVision && input.enemy.target == current_.target;
    case ActionType::Reload:
        return !input.weapon.reloading && input.weapon.canReload &&
               input.weapon.reserveAmmo > 0 &&
               input.weapon.clipAmmo <= settings_.reloadClipThreshold &&
               !input.enemy.directVision;
    case ActionType::TakeCover:
        return input.enemy.pressure && current_.targetArea.isValid();
    case ActionType::Retreat:
        return input.enemy.pressure && input.healthPercent <= settings_.retreatHealthPercent &&
               current_.targetArea.isValid();
    case ActionType::Peek:
        return input.enemy.known && !input.enemy.directVision &&
               input.enemy.target == current_.target;
    case ActionType::Plant:
        return input.objective.bomb == BombState::Carried && input.objective.canPlant;
    case ActionType::Defuse:
        return input.objective.bomb == BombState::Planted &&
               (input.objective.canDefuse || input.objective.hasDefuseKit);
    case ActionType::GuardBomb:
        return input.objective.bomb == BombState::Planted && input.objective.canGuardBomb;
    case ActionType::RecoverBomb:
        return input.objective.bomb == BombState::Dropped && input.objective.canRecoverBomb;
    case ActionType::RescueHostage:
        return input.objective.kind == ObjectiveKind::Hostage &&
               input.objective.canRescueHostage;
    case ActionType::EscortHostage:
        return input.objective.kind == ObjectiveKind::Hostage &&
               input.objective.canEscortHostage;
    case ActionType::AcquireWeapon: {
        const auto limit = (std::min)(input.weaponCount, input.weapons.size());
        for (std::size_t i = 0; i < limit; ++i) {
            if (input.weapons[i].observation.reference == current_.candidateReference) {
                return evaluateWeaponPickup(input, input.weapons[i], settings_).actionable;
            }
        }
        return false;
    }
    case ActionType::FollowCurrentIntent:
        return input.teammateCount > 0;
    case ActionType::Hold:
        return true;
    case ActionType::NoAction:
        return false;
    }
    return false;
}

void ActionPlanner::activate(const ActionDecision& decision) noexcept {
    current_ = decision.intent;
    currentScore_ = decision.score;
    currentPrecondition_ = decision.precondition;
    activeSinceMicros_ = decision.intent.createdMicros;
    active_ = decision.accepted && decision.intent.action != ActionType::NoAction;
}

void ActionPlanner::clear(ActionAbortReason reason) noexcept {
    if (active_ && reason != ActionAbortReason::None) lastAbortReason_ = reason;
    active_ = false;
    current_ = {};
    currentScore_ = {};
    currentPrecondition_ = {};
    activeSinceMicros_ = 0;
}

ActionDecision ActionPlanner::decide(const ActionPlannerInput& input) noexcept {
    if (!settings_.valid() || !input.valid(settings_)) {
        return invalidDecision(input, ActionAbortReason::InvalidInput);
    }
    if (!input.alive) return invalidDecision(input, ActionAbortReason::Dead);

    ActionAbortReason transition = lastAbortReason_;
    lastAbortReason_ = ActionAbortReason::None;
    if (active_ && input.nowMicros >= current_.expiresMicros) {
        clear(ActionAbortReason::TimedOut);
        transition = ActionAbortReason::TimedOut;
        lastAbortReason_ = ActionAbortReason::None;
    }
    if (active_ && !currentCandidatePresent(input)) {
        clear(ActionAbortReason::CandidateGone);
        transition = ActionAbortReason::CandidateGone;
        lastAbortReason_ = ActionAbortReason::None;
    }

    std::array<CandidateDecision, kMaxActionCandidates> candidates{};
    std::size_t count = 0;
    auto add = [&](CandidateDecision candidate) {
        if (count < candidates.size()) candidates[count++] = candidate;
    };

    auto addObjective = [&]() {
        if (input.objective.bomb == BombState::Planted &&
            (input.objective.canDefuse || input.objective.hasDefuseKit)) {
            auto candidate = basicCandidate(ActionType::Defuse, input, settings_, 850);
            candidate.precondition.objectiveAvailable = true;
            candidate.precondition.enoughTime = input.objective.remainingMicros == 0 ||
                                                input.objective.estimatedActionMicros == 0 ||
                                                input.objective.estimatedActionMicros <=
                                                    input.objective.remainingMicros;
            candidate.precondition.satisfied = candidate.precondition.enoughTime;
            candidate.score.objective = 140;
            candidate.score.urgency = input.objective.urgent(settings_.objectiveUrgencyMicros)
                                          ? 120
                                          : 40;
            candidate.score.recompute();
            add(candidate);
        }
        if (input.objective.bomb == BombState::Carried && input.objective.canPlant) {
            auto candidate = basicCandidate(ActionType::Plant, input, settings_, 820);
            candidate.precondition.objectiveAvailable = true;
            candidate.precondition.satisfied = true;
            candidate.score.objective = 160;
            candidate.score.urgency = 100;
            candidate.score.recompute();
            add(candidate);
        }
        if (input.objective.bomb == BombState::Dropped && input.objective.canRecoverBomb) {
            auto candidate = basicCandidate(ActionType::RecoverBomb, input, settings_, 780);
            const auto location = bestLocation(input);
            candidate.intent.targetArea = location.area;
            candidate.intent.targetPosition = location.position;
            candidate.precondition.objectiveAvailable = true;
            candidate.precondition.safeRoute = !input.navigationCount || hasLocation(location);
            candidate.precondition.enoughTime = input.objective.remainingMicros == 0 ||
                                                input.objective.estimatedActionMicros == 0 ||
                                                input.objective.estimatedActionMicros <=
                                                    input.objective.remainingMicros;
            candidate.precondition.satisfied = candidate.precondition.safeRoute &&
                                               candidate.precondition.enoughTime;
            candidate.score.objective = 150;
            candidate.score.navigation = candidate.precondition.safeRoute ? 20 : -100;
            candidate.score.recompute();
            if (candidate.precondition.satisfied) add(candidate);
        }
        if (input.objective.bomb == BombState::Planted && input.objective.canGuardBomb) {
            auto candidate = basicCandidate(ActionType::GuardBomb, input, settings_, 700);
            candidate.precondition.objectiveAvailable = true;
            candidate.precondition.satisfied = true;
            candidate.score.objective = 100;
            candidate.score.recompute();
            add(candidate);
        }
        if (input.objective.kind == ObjectiveKind::Hostage &&
            input.objective.canRescueHostage) {
            auto candidate = basicCandidate(ActionType::RescueHostage, input, settings_, 760);
            candidate.precondition.objectiveAvailable = true;
            candidate.precondition.satisfied = true;
            candidate.score.objective = 145;
            candidate.score.recompute();
            add(candidate);
        }
        if (input.objective.kind == ObjectiveKind::Hostage &&
            input.objective.canEscortHostage) {
            auto candidate = basicCandidate(ActionType::EscortHostage, input, settings_, 740);
            candidate.precondition.objectiveAvailable = true;
            candidate.precondition.satisfied = true;
            candidate.score.objective = 135;
            candidate.score.recompute();
            add(candidate);
        }
    };
    addObjective();

    const auto location = bestLocation(input);
    if (input.healthPercent <= settings_.retreatHealthPercent && input.enemy.pressure &&
        hasLocation(location)) {
        auto candidate = basicCandidate(ActionType::Retreat, input, settings_, 680);
        candidate.intent.targetArea = location.area;
        candidate.intent.targetPosition = location.position;
        candidate.precondition.safeRoute = true;
        candidate.precondition.enoughTime = true;
        candidate.precondition.satisfied = true;
        candidate.score.threat = input.enemy.directVision ? 160 : 95;
        candidate.score.navigation = 40;
        candidate.score.urgency = roundedScore(100.0 - input.healthPercent);
        candidate.score.risk = riskFrom(location.exposure, 20.0);
        candidate.score.recompute();
        add(candidate);
    }
    if (input.enemy.directVision && input.enemy.confidence >= 0.25) {
        auto candidate = basicCandidate(ActionType::Engage, input, settings_, 570);
        candidate.intent.target = input.enemy.target;
        candidate.intent.targetPosition = input.enemy.lastKnownPosition;
        candidate.precondition.knownEnemy = true;
        candidate.precondition.directThreat = true;
        candidate.precondition.satisfied = true;
        candidate.score.threat = roundedScore(input.enemy.confidence * 180.0);
        candidate.score.role = input.role == TacticalRole::Entry ? 35 : 15;
        candidate.score.recompute();
        add(candidate);
    }
    if (!input.weapon.reloading && input.weapon.canReload && input.weapon.reserveAmmo > 0 &&
        input.weapon.clipAmmo <= settings_.reloadClipThreshold && !input.enemy.directVision) {
        auto candidate = basicCandidate(ActionType::Reload, input, settings_, 490);
        candidate.precondition.hasReserveAmmo = true;
        candidate.precondition.canReload = true;
        candidate.precondition.enoughTime = true;
        candidate.precondition.satisfied = true;
        candidate.score.ammunition = input.weapon.clipAmmo == 0 ? 120 : 80;
        candidate.score.risk = input.enemy.pressure ? 20 : 0;
        candidate.score.recompute();
        add(candidate);
    }

    const auto selection = selectWeaponPickup(input, settings_);
    if (selection.found && !input.objective.urgent(settings_.objectiveUrgencyMicros) &&
        !input.enemy.directVision) {
        const auto& pickup = input.weapons[selection.candidateIndex];
        auto candidate = basicCandidate(ActionType::AcquireWeapon, input, settings_, 350);
        candidate.intent.targetArea = pickup.observation.area;
        candidate.intent.targetPosition = pickup.observation.position;
        candidate.intent.weapon = pickup.observation.weapon;
        candidate.intent.candidateReference = pickup.observation.reference;
        setExpiry(candidate.intent, input, settings_,
                  (std::max)(settings_.actionTimeoutMicros,
                             pickup.timeRequiredMicros + settings_.actionTimeoutMicros));
        candidate.precondition.candidateAvailable = true;
        candidate.precondition.safeRoute = pickup.safeRoute;
        candidate.precondition.enoughTime = true;
        candidate.precondition.secondarySufficient = secondarySolvesImmediateProblem(input);
        candidate.precondition.satisfied = true;
        candidate.score.ammunition = selection.evaluation.ammoNeed;
        candidate.score.navigation = selection.evaluation.upgradeValue;
        candidate.score.role = selection.evaluation.roleFit;
        candidate.score.risk = selection.evaluation.travelRisk +
                              selection.evaluation.exposureRisk +
                              selection.evaluation.objectiveDelay + selection.evaluation.swapCost;
        candidate.score.recompute();
        candidate.pickup = selection.evaluation;
        candidate.hasPickup = true;
        candidate.acquisition = {pickup.observation.reference, pickup.observation.weapon,
                                 pickup.observation.area, pickup.observation.position,
                                 pickup.routeCost, input.nowMicros};
        candidate.hasAcquisition = true;
        add(candidate);
    }
    if (input.enemy.known && !input.enemy.directVision && hasLocation(location)) {
        auto candidate = basicCandidate(ActionType::Peek, input, settings_, 390);
        candidate.intent.target = input.enemy.target;
        candidate.intent.targetArea = location.area;
        candidate.intent.targetPosition = location.position;
        candidate.intent.returnArea = input.currentArea;
        candidate.intent.returnPosition = input.currentPosition;
        setExpiry(candidate.intent, input, settings_, settings_.peekExposureMicros);
        candidate.precondition.safeRoute = true;
        candidate.precondition.satisfied = true;
        candidate.score.threat = roundedScore(input.enemy.confidence * 100.0);
        candidate.score.navigation = 20;
        candidate.score.risk = riskFrom(location.exposure, 20.0);
        candidate.score.recompute();
        add(candidate);
    }
    if (input.enemy.pressure && hasLocation(location)) {
        auto candidate = basicCandidate(ActionType::TakeCover, input, settings_, 380);
        candidate.intent.targetArea = location.area;
        candidate.intent.targetPosition = location.position;
        candidate.precondition.safeRoute = true;
        candidate.precondition.satisfied = true;
        candidate.score.threat = 75;
        candidate.score.navigation = 35;
        candidate.score.risk = riskFrom(location.exposure, 15.0);
        candidate.score.recompute();
        add(candidate);
    }
    if (input.teammateCount > 0) {
        auto candidate = basicCandidate(ActionType::FollowCurrentIntent, input, settings_, 240);
        candidate.precondition.satisfied = true;
        candidate.score.role = input.personality == Personality::Supportive ? 45 : 20;
        candidate.score.recompute();
        add(candidate);
    }
    const auto hold = holdDecision(input);
    add(CandidateDecision{hold.intent, hold.score, hold.precondition, {}, {}, false, false});

    if (count == 0) return holdDecision(input);
    auto best = candidates[0];
    for (std::size_t i = 1; i < count; ++i) {
        if (betterCandidate(candidates[i], best)) best = candidates[i];
    }

    ActionDecision decision{};
    decision.intent = best.intent;
    decision.score = best.score;
    decision.precondition = best.precondition;
    decision.pickup = best.pickup;
    decision.acquisition = best.acquisition;
    decision.hasPickupEvaluation = best.hasPickup;
    decision.hasAcquisition = best.hasAcquisition;
    decision.evaluatedCandidates = selection.evaluatedCandidates;
    decision.accepted = best.precondition.satisfied;
    decision.transitionReason = transition;

    if (active_ && decision.intent.action != current_.action) {
        const bool critical = actionPriority(decision.intent.action) >
                                  actionPriority(current_.action) + 20 ||
                              decision.score.total > currentScore_.total +
                                  settings_.actionSwitchMargin;
        if (!critical && currentPrecondition_.validFor(current_.action) &&
            currentStillValid(input)) {
            decision.intent = current_;
            decision.score = currentScore_;
            decision.precondition = currentPrecondition_;
            decision.accepted = true;
            decision.changed = false;
            return decision;
        }
        if (transition == ActionAbortReason::None) {
            decision.transitionReason = critical
                                          ? ActionAbortReason::BetterActionCritical
                                          : ActionAbortReason::Replaced;
        }
        decision.changed = true;
    } else {
        decision.changed = !active_ || !sameIntent(decision.intent, current_);
    }
    lastAbortReason_ = ActionAbortReason::None;
    if (!active_ || decision.changed) activate(decision);
    return decision;
}

ActionResult ActionPlanner::update(const ActionPlannerInput& input,
                                   const ActionObservation& observation) noexcept {
    if (!settings_.valid() || !input.valid(settings_)) {
        clear(ActionAbortReason::InvalidInput);
        return {ActionResultState::Rejected, ActionType::NoAction,
                ActionAbortReason::InvalidInput, 0, true};
    }
    if (!active_) {
        const auto decision = decide(input);
        if (!decision.accepted) {
            return {ActionResultState::Rejected, decision.intent.action,
                    decision.transitionReason, 0, true};
        }
    }
    const auto elapsed = input.nowMicros >= activeSinceMicros_
                             ? input.nowMicros - activeSinceMicros_
                             : 0;
    const auto action = current_.action;
    if (observation.actionComplete) {
        clear(ActionAbortReason::Completed);
        return {ActionResultState::Complete, action, ActionAbortReason::Completed,
                elapsed, true};
    }
    if (input.nowMicros >= current_.expiresMicros) {
        clear(ActionAbortReason::TimedOut);
        return {ActionResultState::Aborted, action, ActionAbortReason::TimedOut,
                elapsed, true};
    }
    if (observation.candidatePickedUp) {
        clear(ActionAbortReason::CandidatePickedUp);
        return {ActionResultState::Aborted, action,
                ActionAbortReason::CandidatePickedUp, elapsed, true};
    }
    if (action == ActionType::AcquireWeapon && !observation.candidatePresent) {
        clear(ActionAbortReason::CandidateGone);
        return {ActionResultState::Aborted, action, ActionAbortReason::CandidateGone,
                elapsed, true};
    }
    if (!observation.routeSafe) {
        clear(ActionAbortReason::RouteUnsafe);
        return {ActionResultState::Aborted, action, ActionAbortReason::RouteUnsafe,
                elapsed, true};
    }
    if (!observation.enoughTime) {
        clear(ActionAbortReason::InsufficientTime);
        return {ActionResultState::Aborted, action,
                ActionAbortReason::InsufficientTime, elapsed, true};
    }
    if (observation.objectiveUrgent) {
        clear(ActionAbortReason::ObjectiveUrgent);
        return {ActionResultState::Aborted, action, ActionAbortReason::ObjectiveUrgent,
                elapsed, true};
    }
    if (observation.enemyAppeared) {
        clear(ActionAbortReason::EnemyAppeared);
        return {ActionResultState::Aborted, action, ActionAbortReason::EnemyAppeared,
                elapsed, true};
    }
    if (observation.threatChanged) {
        clear(ActionAbortReason::ThreatChanged);
        return {ActionResultState::Aborted, action, ActionAbortReason::ThreatChanged,
                elapsed, true};
    }
    if (observation.betterActionCritical) {
        clear(ActionAbortReason::BetterActionCritical);
        return {ActionResultState::Aborted, action,
                ActionAbortReason::BetterActionCritical, elapsed, true};
    }
    return {ActionResultState::Running, action, ActionAbortReason::None, elapsed, false};
}

const char* actionName(ActionType action) noexcept {
    switch (action) {
    case ActionType::NoAction: return "NoAction";
    case ActionType::Engage: return "Engage";
    case ActionType::Reload: return "Reload";
    case ActionType::TakeCover: return "TakeCover";
    case ActionType::Retreat: return "Retreat";
    case ActionType::Peek: return "Peek";
    case ActionType::Hold: return "Hold";
    case ActionType::FollowCurrentIntent: return "FollowCurrentIntent";
    case ActionType::Plant: return "Plant";
    case ActionType::Defuse: return "Defuse";
    case ActionType::GuardBomb: return "GuardBomb";
    case ActionType::RecoverBomb: return "RecoverBomb";
    case ActionType::RescueHostage: return "RescueHostage";
    case ActionType::EscortHostage: return "EscortHostage";
    case ActionType::AcquireWeapon: return "AcquireWeapon";
    }
    return "Unknown";
}

const char* actionAbortReasonName(ActionAbortReason reason) noexcept {
    switch (reason) {
    case ActionAbortReason::None: return "None";
    case ActionAbortReason::InvalidInput: return "InvalidInput";
    case ActionAbortReason::Dead: return "Dead";
    case ActionAbortReason::Replaced: return "Replaced";
    case ActionAbortReason::TimedOut: return "TimedOut";
    case ActionAbortReason::EnemyAppeared: return "EnemyAppeared";
    case ActionAbortReason::ThreatChanged: return "ThreatChanged";
    case ActionAbortReason::ObjectiveUrgent: return "ObjectiveUrgent";
    case ActionAbortReason::CandidateGone: return "CandidateGone";
    case ActionAbortReason::CandidatePickedUp: return "CandidatePickedUp";
    case ActionAbortReason::RouteUnsafe: return "RouteUnsafe";
    case ActionAbortReason::InsufficientTime: return "InsufficientTime";
    case ActionAbortReason::BetterActionCritical: return "BetterActionCritical";
    case ActionAbortReason::NoSafeRoute: return "NoSafeRoute";
    case ActionAbortReason::NoTarget: return "NoTarget";
    case ActionAbortReason::Completed: return "Completed";
    }
    return "Unknown";
}

} // namespace astrabot::core::action
