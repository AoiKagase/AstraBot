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
	// Matches ZBot's far ground-probe range in MoveTowardsPosition().
	constexpr float kMaximumGapJumpLookAhead = 80.0f;
	// Conservative NAV-only bound; live BSP physics remains the acceptance gate.
	constexpr float kMaximumSafeGapDrop = 160.0f;

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
	const float gapX = axisGap(
		from.extent.lo.x, from.extent.hi.x, to.extent.lo.x, to.extent.hi.x);
	const float gapY = axisGap(
		from.extent.lo.y, from.extent.hi.y, to.extent.lo.y, to.extent.hi.y);
	return std::hypot(gapX, gapY);
}
}

LocomotionController::LocomotionController() :
	pathFollower_(),
	config_{32.0f, 20.0f, 1.0f, 16.0f, 240.0f, 8U},
	lastPosition_{0.0f, 0.0f, 0.0f},
	jumpTargetArea_(0U),
	stuckFrames_(0U),
	hasLastPosition_(false),
	jumpIssued_(false),
	active_(false)
{
}

LocomotionController::LocomotionController(
	const LocomotionConfig &config) :
	pathFollower_(),
	config_(config),
	lastPosition_{0.0f, 0.0f, 0.0f},
	jumpTargetArea_(0U),
	stuckFrames_(0U),
	hasLastPosition_(false),
	jumpIssued_(false),
	active_(false)
{
}

LocomotionResult LocomotionController::start(const NavCorridor &corridor)
{
	active_ = false;
	hasLastPosition_ = false;
	jumpTargetArea_ = 0U;
	jumpIssued_ = false;
	stuckFrames_ = 0U;
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
	const NavSnapshot &snapshot,
	const LocomotionObservation &observation,
	LocomotionIntent *intent)
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
	if (!std::isfinite(config_.requiredClearance) ||
			config_.requiredClearance < 0.0f)
	{
		return LocomotionResult::InvalidClearance;
	}

	NavAreaMatch currentMatch = {};
	const NavQueryResult queryResult = findCurrentArea(
		snapshot,
		observation,
		&currentMatch);
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
	const NavFollowerResult followerResult = pathFollower_.update(
		snapshot,
		observation.position,
		config_.targetHorizontalTolerance,
		config_.targetVerticalTolerance,
		&target,
		&targetArea);
	const LocomotionResult mappedResult = mapFollowerResult(followerResult);
	if (mappedResult == LocomotionResult::StaleSnapshot ||
			mappedResult == LocomotionResult::InvalidSnapshot ||
			mappedResult == LocomotionResult::InvalidCorridor ||
			mappedResult == LocomotionResult::ResourceLimit)
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

	return buildIntent(
		snapshot,
		observation,
		currentMatch,
		target,
		targetArea,
		intent);
}

NavQueryResult LocomotionController::findCurrentArea(
	const NavSnapshot &snapshot,
	const LocomotionObservation &observation,
	NavAreaMatch *match) const
{
	NavQuery query(snapshot);
	const NavQueryResult containingResult = query.findContaining(
		observation.position,
		config_.targetVerticalTolerance,
		match);
	if (containingResult != NavQueryResult::NoAreaContaining)
	{
		return containingResult;
	}
	return query.findNearest(observation.position, kMaximumRecoveryDistance, match);
}

bool LocomotionController::recordProgress(
	const NavVector &position,
	const NavVector &target)
{
	if (hasLastPosition_)
	{
		const NavVector direction = normalizeDirection(position, target);
		const float deltaX = position.x - lastPosition_.x;
		const float deltaY = position.y - lastPosition_.y;
		const float forwardProgress = deltaX * direction.x + deltaY * direction.y;
		if (!std::isfinite(forwardProgress) ||
			forwardProgress < kMinimumProgressDistance)
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

LocomotionResult LocomotionController::buildIntent(
	const NavSnapshot &snapshot,
	const LocomotionObservation &observation,
	const NavAreaMatch &currentMatch,
	const NavVector &target,
	AreaId targetArea,
	LocomotionIntent *intent)
{
	const NavDocument *document = snapshot.document();
	const NavArea *currentArea = document->findArea(currentMatch.area);
	const NavArea *destinationArea = document->findArea(targetArea);
	if (currentArea == nullptr || destinationArea == nullptr)
	{
		active_ = false;
		return LocomotionResult::InvalidCorridor;
	}
	if (currentArea->id != destinationArea->id &&
			!hasDirectedConnection(*currentArea, destinationArea->id))
	{
		active_ = false;
		return LocomotionResult::InvalidCorridor;
	}

	const float currentSurfaceHeight = surfaceZAt(
		*currentArea, target.x, target.y);
	const float stepHeight = target.z - currentSurfaceHeight;
	const bool noJump = (destinationArea->attributes & NavArea::kNoJump) != 0U;
	const bool navJump = !noJump &&
		(destinationArea->attributes & NavArea::kJump) != 0U &&
		stepHeight <= LocomotionConfig::kMaximumJumpHeight;
	const bool terrainJump = !noJump && !navJump && observation.grounded &&
		!observation.onLadder && stepHeight > config_.maximumStepHeight &&
		stepHeight <= LocomotionConfig::kMaximumJumpHeight;
	const float transitionGap = areaGap(*currentArea, *destinationArea);
	const float dropHeight = currentSurfaceHeight - target.z;
	const bool descendingGapJump = !noJump && !navJump && observation.grounded &&
		!observation.onLadder && transitionGap > 0.0f &&
		transitionGap <= kMaximumGapJumpLookAhead &&
		dropHeight > LocomotionConfig::kMaximumJumpHeight &&
		dropHeight <= kMaximumSafeGapDrop &&
		horizontalDistance(observation.position, target) <= kMaximumGapJumpLookAhead;
	const bool continuingJump = jumpIssued_ && jumpTargetArea_ == targetArea;
	const bool requiresJump =
		navJump || terrainJump || descendingGapJump || continuingJump;
	const bool emitJump = (navJump || terrainJump || descendingGapJump) &&
		(!jumpIssued_ || jumpTargetArea_ != targetArea);
	if (stepHeight > config_.maximumStepHeight && !requiresJump)
	{
		active_ = false;
		return LocomotionResult::StepTooHigh;
	}

	const bool requiresCrouch =
			(destinationArea->attributes & NavArea::kCrouch) != 0U;
	LocomotionPosture posture =
			continuingJump && !observation.grounded
					? LocomotionPosture::Crouching
					: LocomotionPosture::Standing;
	if (!requiresJump &&
			(requiresCrouch || observation.standingClearance < config_.requiredClearance))
	{
		const bool clearanceAvailable = observation.clearanceAvailable ||
			observation.standingClearance > 0.0f ||
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

	if (observation.onLadder)
	{
		lastPosition_ = observation.position;
		hasLastPosition_ = true;
		stuckFrames_ = 0U;
	}
	else if (recordProgress(observation.position, target))
	{
		active_ = false;
		return LocomotionResult::Stuck;
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
	intent->traversal = requiresJump
			? (emitJump ? TraversalAction::Jump : TraversalAction::Walk)
			: (posture == LocomotionPosture::Crouching
					? TraversalAction::Crouch
					: (stepHeight > 0.0f ? TraversalAction::Step : TraversalAction::Walk));
	intent->stepUp = stepHeight > 0.0f && !requiresJump;
	intent->currentArea = currentArea->id;
	intent->targetArea = targetArea;
	return LocomotionResult::IntentReady;
}

bool LocomotionController::isActive() const
{
	return active_;
}

std::size_t LocomotionController::currentCorridorIndex() const
{
	return pathFollower_.currentIndex();
}

bool LocomotionController::isValidConfig(const LocomotionConfig &config)
{
	return std::isfinite(config.requiredClearance) &&
		config.requiredClearance >= 0.0f &&
		config.requiredClearance <= LocomotionConfig::kMaximumClearance &&
		std::isfinite(config.targetHorizontalTolerance) &&
		config.targetHorizontalTolerance >= 0.0f &&
		config.targetHorizontalTolerance <= LocomotionConfig::kMaximumTolerance &&
		std::isfinite(config.targetVerticalTolerance) &&
		config.targetVerticalTolerance >= 0.0f &&
		config.targetVerticalTolerance <= LocomotionConfig::kMaximumTolerance &&
		std::isfinite(config.maximumStepHeight) &&
		config.maximumStepHeight >= 0.0f &&
		config.maximumStepHeight <= LocomotionConfig::kMaximumStepHeight &&
		std::isfinite(config.maximumSpeed) &&
		config.maximumSpeed > 0.0f &&
		config.maximumSpeed <= LocomotionConfig::kMaximumSpeed &&
		config.stuckFrameLimit != 0U &&
		config.stuckFrameLimit <= LocomotionConfig::kMaximumStuckFrameLimit;
}

bool LocomotionController::isFiniteObservation(
	const LocomotionObservation &observation)
{
	return std::isfinite(observation.position.x) &&
		std::isfinite(observation.position.y) &&
		std::isfinite(observation.position.z) &&
		std::isfinite(observation.standingClearance) &&
		observation.standingClearance >= 0.0f &&
		std::isfinite(observation.crouchingClearance) &&
		observation.crouchingClearance >= 0.0f;
}

NavVector LocomotionController::normalizeDirection(
	const NavVector &from,
	const NavVector &to)
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

float LocomotionController::horizontalDistance(
	const NavVector &from,
	const NavVector &to)
{
	const float deltaX = to.x - from.x;
	const float deltaY = to.y - from.y;
	return std::hypot(deltaX, deltaY);
}
}
}
