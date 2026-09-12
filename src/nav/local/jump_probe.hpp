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
    std::optional<runtime::HullDimensions> standingHull{}, crouchingHull{};
    double crouchSpeedMultiplier{};
};
// Origin-rise capability from current engine physics. Geometry converts landing
// floor height through the selected flight hull, including midair duck feet lift.
std::optional<JumpLimits> deriveJumpLimits(JumpLimits, const JumpPhysics&,
    const runtime::MovementSnapshot&, TraversalConstraints) noexcept;
struct JumpProbeLimits {
    std::uint32_t maxQueries{}, maxSegments{};
    double maxSegmentSeconds{}, maxChordRise{}, navTolerance{};
    // Supplied by the owning Walk profile for the one permitted source-floor
    // fallback. Zero keeps the offline launch seam fail-closed.
    double supportTolerance{}, supportProbeDepth{};
};
enum class JumpProbeReason { None, InvalidInput, StaleNavigation, StalePhysics,
    UnsupportedConstraints, OutsideTakeoff, InvalidVelocity, CannotLand,
    BudgetExceeded, StaleQuery, QueryFailed, QueryUnavailable, InvalidResult,
    ActorNotGrounded, FloorHeightMismatch, NavContainmentMissing, NoSupport,
    WrongArea, Blocked };
enum class JumpTrajectoryReason : std::uint8_t {
    None, InvalidInput, BallisticSolution, LandingRadius, MaximumDistance
};
struct JumpTrajectoryResult {
    JumpTrajectoryReason reason{JumpTrajectoryReason::InvalidInput};
    model::NavVector3 touchdown{};
    double seconds{}, landingError{};
    explicit operator bool() const noexcept { return reason==JumpTrajectoryReason::None; }
};
// One closed-form model for candidate selection and the live launch proof.
// `horizontalVelocity` is measured at launch or the candidate's intended
// approach velocity; this helper never changes actor velocity.
JumpTrajectoryResult solveJumpTrajectory(model::NavVector3 origin,model::NavVector3 horizontalVelocity,
    model::NavVector3 landing,JumpLimits,JumpPhysics) noexcept;
struct JumpProbeResult {
    JumpProbeReason reason{JumpProbeReason::None};
    ProbeReason supportReason{ProbeReason::None};
    ProbeReason supportInitialReason{ProbeReason::None};
    bool supportFallbackAttempted{};
    bool supportFallbackAccepted{};
    std::optional<runtime::FloorTraceEvidence> supportInitialTrace{};
    std::optional<runtime::FloorTraceEvidence> supportFallbackTrace{};
    std::array<JumpSupportEvidence,3> supportEvidence{};
    JumpLandingFailure landingFailure{JumpLandingFailure::None};
    std::optional<JumpInspection> inspection{};
    std::optional<model::NavVector3> touchdown{};
    double flightSeconds{};
    double landingError{};
    bool trajectoryReady{};
    std::uint32_t queries{}, segments{};
    JumpProof takeoffProof{JumpProof::TransientUnknown};
    JumpProof flightProof{JumpProof::TransientUnknown};
    JumpProof landingProof{JumpProof::TransientUnknown};
    JumpProofProvenance provenance{JumpProofProvenance::None};
    explicit operator bool() const noexcept { return reason==JumpProbeReason::None && inspection.has_value(); }
};
class JumpProbe final {
public:
    // Grounded landing/recovery observation; no commanded or NAV-only arrival.
    static JumpProbeResult land(const runtime::MovementSnapshot&, Binding,
        JumpPlan, JumpLimits, GroundProbeLimits,
        const query::NavSpatialIndex&, core::MapGeneration indexMap,
        runtime::IWorldQueries&) noexcept;
    // Ground-only approach/acceleration proof. It never grants flight clearance.
    // The next command's maximum 120ms translation is supported and source-bound.
    static JumpProbeResult prepare(const runtime::MovementSnapshot&, Binding,
        JumpPlan, JumpLimits, GroundProbeLimits,
        const query::NavSpatialIndex&, core::MapGeneration indexMap,
        runtime::IWorldQueries&) noexcept;
    // Launch-only: measured velocity must already meet the SimpleJump profile.
    // Each ordinal is unique; a failure discards all partial clearance evidence.
    // At most 21 queries and 8 segments, with no retries or budget expansion.
    static JumpProbeResult launch(const runtime::MovementSnapshot&, Binding,
        JumpPlan, JumpLimits, JumpPhysics, JumpProbeLimits,
        const query::NavSpatialIndex&, core::MapGeneration indexMap,
        runtime::IWorldQueries&) noexcept;
};
}
