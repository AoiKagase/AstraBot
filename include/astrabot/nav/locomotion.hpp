#ifndef ASTRABOT_NAV_LOCOMOTION_HPP
#define ASTRABOT_NAV_LOCOMOTION_HPP

#include "astrabot/nav/nav_query.hpp"

#include <cstdint>

namespace astrabot
{
namespace nav
{
	enum class LocomotionPosture
{
	Standing,
		Crouching
	};

	enum class TraversalAction
	{
		Walk,
		Crouch,
		Step,
		Jump,
		Drop,
		Ladder,
		Door,
		NarrowPassage
	};

enum class LocomotionResult
{
	Started,
	IntentReady,
	TargetReached,
	NeedsRecovery,
	StepTooHigh,
	Stuck,
	Inactive,
	InvalidArgument,
	InvalidConfig,
	InvalidSnapshot,
	InvalidCorridor,
	InvalidObservation,
	InvalidClearance,
	StaleSnapshot,
	ResourceLimit
};

struct LocomotionConfig
{
	static constexpr float kMaximumClearance = 4096.0f;
	static constexpr float kMaximumTolerance = 4096.0f;
	static constexpr float kMaximumStepHeight = 256.0f;
	static constexpr float kMaximumJumpHeight = 41.8f;
	static constexpr float kMaximumSpeed = 1000.0f;
	static constexpr std::uint32_t kMaximumStuckFrameLimit = 1024U;

	float requiredClearance;
	float targetHorizontalTolerance;
	float targetVerticalTolerance;
	float maximumStepHeight;
	float maximumSpeed;
	std::uint32_t stuckFrameLimit;
};

	struct LocomotionObservation
	{
		NavVector position;
		float standingClearance;
		float crouchingClearance;
		NavVector velocity;
	bool grounded;
	bool ducked;
	bool onLadder;
	bool clearanceAvailable;
};

struct LocomotionIntent
{
		NavVector direction;
		float speed;
		LocomotionPosture posture;
		TraversalAction traversal;
		bool stepUp;
	AreaId currentArea;
	AreaId targetArea;
};

class LocomotionController
{
public:
	LocomotionController();
	explicit LocomotionController(const LocomotionConfig &config);

	LocomotionResult start(const NavCorridor &corridor);
	LocomotionResult update(
		const NavSnapshot &snapshot,
		const LocomotionObservation &observation,
		LocomotionIntent *intent);
	bool isActive() const;
	std::size_t currentCorridorIndex() const;

private:
	static bool isValidConfig(const LocomotionConfig &config);
	static bool isFiniteObservation(
		const LocomotionObservation &observation);
	static NavVector normalizeDirection(
		const NavVector &from,
		const NavVector &to);
	static float horizontalDistance(
		const NavVector &from,
		const NavVector &to);
	NavQueryResult findCurrentArea(
		const NavSnapshot &snapshot,
		const LocomotionObservation &observation,
		NavAreaMatch *match) const;
	bool recordProgress(const NavVector &position, const NavVector &target);
	LocomotionResult buildIntent(
		const NavSnapshot &snapshot,
		const LocomotionObservation &observation,
		const NavAreaMatch &currentMatch,
		const NavVector &target,
		AreaId targetArea,
		LocomotionIntent *intent);

	NavPathFollower pathFollower_;
	LocomotionConfig config_;
	NavVector lastPosition_;
	AreaId jumpTargetArea_;
	std::uint32_t stuckFrames_;
	bool hasLastPosition_;
	bool jumpIssued_;
	bool active_;
};
}
}

#endif
