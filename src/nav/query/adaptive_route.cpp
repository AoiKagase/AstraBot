// SPDX-License-Identifier: MPL-2.0
#include "nav/query/adaptive_route.hpp"

#include <algorithm>
#include <cmath>

namespace astrabot::nav::query {
namespace {

bool finiteNonNegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool known(core::perception::Team team) noexcept {
    return team == core::perception::Team::Unknown ||
           team == core::perception::Team::Terrorist ||
           team == core::perception::Team::CounterTerrorist ||
           team == core::perception::Team::Spectator;
}

struct StyleWeights final {
    double distance{1.0};
    double traversal{1.0};
    double danger{1.0};
    double experience{1.0};
    double exposure{1.0};
    double traffic{1.0};
};

StyleWeights styleWeights(AdaptiveRouteStyle style) noexcept {
    switch (style) {
    case AdaptiveRouteStyle::Fast: return {1.0, 1.0, 0.35, 0.75, 0.5, 0.35};
    case AdaptiveRouteStyle::Safe: return {1.1, 1.25, 3.0, 1.5, 2.0, 1.0};
    case AdaptiveRouteStyle::LowExposure: return {1.1, 1.1, 2.0, 1.0, 3.0, 1.0};
    case AdaptiveRouteStyle::LowTraffic: return {1.05, 1.1, 1.25, 1.0, 1.5, 3.0};
    case AdaptiveRouteStyle::Flank: return {1.05, 1.0, 1.5, 1.0, 2.0, 2.5};
    case AdaptiveRouteStyle::ObjectiveFast: return {0.75, 0.85, 0.5, 0.75, 0.5, 0.5};
    }
    return {};
}

double personalityDanger(AdaptiveRoutePersonality personality) noexcept {
    switch (personality) {
    case AdaptiveRoutePersonality::Aggressive: return 0.5;
    case AdaptiveRoutePersonality::Balanced: return 1.0;
    case AdaptiveRoutePersonality::Cautious: return 2.0;
    }
    return 0.0;
}

double teamDanger(const core::experience::AreaExperience& area,
                  core::perception::Team team) noexcept {
    switch (team) {
    case core::perception::Team::Terrorist: return area.dangerT;
    case core::perception::Team::CounterTerrorist: return area.dangerCT;
    case core::perception::Team::Unknown:
    case core::perception::Team::Spectator: return (std::max)(area.dangerT, area.dangerCT);
    }
    return 0.0;
}

double traversalBase(model::NavTraversalKind kind) noexcept {
    switch (kind) {
    case model::NavTraversalKind::Walk: return 0.0;
    case model::NavTraversalKind::Crouch: return 0.25;
    case model::NavTraversalKind::Jump: return 1.0;
    case model::NavTraversalKind::Ladder: return 1.0;
    case model::NavTraversalKind::Drop: return 1.5;
    }
    return 0.0;
}

const AdaptiveTraversalExperience* traversalEvidence(const AdaptiveRouteContext& context,
                                                     const NavCostContext& cost) noexcept {
    if (!cost.edge.external || !context.traversalExperience) return nullptr;
    const auto id = cost.edge.external->linkId;
    for (std::size_t i = 0; i < context.traversalExperienceCount; ++i) {
        if (context.traversalExperience[i].linkId == id) return &context.traversalExperience[i];
    }
    return nullptr;
}

NavCostDecision invalidCost(const NavCostContext&, const void*) noexcept {
    return {false, {-1.0, 0.0, 0.0, 0.0}};
}

NavCostDecision cost(const NavCostContext& input, const void* opaque) noexcept {
    const auto* context = static_cast<const AdaptiveRouteContext*>(opaque);
    if (!context || !context->settings.valid() || !finiteNonNegative(input.geometricDistance))
        return invalidCost(input, opaque);

    const auto weights = styleWeights(context->settings.style);
    const auto* area = context->experience && context->experience->active()
        ? context->experience->area(input.target.id.value) : nullptr;
    const double danger = area ? teamDanger(*area, context->settings.team) : 0.0;
    double exposure = 0.0;
    if (context->exposureProvider != nullptr) {
        exposure = context->exposureProvider(input, context->exposureContext);
        if (!std::isfinite(exposure) || exposure < 0.0 || exposure > 1.0) {
            return invalidCost(input, opaque);
        }
    }
    const double traffic = area ? area->humanTraffic + area->botTraffic : 0.0;
    const double familiarity = area
        ? 1.0 / (1.0 + area->visits)
        : (context->experience && context->experience->active() ? 1.0 : 0.0);

    double traversal = traversalBase(input.edge.traversal);
    if (input.edge.external) traversal += input.edge.external->additionalCost;
    double traversalExperience = 0.0;
    if (const auto* evidence = traversalEvidence(*context, input)) {
        if (context->settings.learnedTraversalOnly &&
            !evidence->eligible(context->settings.minHumanTraversalAttempts,
                                context->settings.minHumanTraversalSuccessRate)) {
            return {true, {0.0, 0.0, 0.0, 0.0}};
        }
        traversal += evidence->failureRisk() * context->settings.traversalRiskWeight;
        traversalExperience = 1.0 - evidence->successRate();
    } else if (context->settings.learnedTraversalOnly && input.edge.external) {
        return {true, {0.0, 0.0, 0.0, 0.0}};
    }

    const double dangerWeight = context->settings.dangerWeight * weights.danger *
        personalityDanger(context->settings.personality);
    const double contextualDanger = context->contextualDanger
        ? context->contextualDanger->risk({input.target.id.value, context->settings.team,
                                           context->settings.approachDirection,
                                           context->settings.enemyWeaponClass,
                                           context->settings.likelyEnemyArea})
        : 0.0;
    const double learnedDanger = area
        ? danger + area->encounterRate + area->grenadeThreat + area->deathRate
        : 0.0;
    const double dangerCost = (learnedDanger + contextualDanger) * dangerWeight +
        traffic * context->settings.trafficWeight * weights.traffic;
    const double experienceCost = (familiarity + traversalExperience) *
        context->settings.experienceWeight * weights.experience;
    const double exposureCost = exposure * context->settings.exposureWeight * weights.exposure;
    return {false, {input.geometricDistance * context->settings.distanceWeight * weights.distance,
                    traversal * context->settings.traversalWeight * weights.traversal,
                    dangerCost, experienceCost, exposureCost}};
}

} // namespace

bool AdaptiveTraversalExperience::valid() const noexcept {
    return linkId != 0U && finiteNonNegative(humanAttempts) && finiteNonNegative(humanSuccess) &&
           finiteNonNegative(botAttempts) && finiteNonNegative(botSuccess) &&
           finiteNonNegative(failures) && humanSuccess <= humanAttempts && botSuccess <= botAttempts;
}

double AdaptiveTraversalExperience::successRate() const noexcept {
    const double attempts = humanAttempts + botAttempts;
    const double successes = humanSuccess + botSuccess;
    return finiteNonNegative(attempts) && finiteNonNegative(successes) &&
            std::isfinite(attempts + failures) && attempts + failures + 1.0 > 0.0
        ? successes / (attempts + failures + 1.0) : 0.0;
}

double AdaptiveTraversalExperience::failureRisk() const noexcept {
    const double attempts = humanAttempts + botAttempts;
    const double failedAttempts = (std::max)(0.0, attempts - humanSuccess - botSuccess);
    const double denominator = attempts + failures + 1.0;
    return finiteNonNegative(attempts) && finiteNonNegative(failures) &&
            std::isfinite(denominator) && denominator > 0.0
        ? (failedAttempts + failures) / denominator : 0.0;
}

bool AdaptiveTraversalExperience::eligible(std::uint32_t minimumHumanAttempts,
                                           double minimumHumanSuccessRate) const noexcept {
    return valid() && minimumHumanAttempts != 0U &&
           finiteNonNegative(minimumHumanSuccessRate) && minimumHumanSuccessRate <= 1.0 &&
           humanAttempts >= static_cast<double>(minimumHumanAttempts) &&
           humanAttempts > 0.0 && humanSuccess / humanAttempts >= minimumHumanSuccessRate;
}

bool AdaptiveRouteSettings::valid() const noexcept {
    const auto styleValue = static_cast<std::uint8_t>(this->style);
    const auto personalityValue = static_cast<std::uint8_t>(this->personality);
    return styleValue <= static_cast<std::uint8_t>(AdaptiveRouteStyle::ObjectiveFast) &&
           personalityValue <= static_cast<std::uint8_t>(AdaptiveRoutePersonality::Cautious) &&
           known(team) && finiteNonNegative(distanceWeight) && finiteNonNegative(traversalWeight) &&
           finiteNonNegative(dangerWeight) && finiteNonNegative(experienceWeight) &&
           finiteNonNegative(exposureWeight) && finiteNonNegative(trafficWeight) &&
           finiteNonNegative(traversalRiskWeight) && minHumanTraversalAttempts != 0U &&
           minHumanTraversalAttempts <= 1'000'000U &&
           finiteNonNegative(minHumanTraversalSuccessRate) &&
           minHumanTraversalSuccessRate <= 1.0 &&
           static_cast<std::uint8_t>(approachDirection) <=
               static_cast<std::uint8_t>(core::learning::ApproachDirection::Down) &&
           static_cast<std::uint8_t>(enemyWeaponClass) <=
               static_cast<std::uint8_t>(core::combat::WeaponSnapshot::WeaponClass::Sniper);
}

bool AdaptiveRouteContext::valid() const noexcept {
    if (!settings.valid() || (traversalExperienceCount != 0U && !traversalExperience)) return false;
    if (!traversalExperience) return traversalExperienceCount == 0U;
    for (std::size_t i = 0; i < traversalExperienceCount; ++i) {
        if (!traversalExperience[i].valid()) return false;
        for (std::size_t j = 0; j < i; ++j) {
            if (traversalExperience[j].linkId == traversalExperience[i].linkId) return false;
        }
    }
    return true;
}

const char* adaptiveRouteStyleName(AdaptiveRouteStyle style) noexcept {
    switch (style) {
    case AdaptiveRouteStyle::Fast: return "FAST";
    case AdaptiveRouteStyle::Safe: return "SAFE";
    case AdaptiveRouteStyle::LowExposure: return "LOW_EXPOSURE";
    case AdaptiveRouteStyle::LowTraffic: return "LOW_TRAFFIC";
    case AdaptiveRouteStyle::Flank: return "FLANK";
    case AdaptiveRouteStyle::ObjectiveFast: return "OBJECTIVE_FAST";
    }
    return "UNKNOWN";
}

const char* adaptiveRoutePersonalityName(AdaptiveRoutePersonality personality) noexcept {
    switch (personality) {
    case AdaptiveRoutePersonality::Aggressive: return "Aggressive";
    case AdaptiveRoutePersonality::Balanced: return "Balanced";
    case AdaptiveRoutePersonality::Cautious: return "Cautious";
    }
    return "Unknown";
}

NavRoutePolicy adaptiveRoutePolicy(const AdaptiveRouteContext& context) noexcept {
    return {&context, context.valid() ? cost : invalidCost, nullptr};
}

} // namespace astrabot::nav::query
