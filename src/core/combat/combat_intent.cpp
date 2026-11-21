#include "astrabot/combat/combat_intent.hpp"

#include <cmath>
#include <limits>

namespace astrabot
{
namespace combat
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr unsigned kDefaultMaximumTargetAgeTicks = 128U;
constexpr float kDefaultMinimumTargetConfidence = 0.25f;

bool isValidTargetState(TargetBeliefState state)
{
	return state == TargetBeliefState::Unknown || state == TargetBeliefState::Visible ||
		state == TargetBeliefState::Remembered || state == TargetBeliefState::Unavailable;
}

bool isFiniteVector(const world::WorldVector &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isBoundedPosition(const world::WorldPosition &value)
{
	return isFiniteVector(value) && std::fabs(value.x) <= world::WorldLimits::kMaximumCoordinate &&
		std::fabs(value.y) <= world::WorldLimits::kMaximumCoordinate &&
		std::fabs(value.z) <= world::WorldLimits::kMaximumCoordinate;
}
}

bool TargetBelief::isValid() const
{
	return target.isValid() && isValidTargetState(state) && isBoundedPosition(position) && std::isfinite(confidence) &&
		confidence >= 0.0f && confidence <= 1.0f && ageTicks <= world::WorldLimits::kMaximumAgeTicks;
}

bool TargetBelief::isUsable() const
{
	return isValid() && (state == TargetBeliefState::Visible || state == TargetBeliefState::Remembered) &&
		confidence > 0.0f && !friendlyFireRisk;
}

bool CombatDecisionContext::isValid() const
{
	return actor.isValid() && frame.isValid() && isBoundedPosition(origin);
}

bool AimIntent::isValid() const
{
	return actor.isValid() && frame.isValid() && target.isValid() && isBoundedPosition(point) && std::isfinite(pitch) &&
		std::isfinite(yaw) && std::fabs(pitch) <= world::WorldLimits::kMaximumAngle &&
		std::fabs(yaw) <= world::WorldLimits::kMaximumAngle;
}

bool AimIntent::isIntent() const
{
	return isValid();
}

bool AimIntent::isDispatchReceipt() const
{
	return false;
}

bool FireIntent::isValid() const
{
	return actor.isValid() && frame.isValid() && target.isValid() && weapon.isValid();
}

bool FireIntent::isIntent() const
{
	return isValid();
}

bool FireIntent::isDamageConfirmation() const
{
	return false;
}

bool ReloadIntent::isValid() const
{
	return actor.isValid() && frame.isValid() && weapon.isValid();
}

bool ReloadIntent::isIntent() const
{
	return isValid();
}

bool ReloadIntent::isAmmoConfirmation() const
{
	return false;
}

bool DamageObservation::isValid() const
{
	constexpr float kMaximumObservedDamage = 100.0f;
	return actor.isValid() && target.isValid() && frame.isValid() &&
		(state == DamageObservationState::Unknown || state == DamageObservationState::Observed ||
			state == DamageObservationState::Unavailable) &&
		std::isfinite(amount) && amount >= 0.0f && amount <= kMaximumObservedDamage;
}

bool DamageObservation::isConfirmed() const
{
	return isValid() && state == DamageObservationState::Observed;
}

bool DamageObservation::isConfirmedDeath() const
{
	return isConfirmed() && targetDied;
}

CombatConfig::CombatConfig()
	: maximumTargetAgeTicks(kDefaultMaximumTargetAgeTicks), minimumTargetConfidence(kDefaultMinimumTargetConfidence)
{
}

bool CombatConfig::isValid() const
{
	return maximumTargetAgeTicks <= world::WorldLimits::kMaximumAgeTicks && std::isfinite(minimumTargetConfidence) &&
		minimumTargetConfidence >= 0.0f && minimumTargetConfidence <= 1.0f;
}

CombatController::CombatController() : config_(), actor_(), lastFrame_(), initialized_(false)
{
}

CombatController::CombatController(const world::ActorKey &actor)
	: config_(), actor_(actor), lastFrame_(), initialized_(actor.isValid())
{
}

CombatController::CombatController(const world::ActorKey &actor, const CombatConfig &config)
	: config_(config), actor_(actor), lastFrame_(), initialized_(actor.isValid() && config.isValid())
{
}

bool CombatController::isValidTargetState(TargetBeliefState state)
{
	return ::astrabot::combat::isValidTargetState(state);
}

bool CombatController::isFiniteVector(const world::WorldVector &value)
{
	return ::astrabot::combat::isFiniteVector(value);
}

bool CombatController::isBoundedPosition(const world::WorldPosition &value)
{
	return ::astrabot::combat::isBoundedPosition(value);
}

bool CombatController::sameFrame(const world::FrameIdentity &left, const world::FrameIdentity &right)
{
	return left == right;
}

bool CombatController::isFrameAfter(const world::FrameIdentity &candidate, const world::FrameIdentity &current)
{
	if (candidate.mapGeneration != current.mapGeneration)
	{
		return candidate.mapGeneration > current.mapGeneration;
	}
	if (candidate.roundGeneration != current.roundGeneration)
	{
		return candidate.roundGeneration > current.roundGeneration;
	}
	return candidate.tick > current.tick;
}

bool CombatController::offsetAimPoint(const CombatDecisionContext &context, world::WorldPosition target,
	const compat::AimSpringState &spring, world::WorldPosition *point)
{
	constexpr float kFocusResetAngularVelocity = 100.0f;
	constexpr float kFocusSkillSeconds = 5.0f;
	constexpr float kMinimumFocusSeconds = 2.0f;
	constexpr float kMaximumFocusAccuracy = 0.75f;
	constexpr float kAimErrorReferenceFieldOfView = 90.0f;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_COMBAT_INTENT_AIM_ERROR_SCALE 0.1f
	constexpr int kAimRandomDrawCount = 4;
	constexpr int kAimRefreshDrawIndex = 3;
	constexpr float kMinimumAimRefreshSeconds = 0.25f;
	constexpr float kMaximumAimRefreshSeconds = 1.0f;
#define ASTRABOT_COMBAT_INTENT_AIM_OFFSET_SMOOTHING 0.1f
	constexpr float kMaximumFieldOfView = 180.0f;
	if (!point || !context.randomSource || !std::isfinite(context.skill) || context.skill < 0.0f ||
		context.skill > 1.0f || !std::isfinite(context.timestamp) || context.timestamp < 0.0f ||
		!std::isfinite(context.fieldOfView) || context.fieldOfView <= 0.0f || context.fieldOfView > kMaximumFieldOfView)
		return false;
	if (std::fabs(spring.yawVelocity) > kFocusResetAngularVelocity ||
		std::fabs(spring.pitchVelocity) > kFocusResetAngularVelocity)
		aimFocusStart_ = context.timestamp;
	if (context.timestamp >= aimRefreshTime_)
	{
		const float dx = target.x - context.origin.x, dy = target.y - context.origin.y,
					dz = target.z - context.origin.z;
		const float focusTime = std::fmax(kFocusSkillSeconds * (1.0f - context.skill), kMinimumFocusSeconds);
		const float focusAccuracy =
			std::fmin(kMaximumFocusAccuracy, std::fmax(0.0f, (context.timestamp - aimFocusStart_) / focusTime));
		const float accuracy = std::fmax(context.skill, focusAccuracy);
		const float errorRange =
			context.aimErrorRange >= 0.0f ? context.aimErrorRange : std::sqrt(dx * dx + dy * dy + dz * dz);
		const float error = errorRange * context.fieldOfView / kAimErrorReferenceFieldOfView *
			ASTRABOT_COMBAT_INTENT_AIM_ERROR_SCALE * (1.0f - accuracy);
		const compat::RandomActor actor = {context.actor.slot, context.actor.generation};
		float draws[kAimRandomDrawCount] = {};
		const char *ids[kAimRandomDrawCount] = {"AimOffsetX", "AimOffsetY", "AimOffsetZ", "AimOffsetRefresh"};
		for (unsigned i = 0; i != kAimRandomDrawCount; ++i)
		{
			const compat::RandomFloatResult draw =
				context.randomSource->nextFloat(compat::RandomRequest::floatRequest(ids[i], actor,
					context.randomTiming, i == kAimRefreshDrawIndex ? kMinimumAimRefreshSeconds : -error,
					i == kAimRefreshDrawIndex ? kMaximumAimRefreshSeconds : error));
			if (draw.status != compat::RandomStatus::Ok || !std::isfinite(draw.value))
				return false;
			draws[i] = draw.value;
		}
		aimOffsetGoal_ = {draws[0], draws[1], draws[2]};
		aimRefreshTime_ = context.timestamp + draws[kAimRefreshDrawIndex];
	}
	aimOffset_.x += ASTRABOT_COMBAT_INTENT_AIM_OFFSET_SMOOTHING * (aimOffsetGoal_.x - aimOffset_.x);
	aimOffset_.y += ASTRABOT_COMBAT_INTENT_AIM_OFFSET_SMOOTHING * (aimOffsetGoal_.y - aimOffset_.y);
	aimOffset_.z += ASTRABOT_COMBAT_INTENT_AIM_OFFSET_SMOOTHING * (aimOffsetGoal_.z - aimOffset_.z);
	point->x += aimOffset_.x;
	point->y += aimOffset_.y;
	point->z += aimOffset_.z;
	return true;
}
#undef ASTRABOT_COMBAT_INTENT_AIM_ERROR_SCALE
#undef ASTRABOT_COMBAT_INTENT_AIM_OFFSET_SMOOTHING

bool CombatController::buildAim(const CombatDecisionContext &context, const TargetBelief &target, AimIntent *intent)
{
	constexpr float kMovementPredictionMinimumSkill = 0.5f;
	constexpr float kAimUpkeepRateHz = 30.0f;
	constexpr float kMovementPredictionFrames = 3.0f;
	constexpr float kMaximumAimElapsedSeconds = 0.25f;
	constexpr float kMaximumFieldOfView = 180.0f;
#define ASTRABOT_COMBAT_INTENT_MINIMUM_HORIZONTAL_AIM_DISTANCE 0.001f
	constexpr float kHalfTurnDegrees = 180.0f;
	if (intent == nullptr || !context.isValid() || !target.isUsable())
	{
		return false;
	}

	world::WorldPosition aimPoint = target.position;
	if (context.predictMovement && context.skill > kMovementPredictionMinimumSkill)
	{
		if (!isFiniteVector(context.observerVelocity) || !isFiniteVector(context.targetVelocity))
			return false;
		const float lead = (1.0f / kAimUpkeepRateHz) * kMovementPredictionFrames;
		aimPoint.x += (context.targetVelocity.x - context.observerVelocity.x) * lead;
		aimPoint.y += (context.targetVelocity.y - context.observerVelocity.y) * lead;
		aimPoint.z += (context.targetVelocity.z - context.observerVelocity.z) * lead;
	}
	aimAligned_ = true;
	if (context.aimTrackingEnabled)
	{
		if (!context.randomSource || !std::isfinite(context.skill) || context.skill < 0.0f || context.skill > 1.0f ||
			!std::isfinite(context.elapsedSeconds) || context.elapsedSeconds < 0.0f ||
			context.elapsedSeconds > kMaximumAimElapsedSeconds || !std::isfinite(context.timestamp) ||
			context.timestamp < 0.0f || !std::isfinite(context.currentPitch) || !std::isfinite(context.currentYaw) ||
			!std::isfinite(context.fieldOfView) || context.fieldOfView <= 0.0f ||
			context.fieldOfView > kMaximumFieldOfView)
			return false;
		if (!offsetAimPoint(context, target.position, aimSpring_, &aimPoint))
			return false;
	}
	const float deltaX = aimPoint.x - context.origin.x;
	const float deltaY = aimPoint.y - context.origin.y;
	const float deltaZ = aimPoint.z - context.origin.z;
	const float horizontalDistance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
	if (!std::isfinite(horizontalDistance) ||
		horizontalDistance <= ASTRABOT_COMBAT_INTENT_MINIMUM_HORIZONTAL_AIM_DISTANCE)
	{
		return false;
	}

	*intent = {};
	intent->actor = context.actor;
	intent->frame = context.frame;
	intent->target = target.target;
	intent->point = aimPoint;
	intent->yaw = std::atan2(deltaY, deltaX) * kHalfTurnDegrees / kPi;
	intent->pitch = -std::atan2(deltaZ, horizontalDistance) * kHalfTurnDegrees / kPi;
	const float useYaw = context.lookOverride ? context.lookYaw : intent->yaw;
	const float usePitch = context.lookOverride && context.lookPitchOverride ? context.lookPitch : intent->pitch;
	if (!std::isfinite(useYaw) || !std::isfinite(usePitch))
		return false;
	if (context.aimTrackingEnabled)
	{
		if (!(springActor_ == context.actor) || springFrame_.mapGeneration != context.frame.mapGeneration ||
			springFrame_.roundGeneration != context.frame.roundGeneration)
			aimSpring_ = {};
		springActor_ = context.actor;
		springFrame_ = context.frame;
		// The reference spring is tuned for 30 Hz upkeep. Full decisions arrive less
		// often: integrate bounded steps without repeating observations or RNG draws.
		const unsigned steps =
			(std::max)(1U, static_cast<unsigned>(std::ceil(context.elapsedSeconds / (1.0f / kAimUpkeepRateHz))));
		const float elapsed = context.elapsedSeconds / static_cast<float>(steps);
		compat::AimStep step = {context.currentYaw, context.currentPitch, false};
		for (unsigned i = 0U; i < steps; ++i)
			step = compat::trackAim(step.yaw, step.pitch, useYaw, usePitch, elapsed, context.skill, &aimSpring_);
		intent->yaw = step.yaw;
		intent->pitch = step.pitch;
		aimAligned_ = step.aligned;
	}
	if (!intent->isValid())
		return false;
	trackedAim_ = *intent;
	return true;
}
#undef ASTRABOT_COMBAT_INTENT_MINIMUM_HORIZONTAL_AIM_DISTANCE

bool CombatController::upkeepLook(
	const CombatDecisionContext &context, float yaw, float pitch, bool attacking, compat::AimStep *look)
{
	if (!look || !context.isValid() || !std::isfinite(yaw) || !std::isfinite(pitch) ||
		!std::isfinite(context.elapsedSeconds) || context.elapsedSeconds < 0 || context.elapsedSeconds > .1f ||
		!std::isfinite(context.currentYaw) || !std::isfinite(context.currentPitch) || !std::isfinite(context.skill) ||
		context.skill < 0 || context.skill > 1 || (initialized_ && !(actor_ == context.actor)))
		return false;
	if (!(springActor_ == context.actor) || springFrame_.mapGeneration != context.frame.mapGeneration ||
		springFrame_.roundGeneration != context.frame.roundGeneration)
		aimSpring_ = {};
	else if (context.frame.tick < springFrame_.tick)
		return false;
	springActor_ = context.actor;
	springFrame_ = context.frame;
	const unsigned steps = (std::max)(1U, static_cast<unsigned>(std::ceil(context.elapsedSeconds * 30.0f)));
	const float elapsed = context.elapsedSeconds / static_cast<float>(steps);
	compat::AimStep step = {context.currentYaw, context.currentPitch, false};
	for (unsigned i = 0; i < steps; ++i)
		step = compat::trackAim(step.yaw, step.pitch, yaw, pitch, elapsed, context.skill, &aimSpring_, attacking);
	*look = step;
	return true;
}

bool CombatController::upkeepAim(const TargetBelief &target, const CombatDecisionContext &context, AimIntent *aim)
{
	if (!initialized_ || !(actor_ == context.actor) || !trackedAim_.isValid() ||
		!(trackedAim_.actor == context.actor) || !(trackedAim_.target == target.target) ||
		trackedAim_.frame.mapGeneration != context.frame.mapGeneration ||
		trackedAim_.frame.roundGeneration != context.frame.roundGeneration ||
		!isFrameAfter(context.frame, trackedAim_.frame))
		return false;
	return buildAim(context, target, aim);
}

CombatResult CombatController::decide(const world::WorldSnapshot &snapshot, const WeaponInventory &inventory,
	const TargetBelief &target, const CombatDecisionContext &context, CombatDecision *decision)
{
	if (decision == nullptr)
	{
		return CombatResult::InvalidArgument;
	}
	*decision = {};
	trackedAim_ = {};
	if (!config_.isValid())
	{
		return CombatResult::InvalidConfig;
	}
	if (!snapshot.isValid())
	{
		return CombatResult::InvalidSnapshot;
	}
	if (!context.isValid())
	{
		return CombatResult::InvalidObservation;
	}
	if (!(snapshot.identity().frame == context.frame) || !(snapshot.identity().observer == context.actor))
	{
		return CombatResult::InvalidObservation;
	}
	if (!target.isValid())
	{
		return CombatResult::InvalidObservation;
	}
	if (!target.isUsable())
	{
		return target.friendlyFireRisk ? CombatResult::FriendlyFireRisk : CombatResult::InvalidTarget;
	}
	if (target.confidence < config_.minimumTargetConfidence || target.ageTicks > config_.maximumTargetAgeTicks)
	{
		return CombatResult::InvalidTarget;
	}

	if (!initialized_)
	{
		actor_ = context.actor;
		initialized_ = true;
	}
	else if (!(actor_ == context.actor))
	{
		if (actor_.slot != context.actor.slot || context.actor.generation <= actor_.generation)
		{
			return CombatResult::StaleGeneration;
		}
		actor_ = context.actor;
		lastFrame_ = {};
		aimSpring_ = {};
		aimOffset_ = {};
		aimOffsetGoal_ = {};
		aimRefreshTime_ = 0.0f;
		aimFocusStart_ = context.timestamp;
		return CombatResult::ActorGenerationChanged;
	}

	const bool firstFrame = !lastFrame_.isValid();
	if (!firstFrame && sameFrame(context.frame, lastFrame_))
	{
		return CombatResult::DuplicateFrame;
	}
	if (!firstFrame && !isFrameAfter(context.frame, lastFrame_))
	{
		return CombatResult::StaleFrame;
	}
	if (firstFrame || context.frame.mapGeneration != lastFrame_.mapGeneration ||
		context.frame.roundGeneration != lastFrame_.roundGeneration)
	{
		aimOffset_ = {};
		aimOffsetGoal_ = {};
		aimRefreshTime_ = 0.0f;
		aimFocusStart_ = context.timestamp;
	}
	lastFrame_ = context.frame;

	AimIntent aim = {};
	if (!buildAim(context, target, &aim))
	{
		return CombatResult::InvalidTarget;
	}

	const WeaponRecord *weapon = nullptr;
	if (inventory.selectReady(context.frame, &weapon) == WeaponInventoryResult::Selected && weapon != nullptr)
	{
		decision->actor = context.actor;
		decision->frame = context.frame;
		decision->hasAim = true;
		decision->aim = aim;
		const float dx = target.position.x - context.origin.x, dy = target.position.y - context.origin.y,
					dz = target.position.z - context.origin.z;
		if (!compat::weaponEngagementAllowed(weapon->kind, std::sqrt(dx * dx + dy * dy + dz * dz)) ||
			(context.aimTrackingEnabled &&
				!compat::combatAimOnTarget(aim.point, context.origin, aim.yaw, weapon->kind == WeaponKind::Sniper)))
			return CombatResult::NoAction;
		decision->hasFire = true;
		decision->fire.actor = context.actor;
		decision->fire.frame = context.frame;
		decision->fire.target = target.target;
		decision->fire.weapon = weapon->weapon;
		return CombatResult::FireIntentReady;
	}

	if (inventory.selectReloadable(context.frame, &weapon) == WeaponInventoryResult::Reloadable && weapon != nullptr)
	{
		decision->actor = context.actor;
		decision->frame = context.frame;
		decision->hasReload = true;
		decision->reload.actor = context.actor;
		decision->reload.frame = context.frame;
		decision->reload.weapon = weapon->weapon;
		return CombatResult::ReloadIntentReady;
	}

	return CombatResult::NoAction;
}

CombatResult CombatController::observeDamage(const DamageObservation &observation) const
{
	if (!observation.isValid())
	{
		return CombatResult::InvalidObservation;
	}
	if (!initialized_ || !(observation.actor == actor_))
	{
		return CombatResult::StaleGeneration;
	}
	if (observation.state != DamageObservationState::Observed)
	{
		return CombatResult::NoAction;
	}
	return observation.isConfirmedDeath() ? CombatResult::DeathObserved : CombatResult::DamageObserved;
}

const world::ActorKey &CombatController::actor() const
{
	return actor_;
}

bool CombatController::isInitialized() const
{
	return initialized_;
}
}
}
