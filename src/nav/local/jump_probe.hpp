// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/simple_jump.hpp"

namespace astrabot::nav::local {
// A trusted host supplies a current constant-gravity, constant-horizontal-speed
// flight model. No default GoldSrc physics, moving platforms or air steering is
// inferred here. Host dispatch must revalidate this model before a Press.
struct JumpPhysics {
    Binding binding{};
    core::TickId tick{};
    double gravity{}, verticalImpulse{};
};
struct JumpProbeLimits {
    std::uint32_t maxQueries{}, maxSegments{};
    double maxSegmentSeconds{}, maxChordRise{}, navTolerance{};
};
enum class JumpProbeReason { None, InvalidInput, StaleNavigation, StalePhysics,
    UnsupportedConstraints, OutsideTakeoff, InvalidVelocity, CannotLand,
    BudgetExceeded, StaleQuery, QueryFailed, InvalidResult, NoSupport, WrongArea, Blocked };
struct JumpProbeResult {
    JumpProbeReason reason{JumpProbeReason::None};
    std::optional<JumpInspection> inspection{};
    std::optional<model::NavVector3> touchdown{};
    double flightSeconds{};
    std::uint32_t queries{}, segments{};
    explicit operator bool() const noexcept { return reason==JumpProbeReason::None && inspection.has_value(); }
};
class JumpProbe final {
public:
    // Launch-only: measured velocity must already meet the SimpleJump profile.
    // Each ordinal is unique; a failure discards all partial clearance evidence.
    // At most 21 queries and 8 segments, with no retries or budget expansion.
    static JumpProbeResult launch(const runtime::MovementSnapshot&, Binding,
        JumpPlan, JumpLimits, JumpPhysics, JumpProbeLimits,
        const query::NavSpatialIndex&, core::MapGeneration indexMap,
        runtime::IWorldQueries&) noexcept;
};
}
