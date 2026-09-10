// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/tactical_planner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::core::tactical {
namespace {

bool finitePoint(const perception::Point& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
}

bool playing(perception::Team team) noexcept {
    return team == perception::Team::Terrorist ||
           team == perception::Team::CounterTerrorist;
}

bool sameTarget(const TargetArea& left, const TargetArea& right) noexcept {
    return left.area == right.area && left.stableId == right.stableId;
}

std::uint64_t saturatingAdd(std::uint64_t left, std::uint64_t right) noexcept {
    const auto max = (std::numeric_limits<std::uint64_t>::max)();
    return left > max - right ? max : left + right;
}

const TacticalRoute* routeFor(const TacticalContext& context,
                              RouteStyle style) noexcept {
    const auto count = (std::min)(context.navigation.routeCount,
                                  context.navigation.routes.size());
    const TacticalRoute* result = nullptr;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& candidate = context.navigation.routes[i];
        if (!candidate.valid() || candidate.style != style) continue;
        if (!result || candidate.target.stableId < result->target.stableId ||
            (candidate.target.stableId == result->target.stableId &&
             candidate.etaMicros < result->etaMicros)) {
            result = &candidate;
        }
    }
    return result;
}

const TacticalRoute* routeForTarget(const TacticalContext& context,
                                    RouteStyle style,
                                    const TargetArea& target) noexcept {
    const auto count = (std::min)(context.navigation.routeCount,
                                  context.navigation.routes.size());
    const TacticalRoute* result = nullptr;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& candidate = context.navigation.routes[i];
        if (!candidate.valid() || candidate.style != style ||
            !sameTarget(candidate.target, target)) continue;
        if (!result || candidate.etaMicros < result->etaMicros) result = &candidate;
    }
    return result;
}

std::size_t confirmedEnemies(const TacticalContext& context,
                             const TacticalPlannerSettings& settings) noexcept {
    std::size_t count = 0;
    const auto limit = (std::min)(context.enemyCount, context.enemies.size());
    for (std::size_t i = 0; i < limit && count < settings.maxEnemies; ++i) {
        if (context.enemies[i].valid(context.nowMicros,
                                     settings.enemyFreshnessMicros) &&
            context.enemies[i].confirmed) {
            ++count;
        }
    }
    return count;
}

std::size_t aliveTeammates(const TacticalContext& context,
                           const TacticalPlannerSettings& settings) noexcept {
    std::size_t count = 0;
    const auto limit = (std::min)(context.teammateCount,
                                  context.teammates.size());
    for (std::size_t i = 0; i < limit && count < settings.maxTeammates; ++i) {
        if (context.teammates[i].valid() && context.teammates[i].alive) ++count;
    }
    return count;
}

bool entryAlive(const TacticalContext& context,
                const TacticalPlannerSettings& settings) noexcept {
    const auto limit = (std::min)(context.teammateCount,
                                  context.teammates.size());
    for (std::size_t i = 0; i < limit; ++i) {
        const auto& teammate = context.teammates[i];
        if (teammate.valid() && teammate.role == RolePreference::Entry &&
            teammate.alive) return true;
    }
    (void)settings;
    return false;
}

std::size_t concentrationAt(const TacticalContext& context,
                             const TargetArea& target,
                             const TacticalPlannerSettings& settings) noexcept {
    if (!target.area.isValid()) return 0;
    std::size_t count = 0;
    const auto limit = (std::min)(context.enemyCount, context.enemies.size());
    for (std::size_t i = 0; i < limit && i < settings.maxEnemies; ++i) {
        const auto& enemy = context.enemies[i];
        if (enemy.valid(context.nowMicros, settings.enemyFreshnessMicros) &&
            enemy.confirmed && enemy.area == target.area) ++count;
    }
    return count;
}

bool hasEnemyConcentration(const TacticalContext& context,
                           const TacticalPlannerSettings& settings) noexcept {
    const auto limit = (std::min)(context.enemyCount, context.enemies.size());
    for (std::size_t i = 0; i < limit && i < settings.maxEnemies; ++i) {
        const auto& enemy = context.enemies[i];
        if (!enemy.valid(context.nowMicros, settings.enemyFreshnessMicros) ||
            !enemy.confirmed || !enemy.area.isValid()) continue;
        const TargetArea observed{enemy.area, enemy.position, 0};
        if (concentrationAt(context, observed, settings) >= 2) return true;
    }
    return false;
}

TacticalIntent baseIntent(IntentType type, const TacticalRoute* route,
                          const TacticalContext& context,
                          const TacticalPlannerSettings& settings,
                          Urgency urgency, Reason reason,
                          RolePreference role = RolePreference::Any) noexcept {
    TacticalIntent result{};
    result.type = type;
    result.role = role;
    result.urgency = urgency;
    result.reason = reason;
    result.validity = Validity::Valid;
    result.createdMicros = context.nowMicros;
    result.expiresMicros = saturatingAdd(context.nowMicros,
                                         settings.intentHorizonMicros);
    if (route) {
        result.target = route->target;
        result.route = route->style;
    } else {
        result.target = {context.self.currentArea, context.self.position, 0};
        result.route = RouteStyle::Hold;
    }
    return result;
}

ReplanTrigger triggerFor(const ReplanEvents& events, bool periodic,
                         bool invalidated) noexcept {
    if (invalidated || events.intentInvalidated) return ReplanTrigger::IntentInvalidated;
    if (events.routeBlocked) return ReplanTrigger::RouteBlocked;
    if (events.bombPlanted) return ReplanTrigger::BombPlanted;
    if (events.bombDropped) return ReplanTrigger::BombDropped;
    if (events.teammateDeath || events.entryPlayerDied) return ReplanTrigger::TeammateDeath;
    if (events.objectiveTransition) return ReplanTrigger::ObjectiveTransition;
    if (events.enemySighting) return ReplanTrigger::EnemySighting;
    return periodic ? ReplanTrigger::Periodic : ReplanTrigger::None;
}

} // namespace

bool TargetArea::valid() const noexcept {
    return area.isValid() && finitePoint(position);
}

bool TacticalRoute::valid() const noexcept {
    return available && style != RouteStyle::None && target.valid() &&
           etaMicros != 0 && std::isfinite(danger) && danger >= 0.0;
}

bool SelfState::valid() const noexcept {
    return player.isValid() && agent.isValid() && playing(team) &&
           finitePoint(position) && currentArea.isValid() && alive &&
           std::isfinite(healthPercent) && healthPercent > 0.0F &&
           healthPercent <= 100.0F && weaponValue >= 0;
}

bool TeammateState::valid() const noexcept {
    // World Model roster entries do not contain NAV-area truth. Area is an
    // optional value-level annotation supplied by the host when available.
    return player.isValid();
}

bool EnemyBelief::valid(std::uint64_t nowMicros,
                        std::uint64_t maxAgeMicros) const noexcept {
    return target.isValid() && finitePoint(position) &&
           std::isfinite(confidence) && confidence >= 0.0 && confidence <= 1.0 &&
           observedAgeMicros <= maxAgeMicros && observedAgeMicros <= nowMicros;
}

bool ObjectiveState::valid() const noexcept {
    if (kind == ObjectiveKind::None) return bomb == BombState::None;
    if (kind == ObjectiveKind::Bomb && bomb == BombState::Planted) {
        return target.valid() && remainingMicros != 0;
    }
    if (kind == ObjectiveKind::Hostage) return canEscort || target.valid();
    return kind == ObjectiveKind::Bomb;
}

bool EconomySummary::valid() const noexcept {
    return weaponValue >= 0 && (!available || weaponValue >= 0);
}

bool NavigationState::valid() const noexcept {
    if (routeCount > routes.size() || roamCandidateCount > roamCandidates.size())
        return false;
    if (explicitRouteAvailable && !explicitRoute.valid()) return false;
    const auto count = (std::min)(routeCount, routes.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (routes[i].available && !routes[i].valid()) return false;
    }
    const auto roamCount = (std::min)(roamCandidateCount, roamCandidates.size());
    for (std::size_t i = 0; i < roamCount; ++i)
        if (!roamCandidates[i].valid()) return false;
    return true;
}

bool TacticalContext::valid() const noexcept {
    if (!map.isValid() || !round.isValid() || !tick.isValid() ||
        !self.valid() || !objective.valid() || !economy.valid() ||
        !navigation.valid() || teammateCount > teammates.size() ||
        enemyCount > enemies.size()) return false;
    const auto teammatesToCheck = (std::min)(teammateCount, teammates.size());
    for (std::size_t i = 0; i < teammatesToCheck; ++i) {
        if (!teammates[i].valid()) return false;
    }
    return true;
}

TacticalContext buildTacticalContext(const world::WorldSnapshot& snapshot,
                                     const TacticalContextSeed& seed) noexcept {
    TacticalContext result{};
    result.map = snapshot.stamp.map;
    result.round = snapshot.stamp.round;
    result.tick = snapshot.stamp.tick;
    result.nowMicros = snapshot.stamp.timeMicros;
    result.self = seed.self;
    result.objective = seed.objective;
    result.economy = seed.economy;
    result.navigation = seed.navigation;
    if (!snapshot.visual || !snapshot.sounds || !playing(seed.self.team)) return result;

    for (const auto& member : snapshot.roster) {
        if (!member.player.isValid() || member.player == seed.self.player) continue;
        const auto relation = snapshot.relation(seed.self.team, member.player);
        if (relation == perception::Relation::Ally &&
            result.teammateCount < result.teammates.size()) {
            result.teammates[result.teammateCount++] =
                {member.player, RolePreference::Any, {}, true, false};
        }
        if (relation != perception::Relation::Opponent ||
            result.enemyCount == result.enemies.size()) continue;
        const auto known = snapshot.known(member.player);
        if (!known) continue;
        const auto age = result.nowMicros >= known->origin.observedMicros
                             ? result.nowMicros - known->origin.observedMicros
                             : result.nowMicros;
        result.enemies[result.enemyCount++] = {
            member.player, {}, known->position, known->confidence, age,
            known->confidence >= 0.75, known->source == perception::ObservationSource::Vision};
    }
    return result;
}

bool ReplanEvents::any() const noexcept {
    return enemySighting || bombPlanted || bombDropped || teammateDeath ||
           entryPlayerDied || objectiveTransition || routeBlocked ||
           intentInvalidated;
}

bool TacticalPlannerSettings::valid() const noexcept {
    return replanIntervalMicros != 0 && intentHorizonMicros != 0 &&
           saveTimeMicros != 0 && enemyFreshnessMicros != 0 &&
           std::isfinite(saveHealthPercent) && saveHealthPercent > 0.0F &&
           saveHealthPercent <= 100.0F && valuableWeaponValue >= 0 &&
           maxEnemies <= kMaxTacticalEnemies &&
           maxTeammates <= kMaxTacticalTeammates;
}

void TacticalPlanner::reset() noexcept {
    recentRoam_ = {};
    recentRoamCount_ = 0;
    current_ = {};
    lastPlanMicros_ = 0;
    generation_ = 0;
    active_ = false;
}

bool TacticalPlanner::currentStillValid(const TacticalContext& context) const noexcept {
    if (!active_ || current_.validity != Validity::Valid ||
        context.nowMicros >= current_.expiresMicros) return false;
    if (current_.route == RouteStyle::Hold) {
        if (current_.target.area == context.self.currentArea) return true;
        return routeForTarget(context, RouteStyle::Hold, current_.target) != nullptr;
    }
    if (current_.route == RouteStyle::Roam) {
        const auto count = (std::min)(context.navigation.roamCandidateCount,
                                      context.navigation.roamCandidates.size());
        for (std::size_t i = 0; i < count; ++i)
            if (sameTarget(context.navigation.roamCandidates[i], current_.target))
                return true;
        return false;
    }
    return routeForTarget(context, current_.route, current_.target) != nullptr;
}

bool TacticalPlanner::needsReplan(const TacticalContext& context,
                                  const ReplanEvents& events) const noexcept {
    if (!settings_.valid() || !context.valid() || !active_) return true;
    if (!currentStillValid(context) || events.any()) return true;
    return context.nowMicros >= saturatingAdd(lastPlanMicros_,
                                              settings_.replanIntervalMicros);
}

TacticalDecision TacticalPlanner::invalidDecision(const TacticalContext& context,
                                                  Reason reason) noexcept {
    reset();
    TacticalDecision result{};
    result.intent.reason = reason;
    result.intent.validity = Validity::Invalid;
    result.intent.createdMicros = context.nowMicros;
    result.trigger = ReplanTrigger::IntentInvalidated;
    return result;
}

TacticalIntent TacticalPlanner::choose(const TacticalContext& context,
                                       ReplanTrigger trigger) const noexcept {
    const auto planted = context.objective.kind == ObjectiveKind::Bomb &&
                         context.objective.bomb == BombState::Planted;
    const auto confirmed = confirmedEnemies(context, settings_);
    const auto* retake = context.objective.target.valid()
                             ? routeForTarget(context, RouteStyle::Retake,
                                              context.objective.target)
                             : routeFor(context, RouteStyle::Retake);
    const bool valuableWeapon = context.economy.available &&
        context.economy.weaponValue >= settings_.valuableWeaponValue;
    const bool lowTime = context.objective.remainingMicros != 0 &&
        context.objective.remainingMicros <= settings_.saveTimeMicros;
    const bool lowHealth = context.self.healthPercent <= settings_.saveHealthPercent;

    if (planted) {
        const bool enoughTime = retake && context.objective.retakeFeasible &&
            context.objective.retakeTimeMicros < context.objective.remainingMicros;
        if (enoughTime) {
            auto result = baseIntent(IntentType::Retake, retake, context, settings_,
                                     Urgency::Critical, Reason::BombPlanted,
                                     RolePreference::Support);
            return result;
        }
        if (valuableWeapon || lowTime || lowHealth) {
            auto result = baseIntent(IntentType::Save, nullptr, context, settings_,
                                     Urgency::High, Reason::RetakeUnavailable,
                                     RolePreference::Any);
            result.route = RouteStyle::Hold;
            return result;
        }
    }

    if (valuableWeapon && (lowTime || lowHealth) &&
        (!context.objective.retakeFeasible || !retake)) {
        return baseIntent(IntentType::Save, nullptr, context, settings_, Urgency::High,
                          Reason::SaveWeapon);
    }

    if (trigger == ReplanTrigger::TeammateDeath ||
        trigger == ReplanTrigger::ObjectiveTransition) {
        const auto* support = routeFor(context, RouteStyle::Support);
        if (support && (trigger == ReplanTrigger::ObjectiveTransition ||
                        !entryAlive(context, settings_))) {
            return baseIntent(IntentType::Support, support, context, settings_,
                              Urgency::High,
                              trigger == ReplanTrigger::TeammateDeath
                                  ? Reason::EntryLost : Reason::ObjectiveTransition,
                              RolePreference::Support);
        }
    }

    const auto* rotate = routeFor(context, RouteStyle::Rotate);
    if (confirmed >= 2 && hasEnemyConcentration(context, settings_) && rotate) {
        return baseIntent(IntentType::Rotate, rotate, context, settings_, Urgency::High,
                          Reason::EnemyConcentration);
    }

    if (context.objective.kind == ObjectiveKind::Bomb &&
        context.objective.bomb == BombState::Dropped) {
        const auto* direct = routeFor(context, RouteStyle::Direct);
        if (direct) {
            return baseIntent(IntentType::AttackSite, direct, context, settings_,
                              Urgency::High, Reason::BombDropped);
        }
    }

    if (context.objective.canAttack) {
        const auto* attack = routeForTarget(context, RouteStyle::Direct,
                                            context.objective.target);
        if (!attack) attack = routeFor(context, RouteStyle::Direct);
        if (attack) {
            const auto type = context.self.role == RolePreference::Entry
                                  ? IntentType::Entry : IntentType::AttackSite;
            return baseIntent(type, attack, context, settings_, Urgency::Normal,
                              Reason::AttackObjective,
                              type == IntentType::Entry ? RolePreference::Entry
                                                        : RolePreference::Any);
        }
    }

    if (context.objective.canDefend) {
        const auto* defend = routeFor(context, RouteStyle::Safe);
        if (defend) {
            return baseIntent(IntentType::DefendSite, defend, context, settings_,
                              Urgency::Normal, Reason::DefendObjective,
                              RolePreference::Anchor);
        }
    }

    if (confirmed > 0) {
        const auto* flank = routeFor(context, RouteStyle::Flank);
        if (flank && (context.self.role == RolePreference::Entry ||
                      context.self.role == RolePreference::Support)) {
            return baseIntent(IntentType::Flank, flank, context, settings_,
                              Urgency::Normal, Reason::EnemySighting);
        }
    }

    if (context.self.role == RolePreference::Anchor) {
        const auto* lurk = routeFor(context, RouteStyle::Lurk);
        if (lurk) {
            return baseIntent(IntentType::Lurk, lurk, context, settings_, Urgency::Low,
                              Reason::Periodic, RolePreference::Anchor);
        }
    }

    if (aliveTeammates(context, settings_) > 0) {
        const auto* support = routeFor(context, RouteStyle::Support);
        if (support && context.self.role == RolePreference::Support) {
            return baseIntent(IntentType::Support, support, context, settings_,
                              Urgency::Normal, Reason::Periodic,
                              RolePreference::Support);
        }
    }

    if (context.objective.kind == ObjectiveKind::Hostage &&
        context.objective.canEscort) {
        const auto* escort = routeFor(context, RouteStyle::Escort);
        if (escort) {
            return baseIntent(IntentType::Escort, escort, context, settings_,
                              Urgency::Normal, Reason::ObjectiveTransition,
                              RolePreference::Escort);
        }
    }

    if (context.navigation.explicitRouteAvailable &&
        context.navigation.explicitRoute.valid()) {
        return baseIntent(IntentType::Hold, &context.navigation.explicitRoute,
                          context, settings_, Urgency::Low, Reason::Periodic);
    }
    const auto roamCount = (std::min)(context.navigation.roamCandidateCount,
                                      context.navigation.roamCandidates.size());
    if (roamCount != 0) {
        const auto seed = static_cast<std::uint64_t>(context.self.agent.value) *
            2'654'435'761ULL + static_cast<std::uint64_t>(context.round.value);
        const auto offset = static_cast<std::size_t>(seed % roamCount);
        for (std::size_t step = 0; step < roamCount; ++step) {
            const auto& candidate =
                context.navigation.roamCandidates[(offset + step) % roamCount];
            if (!candidate.valid() || candidate.area == context.self.currentArea ||
                wasRecentRoam(candidate))
                continue;
            auto result = baseIntent(IntentType::Roam, nullptr, context, settings_,
                                     Urgency::Low, Reason::AutonomousRoam);
            result.target = candidate;
            result.route = RouteStyle::Roam;
            return result;
        }
    }


    const auto* hold = routeFor(context, RouteStyle::Hold);
    return baseIntent(IntentType::Hold, hold, context, settings_, Urgency::Low,
                      Reason::HoldCurrentArea);
}

TacticalDecision TacticalPlanner::plan(const TacticalContext& context,
                                       const ReplanEvents& events) noexcept {
    if (!settings_.valid() || !context.valid()) {
        return invalidDecision(context, Reason::InvalidContext);
    }

    const bool invalidated = active_ && !currentStillValid(context);
    const bool periodic = active_ && context.nowMicros >=
        saturatingAdd(lastPlanMicros_, settings_.replanIntervalMicros);
    if (active_ && !events.any() && !invalidated && !periodic) {
        TacticalDecision result{};
        result.intent = current_;
        result.accepted = true;
        result.evaluatedEnemies = context.enemyCount;
        result.evaluatedTeammates = context.teammateCount;
        return result;
    }

    const auto trigger = triggerFor(events, periodic, invalidated);
    auto intent = choose(context, trigger);
    intent.generation = generation_ + 1;
    const bool changed = !active_ || intent.type != current_.type ||
        !sameTarget(intent.target, current_.target) || intent.route != current_.route;
    activate(intent);

    TacticalDecision result{};
    result.intent = current_;
    result.trigger = trigger;
    result.evaluatedEnemies = context.enemyCount;
    result.evaluatedTeammates = context.teammateCount;
    result.accepted = current_.validity == Validity::Valid;
    result.changed = changed;
    result.replanned = true;
    return result;
}

void TacticalPlanner::activate(TacticalIntent intent) noexcept {
    const bool newRoam = intent.type == IntentType::Roam &&
        (!active_ || current_.type != IntentType::Roam ||
         !sameTarget(current_.target, intent.target));
    if (newRoam) rememberRoam(intent.target);

    current_ = intent;
    lastPlanMicros_ = intent.createdMicros;
    generation_ = intent.generation;
    active_ = true;
}

bool TacticalPlanner::wasRecentRoam(const TargetArea& target) const noexcept {
    for (std::size_t i = 0; i < recentRoamCount_; ++i)
        if (sameTarget(recentRoam_[i], target)) return true;
    return false;
}

void TacticalPlanner::rememberRoam(const TargetArea& target) noexcept {
    if (!target.valid()) return;
    const auto count = (std::min)(recentRoamCount_, recentRoam_.size());
    for (std::size_t i = count; i > 0; --i)
        if (i < recentRoam_.size()) recentRoam_[i] = recentRoam_[i - 1U];
    recentRoam_[0] = target;
    if (recentRoamCount_ < recentRoam_.size()) ++recentRoamCount_;
}
const char* intentName(IntentType intent) noexcept {
    switch (intent) {
    case IntentType::None: return "None";
    case IntentType::AttackSite: return "AttackSite";
    case IntentType::DefendSite: return "DefendSite";
    case IntentType::Rotate: return "Rotate";
    case IntentType::Retake: return "Retake";
    case IntentType::Save: return "Save";
    case IntentType::Flank: return "Flank";
    case IntentType::Lurk: return "Lurk";
    case IntentType::Roam: return "Roam";
    case IntentType::Support: return "Support";
    case IntentType::Entry: return "Entry";
    case IntentType::Trade: return "Trade";
    case IntentType::Hold: return "Hold";
    case IntentType::Escort: return "Escort";
    }
    return "Unknown";
}

const char* routeStyleName(RouteStyle route) noexcept {
    switch (route) {
    case RouteStyle::None: return "None";
    case RouteStyle::Direct: return "Direct";
    case RouteStyle::Safe: return "Safe";
    case RouteStyle::Fast: return "Fast";
    case RouteStyle::Rotate: return "Rotate";
    case RouteStyle::Roam: return "Roam";
    case RouteStyle::Retake: return "Retake";
    case RouteStyle::Flank: return "Flank";
    case RouteStyle::Lurk: return "Lurk";
    case RouteStyle::Support: return "Support";
    case RouteStyle::Escort: return "Escort";
    case RouteStyle::Hold: return "Hold";
    }
    return "Unknown";
}

const char* reasonName(Reason reason) noexcept {
    switch (reason) {
    case Reason::None: return "None";
    case Reason::Initial: return "Initial";
    case Reason::AttackObjective: return "AttackObjective";
    case Reason::DefendObjective: return "DefendObjective";
    case Reason::HoldCurrentArea: return "HoldCurrentArea";
    case Reason::EnemyConcentration: return "EnemyConcentration";
    case Reason::EnemySighting: return "EnemySighting";
    case Reason::BombPlanted: return "BombPlanted";
    case Reason::BombDropped: return "BombDropped";
    case Reason::RetakeUnavailable: return "RetakeUnavailable";
    case Reason::SaveWeapon: return "SaveWeapon";
    case Reason::EntryLost: return "EntryLost";
    case Reason::AutonomousRoam: return "AutonomousRoam";
    case Reason::TeammateDeath: return "TeammateDeath";
    case Reason::ObjectiveTransition: return "ObjectiveTransition";
    case Reason::RouteBlocked: return "RouteBlocked";
    case Reason::IntentInvalidated: return "IntentInvalidated";
    case Reason::Periodic: return "Periodic";
    case Reason::InvalidContext: return "InvalidContext";
    }
    return "Unknown";
}

const char* validityName(Validity validity) noexcept {
    switch (validity) {
    case Validity::Invalid: return "Invalid";
    case Validity::Valid: return "Valid";
    case Validity::Expired: return "Expired";
    case Validity::Superseded: return "Superseded";
    case Validity::Unreachable: return "Unreachable";
    case Validity::StaleContext: return "StaleContext";
    }
    return "Unknown";
}

} // namespace astrabot::core::tactical
