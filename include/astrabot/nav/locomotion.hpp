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
	ResourceLimit,
	UnsafeDrop
};

struct LocomotionConfig
{
	static constexpr float kMaximumClearance = 4096.0f;
	static constexpr float kMaximumTolerance = 4096.0f;
	static constexpr float kMaximumStepHeight = 256.0f;
	static constexpr float kMaximumJumpHeight = 41.8f;
	// ReGameDLL-CS b0889847 starts fall damage only above 500 units/s.
	static constexpr float kMaximumSafeFallSpeed = 500.0f;
	static constexpr float kMaximumSpeed = 1000.0f;
	static constexpr std::uint32_t kMaximumStuckFrameLimit = 1024U;

	float requiredClearance;
	float targetHorizontalTolerance;
	float targetVerticalTolerance;
	float maximumStepHeight;
	float maximumSpeed;
	std::uint32_t stuckFrameLimit;
	bool averageVelocityStuckMonitor = false;
};

struct GroundProbeObservation
{
	bool sampled;
	bool hasGround;
	float floorZ;
	float normalZ;
};

struct GroundLookaheadObservation
{
	bool valid;
	AreaId targetArea;
	NavVector direction;
	bool running;
	GroundProbeObservation far80;
	GroundProbeObservation near30;
	GroundProbeObservation gap10;
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
	bool clearanceAvailable = false;
	float maximumSafeDropHeight = 0.0f;
	bool safeDropHeightAvailable = false;
	GroundLookaheadObservation groundLookahead = {};
	bool timingAvailable = false;
	float observedAt = 0.0f;
	bool viewYawAvailable = false;
	float viewYaw = 0.0f;
	const NavHuntPathProbe *huntPathProbe = nullptr;
	bool huntForcedRunning = false;
};

struct LocomotionIntent
{
	NavVector direction;
	NavVector targetPosition;
	float speed;
	LocomotionPosture posture;
	TraversalAction traversal;
	bool stepUp;
	AreaId currentArea;
	AreaId targetArea;
	bool nativeLadderControl = false;
	float nativeForward = 0.0f, nativeSide = 0.0f;
	bool nativeWalking = false, nativeMustJump = false, nativeWiggle = false;
	bool walkingToHuntEndpoint = false;
};

class LocomotionController
{
public:
	LocomotionController();
	explicit LocomotionController(const LocomotionConfig &config);

	LocomotionResult start(const NavCorridor &corridor);
	LocomotionResult update(
		const NavSnapshot &snapshot, const LocomotionObservation &observation, LocomotionIntent *intent);
	bool isActive() const;
	void clearPath();
	void observeMotion(const LocomotionObservation &observation);
	bool previewTarget(const NavSnapshot &snapshot, const LocomotionObservation &observation, NavVector *target) const;
	void resetProgressMonitor();
	bool isStuck() const
	{
		return config_.averageVelocityStuckMonitor ? referenceStuck_ : stuckFrames_ >= config_.stuckFrameLimit;
	}
	std::size_t currentCorridorIndex() const;

private:
	static bool isValidConfig(const LocomotionConfig &config);
	static bool isFiniteObservation(const LocomotionObservation &observation);
	static NavVector normalizeDirection(const NavVector &from, const NavVector &to);
	static float horizontalDistance(const NavVector &from, const NavVector &to);
	NavQueryResult findCurrentArea(
		const NavSnapshot &snapshot, const LocomotionObservation &observation, NavAreaMatch *match) const;
	bool recordProgress(const LocomotionObservation &observation, const NavVector &target);
	LocomotionResult buildIntent(const NavSnapshot &snapshot, const LocomotionObservation &observation,
		const NavAreaMatch &currentMatch, const NavVector &target, AreaId targetArea, LocomotionIntent *intent);

	NavPathFollower pathFollower_;
	LocomotionConfig config_;
	NavVector lastPosition_;
	AreaId jumpTargetArea_;
	std::uint32_t stuckFrames_;
	float lastObservedAt_ = 0.0f;
	static constexpr int kVelocitySampleCapacity = 5;
	float velocitySamples_[kVelocitySampleCapacity] = {};
	std::size_t velocitySampleCount_ = 0U;
	std::size_t velocitySampleIndex_ = 0U;
	bool hasLastPosition_;
	bool jumpIssued_;
	bool active_;
	bool referenceStuck_ = false;
	NavVector referenceStuckSpot_ = {};
};
}
}

#endif
