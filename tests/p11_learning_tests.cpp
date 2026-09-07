// SPDX-License-Identifier: MPL-2.0

#include "core/p11_learning.hpp"
#include "nav/enrichment/traversal_learning.hpp"
#include "nav/query/adaptive_route.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>

namespace {
namespace l = astrabot::core::learning;
namespace c = astrabot::core::combat;
namespace p = astrabot::core::perception;
namespace e = astrabot::nav::enrichment;

void contextualDangerIsBoundedAndDeterministic() {
    l::ContextualDangerModel model;
    const l::ContextualDangerKey key{7, p::Team::Terrorist, l::ApproachDirection::East,
                                     c::WeaponSnapshot::WeaponClass::Rifle, 9};
    assert(key.valid());
    assert(model.observe({key, 0.8, 1.0, 100}).accepted());
    assert(std::abs(model.risk(key) - 0.8) < 0.00001);
    assert(model.observe({key, 0.2, 1.0, 90}).reason ==
           l::ContextualDangerUpdateReason::StaleObservation);
    assert(model.observe({key, 0.2, 1.0, 110}).accepted());
    assert(model.find(key) != nullptr && model.find(key)->observations == 2.0);
    assert(model.risk(key) < 0.8 && model.risk(key) > 0.2);
}

void opponentProfilesAreMapSessionLocal() {
    l::OpponentProfileModel model;
    assert(model.beginMap({1}));
    assert(model.beginRound({1}));
    const l::OpponentObservation observation{
        {2, {1}}, {1}, {1}, {1}, 12, c::WeaponSnapshot::WeaponClass::Rifle,
        0.8, 0.7, 0.1, 0.4};
    assert(model.observe(observation).accepted());
    auto second = observation;
    second.aggression = 0.4;
    second.area = 13;
    second.weapon = c::WeaponSnapshot::WeaponClass::SMG;
    assert(model.observe(second).accepted());
    const auto* profile = model.find(observation.player);
    assert(profile != nullptr && profile->observations == 2U);
    assert(profile->preferredArea == 13U);
    assert(profile->preferredWeapon == c::WeaponSnapshot::WeaponClass::SMG);
    assert(profile->aggression > 0.4 && profile->aggression < 0.8);
    assert(model.beginRound({2}));
    assert(model.size() == 1U);
    assert(model.observe(observation).reason == l::OpponentProfileUpdateReason::WrongRound);

    auto nextRound = observation;
    nextRound.round = {2};
    nextRound.aggression = 0.2;
    assert(model.observe(nextRound).accepted());
    assert(model.find(observation.player)->observations == 3U);

    const auto oldPlayer = observation.player;
    model.forget(oldPlayer);
    assert(model.size() == 0U && model.find(oldPlayer) == nullptr);

    const l::OpponentObservation replacement{
        {2, {2}}, {1}, {2}, {1}, 21, c::WeaponSnapshot::WeaponClass::Pistol,
        0.6, 0.3, 0.2, 0.5};
    assert(model.observe(replacement).accepted());
    model.forget(oldPlayer);
    assert(model.find(replacement.player) != nullptr);
    model.forget(replacement.player);
    assert(model.size() == 0U);

    assert(model.beginMap({2}));
    assert(model.size() == 0U && model.map() == astrabot::core::MapGeneration{2});
    assert(model.beginRound({1}));
    assert(model.observe(observation).reason == l::OpponentProfileUpdateReason::WrongMap);
    auto newMap = replacement;
    newMap.map = {2};
    newMap.round = {1};
    assert(model.observe(newMap).accepted());
}

e::TraversalObservation traversal(std::uint64_t sequence,
                                  bool succeeded,
                                  e::TraversalActor actor = e::TraversalActor::Human) {
    e::TraversalObservation value{};
    value.fingerprint[0] = 0x42;
    value.from = {1}; value.to = {2};
    value.entry = {0, 0, 0}; value.exit = {32, 0, 0};
    value.traversal = astrabot::nav::model::NavTraversalKind::Jump;
    value.direction = e::NavLinkDirection::Forward;
    value.sequence = sequence;
    value.succeeded = succeeded;
    value.actor = actor;
    return value;
}

void traversalLearningRequiresHumanEvidence() {
    e::TraversalLearningModel model;
    e::NavMapFingerprint fingerprint{};
    fingerprint[0] = 0x42;
    assert(model.activate(fingerprint, 5, 2));
    assert(model.observe(traversal(1, true)).accepted());
    assert(model.activeEnrichment().links.empty());
    assert(model.observe(traversal(2, true)).accepted());
    assert(model.observe(traversal(3, true)).accepted());
    auto active = model.activeEnrichment();
    assert(active.links.size() == 1U);
    assert(active.links[0].sourceId == 5U && active.links[0].generation == 2U);
    assert(active.links[0].from == astrabot::nav::model::NavAreaId{1});

    astrabot::nav::query::AdaptiveTraversalExperience evidence{};
    assert(model.experience(active.links[0].linkId, evidence));
    assert(evidence.humanAttempts == 3.0 && evidence.humanSuccess == 3.0);
    assert(evidence.eligible(3, 0.6));

    auto normal = traversal(4, true);
    normal.hasNormalConnection = true;
    assert(model.observe(normal).reason == e::TraversalLearningReason::NonDiscovery);
    assert(model.observe(traversal(3, true)).reason == e::TraversalLearningReason::StaleSequence);
}

void wallbangUsesOnlyFreshNonAnonymousBelief() {
    l::WallbangInput input{};
    input.belief.target = {2, {1}};
    input.belief.lastKnownPosition = {100, 200, 30};
    input.belief.observedMicros = 100;
    input.belief.confidence = 0.8;
    input.belief.known = true;
    input.source = p::ObservationSource::TeamReport;
    input.nowMicros = 200;
    input.wallThickness = 10;
    input.materialLoss = 1;
    input.weaponPenetration = 20;
    input.friendlyFireRisk = 0.1;
    input.expectedDamage = 100;
    input.availableAmmo = 5;
    auto plan = l::planWallbang(input);
    assert(plan.accepted && plan.mode == c::FireMode::Wallbang);
    assert(plan.target == input.belief.target && plan.aimPoint.x == 100);
    assert(plan.expectedDamage > 0.0 && plan.utility > 0.0);

    input.source = p::ObservationSource::Sound;
    assert(l::planWallbang(input).reason == l::WallbangReason::AnonymousSource);
    input.source = p::ObservationSource::Vision;
    input.wallThickness = 30;
    assert(l::planWallbang(input).reason == l::WallbangReason::InsufficientPenetration);
}

void suppressiveFireTargetsARegion() {
    l::SuppressiveFireInput input{};
    input.regionArea = 4;
    input.region = {50, 60, 10};
    input.regionRadius = 96;
    input.probability = 0.75;
    input.expectedDamage = 40;
    input.availableAmmo = 10;
    const auto plan = l::planSuppressiveFire(input);
    assert(plan.accepted && plan.mode == c::FireMode::SuppressiveFire);
    assert(plan.regionArea == 4U && plan.utility > 0.0);
    auto low = input;
    low.probability = 0.2;
    assert(l::planSuppressiveFire(low).reason == l::SuppressiveFireReason::LowProbability);
}

} // namespace

int main() {
    contextualDangerIsBoundedAndDeterministic();
    opponentProfilesAreMapSessionLocal();
    traversalLearningRequiresHumanEvidence();
    wallbangUsesOnlyFreshNonAnonymousBelief();
    suppressiveFireTargetsARegion();
}
