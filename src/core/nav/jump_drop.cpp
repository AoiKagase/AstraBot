#include "astrabot/nav/jump_drop.hpp"

#include <algorithm>
#include <cmath>

namespace astrabot
{
namespace nav
{
namespace
{
constexpr float kMinimumHeight = 0.0f;
constexpr float kMaximumObservedDamage = 10000.0f;

float nonNegativeDifference(float value)
{
	return std::max(value, kMinimumHeight);
}
}

JumpDropController::JumpDropController() :
	config_{
		JumpDropConfig::kDefaultLaunchHorizontalTolerance,
		JumpDropConfig::kDefaultLaunchVerticalTolerance,
		JumpDropConfig::kDefaultLandingTolerance,
		JumpDropConfig::kDefaultIntentSpeed,
		JumpDropConfig::kDefaultTraversalFrames
	},
	envelope_{},
	navRevision_(0U),
	mapGeneration_(0U),
	actorGeneration_(0U),
	launchFrame_(0U),
	hasLaunchFrame_(false),
	launchEmitted_(false),
	active_(false)
{
}

JumpDropController::JumpDropController(const JumpDropConfig &config) :
	config_(config),
	envelope_{},
	navRevision_(0U),
	mapGeneration_(0U),
	actorGeneration_(0U),
	launchFrame_(0U),
	hasLaunchFrame_(false),
	launchEmitted_(false),
	active_(false)
{
}

JumpDropResult JumpDropController::start(
	const NavCorridor &corridor,
	const JumpDropEnvelope &envelope,
	std::uint32_t actorGeneration)
{
	active_ = false;
	hasLaunchFrame_ = false;
	launchEmitted_ = false;
	if (!isValidConfig(config_))
	{
		return JumpDropResult::InvalidConfig;
	}
	if (actorGeneration == 0U)
	{
		return JumpDropResult::InvalidArgument;
	}
	if (!corridor.isValid())
	{
		return JumpDropResult::InvalidCorridor;
	}
	if (!isValidEnvelope(envelope))
	{
		return JumpDropResult::InvalidEnvelope;
	}
	if (exceedsEnvelope(envelope))
	{
		return JumpDropResult::Unsafe;
	}
	if (corridor.areas.front() != envelope.launch.area ||
			corridor.areas.back() != envelope.landing.area)
	{
		return JumpDropResult::InvalidEnvelope;
	}

	envelope_ = envelope;
	navRevision_ = corridor.navRevision;
	mapGeneration_ = corridor.mapGeneration;
	actorGeneration_ = actorGeneration;
	active_ = true;
	return JumpDropResult::Ready;
}

JumpDropResult JumpDropController::update(
	const NavSnapshot &snapshot,
	const JumpDropObservation &observation,
	JumpDropIntent *intent)
{
	if (intent == nullptr)
	{
		return JumpDropResult::InvalidArgument;
	}
	*intent = {};
	if (!active_)
	{
		return JumpDropResult::Inactive;
	}
	if (!snapshot.isValid())
	{
		active_ = false;
		return JumpDropResult::InvalidSnapshot;
	}
	if (snapshot.revision() != navRevision_ ||
			snapshot.mapGeneration() != mapGeneration_ ||
			observation.actorGeneration != actorGeneration_)
	{
		active_ = false;
		return JumpDropResult::Invalidated;
	}
	if (!isFiniteObservation(observation))
	{
		active_ = false;
		return JumpDropResult::InvalidObservation;
	}
	if (!validateEnvelopeAreas(snapshot))
	{
		active_ = false;
		return JumpDropResult::InvalidEnvelope;
	}
	if (!hasLaunchFrame_)
	{
		NavQuery query(snapshot);
		NavAreaMatch match = {};
		if (observation.airborne || observation.landingConfirmed ||
				query.findContaining(
					observation.position,
					config_.launchVerticalTolerance,
					&match) != NavQueryResult::Found ||
				match.area != envelope_.launch.area ||
				!isNearPoint(
					observation.position,
					envelope_.launch,
					config_.launchHorizontalTolerance,
					config_.launchVerticalTolerance))
		{
			active_ = false;
			return JumpDropResult::InvalidObservation;
		}

		launchFrame_ = observation.frame;
		hasLaunchFrame_ = true;
		return emitLaunchIntent(observation, intent);
	}

	if (observation.frame < launchFrame_)
	{
		active_ = false;
		return JumpDropResult::InvalidObservation;
	}
	if (observation.frame - launchFrame_ > config_.maximumTraversalFrames)
	{
		active_ = false;
		return JumpDropResult::TimedOut;
	}
	if (!observation.landingConfirmed)
	{
		return JumpDropResult::Ready;
	}
	if (observation.airborne)
	{
		active_ = false;
		return JumpDropResult::InvalidObservation;
	}

	NavQuery query(snapshot);
	NavAreaMatch match = {};
	if (query.findContaining(
			observation.position,
			config_.landingTolerance,
			&match) != NavQueryResult::Found ||
			match.area != envelope_.landing.area ||
			!isNearPoint(
				observation.position,
				envelope_.landing,
				config_.landingTolerance,
				config_.landingTolerance))
	{
		active_ = false;
		return JumpDropResult::InvalidObservation;
	}
	if (envelope_.kind == JumpDropKind::Drop)
	{
		if (!observation.hasLandingDamage)
		{
			active_ = false;
			return JumpDropResult::InvalidObservation;
		}
		if (observation.landingDamage > envelope_.damageRisk.maximumLandingDamage)
		{
			active_ = false;
			return JumpDropResult::Unsafe;
		}
	}

	active_ = false;
	return JumpDropResult::Landed;
}

bool JumpDropController::isActive() const
{
	return active_;
}

bool JumpDropController::isValidConfig(const JumpDropConfig &config)
{
	return std::isfinite(config.launchHorizontalTolerance) &&
		config.launchHorizontalTolerance >= 0.0f &&
		config.launchHorizontalTolerance <= JumpDropConfig::kMaximumTolerance &&
		std::isfinite(config.launchVerticalTolerance) &&
		config.launchVerticalTolerance >= 0.0f &&
		config.launchVerticalTolerance <= JumpDropConfig::kMaximumTolerance &&
		std::isfinite(config.landingTolerance) &&
		config.landingTolerance >= 0.0f &&
		config.landingTolerance <= JumpDropConfig::kMaximumTolerance &&
		std::isfinite(config.maximumIntentSpeed) &&
		config.maximumIntentSpeed > 0.0f &&
		config.maximumIntentSpeed <= JumpDropConfig::kMaximumIntentSpeed &&
		config.maximumTraversalFrames != 0U &&
		config.maximumTraversalFrames <= JumpDropConfig::kMaximumTraversalFrames;
}

bool JumpDropController::isFiniteVector(const NavVector &value)
{
	return std::isfinite(value.x) &&
		std::isfinite(value.y) &&
		std::isfinite(value.z);
}

bool JumpDropController::isValidEnvelope(const JumpDropEnvelope &envelope)
{
	return (envelope.kind == JumpDropKind::Jump ||
			envelope.kind == JumpDropKind::Drop) &&
		envelope.launch.area != 0U &&
		envelope.landing.area != 0U &&
		isFiniteVector(envelope.launch.position) &&
		isFiniteVector(envelope.landing.position) &&
		std::isfinite(envelope.maximumRise) &&
		envelope.maximumRise >= 0.0f &&
		envelope.maximumRise <= JumpDropEnvelope::kMaximumRise &&
		std::isfinite(envelope.maximumDrop) &&
		envelope.maximumDrop >= 0.0f &&
		envelope.maximumDrop <= JumpDropEnvelope::kMaximumDrop &&
		std::isfinite(envelope.horizontalReach) &&
		envelope.horizontalReach >= 0.0f &&
		envelope.horizontalReach <=
			JumpDropEnvelope::kMaximumHorizontalReach &&
		std::isfinite(envelope.damageRisk.maximumSafeDropHeight) &&
		envelope.damageRisk.maximumSafeDropHeight >= 0.0f &&
		envelope.damageRisk.maximumSafeDropHeight <=
			JumpDropDamageRisk::kMaximumSafeDropHeight &&
		std::isfinite(envelope.damageRisk.maximumLandingDamage) &&
		envelope.damageRisk.maximumLandingDamage >= 0.0f &&
		envelope.damageRisk.maximumLandingDamage <=
			JumpDropDamageRisk::kMaximumLandingDamage;
}

bool JumpDropController::exceedsEnvelope(
	const JumpDropEnvelope &envelope)
{
	const float heightDelta =
		envelope.landing.position.z - envelope.launch.position.z;
	const float rise = nonNegativeDifference(heightDelta);
	const float drop = nonNegativeDifference(-heightDelta);
	const float reach = horizontalDistance(
		envelope.launch.position,
		envelope.landing.position);
	if (!std::isfinite(reach) ||
			rise > envelope.maximumRise ||
			drop > envelope.maximumDrop ||
			reach > envelope.horizontalReach)
	{
		return true;
	}
	return envelope.kind == JumpDropKind::Drop &&
		drop > envelope.damageRisk.maximumSafeDropHeight;
}

bool JumpDropController::isFiniteObservation(
	const JumpDropObservation &observation)
{
	return observation.actorGeneration != 0U &&
		isFiniteVector(observation.position) &&
		(!observation.hasLandingDamage ||
			(std::isfinite(observation.landingDamage) &&
			observation.landingDamage >= 0.0f &&
			observation.landingDamage <= kMaximumObservedDamage));
}

float JumpDropController::floorDistance(
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
	return 0.0f;
}

float JumpDropController::horizontalDistance(
	const NavVector &from,
	const NavVector &to)
{
	return std::hypot(to.x - from.x, to.y - from.y);
}

bool JumpDropController::isNearPoint(
	const NavVector &position,
	const JumpDropPoint &point,
	float horizontalTolerance,
	float verticalTolerance)
{
	const float distance = horizontalDistance(position, point.position);
	return std::isfinite(distance) &&
		distance <= horizontalTolerance &&
		std::fabs(position.z - point.position.z) <= verticalTolerance;
}

bool JumpDropController::isPointInArea(
	const NavArea &area,
	const JumpDropPoint &point,
	float verticalTolerance)
{
	return point.position.x >= area.extent.lo.x &&
		point.position.x <= area.extent.hi.x &&
		point.position.y >= area.extent.lo.y &&
		point.position.y <= area.extent.hi.y &&
		floorDistance(area, point.position.z) <= verticalTolerance;
}

bool JumpDropController::validateEnvelopeAreas(
	const NavSnapshot &snapshot) const
{
	const NavDocument *document = snapshot.document();
	if (document == nullptr)
	{
		return false;
	}
	const NavArea *launchArea = document->findArea(envelope_.launch.area);
	const NavArea *landingArea = document->findArea(envelope_.landing.area);
	return launchArea != nullptr &&
		landingArea != nullptr &&
		isPointInArea(
			*launchArea,
			envelope_.launch,
			config_.launchVerticalTolerance) &&
		isPointInArea(
			*landingArea,
			envelope_.landing,
			config_.landingTolerance);
}

JumpDropResult JumpDropController::emitLaunchIntent(
	const JumpDropObservation &observation,
	JumpDropIntent *intent)
{
	if (launchEmitted_ || observation.airborne)
	{
		return JumpDropResult::InvalidObservation;
	}

	const float distance = horizontalDistance(
		envelope_.launch.position,
		envelope_.landing.position);
	if (!std::isfinite(distance))
	{
		active_ = false;
		return JumpDropResult::InvalidEnvelope;
	}
	if (distance > 0.0f)
	{
		intent->direction = {
			(envelope_.landing.position.x - envelope_.launch.position.x) /
				distance,
			(envelope_.landing.position.y - envelope_.launch.position.y) /
				distance,
			0.0f
		};
	}
	else
	{
		intent->direction = {0.0f, 0.0f, 0.0f};
	}
	intent->kind = envelope_.kind;
	intent->speed = config_.maximumIntentSpeed;
	intent->launchArea = envelope_.launch.area;
	intent->landingArea = envelope_.landing.area;
	launchEmitted_ = true;
	return JumpDropResult::Emitted;
}
}
}
