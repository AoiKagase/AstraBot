// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "core/experience.hpp"
#include "core/p11_learning.hpp"
#include "core/perception_identity.hpp"
#include "nav/enrichment/traversal_learning.hpp"
#include "nav/query/route_types.hpp"

#include <cstddef>
#include <cstdint>

namespace astrabot::nav::query {

enum class AdaptiveRouteStyle : std::uint8_t {
    Fast,
    Safe,
    LowExposure,
    LowTraffic,
    Flank,
    ObjectiveFast,
};

enum class AdaptiveRoutePersonality : std::uint8_t {
    Aggressive,
    Balanced,
    Cautious,
};

// Evidence for an already-published traversal link. Discovery and persistence
// stay outside this policy; callers may supply evidence as it becomes valid.
struct AdaptiveTraversalExperience final {
    std::uint64_t linkId{0};
    double humanAttempts{0.0};
    double humanSuccess{0.0};
    double botAttempts{0.0};
    double botSuccess{0.0};
    double failures{0.0};

    bool valid() const noexcept;
    double successRate() const noexcept;
    double failureRisk() const noexcept;
    bool eligible(std::uint32_t minimumHumanAttempts,
                  double minimumHumanSuccessRate) const noexcept;
};

struct AdaptiveRouteSettings final {
    AdaptiveRouteStyle style{AdaptiveRouteStyle::Fast};
    AdaptiveRoutePersonality personality{AdaptiveRoutePersonality::Balanced};
    core::perception::Team team{core::perception::Team::Unknown};
    double distanceWeight{1.0};
    double traversalWeight{1.0};
    double dangerWeight{1.0};
    double experienceWeight{1.0};
    double exposureWeight{1.0};
    double trafficWeight{1.0};
    double traversalRiskWeight{1.0};
    bool learnedTraversalOnly{false};
    std::uint32_t minHumanTraversalAttempts{3};
    double minHumanTraversalSuccessRate{0.6};
    core::learning::ApproachDirection approachDirection{
        core::learning::ApproachDirection::Unknown};
    core::combat::WeaponSnapshot::WeaponClass enemyWeaponClass{
        core::combat::WeaponSnapshot::WeaponClass::Unknown};
    std::uint32_t likelyEnemyArea{0};

    bool valid() const noexcept;
};

// All pointers are borrowed for the synchronous NavRouteSearch::search call.
// An absent experience model means that the policy remains a deterministic
// route-style policy over geometry and traversal metadata alone.
struct AdaptiveRouteContext final {
    const core::experience::ExperienceModel* experience{nullptr};
    const core::learning::ContextualDangerModel* contextualDanger{nullptr};
    const AdaptiveTraversalExperience* traversalExperience{nullptr};
    std::size_t traversalExperienceCount{0};
    AdaptiveRouteSettings settings{};

    bool valid() const noexcept;
};

const char* adaptiveRouteStyleName(AdaptiveRouteStyle style) noexcept;
const char* adaptiveRoutePersonalityName(AdaptiveRoutePersonality personality) noexcept;

// The returned policy borrows context. It is intended to be passed directly
// to NavRouteSearch or RouteOptions and not retained after that request.
NavRoutePolicy adaptiveRoutePolicy(const AdaptiveRouteContext& context) noexcept;

} // namespace astrabot::nav::query
