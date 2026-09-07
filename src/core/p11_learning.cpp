// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/p11_learning.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace astrabot::core::learning {
namespace {

bool finiteNonNegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool finitePoint(const perception::Point& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool validTeam(perception::Team team) noexcept {
    return team == perception::Team::Unknown || team == perception::Team::Terrorist ||
           team == perception::Team::CounterTerrorist || team == perception::Team::Spectator;
}

bool validWeapon(combat::WeaponSnapshot::WeaponClass weapon) noexcept {
    return static_cast<std::uint8_t>(weapon) <=
           static_cast<std::uint8_t>(combat::WeaponSnapshot::WeaponClass::Sniper);
}

bool validApproach(ApproachDirection approach) noexcept {
    return static_cast<std::uint8_t>(approach) <=
           static_cast<std::uint8_t>(ApproachDirection::Down);
}

double clampUnit(double value) noexcept {
    return (std::min)(1.0, (std::max)(0.0, value));
}

} // namespace

bool ContextualDangerKey::valid() const noexcept {
    return area != 0U && validTeam(team) && validApproach(approach) && validWeapon(enemyWeaponClass);
}

bool operator<(const ContextualDangerKey& left,
               const ContextualDangerKey& right) noexcept {
    return std::tie(left.area, left.team, left.approach, left.enemyWeaponClass,
                    left.likelyEnemyArea) <
           std::tie(right.area, right.team, right.approach, right.enemyWeaponClass,
                    right.likelyEnemyArea);
}

bool ContextualDangerObservation::valid() const noexcept {
    return key.valid() && finiteNonNegative(risk) && risk <= 1.0 &&
           finiteNonNegative(weight) && weight > 0.0 && weight <= 1'000'000.0;
}

bool ContextualDangerRecord::valid() const noexcept {
    return key.valid() && finiteNonNegative(risk) && risk <= 1.0 &&
           finiteNonNegative(observations) && observations <= 1'000'000.0;
}

ContextualDangerUpdate ContextualDangerModel::observe(
    const ContextualDangerObservation& observation) noexcept {
    if (!observation.valid()) return {ContextualDangerUpdateReason::InvalidObservation, false};
    auto it = std::lower_bound(records_.begin(), records_.end(), observation.key,
        [](const ContextualDangerRecord& record, const ContextualDangerKey& key) {
            return record.key < key;
        });
    if (it == records_.end() || it->key < observation.key || observation.key < it->key) {
        if (records_.size() >= kMaxContextualDangerEntries) {
            return {ContextualDangerUpdateReason::CapacityExceeded, false};
        }
        try {
            ContextualDangerRecord record{};
            record.key = observation.key;
            record.risk = observation.risk;
            record.observations = observation.weight;
            record.lastTimeMicros = observation.timeMicros;
            records_.insert(it, record);
            return {ContextualDangerUpdateReason::Accepted, true};
        } catch (...) {
            return {ContextualDangerUpdateReason::CapacityExceeded, false};
        }
    }
    if (observation.timeMicros < it->lastTimeMicros) {
        return {ContextualDangerUpdateReason::StaleObservation, false};
    }
    const double nextObservations = (std::min)(1'000'000.0,
                                               it->observations + observation.weight);
    const double ratio = observation.weight / nextObservations;
    it->risk = clampUnit(it->risk + (observation.risk - it->risk) * ratio);
    it->observations = nextObservations;
    it->lastTimeMicros = observation.timeMicros;
    return {ContextualDangerUpdateReason::Accepted, true};
}

const ContextualDangerRecord* ContextualDangerModel::find(
    const ContextualDangerKey& key) const noexcept {
    const auto it = std::lower_bound(records_.begin(), records_.end(), key,
        [](const ContextualDangerRecord& record, const ContextualDangerKey& value) {
            return record.key < value;
        });
    if (it == records_.end() || it->key < key || key < it->key) return nullptr;
    return &*it;
}

double ContextualDangerModel::risk(const ContextualDangerKey& key) const noexcept {
    const auto* record = find(key);
    return record ? record->risk : 0.0;
}

bool OpponentObservation::valid() const noexcept {
    return player.isValid() && map.isValid() && round.isValid() && tick.isValid() && area != 0U &&
           validWeapon(weapon) && finiteNonNegative(aggression) && aggression <= 1.0 &&
           finiteNonNegative(rushProbability) && rushProbability <= 1.0 &&
           finiteNonNegative(campProbability) && campProbability <= 1.0 &&
           finiteNonNegative(rotationSpeed) && rotationSpeed <= 1.0;
}

bool OpponentProfile::valid() const noexcept {
    return player.isValid() && map.isValid() && round.isValid() && finiteNonNegative(aggression) &&
           aggression <= 1.0 && finiteNonNegative(rushProbability) && rushProbability <= 1.0 &&
           finiteNonNegative(campProbability) && campProbability <= 1.0 &&
           finiteNonNegative(rotationSpeed) && rotationSpeed <= 1.0 &&
           validWeapon(preferredWeapon) && observations <= 1'000'000U;
}

bool OpponentProfileModel::beginMap(MapGeneration map) noexcept {
    if (!map.isValid() || (map_.isValid() && map.value <= map_.value)) return false;
    map_ = map;
    round_ = {};
    profiles_ = {};
    count_ = 0;
    return true;
}

bool OpponentProfileModel::beginRound(perception::RoundGeneration round) noexcept {
    if (!round.isValid() || (round_.isValid() && round.value <= round_.value)) return false;
    round_ = round;
    return true;
}

void OpponentProfileModel::reset() noexcept {
    profiles_ = {};
    map_ = {};
    round_ = {};
    count_ = 0;
}

void OpponentProfileModel::forget(PlayerId player) noexcept {
    if (!player.isValid()) return;
    for (std::size_t i = 0; i < count_; ++i) {
        if (profiles_[i].player != player) continue;
        for (std::size_t next = i + 1; next < count_; ++next) {
            profiles_[next - 1] = profiles_[next];
        }
        profiles_[count_ - 1] = {};
        --count_;
        return;
    }
}

OpponentProfileUpdate OpponentProfileModel::observe(
    const OpponentObservation& observation) noexcept {
    if (!observation.valid()) return {OpponentProfileUpdateReason::InvalidObservation, false};
    if (!map_.isValid() || observation.map != map_) {
        return {OpponentProfileUpdateReason::WrongMap, false};
    }
    if (!round_.isValid() || observation.round != round_) {
        return {OpponentProfileUpdateReason::WrongRound, false};
    }
    OpponentProfile* profile = nullptr;
    for (std::size_t i = 0; i < count_; ++i) {
        if (profiles_[i].player == observation.player) {
            profile = &profiles_[i];
            break;
        }
    }
    if (profile == nullptr) {
        if (count_ >= profiles_.size()) return {OpponentProfileUpdateReason::CapacityExceeded, false};
        profile = &profiles_[count_++];
        profile->player = observation.player;
        profile->map = observation.map;
        profile->round = observation.round;
    }
    const auto next = (std::min)(1'000'000U, profile->observations + 1U);
    const double ratio = 1.0 / static_cast<double>(next);
    profile->aggression += (observation.aggression - profile->aggression) * ratio;
    profile->rushProbability += (observation.rushProbability - profile->rushProbability) * ratio;
    profile->campProbability += (observation.campProbability - profile->campProbability) * ratio;
    profile->rotationSpeed += (observation.rotationSpeed - profile->rotationSpeed) * ratio;
    profile->preferredArea = observation.area;
    if (observation.weapon != combat::WeaponSnapshot::WeaponClass::Unknown) {
        profile->preferredWeapon = observation.weapon;
    }
    profile->observations = next;
    return {OpponentProfileUpdateReason::Accepted, true};
}

const OpponentProfile* OpponentProfileModel::find(PlayerId player) const noexcept {
    for (std::size_t i = 0; i < count_; ++i) {
        if (profiles_[i].player == player) return &profiles_[i];
    }
    return nullptr;
}

WallbangPlan planWallbang(const WallbangInput& input) noexcept {
    WallbangPlan result{};
    if (!input.belief.valid(input.nowMicros) || !finitePoint(input.belief.lastKnownPosition) ||
        input.maxAgeMicros == 0U || input.minimumConfidence <= 0.0 ||
        input.minimumConfidence > 1.0 || !finiteNonNegative(input.wallThickness) ||
        !std::isfinite(input.materialLoss) || input.materialLoss <= 0.0 ||
        !finiteNonNegative(input.weaponPenetration) || !finiteNonNegative(input.friendlyFireRisk) ||
        input.friendlyFireRisk > 1.0 || !finiteNonNegative(input.expectedDamage) ||
        input.ammoCost <= 0 || input.availableAmmo < 0) {
        result.reason = WallbangReason::InvalidInput;
        return result;
    }
    if (input.source != perception::ObservationSource::Vision &&
        input.source != perception::ObservationSource::TeamReport) {
        result.reason = WallbangReason::AnonymousSource;
        return result;
    }
    const auto age = input.nowMicros - input.belief.observedMicros;
    if (age > input.maxAgeMicros) {
        result.reason = WallbangReason::StaleBelief;
        return result;
    }
    if (input.belief.confidence < input.minimumConfidence) {
        result.reason = WallbangReason::LowConfidence;
        return result;
    }
    const double resistance = input.wallThickness * input.materialLoss;
    if (!std::isfinite(resistance) || input.weaponPenetration <= resistance) {
        result.reason = WallbangReason::InsufficientPenetration;
        return result;
    }
    if (input.availableAmmo < input.ammoCost) {
        result.reason = WallbangReason::InsufficientAmmo;
        return result;
    }
    if (input.friendlyFireRisk > 0.5) {
        result.reason = WallbangReason::FriendlyFireRisk;
        return result;
    }
    const double penetration = clampUnit(1.0 - resistance / input.weaponPenetration);
    const double effectiveDamage = input.expectedDamage * input.belief.confidence * penetration;
    const double utility = effectiveDamage / static_cast<double>(input.ammoCost) -
                           input.friendlyFireRisk * input.expectedDamage;
    if (effectiveDamage <= 0.0 || utility <= 0.0) {
        result.reason = WallbangReason::LowExpectedDamage;
        return result;
    }
    result.target = input.belief.target;
    result.aimPoint = input.belief.lastKnownPosition;
    result.penetrationFactor = penetration;
    result.expectedDamage = effectiveDamage;
    result.utility = utility;
    result.reason = WallbangReason::Accepted;
    result.accepted = true;
    return result;
}

SuppressiveFirePlan planSuppressiveFire(const SuppressiveFireInput& input) noexcept {
    SuppressiveFirePlan result{};
    if (input.regionArea == 0U || !finitePoint(input.region) ||
        !finiteNonNegative(input.regionRadius) || input.regionRadius <= 0.0 ||
        !finiteNonNegative(input.probability) || input.probability > 1.0 ||
        !finiteNonNegative(input.minimumProbability) || input.minimumProbability > 1.0 ||
        !finiteNonNegative(input.expectedDamage) || !finiteNonNegative(input.friendlyFireRisk) ||
        input.friendlyFireRisk > 1.0 || input.ammoCost <= 0 || input.availableAmmo < 0) {
        result.reason = SuppressiveFireReason::InvalidInput;
        return result;
    }
    if (input.probability < input.minimumProbability) {
        result.reason = SuppressiveFireReason::LowProbability;
        return result;
    }
    if (input.availableAmmo < input.ammoCost) {
        result.reason = SuppressiveFireReason::InsufficientAmmo;
        return result;
    }
    if (input.friendlyFireRisk > 0.5) {
        result.reason = SuppressiveFireReason::FriendlyFireRisk;
        return result;
    }
    result.regionArea = input.regionArea;
    result.region = input.region;
    result.probability = input.probability;
    result.utility = input.expectedDamage * input.probability *
                     (1.0 - input.friendlyFireRisk);
    result.reason = SuppressiveFireReason::Accepted;
    result.accepted = result.utility > 0.0;
    if (!result.accepted) result.reason = SuppressiveFireReason::LowProbability;
    return result;
}

} // namespace astrabot::core::learning
