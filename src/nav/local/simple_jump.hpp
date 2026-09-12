// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"
#include "nav/local/ground_probe.hpp"
#include "nav/local/traversal_constraints.hpp"
#include <array>
namespace astrabot::nav::local {
enum class JumpState { Approach, Align, Accelerate, Takeoff, Airborne, Recover, Complete, Failed, Aborted };
enum class JumpReason { None, InvalidInput, InvalidActor, StaleTick, StaleInspection,
    MissingObservation, MissingSupport, Blocked, OutsideTakeoff, MissingDispatch,
    StaleDispatch, DispatchRejected, TakeoffTimeout, AirborneTimeout, ApproachTimeout,
    WrongLanding, LostSupport, ProofTimeout, VelocityTimeout, NoLandingCandidate, Cancelled };
enum class JumpProof : std::uint8_t { Passed, TransientUnknown, StaticBlocked };
enum class JumpProofProvenance : std::uint8_t {
    None, GroundProbe, SweptHull, StaticBsp, DynamicBlocker, QueryBudget,
    QueryUnavailable, Stale
};
enum class JumpReadiness : std::uint8_t {
    Ready, UnderSpeed, LateralError, OverSpeed, SpeedLimitInsufficient
};
enum class RecoveryDisposition : std::uint8_t {
    None, Hold, Retry, EdgeCooldown, StructuralEdgeExclusion
};
enum class JumpSupportRole : std::uint8_t {
    Source, PlannedLanding, PredictedLanding
};
enum class JumpLandingFailure : std::uint8_t {
    None, PlannedSupport, PredictedSupport, BallisticSolution,
    LandingRadius, MaximumDistance, FloorHeightMismatch
};
// Evidence is retained per physical support probe.  In particular, a
// successful source probe must never hide a failed planned/predicted landing.
struct JumpSupportEvidence {
    JumpSupportRole role{JumpSupportRole::Source};
    model::NavVector3 origin{};
    model::NavAreaId expectedArea{};
    std::optional<runtime::HullDimensions> hull{};
    ProbeReason initialReason{ProbeReason::None};
    ProbeReason fallbackReason{ProbeReason::None};
    ProbeReason reason{ProbeReason::None};
    bool fallbackAttempted{};
    bool fallbackAccepted{};
    std::optional<runtime::FloorTraceEvidence> initialTrace{};
    std::optional<runtime::FloorTraceEvidence> fallbackTrace{};
};
struct JumpPlan {
    model::NavAreaId source{}, target{};
    model::NavVector3 takeoff{}, landing{}; // Origins in observed launch and selected landing poses, not area centers.
    std::uint8_t sourceAttributes{}, targetAttributes{};
    // Landing/airborne hull, when a crouch hint requires a midair shrink.
    std::optional<runtime::HullDimensions> flightHull{};
    // target remains the directed edge used for recovery.  landingArea may be
    // the immediately following, route-contiguous area only when a micro
    // target patch cannot hold a ballistic landing.  The cursor validates the
    // exact number of completed transitions before accepting it.
    model::NavAreaId landingArea{};
    std::uint8_t landingAdvance{1};
};
inline model::NavAreaId jumpLandingArea(const JumpPlan& plan) noexcept {
    return plan.landingArea.isValid() ? plan.landingArea:plan.target;
}
inline std::uint8_t jumpLandingAdvance(const JumpPlan& plan) noexcept {
    return plan.landingAdvance==2 ? 2:1;
}
struct JumpLimits {
    double approachSpeed{}, minimumSpeed{}, maximumSpeed{}, takeoffRadius{}, landingRadius{}, facingDegrees{},
        maximumDistance{}, maximumRise{}, supportTolerance{};
    std::uint32_t maxQueries{};
    std::uint64_t approachTimeoutUs{}, takeoffTimeoutUs{}, airborneTimeoutUs{}, cooldownUs{};
    std::optional<runtime::HullDimensions> flightHull{};
    std::optional<runtime::HullDimensions> standingHull{};
};
struct JumpAttemptKey {
    core::BotAgentId agent{};
    core::PlayerId actor{};
    core::MapGeneration map{};
    model::NavAreaId source{}, target{};
    model::NavTraversalKind traversal{model::NavTraversalKind::Jump};
    friend bool operator==(const JumpAttemptKey& a,const JumpAttemptKey& b) noexcept {
        return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
            a.source==b.source && a.target==b.target && a.traversal==b.traversal;
    }
};
struct JumpAttemptContext {
    JumpAttemptKey key{};
    std::uint64_t attemptId{}, startedUs{};
    core::TickId pressTick{};
    bool pressed{};
};
class JumpAttemptRegistry final {
public:
    JumpAttemptContext& acquire(JumpAttemptKey,std::uint64_t nowUs) noexcept;
    void erase(JumpAttemptKey) noexcept;
    void clear() noexcept;
private:
    static constexpr std::size_t capacity=8;
    std::array<JumpAttemptContext,capacity> attempts_{};
    std::uint64_t nextAttemptId_{};
};
// Produced by a trusted, bounded world-query planner, never by NAV hints alone.
// One current-stamped batch owns support and optional approach/flight evidence.
// This controller validates ownership/geometry; it does not manufacture proof.
struct JumpInspection {
    runtime::QueryStamp stamp{}; // Batch ordinal zero.
    std::size_t step{};
    std::uint32_t queries{};
    model::NavVector3 origin{};
    runtime::HullDimensions hull{};
    std::optional<model::NavVector3> velocity{}; // Required for launch clearance.
    std::optional<GroundedTarget> support{}, approach{};
    std::array<JumpSupportEvidence,3> supportEvidence{};
    JumpLandingFailure landingFailure{JumpLandingFailure::None};
    std::optional<bool> approachClear{}, takeoffClear{}, flightClear{}, landingClear{};
    std::uint64_t attemptId{};
    JumpProof approachProof{JumpProof::TransientUnknown};
    JumpProof takeoffProof{JumpProof::TransientUnknown};
    JumpProof flightProof{JumpProof::TransientUnknown};
    JumpProof landingProof{JumpProof::TransientUnknown};
    JumpProofProvenance provenance{JumpProofProvenance::None};
    double validatedDistance{};
    std::uint64_t validForUs{};
    model::NavVector3 takeoff{}, landing{};
    model::NavVector3 predictedLanding{};
    double landingError{};
    bool trajectoryReady{};
};
struct JumpDispatch {
    Binding binding{};
    core::TickId commandTick{}, dispatchTick{};
    bool dispatched{};
};
struct JumpFeedback {
    Binding binding{};
    runtime::MovementSnapshot movement{};
    std::uint64_t nowUs{};
    std::optional<JumpInspection> inspection{};
    std::optional<JumpDispatch> dispatch{};
    JumpProof takeoffProof{JumpProof::TransientUnknown};
    JumpProof flightProof{JumpProof::TransientUnknown};
    JumpProof landingProof{JumpProof::TransientUnknown};
    JumpProofProvenance proofProvenance{JumpProofProvenance::None};
};
struct JumpDecision {
    JumpState state{JumpState::Approach};
    JumpReason reason{JumpReason::None};
    bool accepted{}, terminalEvent{};
    MovementIntent intent{};
    core::TickId pressTick{};
    std::uint64_t attemptId{}, attemptStartedUs{};
    JumpProof takeoffProof{JumpProof::TransientUnknown};
    JumpProof flightProof{JumpProof::TransientUnknown};
    JumpProof landingProof{JumpProof::TransientUnknown};
    JumpProofProvenance proofProvenance{JumpProofProvenance::None};
    JumpReadiness readiness{JumpReadiness::UnderSpeed};
    RecoveryDisposition recoveryDisposition{RecoveryDisposition::None};
    core::IntentVector jumpAxis{}, commandDirection{};
    double fromTakeoff{}, along{}, lateral{}, minimumSpeed{}, maximumSpeed{},
        speedLimit{}, desiredSpeed{}, validatedDistance{};
    std::uint64_t validForUs{};
};
class SimpleJump final {
public:
    SimpleJump(Binding binding,JumpPlan plan,JumpLimits limits,
               JumpAttemptContext* attempt=nullptr) noexcept
        : binding_(binding),plan_(plan),limits_(limits),attempt_(attempt) {}
    JumpDecision update(const JumpFeedback&) noexcept;
    JumpDecision abort() noexcept;
    JumpState state() const noexcept { return state_; }
private:
    Binding binding_{};
    JumpPlan plan_{};
    JumpLimits limits_{};
    JumpState state_{JumpState::Approach};
    core::TickId tick_{},pressTick_{};
    std::uint64_t lastUs_{},startedUs_{},phaseUs_{};
    bool started_{},dispatched_{};
    double flightSpeed_{};
    JumpAttemptContext localAttempt_{};
    JumpAttemptContext* attempt_{};
    JumpDecision result(JumpReason=JumpReason::None) const noexcept;
    JumpDecision finish(JumpState,JumpReason) noexcept;
};
}
