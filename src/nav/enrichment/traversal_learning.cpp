// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/enrichment/traversal_learning.hpp"

#include "nav/query/adaptive_route.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <tuple>

namespace astrabot::nav::enrichment {
namespace {

bool finitePoint(NavLinkPoint point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool finiteNonNegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool validDirection(NavLinkDirection direction) noexcept {
    return direction == NavLinkDirection::Forward || direction == NavLinkDirection::Up ||
           direction == NavLinkDirection::Down;
}

bool validActor(TraversalActor actor) noexcept {
    return actor == TraversalActor::Human || actor == TraversalActor::Bot;
}

std::uint64_t mix(std::uint64_t value) noexcept {
    value ^= value >> 30U;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

void add(std::uint64_t& hash, std::uint64_t value) noexcept {
    hash = mix(hash ^ value);
}

void addPoint(std::uint64_t& hash, NavLinkPoint point) noexcept {
    std::uint64_t x{}, y{}, z{};
    static_assert(sizeof(x) == sizeof(point.x));
    std::memcpy(&x, &point.x, sizeof(x));
    std::memcpy(&y, &point.y, sizeof(y));
    std::memcpy(&z, &point.z, sizeof(z));
    add(hash, x); add(hash, y); add(hash, z);
}

std::uint64_t stableLinkId(const TraversalObservation& observation) noexcept {
    std::uint64_t hash = UINT64_C(0x9e3779b97f4a7c15);
    for (const auto value : observation.fingerprint) add(hash, value);
    add(hash, observation.from.value);
    add(hash, observation.to.value);
    add(hash, static_cast<std::uint8_t>(observation.traversal));
    add(hash, static_cast<std::uint8_t>(observation.direction));
    addPoint(hash, observation.entry);
    addPoint(hash, observation.exit);
    return hash == 0U ? 1U : hash;
}

double distance(NavLinkPoint a, NavLinkPoint b) noexcept {
    return std::hypot(std::hypot(a.x - b.x, a.y - b.y), a.z - b.z);
}

bool samePayload(const NavTraversalLink& left, const NavTraversalLink& right) noexcept {
    return std::tie(left.from, left.to, left.entry.x, left.entry.y, left.entry.z,
                    left.exit.x, left.exit.y, left.exit.z, left.traversal, left.direction,
                    left.additionalCost) ==
           std::tie(right.from, right.to, right.entry.x, right.entry.y, right.entry.z,
                    right.exit.x, right.exit.y, right.exit.z, right.traversal, right.direction,
                    right.additionalCost);
}

} // namespace

bool TraversalObservation::valid() const noexcept {
    if (from.value == 0U || to.value == 0U || from == to || sequence == 0U ||
        !finitePoint(entry) || !finitePoint(exit) || !model::isKnownTraversalKind(traversal) ||
        traversal == model::NavTraversalKind::Walk || !validDirection(direction) ||
        !finiteNonNegative(additionalCost) || !validActor(actor)) {
        return false;
    }
    if (direction == NavLinkDirection::Up && !(exit.z > entry.z)) return false;
    if (direction == NavLinkDirection::Down && !(exit.z < entry.z)) return false;
    return finiteNonNegative(distance(entry, exit));
}

bool TraversalLearningSettings::valid() const noexcept {
    return maxCandidates != 0U && maxCandidates <= 1024U && finiteNonNegative(maxDistance) &&
           maxDistance > 0.0 && maxDistance <= 4096.0 && minHumanAttempts != 0U &&
           minHumanAttempts <= 1'000'000U && finiteNonNegative(minHumanSuccessRate) &&
           minHumanSuccessRate <= 1.0;
}

bool TraversalLearningModel::activate(const NavMapFingerprint& fingerprint,
                                      std::uint64_t sourceId,
                                      std::uint64_t generation) noexcept {
    if (!settings_.valid() || sourceId == 0U || generation == 0U) return false;
    fingerprint_ = fingerprint;
    sourceId_ = sourceId;
    generation_ = generation;
    lastSequence_ = 0U;
    candidates_.clear();
    active_ = true;
    return true;
}

void TraversalLearningModel::reset() noexcept {
    fingerprint_ = {};
    sourceId_ = 0U;
    generation_ = 0U;
    lastSequence_ = 0U;
    candidates_.clear();
    active_ = false;
}

TraversalLearningModel::Candidate* TraversalLearningModel::find(
    std::uint64_t linkId) noexcept {
    const auto it = std::lower_bound(candidates_.begin(), candidates_.end(), linkId,
        [](const Candidate& candidate, std::uint64_t id) { return candidate.link.linkId < id; });
    return it != candidates_.end() && it->link.linkId == linkId ? &*it : nullptr;
}

const TraversalLearningModel::Candidate* TraversalLearningModel::find(
    std::uint64_t linkId) const noexcept {
    const auto it = std::lower_bound(candidates_.begin(), candidates_.end(), linkId,
        [](const Candidate& candidate, std::uint64_t id) { return candidate.link.linkId < id; });
    return it != candidates_.end() && it->link.linkId == linkId ? &*it : nullptr;
}

bool TraversalLearningModel::eligible(const Candidate& candidate) const noexcept {
    if (candidate.humanAttempts < static_cast<double>(settings_.minHumanAttempts)) return false;
    const double rate = candidate.humanAttempts > 0.0
        ? candidate.humanSuccess / candidate.humanAttempts : 0.0;
    return std::isfinite(rate) && rate >= settings_.minHumanSuccessRate;
}

TraversalLearningUpdate TraversalLearningModel::observe(
    const TraversalObservation& observation) noexcept {
    if (!active_ || !observation.valid()) {
        return {TraversalLearningReason::InvalidObservation, 0U, false};
    }
    if (observation.fingerprint != fingerprint_) {
        return {TraversalLearningReason::WrongMap, 0U, false};
    }
    if (observation.sequence <= lastSequence_) {
        return {TraversalLearningReason::StaleSequence, 0U, false};
    }
    lastSequence_ = observation.sequence;
    if (observation.hasNormalConnection) {
        return {TraversalLearningReason::NonDiscovery, 0U, false};
    }
    const auto linkId = stableLinkId(observation);
    NavTraversalLink link{sourceId_, generation_, linkId, observation.from, observation.to,
                          observation.entry, observation.exit, observation.traversal,
                          observation.direction, observation.additionalCost};
    auto* candidate = find(linkId);
    if (candidate == nullptr) {
        if (!observation.succeeded || observation.actor != TraversalActor::Human) {
            return {TraversalLearningReason::NonDiscovery, linkId, false};
        }
        if (observation.hasNormalConnection || distance(observation.entry, observation.exit) >
            settings_.maxDistance) {
            return {TraversalLearningReason::NonDiscovery, linkId, false};
        }
        if (candidates_.size() >= settings_.maxCandidates) {
            return {TraversalLearningReason::CapacityExceeded, linkId, false};
        }
        try {
            Candidate created{};
            created.link = link;
            created.humanAttempts = 1.0;
            created.humanSuccess = 1.0;
            const auto it = std::lower_bound(candidates_.begin(), candidates_.end(), linkId,
                [](const Candidate& value, std::uint64_t id) {
                    return value.link.linkId < id;
                });
            candidates_.insert(it, created);
            return {TraversalLearningReason::Accepted, linkId, true};
        } catch (...) {
            return {TraversalLearningReason::CapacityExceeded, linkId, false};
        }
    }
    if (!samePayload(candidate->link, link)) {
        return {TraversalLearningReason::LinkConflict, linkId, false};
    }
    if (observation.actor == TraversalActor::Human) {
        candidate->humanAttempts += 1.0;
        if (observation.succeeded) candidate->humanSuccess += 1.0;
    } else {
        candidate->botAttempts += 1.0;
        if (observation.succeeded) candidate->botSuccess += 1.0;
    }
    return {TraversalLearningReason::Accepted, linkId, true};
}

NavTraversalLinkSet TraversalLearningModel::activeEnrichment() const {
    NavTraversalLinkSet result{};
    result.fingerprint = fingerprint_;
    for (const auto& candidate : candidates_) {
        if (eligible(candidate)) result.links.push_back(candidate.link);
    }
    return result;
}

bool TraversalLearningModel::experience(
    std::uint64_t linkId, query::AdaptiveTraversalExperience& result) const noexcept {
    const auto* candidate = find(linkId);
    if (!candidate) return false;
    result = {linkId, candidate->humanAttempts, candidate->humanSuccess,
              candidate->botAttempts, candidate->botSuccess, 0.0};
    return true;
}

} // namespace astrabot::nav::enrichment
