// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/jump_probe.hpp"
namespace astrabot::nav::local {
enum class DropState { Approach, StepOff, Airborne, Landed, Failed };
enum class DropReason { None, Disabled, InvalidInput, MissingObservation, StalePhysics,
    Timeout, MissingSupport, WrongLanding, UnsafeGeometry, UnsafeVelocity, Blocked,
    StaleQuery, QueryFailed, BudgetExceeded };
struct DropLimits {
    double maximumFall{128}, maximumGap{32}, speed{100}, arrivalTolerance{4};
    std::uint64_t approachTimeoutUs{5000000}, airborneTimeoutUs{2000000};
    std::uint32_t maxQueries{21}, maxSegments{12};
};
struct DropPlan {
    model::NavAreaId source{}, target{};
    model::NavVector3 takeoff{}, landing{};
    double fall{}, gap{};
};
}
