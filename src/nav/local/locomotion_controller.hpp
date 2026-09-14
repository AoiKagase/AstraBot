// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/walk.hpp"
#include "nav/local/path_follower.hpp"
#include "nav/local/terrain_sampler.hpp"
#include "nav/local/motion_envelope.hpp"
#include "nav/local/local_door.hpp"

namespace astrabot::nav::local
{
// Native locomotion owns ground, local jump/drop and recovery. Walk is retained
// solely as the temporary comparison implementation and ladder transport owner.
class LocomotionController final
{
public:
	static constexpr std::uint32_t queryLimit = 96, dispatchReserve = 20;
	LocomotionController(Binding, std::shared_ptr<const corridor::Corridor>, model::NavVector3, WalkLimits,
						 JumpAttemptRegistry* = nullptr) noexcept;
	WalkDecision update(const runtime::MovementSnapshot&, const query::NavSpatialIndex&, core::MapGeneration,
						runtime::IWorldQueries&, std::uint64_t = 0, std::uint32_t = 0, std::optional<JumpPhysics> = {},
						std::optional<LadderObservation> = {}) noexcept;
	WalkDecision recover(const runtime::MovementSnapshot&, const query::NavSpatialIndex&, core::MapGeneration,
						 runtime::IWorldQueries&, const RecoveryDecision&, std::uint32_t = 0) noexcept;
	WalkDecision abort() noexcept;
	WalkState state() const noexcept;
	std::size_t step() const noexcept;
	const corridor::Transition* activeTransition() const noexcept;
	bool needsJumpProofBudget() const noexcept;
	bool reportJumpDispatch(const JumpDispatch&) noexcept;
	bool reportLadderDispatch(const LadderDispatch&) noexcept;
	std::optional<enrichment::NavTraversalLink> selectedLadderLink() const noexcept;
	model::NavVector3 ladderTarget(const LadderPlan&, model::NavVector3) const noexcept;
	bool native() const noexcept
	{
		return !comparison_ && !ladderOwned_;
	}
	void stepHeight(double value) noexcept
	{
		limits_.probe.maxStepUp = value;
	}
	const std::optional<MotionEnvelope>& envelope() const noexcept
	{
		return envelope_;
	}
	void feedback(const MovementFeedback&) noexcept;
	ProbeResult guard(const runtime::MovementSnapshot&, const MotionEnvelope&, model::NavVector3,
					  const query::NavSpatialIndex&, core::MapGeneration, runtime::IWorldQueries&, std::uint32_t,
					  std::optional<JumpPhysics>) noexcept;

private:
	Binding binding_{};
	std::shared_ptr<const corridor::Corridor> corridor_{};
	PathFollower follower_;
	Walk legacy_;
	WalkLimits limits_{};
	WalkState state_{WalkState::Running};
	core::TickId tick_{}, pressTick_{}, dispatchTick_{};
	LocomotionPhase phase_{LocomotionPhase::Ground};
	std::optional<MotionEnvelope> envelope_{};
	std::optional<JumpPhysics> flightPhysics_{};
	std::optional<Crouch> posture_{};
	LocalDoor door_{};
	std::optional<BlockerWait> blockerWait_{};
	std::size_t blockerStep_{};
	model::NavVector3 airborneTarget_{}, progressAnchor_{};
	core::IntentVector progressAxis_{};
	double progressBest_{};
	model::NavAreaId airborneArea_{};
	std::uint64_t nowUs_{}, phaseStartedUs_{}, progressUs_{}, blockedUs_{}, sideUntilUs_{};
	std::uint64_t jumpRetryUntilUs_{};
	// Keep a successful jump from being retriggered by the same contact
	// surface immediately after landing. The route step can change while
	// climbing a continuous ramp, so this guard outlives step_.
	std::uint64_t jumpCooldownUntilUs_{};
	int side_{};
	bool comparison_{}, ladderOwned_{}, pressDispatched_{}, progressInitialized_{};
	bool airborneSeen_{}, jumpWasDrop_{};
	std::optional<std::size_t> jumpSuppressedStep_{};
	model::NavVector3 jumpSuppressedOrigin_{};
	bool reachableArea(model::NavAreaId) const noexcept;
	WalkDecision finish(WalkDecision, WalkState, WalkReason) noexcept;
	WalkDecision hold(WalkDecision, ProbeReason) noexcept;
	void issue(WalkDecision&, const runtime::MovementSnapshot&, const GroundedTarget&, core::IntentVector,
			   LocomotionPhase) noexcept;
	WalkDecision airborne(WalkDecision, const runtime::MovementSnapshot&, const query::NavSpatialIndex&,
						  core::MapGeneration, runtime::IWorldQueries&, std::uint32_t);
	bool beginJump(WalkDecision&, const runtime::MovementSnapshot&, const GroundedTarget&, core::IntentVector,
				   const query::NavSpatialIndex&, core::MapGeneration, runtime::IWorldQueries&, std::uint32_t&,
				   JumpPhysics, bool, const runtime::WorldQueryResult*);
};
} // namespace astrabot::nav::local
