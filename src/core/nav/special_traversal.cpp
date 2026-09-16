#include "astrabot/nav/special_traversal.hpp"

#include <algorithm>
#include <cmath>

namespace astrabot
{
namespace nav
{
namespace
{
constexpr float kMinimumHeight = 0.0f;

bool isValidKind(SpecialTraversalKind kind)
{
	return kind == SpecialTraversalKind::Ladder ||
		kind == SpecialTraversalKind::Door ||
		kind == SpecialTraversalKind::NarrowPassage;
}

bool isValidAvailability(SpecialTraversalAvailability availability)
{
	return availability == SpecialTraversalAvailability::Unknown ||
		availability == SpecialTraversalAvailability::Closed ||
		availability == SpecialTraversalAvailability::Open;
}

bool isValidPosture(SpecialTraversalPosture posture)
{
	return posture == SpecialTraversalPosture::Standing ||
		posture == SpecialTraversalPosture::Crouching;
}
}

SpecialTraversalController::SpecialTraversalController() :
	config_{
		SpecialTraversalConfig::kDefaultPositionTolerance,
		SpecialTraversalConfig::kDefaultSpeed,
		SpecialTraversalConfig::kDefaultTraversalFrames,
		SpecialTraversalConfig::kDefaultNoProgressSamples,
		SpecialTraversalConfig::kDefaultMinimumProgressDistance
	},
	capability_{},
	firstNextArea_(0U),
	navRevision_(0U),
	mapGeneration_(0U),
	actorGeneration_(0U),
	startFrame_(0U),
	noProgressSamples_(0U),
	lastPosition_{0.0f, 0.0f, 0.0f},
	state_(SpecialTraversalState::Idle),
	failureReason_(SpecialTraversalFailureReason::None),
	hasStartFrame_(false),
	hasLastPosition_(false),
	active_(false)
{
}

SpecialTraversalController::SpecialTraversalController(
	const SpecialTraversalConfig &config) :
	config_(config),
	capability_{},
	firstNextArea_(0U),
	navRevision_(0U),
	mapGeneration_(0U),
	actorGeneration_(0U),
	startFrame_(0U),
	noProgressSamples_(0U),
	lastPosition_{0.0f, 0.0f, 0.0f},
	state_(SpecialTraversalState::Idle),
	failureReason_(SpecialTraversalFailureReason::None),
	hasStartFrame_(false),
	hasLastPosition_(false),
	active_(false)
{
}

SpecialTraversalResult SpecialTraversalController::start(
	const NavCorridor &corridor,
	const SpecialTraversalCapability &capability,
	std::uint32_t actorGeneration)
{
	active_ = false;
	hasStartFrame_ = false;
	hasLastPosition_ = false;
	noProgressSamples_ = 0U;
	state_ = SpecialTraversalState::Idle;
	failureReason_ = SpecialTraversalFailureReason::None;
	if (!isValidConfig(config_))
	{
		return SpecialTraversalResult::InvalidConfig;
	}
	if (actorGeneration == 0U)
	{
		return SpecialTraversalResult::InvalidArgument;
	}
	if (!corridor.isValid() || corridor.areas.size() < 2U)
	{
		return SpecialTraversalResult::InvalidCorridor;
	}
	if (!isValidCapability(capability) ||
		capability.entry.area != corridor.areas.front() ||
		capability.exit.area != corridor.areas.back() ||
		capability.entry.area == capability.exit.area)
	{
		return SpecialTraversalResult::InvalidCapability;
	}

	capability_ = capability;
	firstNextArea_ = corridor.areas[1U];
	navRevision_ = corridor.navRevision;
	mapGeneration_ = corridor.mapGeneration;
	actorGeneration_ = actorGeneration;
	state_ = SpecialTraversalState::Entering;
	active_ = true;
	return SpecialTraversalResult::Ready;
}

SpecialTraversalResult SpecialTraversalController::update(
	const NavSnapshot &snapshot,
	const SpecialTraversalObservation &observation,
	SpecialTraversalIntent *intent)
{
	if (intent == nullptr)
	{
		return SpecialTraversalResult::InvalidArgument;
	}
	*intent = {};
	if (!active_)
	{
		return SpecialTraversalResult::Inactive;
	}
	if (!snapshot.isValid())
	{
		active_ = false;
		state_ = SpecialTraversalState::Recovering;
		return SpecialTraversalResult::InvalidSnapshot;
	}
	if (snapshot.revision() != navRevision_ ||
		snapshot.mapGeneration() != mapGeneration_ ||
		observation.actorGeneration != actorGeneration_)
	{
		active_ = false;
		state_ = SpecialTraversalState::Invalidated;
		failureReason_ = SpecialTraversalFailureReason::None;
		return SpecialTraversalResult::Invalidated;
	}
	const bool observationValid = isFiniteObservation(observation);
	const bool capabilityValid = validateCapability(snapshot);
	if (!observationValid || !capabilityValid)
	{
		active_ = false;
		state_ = SpecialTraversalState::Recovering;
		return !observationValid ?
			SpecialTraversalResult::InvalidObservation :
			SpecialTraversalResult::InvalidCapability;
	}
	if (!hasStartFrame_)
	{
		startFrame_ = observation.frame;
		hasStartFrame_ = true;
		lastPosition_ = observation.position;
		hasLastPosition_ = true;
		noProgressSamples_ = 1U;
	}
	else
	{
		if (observation.frame < startFrame_)
		{
			active_ = false;
			state_ = SpecialTraversalState::Recovering;
			return SpecialTraversalResult::InvalidObservation;
		}
		if (observation.frame - startFrame_ >
				config_.maximumTraversalFrames)
		{
			active_ = false;
			state_ = SpecialTraversalState::Recovering;
			return SpecialTraversalResult::TimedOut;
		}
		const float progress = horizontalDistance(
			observation.position, lastPosition_);
		if (progress < config_.minimumProgressDistance)
		{
			noProgressSamples_ = std::min(
				config_.maximumNoProgressSamples,
				noProgressSamples_ + 1U);
		}
		else
		{
			noProgressSamples_ = 0U;
		}
		lastPosition_ = observation.position;
		if (noProgressSamples_ >=
				config_.maximumNoProgressSamples)
		{
			return recover(SpecialTraversalFailureReason::NoProgress);
		}
	}

	SpecialTraversalPosture posture =
		SpecialTraversalPosture::Standing;
	if (!selectPosture(observation, &posture))
	{
		return recover(SpecialTraversalFailureReason::InsufficientClearance);
	}
	if (capability_.kind == SpecialTraversalKind::Door)
	{
		if (observation.availability ==
				SpecialTraversalAvailability::Unknown)
		{
			return recover(
				SpecialTraversalFailureReason::UnknownAvailability);
		}
		if (observation.availability ==
				SpecialTraversalAvailability::Closed)
		{
			return recover(SpecialTraversalFailureReason::Unavailable);
		}
	}

	switch (state_)
	{
	case SpecialTraversalState::Entering:
		return updateEntering(snapshot, observation, posture, intent);
	case SpecialTraversalState::Maintaining:
		return updateMaintaining(snapshot, observation, posture, intent);
	case SpecialTraversalState::Exiting:
		return updateExiting(snapshot, observation, posture, intent);
	case SpecialTraversalState::Idle:
	case SpecialTraversalState::Completed:
	case SpecialTraversalState::Recovering:
	case SpecialTraversalState::Invalidated:
		return SpecialTraversalResult::Inactive;
	}
	return SpecialTraversalResult::InvalidObservation;
}

bool SpecialTraversalController::isActive() const
{
	return active_;
}

SpecialTraversalState SpecialTraversalController::state() const
{
	return state_;
}

SpecialTraversalFailureReason SpecialTraversalController::failureReason() const
{
	return failureReason_;
}

bool SpecialTraversalController::isValidConfig(
	const SpecialTraversalConfig &config)
{
	return std::isfinite(config.positionTolerance) &&
		config.positionTolerance >= 0.0f &&
		config.positionTolerance <= SpecialTraversalConfig::kMaximumTolerance &&
		std::isfinite(config.maximumSpeed) &&
		config.maximumSpeed > 0.0f &&
		config.maximumSpeed <= SpecialTraversalConfig::kMaximumSpeed &&
		config.maximumTraversalFrames != 0U &&
		config.maximumTraversalFrames <=
			SpecialTraversalConfig::kMaximumTraversalFrames &&
		config.maximumNoProgressSamples != 0U &&
		config.maximumNoProgressSamples <=
			SpecialTraversalConfig::kMaximumNoProgressSamples &&
		std::isfinite(config.minimumProgressDistance) &&
		config.minimumProgressDistance >= 0.0f &&
		config.minimumProgressDistance <=
			SpecialTraversalConfig::kMaximumTolerance;
}

bool SpecialTraversalController::isFiniteVector(const NavVector &value)
{
	return std::isfinite(value.x) &&
		std::isfinite(value.y) &&
		std::isfinite(value.z);
}

bool SpecialTraversalController::isValidCapability(
	const SpecialTraversalCapability &capability)
{
	return isValidKind(capability.kind) &&
		capability.entry.area != 0U &&
		capability.exit.area != 0U &&
		capability.sourceDirection < NavArea::kDirectionCount &&
		isValidPosture(capability.minimumPosture) &&
		isFiniteVector(capability.entry.position) &&
		isFiniteVector(capability.exit.position) &&
		std::isfinite(capability.requiredClearance) &&
		capability.requiredClearance >= 0.0f &&
		capability.requiredClearance <=
			SpecialTraversalCapability::kMaximumRequiredClearance;
}

bool SpecialTraversalController::isFiniteObservation(
	const SpecialTraversalObservation &observation)
{
	return observation.actorGeneration != 0U &&
		isFiniteVector(observation.position) &&
		std::isfinite(observation.standingClearance) &&
		observation.standingClearance >= 0.0f &&
		observation.standingClearance <=
			SpecialTraversalCapability::kMaximumRequiredClearance &&
		std::isfinite(observation.crouchingClearance) &&
		observation.crouchingClearance >= 0.0f &&
		observation.crouchingClearance <=
			SpecialTraversalCapability::kMaximumRequiredClearance &&
		isValidAvailability(observation.availability);
}

float SpecialTraversalController::horizontalDistance(
	const NavVector &from,
	const NavVector &to)
{
	return std::hypot(to.x - from.x, to.y - from.y);
}

float SpecialTraversalController::floorDistance(
	const NavArea &area,
	float height)
{
	const float low = std::min(area.northEastZ, area.southWestZ);
	const float high = std::max(area.northEastZ, area.southWestZ);
	if (height < low)
	{
		return low - height;
	}
	if (height > high)
	{
		return height - high;
	}
	return kMinimumHeight;
}

bool SpecialTraversalController::isPointInArea(
	const NavArea &area,
	const SpecialTraversalPoint &point,
	float verticalTolerance)
{
	return point.position.x >= area.extent.lo.x &&
		point.position.x <= area.extent.hi.x &&
		point.position.y >= area.extent.lo.y &&
		point.position.y <= area.extent.hi.y &&
		floorDistance(area, point.position.z) <= verticalTolerance;
}

bool SpecialTraversalController::isNearPoint(
	const NavVector &position,
	const SpecialTraversalPoint &point,
	float tolerance)
{
	const float distance = horizontalDistance(position, point.position);
	return std::isfinite(distance) &&
		distance <= tolerance &&
		std::fabs(position.z - point.position.z) <= tolerance;
}

bool SpecialTraversalController::hasDirectedLink(
	const NavArea &area,
	std::uint8_t direction,
	AreaId target)
{
	for (const AreaId candidate : area.connections[direction])
	{
		if (candidate == target)
		{
			return true;
		}
	}
	return false;
}

bool SpecialTraversalController::validateCapability(
	const NavSnapshot &snapshot) const
{
	const NavDocument *document = snapshot.document();
	if (document == nullptr)
	{
		return false;
	}
	const NavArea *entryArea = document->findArea(capability_.entry.area);
	const NavArea *exitArea = document->findArea(capability_.exit.area);
	return entryArea != nullptr &&
		exitArea != nullptr &&
		isPointInArea(
			*entryArea,
			capability_.entry,
			config_.positionTolerance) &&
		isPointInArea(
			*exitArea,
			capability_.exit,
			config_.positionTolerance) &&
		hasDirectedLink(
			*entryArea,
			capability_.sourceDirection,
			firstNextArea_);
}

bool SpecialTraversalController::isAtPoint(
	const NavSnapshot &snapshot,
	const NavVector &position,
	const SpecialTraversalPoint &point) const
{
	NavQuery query(snapshot);
	NavAreaMatch match = {};
	return query.findContaining(
			position,
			config_.positionTolerance,
			&match) == NavQueryResult::Found &&
		match.area == point.area &&
		isNearPoint(position, point, config_.positionTolerance);
}

bool SpecialTraversalController::selectPosture(
	const SpecialTraversalObservation &observation,
	SpecialTraversalPosture *posture) const
{
	if (posture == nullptr)
	{
		return false;
	}
	if (capability_.minimumPosture ==
			SpecialTraversalPosture::Crouching)
	{
		if (observation.crouchingClearance <
				capability_.requiredClearance)
		{
			return false;
		}
		*posture = SpecialTraversalPosture::Crouching;
		return true;
	}
	if (observation.standingClearance >=
			capability_.requiredClearance)
	{
		*posture = SpecialTraversalPosture::Standing;
		return true;
	}
	if (observation.crouchingClearance >=
			capability_.requiredClearance)
	{
		*posture = SpecialTraversalPosture::Crouching;
		return true;
	}
	return false;
}

SpecialTraversalResult SpecialTraversalController::recover(
	SpecialTraversalFailureReason reason)
{
	active_ = false;
	state_ = SpecialTraversalState::Recovering;
	failureReason_ = reason;
	return SpecialTraversalResult::RecoverableFailure;
}

SpecialTraversalResult SpecialTraversalController::emitIntent(
	const SpecialTraversalObservation &observation,
	const SpecialTraversalPoint &target,
	SpecialTraversalIntentPhase phase,
	SpecialTraversalPosture posture,
	SpecialTraversalIntent *intent) const
{
	const float distance = horizontalDistance(
		observation.position, target.position);
	if (!std::isfinite(distance))
	{
		return SpecialTraversalResult::InvalidObservation;
	}
	if (distance > 0.0f)
	{
		intent->direction = {
			(target.position.x - observation.position.x) / distance,
			(target.position.y - observation.position.y) / distance,
			0.0f
		};
	}
	else
	{
		intent->direction = {0.0f, 0.0f, 0.0f};
	}
	intent->kind = capability_.kind;
	intent->phase = phase;
	intent->posture = posture;
	intent->speed = config_.maximumSpeed;
	intent->entryArea = capability_.entry.area;
	intent->exitArea = capability_.exit.area;
	intent->sourceDirection = capability_.sourceDirection;
	switch (phase)
	{
	case SpecialTraversalIntentPhase::Enter:
		return SpecialTraversalResult::EnterIntent;
	case SpecialTraversalIntentPhase::Maintain:
		return SpecialTraversalResult::MaintainIntent;
	case SpecialTraversalIntentPhase::Exit:
		return SpecialTraversalResult::ExitIntent;
	}
	return SpecialTraversalResult::InvalidObservation;
}

SpecialTraversalResult SpecialTraversalController::updateEntering(
	const NavSnapshot &snapshot,
	const SpecialTraversalObservation &observation,
	SpecialTraversalPosture posture,
	SpecialTraversalIntent *intent)
{
	if (capability_.kind == SpecialTraversalKind::Ladder &&
			observation.entryConfirmed && !observation.ladderContact)
	{
		return recover(SpecialTraversalFailureReason::MissingContact);
	}
	if (!observation.entryConfirmed)
	{
		return emitIntent(
			observation,
			capability_.entry,
			SpecialTraversalIntentPhase::Enter,
			posture,
			intent);
	}
	if (!isAtPoint(snapshot, observation.position, capability_.entry))
	{
		active_ = false;
		state_ = SpecialTraversalState::Recovering;
		return SpecialTraversalResult::InvalidObservation;
	}
	state_ = SpecialTraversalState::Maintaining;
	return emitIntent(
		observation,
		capability_.exit,
		SpecialTraversalIntentPhase::Maintain,
		posture,
		intent);
}

SpecialTraversalResult SpecialTraversalController::updateMaintaining(
	const NavSnapshot &snapshot,
	const SpecialTraversalObservation &observation,
	SpecialTraversalPosture posture,
	SpecialTraversalIntent *intent)
{
	if (capability_.kind == SpecialTraversalKind::Ladder &&
			!observation.ladderContact)
	{
		return recover(SpecialTraversalFailureReason::MissingContact);
	}
	if (!observation.exitConfirmed)
	{
		return emitIntent(
			observation,
			capability_.exit,
			SpecialTraversalIntentPhase::Maintain,
			posture,
			intent);
	}
	if (!isAtPoint(snapshot, observation.position, capability_.exit))
	{
		active_ = false;
		state_ = SpecialTraversalState::Recovering;
		return SpecialTraversalResult::InvalidObservation;
	}
	state_ = SpecialTraversalState::Exiting;
	return emitIntent(
		observation,
		capability_.exit,
		SpecialTraversalIntentPhase::Exit,
		posture,
		intent);
}

SpecialTraversalResult SpecialTraversalController::updateExiting(
	const NavSnapshot &snapshot,
	const SpecialTraversalObservation &observation,
	SpecialTraversalPosture posture,
	SpecialTraversalIntent *intent)
{
	if (capability_.kind == SpecialTraversalKind::Ladder &&
			!observation.ladderContact)
	{
		return recover(SpecialTraversalFailureReason::MissingContact);
	}
	if (!observation.exitConfirmed)
	{
		active_ = false;
		state_ = SpecialTraversalState::Recovering;
		return SpecialTraversalResult::InvalidObservation;
	}
	if (!isAtPoint(snapshot, observation.position, capability_.exit))
	{
		return emitIntent(
			observation,
			capability_.exit,
			SpecialTraversalIntentPhase::Exit,
			posture,
			intent);
	}
	active_ = false;
	state_ = SpecialTraversalState::Completed;
	failureReason_ = SpecialTraversalFailureReason::None;
	return SpecialTraversalResult::Completed;
}
}
}
