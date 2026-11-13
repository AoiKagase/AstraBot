#include "astrabot/nav/locomotion.hpp"

#include <algorithm>
#include <cmath>

namespace astrabot
{
namespace nav
{
namespace
{
constexpr float kMinimumProgressDistance = 0.01f;
constexpr float kMaximumRecoveryDistance = 256.0f;
// Ignore sub-unit NAV rounding when neighboring areas share an edge.
constexpr float kMinimumDropGap = 1.0f;
// Matches ZBot's far ground-probe range in MoveTowardsPosition().
constexpr float kMaximumGapJumpLookAhead = 80.0f;
constexpr float kStuckEscapeDistance = 75.0f;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_LOCOMOTION_REFERENCE_FULL_THINK_INTERVAL_SECONDS 0.1f
constexpr auto kVelocitySampleCount = 5U;
constexpr float kVelocitySampleDivisor = 5.0f;
constexpr float kLadderStuckSpeed = 10.0f;
constexpr float kGroundStuckSpeed = 20.0f;
#define ASTRABOT_LOCOMOTION_MINIMUM_UNIT_DIRECTION_LENGTH 0.9f
#define ASTRABOT_LOCOMOTION_MAXIMUM_UNIT_DIRECTION_LENGTH 1.1f
#define ASTRABOT_LOCOMOTION_MINIMUM_LOOKAHEAD_ALIGNMENT 0.9f
#define ASTRABOT_LOCOMOTION_MINIMUM_JUMP_GROUND_NORMAL_Z 0.9f
constexpr float kNearGroundProbeDistance = 30.0f;

LocomotionResult mapFollowerResult(NavFollowerResult result)
{
	switch (result)
	{
	case NavFollowerResult::TargetReady:
	case NavFollowerResult::Advanced:
		return LocomotionResult::IntentReady;
	case NavFollowerResult::Reached:
		return LocomotionResult::TargetReached;
	case NavFollowerResult::InvalidArgument:
		return LocomotionResult::InvalidArgument;
	case NavFollowerResult::InvalidSnapshot:
		return LocomotionResult::InvalidSnapshot;
	case NavFollowerResult::InvalidCorridor:
		return LocomotionResult::InvalidCorridor;
	case NavFollowerResult::InvalidPosition:
		return LocomotionResult::InvalidObservation;
	case NavFollowerResult::InvalidTolerance:
		return LocomotionResult::InvalidConfig;
	case NavFollowerResult::StaleSnapshot:
		return LocomotionResult::StaleSnapshot;
	case NavFollowerResult::ResourceLimit:
		return LocomotionResult::ResourceLimit;
	case NavFollowerResult::Started:
	case NavFollowerResult::Inactive:
		return LocomotionResult::Inactive;
	}
	return LocomotionResult::InvalidObservation;
}

bool hasDirectedConnection(const NavArea &from, AreaId to)
{
	for (const std::vector<AreaId> &connections : from.connections)
	{
		if (std::find(connections.begin(), connections.end(), to) != connections.end())
		{
			return true;
		}
	}
	return false;
}

float axisGap(float firstLo, float firstHi, float secondLo, float secondHi)
{
	if (firstHi < secondLo)
	{
		return secondLo - firstHi;
	}
	if (secondHi < firstLo)
	{
		return firstLo - secondHi;
	}
	return 0.0f;
}

float areaGap(const NavArea &from, const NavArea &to)
{
	const float gapX = axisGap(from.extent.lo.x, from.extent.hi.x, to.extent.lo.x, to.extent.hi.x);
	const float gapY = axisGap(from.extent.lo.y, from.extent.hi.y, to.extent.lo.y, to.extent.hi.y);
	return std::hypot(gapX, gapY);
}
}

LocomotionController::LocomotionController()
	: pathFollower_(), config_{32.0f, 20.0f, 1.0f, 16.0f, 240.0f, 8U}, lastPosition_{0.0f, 0.0f, 0.0f},

	  jumpTargetArea_(0U), stuckFrames_(0U), hasLastPosition_(false), jumpIssued_(false), active_(false)
{
}

LocomotionController::LocomotionController(const LocomotionConfig &config)
	: pathFollower_(), config_(config), lastPosition_{0.0f, 0.0f, 0.0f}, jumpTargetArea_(0U), stuckFrames_(0U),

	  hasLastPosition_(false), jumpIssued_(false), active_(false)
{
}

LocomotionResult LocomotionController::start(const NavCorridor &corridor)
{
	active_ = false;
	jumpTargetArea_ = 0U;
	jumpIssued_ = false;
	if (!config_.averageVelocityStuckMonitor)
		resetProgressMonitor();
	if (!isValidConfig(config_))
	{
		return LocomotionResult::InvalidConfig;
	}

	const NavFollowerResult followerResult = pathFollower_.start(corridor);
	if (followerResult != NavFollowerResult::Started)
	{
		if (followerResult == NavFollowerResult::ResourceLimit)
		{
			return LocomotionResult::ResourceLimit;
		}
		return LocomotionResult::InvalidCorridor;
	}

	active_ = true;
	return LocomotionResult::Started;
}

LocomotionResult LocomotionController::update(
	const NavSnapshot &snapshot, const LocomotionObservation &observation, LocomotionIntent *intent)
{
	if (intent == nullptr)
	{
		return LocomotionResult::InvalidArgument;
	}
	*intent = {};
	if (!active_)
	{
		return LocomotionResult::Inactive;
	}
	if (!snapshot.isValid())
	{
		active_ = false;
		return LocomotionResult::InvalidSnapshot;
	}
	if (!isFiniteObservation(observation))
	{
		return LocomotionResult::InvalidObservation;
	}
	if (!std::isfinite(config_.requiredClearance) || config_.requiredClearance < 0.0f)
	{
		return LocomotionResult::InvalidClearance;
	}

	NavAreaMatch currentMatch = {};
	const NavQueryResult queryResult = findCurrentArea(snapshot, observation, &currentMatch);
	if (queryResult != NavQueryResult::Found)
	{
		if (queryResult == NavQueryResult::EmptySnapshot)
		{
			active_ = false;
			return LocomotionResult::InvalidSnapshot;
		}
		return LocomotionResult::InvalidObservation;
	}

	NavVector target = {};
	AreaId targetArea = 0U;
	const NavFollowerResult followerResult = pathFollower_.update(snapshot, observation.position,
		config_.targetHorizontalTolerance, config_.targetVerticalTolerance, &target, &targetArea);
	const LocomotionResult mappedResult = mapFollowerResult(followerResult);
	if (mappedResult == LocomotionResult::StaleSnapshot || mappedResult == LocomotionResult::InvalidSnapshot ||
		mappedResult == LocomotionResult::InvalidCorridor || mappedResult == LocomotionResult::ResourceLimit)
	{
		active_ = false;
		return mappedResult;
	}
	if (mappedResult == LocomotionResult::TargetReached)
	{
		active_ = false;
		return mappedResult;
	}
	if (mappedResult != LocomotionResult::IntentReady)
	{
		return mappedResult;
	}

	return buildIntent(snapshot, observation, currentMatch, target, targetArea, intent);
}

NavQueryResult LocomotionController::findCurrentArea(
	const NavSnapshot &snapshot, const LocomotionObservation &observation, NavAreaMatch *match) const
{
	NavQuery query(snapshot);
	const NavQueryResult containingResult =
		query.findContaining(observation.position, config_.targetVerticalTolerance, match);
	if (containingResult != NavQueryResult::NoAreaContaining)
	{
		return containingResult;
	}
	return query.findNearest(observation.position, kMaximumRecoveryDistance, match);
}

bool LocomotionController::recordProgress(const LocomotionObservation &observation, const NavVector &target)
{
	const NavVector &position = observation.position;
	if (observation.timingAvailable && hasLastPosition_ && observation.observedAt < lastObservedAt_)
		resetProgressMonitor();
	if (!observation.grounded && !observation.onLadder)
	{
		// Airborne motion must not start a ground escape manoeuvre, even when
		// horizontal displacement temporarily reaches zero at an obstruction.
		lastPosition_ = position;
		lastObservedAt_ = observation.observedAt;
		hasLastPosition_ = true;
		stuckFrames_ = 0U;
		velocitySampleCount_ = 0U;
		velocitySampleIndex_ = 0U;
		return false;
	}
	if (config_.averageVelocityStuckMonitor && observation.timingAvailable && hasLastPosition_ &&
		observation.observedAt == lastObservedAt_)
		return referenceStuck_;
	if (config_.averageVelocityStuckMonitor && observation.timingAvailable && referenceStuck_)
	{
		const float x = position.x - referenceStuckSpot_.x, y = position.y - referenceStuckSpot_.y,
					z = position.z - referenceStuckSpot_.z;
		if (x * x + y * y + z * z <= kStuckEscapeDistance * kStuckEscapeDistance)
			return true;
		resetProgressMonitor();
	}
	if (config_.averageVelocityStuckMonitor && observation.timingAvailable)
	{
		if (hasLastPosition_ && observation.observedAt > lastObservedAt_)
		{
			const float dx = position.x - lastPosition_.x;
			const float dy = position.y - lastPosition_.y;
			const float dz = observation.grounded || observation.onLadder ? position.z - lastPosition_.z : 0.0f;
			velocitySamples_[velocitySampleIndex_] = std::sqrt(dx * dx + dy * dy + dz * dz) /
				ASTRABOT_LOCOMOTION_REFERENCE_FULL_THINK_INTERVAL_SECONDS; // CSBot g_flBotFullThinkInterval, including delayed full thinks.
			velocitySampleIndex_ = (velocitySampleIndex_ + 1U) % kVelocitySampleCount;
			if (velocitySampleCount_ < kVelocitySampleCount)
				++velocitySampleCount_;
		}
		else if (hasLastPosition_ && observation.observedAt < lastObservedAt_)
		{
			velocitySampleCount_ = 0U;
			velocitySampleIndex_ = 0U;
		}
		lastPosition_ = position;
		lastObservedAt_ = observation.observedAt;
		hasLastPosition_ = true;
		if (velocitySampleCount_ < kVelocitySampleCount)
			return false;
		float total = 0.0f;
		for (float speed : velocitySamples_)
			total += speed;
		referenceStuck_ =
			total / kVelocitySampleDivisor < (observation.onLadder ? kLadderStuckSpeed : kGroundStuckSpeed);
		if (referenceStuck_)
			referenceStuckSpot_ = position;
		return referenceStuck_;
	}
	if (hasLastPosition_)
	{
		const NavVector direction = normalizeDirection(position, target);
		const float deltaX = position.x - lastPosition_.x;
		const float deltaY = position.y - lastPosition_.y;
		const float forwardProgress = deltaX * direction.x + deltaY * direction.y;
		if (!std::isfinite(forwardProgress) || forwardProgress < kMinimumProgressDistance)
		{
			++stuckFrames_;
		}
		else
		{
			stuckFrames_ = 0U;
		}
	}
	lastPosition_ = position;
	hasLastPosition_ = true;
	return stuckFrames_ >= config_.stuckFrameLimit;
}

LocomotionResult LocomotionController::buildIntent(const NavSnapshot &snapshot,
	const LocomotionObservation &observation, const NavAreaMatch &currentMatch, const NavVector &target,
	AreaId targetArea, LocomotionIntent *intent)
{
	const NavDocument *document = snapshot.document();
	const NavArea *currentArea = document->findArea(currentMatch.area);
	const NavArea *destinationArea = document->findArea(targetArea);
	if (currentArea == nullptr || destinationArea == nullptr)
	{
		active_ = false;
		return LocomotionResult::InvalidCorridor;
	}
	if (currentArea->id != destinationArea->id && !hasDirectedConnection(*currentArea, destinationArea->id))
	{
		active_ = false;
		return LocomotionResult::InvalidCorridor;
	}

	const float currentSurfaceHeight = surfaceZAt(*currentArea, target.x, target.y);
	const float stepHeight = target.z - currentSurfaceHeight;
	const bool noJump = (currentArea->attributes & NavArea::kNoJump) != 0U;
	const bool navJump = !noJump && (destinationArea->attributes & NavArea::kJump) != 0U &&
		stepHeight <= LocomotionConfig::kMaximumJumpHeight;
	const NavVector requestedDirection = normalizeDirection(observation.position, target);
	const float lookaheadDirectionLength =
		std::hypot(observation.groundLookahead.direction.x, observation.groundLookahead.direction.y);
	const float lookaheadDirectionDot = observation.groundLookahead.direction.x * requestedDirection.x +
		observation.groundLookahead.direction.y * requestedDirection.y;
	const bool lookaheadMatches = observation.groundLookahead.valid &&
		observation.groundLookahead.targetArea == targetArea && std::isfinite(lookaheadDirectionLength) &&
		lookaheadDirectionLength >= ASTRABOT_LOCOMOTION_MINIMUM_UNIT_DIRECTION_LENGTH &&
		lookaheadDirectionLength <= ASTRABOT_LOCOMOTION_MAXIMUM_UNIT_DIRECTION_LENGTH &&
		lookaheadDirectionDot / lookaheadDirectionLength >= ASTRABOT_LOCOMOTION_MINIMUM_LOOKAHEAD_ALIGNMENT;
	const float currentFloor = surfaceZAt(*currentArea, observation.position.x, observation.position.y);
	const auto probeFloorZ = [&](const GroundProbeObservation &probe, float distance)
	{
		const float probeX = observation.position.x + observation.groundLookahead.direction.x * distance;
		const float probeY = observation.position.y + observation.groundLookahead.direction.y * distance;
		const bool overlapsCurrentArea = probeX >= currentArea->extent.lo.x && probeX <= currentArea->extent.hi.x &&
			probeY >= currentArea->extent.lo.y && probeY <= currentArea->extent.hi.y;
		return overlapsCurrentArea ? (std::max)(probe.floorZ, surfaceZAt(*currentArea, probeX, probeY)) : probe.floorZ;
	};
	const bool farDropAhead = lookaheadMatches && observation.groundLookahead.running &&
		observation.groundLookahead.far80.sampled && observation.groundLookahead.far80.hasGround &&
		observation.groundLookahead.far80.normalZ > ASTRABOT_LOCOMOTION_MINIMUM_JUMP_GROUND_NORMAL_Z &&
		currentFloor - probeFloorZ(observation.groundLookahead.far80, kMaximumGapJumpLookAhead) >
			LocomotionConfig::kMaximumJumpHeight;
	const bool gapAtTen =
		lookaheadMatches && observation.groundLookahead.gap10.sampled && !observation.groundLookahead.gap10.hasGround;
	// CSBot b0889847 cs_bot_nav.cpp:229-238 falls back to the near probe
	// when the far probe is missing or sloped. Keep known-drop safety vetoes.
	const bool lookaheadJump = lookaheadMatches && !noJump && observation.grounded && !observation.onLadder &&
		observation.groundLookahead.near30.sampled && observation.groundLookahead.near30.hasGround &&
		probeFloorZ(observation.groundLookahead.near30, kNearGroundProbeDistance) - currentFloor >
			config_.maximumStepHeight &&
		probeFloorZ(observation.groundLookahead.near30, kNearGroundProbeDistance) - currentFloor <=
			LocomotionConfig::kMaximumJumpHeight &&
		!farDropAhead && !gapAtTen;
	const bool terrainJump = !noJump && !navJump && observation.grounded && !observation.onLadder &&
		stepHeight > config_.maximumStepHeight && stepHeight <= LocomotionConfig::kMaximumJumpHeight;
	const float transitionGap = areaGap(*currentArea, *destinationArea);
	const float dropHeight = currentSurfaceHeight - target.z;
	const NavVector launchApproachPoint = {
		(std::max)(currentArea->extent.lo.x, (std::min)(target.x, currentArea->extent.hi.x)),
		(std::max)(currentArea->extent.lo.y, (std::min)(target.y, currentArea->extent.hi.y)), observation.position.z};
	const bool descendingGap = !navJump && observation.grounded && !observation.onLadder &&
		dropHeight > LocomotionConfig::kMaximumJumpHeight &&
		horizontalDistance(observation.position, launchApproachPoint) <= kMaximumGapJumpLookAhead;
	const bool safeDescendingDrop = descendingGap && observation.safeDropHeightAvailable &&
		std::isfinite(observation.maximumSafeDropHeight) && observation.maximumSafeDropHeight >= 0.0f &&
		dropHeight <= observation.maximumSafeDropHeight;
	const bool hasHorizontalGap = transitionGap > kMinimumDropGap;
	const bool descendingGapJump =
		safeDescendingDrop && hasHorizontalGap && transitionGap <= kMaximumGapJumpLookAhead && !noJump;
	if (descendingGap && (!safeDescendingDrop || (hasHorizontalGap && !descendingGapJump)))
	{
		active_ = false;
		return LocomotionResult::UnsafeDrop;
	}
	const bool continuingJump = jumpIssued_ && jumpTargetArea_ == targetArea;
	const bool requiresJump = navJump || terrainJump || lookaheadJump || descendingGapJump || continuingJump;
	const bool emitJump = (navJump || terrainJump || lookaheadJump || descendingGapJump) &&
		(!jumpIssued_ || jumpTargetArea_ != targetArea);
	if (stepHeight > config_.maximumStepHeight && !requiresJump)
	{
		active_ = false;
		return LocomotionResult::StepTooHigh;
	}

	const bool requiresCrouch = (destinationArea->attributes & NavArea::kCrouch) != 0U;
	LocomotionPosture posture = LocomotionPosture::Standing;
	if (!requiresJump && (requiresCrouch || observation.standingClearance < config_.requiredClearance))
	{
		const bool clearanceAvailable = observation.clearanceAvailable || observation.standingClearance > 0.0f ||
			observation.crouchingClearance > 0.0f;
		if (!clearanceAvailable)
		{
			active_ = false;
			return LocomotionResult::InvalidClearance;
		}
		if (observation.crouchingClearance < config_.requiredClearance)
		{
			active_ = false;
			return LocomotionResult::NeedsRecovery;
		}
		posture = LocomotionPosture::Crouching;
	}

	if (observation.onLadder && !(config_.averageVelocityStuckMonitor && observation.timingAvailable))
	{
		lastPosition_ = observation.position;
		hasLastPosition_ = true;
		stuckFrames_ = 0U;
	}
	else if (recordProgress(observation, target))
	{
		if (config_.averageVelocityStuckMonitor && observation.ducked)
			resetProgressMonitor();
		else
		{
			active_ = false;
			return LocomotionResult::Stuck;
		}
	}
	if (requiresJump)
	{
		jumpTargetArea_ = targetArea;
		jumpIssued_ = true;
	}
	else
	{
		jumpTargetArea_ = 0U;
		jumpIssued_ = false;
	}

	intent->direction = normalizeDirection(observation.position, target);
	intent->targetPosition = target;
	intent->speed = config_.maximumSpeed;
	intent->posture = posture;
	intent->traversal = safeDescendingDrop && !descendingGapJump ? TraversalAction::Drop
		: requiresJump ? (emitJump ? TraversalAction::Jump : TraversalAction::Walk)
					   : (posture == LocomotionPosture::Crouching
								 ? TraversalAction::Crouch
								 : (stepHeight > 0.0f ? TraversalAction::Step : TraversalAction::Walk));
	intent->stepUp = stepHeight > 0.0f && !requiresJump && !safeDescendingDrop;
	intent->currentArea = currentArea->id;
	intent->targetArea = targetArea;
	return LocomotionResult::IntentReady;
}

bool LocomotionController::isActive() const
{
	return active_;
}

void LocomotionController::clearPath()
{
	active_ = false;
	pathFollower_ = {};
	jumpTargetArea_ = 0U;
	jumpIssued_ = false;
	if (!config_.averageVelocityStuckMonitor)
		resetProgressMonitor();
}

void LocomotionController::observeMotion(const LocomotionObservation &observation)
{
	if (config_.averageVelocityStuckMonitor && observation.timingAvailable && std::isfinite(observation.observedAt) &&
		std::isfinite(observation.position.x) && std::isfinite(observation.position.y) &&
		std::isfinite(observation.position.z))
		(void)recordProgress(observation, {});
}

bool LocomotionController::previewTarget(
	const NavSnapshot &snapshot, const LocomotionObservation &observation, NavVector *target) const
{
	if (!active_ || target == nullptr || !isFiniteObservation(observation))
		return false;
	auto follower = pathFollower_;
	AreaId area = 0U;
	const auto result = follower.update(snapshot, observation.position, config_.targetHorizontalTolerance,
		config_.targetVerticalTolerance, target, &area);
	return result == NavFollowerResult::TargetReady || result == NavFollowerResult::Advanced;
}

void LocomotionController::resetProgressMonitor()
{
	referenceStuck_ = false;
	hasLastPosition_ = false;
	stuckFrames_ = 0U;
	velocitySampleCount_ = 0U;
	velocitySampleIndex_ = 0U;
}

std::size_t LocomotionController::currentCorridorIndex() const
{
	return pathFollower_.currentIndex();
}

bool LocomotionController::isValidConfig(const LocomotionConfig &config)
{
	return std::isfinite(config.requiredClearance) && config.requiredClearance >= 0.0f &&
		config.requiredClearance <= LocomotionConfig::kMaximumClearance &&
		std::isfinite(config.targetHorizontalTolerance) && config.targetHorizontalTolerance >= 0.0f &&
		config.targetHorizontalTolerance <= LocomotionConfig::kMaximumTolerance &&
		std::isfinite(config.targetVerticalTolerance) && config.targetVerticalTolerance >= 0.0f &&
		config.targetVerticalTolerance <= LocomotionConfig::kMaximumTolerance &&
		std::isfinite(config.maximumStepHeight) && config.maximumStepHeight >= 0.0f &&
		config.maximumStepHeight <= LocomotionConfig::kMaximumStepHeight && std::isfinite(config.maximumSpeed) &&
		config.maximumSpeed > 0.0f && config.maximumSpeed <= LocomotionConfig::kMaximumSpeed &&
		config.stuckFrameLimit != 0U && config.stuckFrameLimit <= LocomotionConfig::kMaximumStuckFrameLimit;
}

bool LocomotionController::isFiniteObservation(const LocomotionObservation &observation)
{
	const bool baseObservationIsFinite = std::isfinite(observation.position.x) &&
		std::isfinite(observation.position.y) && std::isfinite(observation.position.z) &&
		std::isfinite(observation.standingClearance) && observation.standingClearance >= 0.0f &&
		std::isfinite(observation.crouchingClearance) && observation.crouchingClearance >= 0.0f;
	if (observation.timingAvailable && (!std::isfinite(observation.observedAt) || observation.observedAt < 0.0f))
		return false;
	if (!baseObservationIsFinite)
	{
		return false;
	}
	if (!observation.groundLookahead.valid)
	{
		return true;
	}

	const GroundLookaheadObservation &lookahead = observation.groundLookahead;
	const float directionLength = std::hypot(lookahead.direction.x, lookahead.direction.y);
	if (lookahead.targetArea == 0U || !std::isfinite(lookahead.direction.x) || !std::isfinite(lookahead.direction.y) ||
		!std::isfinite(lookahead.direction.z) || !std::isfinite(directionLength) ||
		directionLength < ASTRABOT_LOCOMOTION_MINIMUM_UNIT_DIRECTION_LENGTH ||
		directionLength > ASTRABOT_LOCOMOTION_MAXIMUM_UNIT_DIRECTION_LENGTH)
	{
		return false;
	}

	const auto isValidProbe = [](const GroundProbeObservation &probe)
	{
		if (probe.hasGround && !probe.sampled)
		{
			return false;
		}
		return !probe.sampled || !probe.hasGround ||
			(std::isfinite(probe.floorZ) && std::isfinite(probe.normalZ) && probe.normalZ >= -1.0f &&
				probe.normalZ <= 1.0f);
	};
	return isValidProbe(lookahead.far80) && isValidProbe(lookahead.near30) && isValidProbe(lookahead.gap10);
}

NavVector LocomotionController::normalizeDirection(const NavVector &from, const NavVector &to)
{
	const float deltaX = to.x - from.x;
	const float deltaY = to.y - from.y;
	const float distance = std::hypot(deltaX, deltaY);
	if (!std::isfinite(distance) || distance <= 0.0f)
	{
		return {0.0f, 0.0f, 0.0f};
	}
	return {deltaX / distance, deltaY / distance, 0.0f};
}

float LocomotionController::horizontalDistance(const NavVector &from, const NavVector &to)
{
	const float deltaX = to.x - from.x;
	const float deltaY = to.y - from.y;
	return std::hypot(deltaX, deltaY);
}
}
}
#undef ASTRABOT_LOCOMOTION_REFERENCE_FULL_THINK_INTERVAL_SECONDS
#undef ASTRABOT_LOCOMOTION_MINIMUM_UNIT_DIRECTION_LENGTH
#undef ASTRABOT_LOCOMOTION_MAXIMUM_UNIT_DIRECTION_LENGTH
#undef ASTRABOT_LOCOMOTION_MINIMUM_LOOKAHEAD_ALIGNMENT
#undef ASTRABOT_LOCOMOTION_MINIMUM_JUMP_GROUND_NORMAL_Z
