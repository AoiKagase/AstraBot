// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/ground_probe.hpp"
#include <array>
#include "nav/local/primitive.hpp"
#include "nav/local/door_wait.hpp"
#include "nav/local/blocker_wait.hpp"
#include "nav/local/crouch.hpp"
#include "nav/local/traversal_constraints.hpp"
#include "nav/local/jump_geometry.hpp"
#include "nav/local/jump_probe.hpp"
#include "nav/local/drop.hpp"
#include "nav/local/ladder.hpp"
#include "nav/local/recovery.hpp"

namespace astrabot::nav::local {
enum class WalkState { Running, Arrived, Failed, Aborted };
enum class MotionDisposition : std::uint8_t { Execute, Hold, Recovery };
enum class WalkReason { None, InvalidInput, StaleTick, InvalidActor, StaleNavigation,
    UnsupportedTraversal, InvalidGoal, OffCorridor, InvalidPortal, ProbeFailed, Cancelled, DoorBlocked, DynamicBlocked, PostureFailed, JumpFailed, LadderFailed, Stuck, RecoveryReplan, AvoidanceCollapsed, InsufficientMovementProof };
enum class AvoidanceReason { None, CandidateCollapsed, CandidateBlocked, CandidateOffCorridor, BudgetExceeded };
enum class ObstacleClass : std::uint8_t { None, Step, Jumpable, TooHigh, Dynamic, Unknown };
enum class JumpCandidateRegionMode : std::uint8_t {
    Margin, PhysicalHull, CentreInset, SuccessorCentreInset, Collapsed, NoBallisticLanding
};
enum class JumpCandidateSwitchReason : std::uint8_t {
    None, PlannedLandingStaticFailure, BallisticLandingMismatch
};
enum class JumpProofPhase : std::uint8_t { None, Prepare, Launch, Land };
enum class JumpProofDeferReason : std::uint8_t { None, ReservedQueries };
struct WalkJumpLimits { JumpLimits motion{}; JumpGeometryLimits geometry{}; JumpProbeLimits flight{}; };
struct WalkLimits {
    GroundProbeLimits probe{};
    double finalApproachRange{}, arrivalTolerance{}, crossingMargin{};
    std::size_t lookAhead{};
    std::uint64_t doorTimeoutUs{}; // Zero disables door handling. Host supplies a finite profile.
    std::uint64_t touchTimeoutUs{}; // Includes the supported approach, one contact attempt and waiting.
    double sideProbeDistance{}, narrowMargin{}, minimumCrossingDistance{}; // Zero side distance disables steering.
    std::uint32_t maxAvoidanceDecisions{};
    BlockerLimits blocker{}; // Zero timeout disables reactive player handling.
    CrouchLimits crouch{}; // Zero timeout keeps special traversal disabled.
    std::optional<WalkJumpLimits> jump{}; // Requires explicit current host physics as well.
    std::optional<DropLimits> drop{}; // Explicit host opt-in; observed gravity required.
    std::optional<LadderLimits> ladder{};
};
struct DoorContact {
    std::uint64_t id{};
    model::NavVector3 end{}; // Bounded trace endpoint, not an ordinary clear movement segment.
};
struct ObservedObstacleHull {
    runtime::HullObservation hull{};
    runtime::QueryStamp stamp{};
    std::uint64_t step{};
};
struct WalkDecision {
    RecoveryDecision recovery{};
    WalkState state{WalkState::Running};
    WalkReason reason{WalkReason::None};
    MotionDisposition disposition{MotionDisposition::Execute};
    bool accepted{}, terminalEvent{};
    Binding binding{};
    core::TickId tick{};
    MovementIntent intent{};
    core::IntentVector progressDirection{}; // Selected corridor direction before lateral steering.
    PrimitiveEvent primitiveEvent{PrimitiveEvent::None};
    std::optional<GroundedTarget> support{}, target{};
    ProbeReason probeReason{ProbeReason::None};
    std::uint32_t queries{}, samples{}, steps{};
    std::optional<DoorWaitState> doorState{};
    DoorWaitReason doorReason{DoorWaitReason::None};
    std::uint64_t doorId{};
    std::optional<DoorContact> contact{}; // Single-frame pulse; host must revalidate before dispatch.
    double leftClearance{}, rightClearance{};
    bool narrow{}, avoiding{};
    AvoidanceReason avoidanceReason{AvoidanceReason::None};
    int avoidanceSide{};
    double avoidanceDistance{};
    std::optional<model::NavVector3> avoidanceCandidate{};
    BlockerAction blockerAction{BlockerAction::Neutral};
    BlockerReason blockerReason{BlockerReason::None};
    std::optional<runtime::BlockerObservation> blocker{};
    std::optional<runtime::HullObservation> obstacleHull{};
    ObstacleClass obstacleClass{ObstacleClass::None};
    bool floorEvidenceValid{};
    double startFloorHeight{}, lastFloorHeight{}, floorDelta{}, cumulativeDownDrop{}, maxDownStep{};
    std::optional<CrouchState> posture{};
    CrouchReason postureReason{CrouchReason::None};
    ConstraintReason constraintReason{ConstraintReason::None};
    std::optional<JumpState> jumpState{};
    std::optional<DropState> dropState{};
    DropReason dropReason{DropReason::None};
    std::optional<DropPlan> dropPlan{};
    JumpReason jumpReason{JumpReason::None};
    JumpProbeReason jumpProbeReason{JumpProbeReason::None};
    ProbeReason jumpSupportReason{ProbeReason::None};
    ProbeReason jumpSupportInitialReason{ProbeReason::None};
    bool jumpSupportFallbackAttempted{};
    bool jumpSupportFallbackAccepted{};
    std::optional<runtime::FloorTraceEvidence> jumpSupportInitialTrace{};
    std::optional<runtime::FloorTraceEvidence> jumpSupportFallbackTrace{};
    std::array<JumpSupportEvidence,3> jumpSupportEvidence{};
    JumpLandingFailure jumpLandingFailure{JumpLandingFailure::None};
    JumpGeometryReason jumpGeometryReason{JumpGeometryReason::None};
    std::optional<JumpPlan> jumpPlan{};
    std::uint8_t jumpCandidateIndex{};
    std::uint8_t jumpCandidateCount{};
    std::optional<model::NavVector3> jumpCandidateLanding{};
    model::NavAreaId jumpCandidateArea{};
    std::uint8_t jumpCandidateAdvance{};
    JumpCandidateRegionMode jumpCandidateRegionMode{JumpCandidateRegionMode::Collapsed};
    std::optional<JumpLandingEnvelope> jumpLandingEnvelope{};
    JumpCandidateSwitchReason jumpCandidateSwitchReason{JumpCandidateSwitchReason::None};
    JumpProofPhase jumpProofPhase{JumpProofPhase::None};
    JumpProofDeferReason jumpProofDeferReason{JumpProofDeferReason::None};
    std::uint32_t jumpLaunchRequiredQueries{};
    std::uint32_t jumpReservedQueries{};
    std::uint32_t jumpLaunchAvailableQueries{};
    bool jumpGuardSuppressed{};
    bool jumpLaunchDeferred{};
    std::optional<JumpPhysics> jumpPhysics{};
    core::TickId jumpPressTick{};
    std::uint64_t jumpAttemptId{}, jumpAttemptStartedUs{};
    JumpProof jumpTakeoffProof{JumpProof::TransientUnknown};
    JumpProof jumpFlightProof{JumpProof::TransientUnknown};
    JumpProof jumpLandingProof{JumpProof::TransientUnknown};
    JumpProofProvenance jumpProofProvenance{JumpProofProvenance::None};
    JumpReadiness jumpReadiness{JumpReadiness::UnderSpeed};
    RecoveryDisposition jumpRecoveryDisposition{RecoveryDisposition::None};
    model::NavVector3 jumpPredictedLanding{};
    double jumpLandingError{};
    bool jumpTrajectoryReady{};
    core::IntentVector jumpAxis{}, jumpCommandDirection{};
    double jumpFromTakeoff{}, jumpAlong{}, jumpLateral{}, jumpMinimumSpeed{},
        jumpMaximumSpeed{}, jumpSpeedLimit{}, jumpDesiredSpeed{}, jumpValidatedDistance{};
    std::uint64_t jumpValidForUs{};
    std::optional<LadderState> ladderState{};
    LadderReason ladderReason{LadderReason::None};
    std::optional<LadderPlan> ladderPlan{};
    core::TickId ladderPressTick{};
};
StuckCause observedStuckCause(const WalkDecision&) noexcept;
// One owned route, synchronous decision seam. Caller schedules decisions and
// invalidates on route replacement; this class never submits a host command.
// A projected endpoint is not arrival: only later measured support advances.
// reservedQueries are same-tick host guard queries already issued (ordinals
// 1..reservedQueries). They count toward the returned total and fixed budget.
class Walk final {
public:
    Walk(Binding, std::shared_ptr<const corridor::Corridor>, model::NavVector3 goal,
         WalkLimits,JumpAttemptRegistry* jumpAttempts=nullptr) noexcept;
    WalkDecision update(const runtime::MovementSnapshot&, const query::NavSpatialIndex&,
                        core::MapGeneration indexMap, runtime::IWorldQueries&,
                        std::uint64_t nowUs=0, std::uint32_t reservedQueries=0,
                        std::optional<JumpPhysics> physics={},std::optional<LadderObservation> ladder={}) noexcept;
    // Only the first result for this step's exact Press command is consumed.
    bool reportJumpDispatch(const JumpDispatch&) noexcept;
    bool reportLadderDispatch(const LadderDispatch&) noexcept;
    std::optional<enrichment::NavTraversalLink> selectedLadderLink() const noexcept;
    model::NavVector3 ladderTarget(const LadderPlan&,model::NavVector3 origin) const noexcept;
    WalkDecision abort() noexcept;
    WalkDecision recover(const runtime::MovementSnapshot&,const query::NavSpatialIndex&,
        core::MapGeneration,runtime::IWorldQueries&,const RecoveryDecision&,std::uint32_t reservedQueries=0) noexcept;
    WalkState state() const noexcept { return state_; }
    bool needsJumpProofBudget() const noexcept {
        if(!jump_) return false;
        const auto jumpState=jump_->state();
        return jumpState==JumpState::Approach || jumpState==JumpState::Align ||
            jumpState==JumpState::Accelerate || jumpState==JumpState::Airborne ||
            jumpState==JumpState::Recover;
    }
    std::size_t step() const noexcept { return cursor_.index(); }
    const corridor::Transition* activeTransition() const noexcept {
        return corridor_ && cursor_.index() < corridor_->transitions().size() ?
            &corridor_->transitions()[cursor_.index()] : nullptr;
    }
private:
    Binding binding_{};
    std::shared_ptr<const corridor::Corridor> corridor_{};
    corridor::Cursor cursor_;
    model::NavVector3 goal_{};
    WalkLimits limits_{};
    core::TickId tick_{};
    WalkState state_{WalkState::Running};
    WalkReason reason_{WalkReason::None};
    Primitive primitive_{};
    std::optional<DoorWait> door_{};
    model::NavVector3 doorStart_{}, doorEnd_{};
    std::uint64_t doorId_{}, lastDoorId_{};
    bool touch_{}, contactSent_{};
    int avoidSide_{};
    int recoverySide_{};
    std::uint32_t avoidDecisions_{};
    std::optional<BlockerWait> blocker_{};
    // Keep the stamped forward obstruction long enough for the next-tick
    // wall-Jump proof.  WalkDecision is frame-local, so copying this fact
    // into the member is required when the proof is deliberately deferred.
    std::optional<ObservedObstacleHull> observedObstacleHull_{};
    std::optional<Crouch> crouch_{};
    ActionRequest postureAction_{ActionRequest::None};
    std::optional<CrouchState> posture_{};
    CrouchReason postureReason_{CrouchReason::None};
    std::optional<SimpleJump> jump_{};
    JumpAttemptRegistry localJumpAttempts_{};
    JumpAttemptRegistry* jumpAttempts_{};
    std::optional<JumpAttemptKey> jumpAttemptKey_{};
    JumpAttemptContext* jumpAttempt_{};
    bool observedJumpCandidate_{};
    std::optional<DropPlan> dropPlan_{};
    DropState dropState_{DropState::Approach};
    std::uint64_t dropStartedUs_{}, dropAirborneUs_{}, dropLastUs_{};
    double dropGravity_{};
    std::optional<JumpPlan> jumpPlan_{};
    std::optional<JumpLandingEnvelope> jumpLandingEnvelope_{};
    static constexpr std::size_t jumpCandidateCapacity_{5};
    std::array<JumpPlan,jumpCandidateCapacity_> jumpCandidates_{};
    std::size_t jumpCandidateCount_{};
    std::size_t jumpCandidateIndex_{};
    JumpCandidateRegionMode jumpCandidateRegionMode_{JumpCandidateRegionMode::Collapsed};
    std::optional<JumpPhysics> jumpPhysics_{};
    std::optional<JumpDispatch> jumpDispatch_{};
    core::TickId jumpPressTick_{};
    bool jumpDispatchSeen_{};
    std::optional<std::size_t> completedJumpStep_{};
    std::optional<Ladder> ladder_{};
    std::optional<LadderPlan> ladderPlan_{};
    bool microTransitValidated_{};
    WalkDecision updateLadder(WalkDecision,const runtime::MovementSnapshot&,const query::NavSpatialIndex&,
        std::uint64_t,std::uint32_t,const std::optional<LadderObservation>&) noexcept;
    WalkDecision updateJump(WalkDecision,const runtime::MovementSnapshot&,const query::NavSpatialIndex&,
        core::MapGeneration,runtime::IWorldQueries&,std::uint64_t,std::uint32_t,std::optional<JumpPhysics>) noexcept;
    WalkDecision updateDrop(WalkDecision,const runtime::MovementSnapshot&,const query::NavSpatialIndex&,
        core::MapGeneration,runtime::IWorldQueries&,std::uint64_t,std::uint32_t,std::optional<JumpPhysics>) noexcept;
    WalkDecision updateMotion(const runtime::MovementSnapshot&,const query::NavSpatialIndex&,
        core::MapGeneration,runtime::IWorldQueries&,std::uint64_t,std::uint32_t,std::optional<JumpPhysics>,std::optional<LadderObservation>) noexcept;
    WalkDecision updateDoor(WalkDecision, const runtime::MovementSnapshot&,
        const query::NavSpatialIndex&, core::MapGeneration, runtime::IWorldQueries&,
        model::NavVector3 end, std::uint64_t nowUs) noexcept;
    WalkDecision approachDoor(WalkDecision, const runtime::MovementSnapshot&,
        const query::NavSpatialIndex&, core::MapGeneration, runtime::IWorldQueries&,
        const runtime::WorldQueryResult&) noexcept;
    WalkDecision finish(WalkDecision, WalkState, WalkReason) noexcept;
};
}
