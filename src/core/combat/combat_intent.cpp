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

bool isValidTargetState(TargetBeliefState state)
{
	return state == TargetBeliefState::Unknown ||
		state == TargetBeliefState::Visible ||
		state == TargetBeliefState::Remembered ||
		state == TargetBeliefState::Unavailable;
}

bool isFiniteVector(const world::WorldVector &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
		std::isfinite(value.z);
}

bool isBoundedPosition(const world::WorldPosition &value)
{
	return isFiniteVector(value) &&
		std::fabs(value.x) <= world::WorldLimits::kMaximumCoordinate &&
		std::fabs(value.y) <= world::WorldLimits::kMaximumCoordinate &&
		std::fabs(value.z) <= world::WorldLimits::kMaximumCoordinate;
}
}

bool TargetBelief::isValid() const
{
	return target.isValid() && isValidTargetState(state) &&
		isBoundedPosition(position) && std::isfinite(confidence) &&
		confidence >= 0.0f && confidence <= 1.0f &&
		ageTicks <= world::WorldLimits::kMaximumAgeTicks;
}

bool TargetBelief::isUsable() const
{
	return isValid() &&
		(state == TargetBeliefState::Visible ||
		 state == TargetBeliefState::Remembered) && confidence > 0.0f &&
		!friendlyFireRisk;
}

bool CombatDecisionContext::isValid() const
{
	return actor.isValid() && frame.isValid() && isBoundedPosition(origin);
}

bool AimIntent::isValid() const
{
	return actor.isValid() && frame.isValid() && target.isValid() &&
		isBoundedPosition(point) && std::isfinite(pitch) &&
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
	return actor.isValid() && frame.isValid() && target.isValid() &&
		weapon.isValid();
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
	return actor.isValid() && target.isValid() && frame.isValid() &&
		(state == DamageObservationState::Unknown ||
		 state == DamageObservationState::Observed ||
		 state == DamageObservationState::Unavailable) &&
		std::isfinite(amount) && amount >= 0.0f && amount <= 100.0f;
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
	: maximumTargetAgeTicks(128U),
	  minimumTargetConfidence(0.25f)
{
}

bool CombatConfig::isValid() const
{
	return maximumTargetAgeTicks <= world::WorldLimits::kMaximumAgeTicks &&
		std::isfinite(minimumTargetConfidence) &&
		minimumTargetConfidence >= 0.0f && minimumTargetConfidence <= 1.0f;
}

CombatController::CombatController()
	: config_(),
	  actor_(),
	  lastFrame_(),
	  initialized_(false)
{
}

CombatController::CombatController(const world::ActorKey &actor)
	: config_(),
	  actor_(actor),
	  lastFrame_(),
	  initialized_(actor.isValid())
{
}

CombatController::CombatController(
	const world::ActorKey &actor,
	const CombatConfig &config)
	: config_(config),
	  actor_(actor),
	  lastFrame_(),
	  initialized_(actor.isValid() && config.isValid())
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

bool CombatController::sameFrame(
	const world::FrameIdentity &left,
	const world::FrameIdentity &right)
{
	return left == right;
}

bool CombatController::isFrameAfter(
	const world::FrameIdentity &candidate,
	const world::FrameIdentity &current)
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

bool CombatController::buildAim(
	const CombatDecisionContext &context,
	const TargetBelief &target,
	AimIntent *intent)
{
	if (intent == nullptr || !context.isValid() || !target.isUsable())
	{
		return false;
	}

	const float deltaX = target.position.x - context.origin.x;
	const float deltaY = target.position.y - context.origin.y;
	const float deltaZ = target.position.z - context.origin.z;
	const float horizontalDistance = std::sqrt(
		deltaX * deltaX + deltaY * deltaY);
	if (!std::isfinite(horizontalDistance) || horizontalDistance <= 0.001f)
	{
		return false;
	}

	*intent = {};
	intent->actor = context.actor;
	intent->frame = context.frame;
	intent->target = target.target;
	intent->point = target.position;
	intent->yaw = std::atan2(deltaY, deltaX) * 180.0f / kPi;
	intent->pitch = -std::atan2(deltaZ, horizontalDistance) * 180.0f / kPi;
	return intent->isValid();
}

CombatResult CombatController::decide(
	const world::WorldSnapshot &snapshot,
	const WeaponInventory &inventory,
	const TargetBelief &target,
	const CombatDecisionContext &context,
	CombatDecision *decision)
{
	if (decision == nullptr)
	{
		return CombatResult::InvalidArgument;
	}
	*decision = {};
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
	if (!(snapshot.identity().frame == context.frame) ||
		!(snapshot.identity().observer == context.actor))
	{
		return CombatResult::InvalidObservation;
	}
	if (!target.isValid())
	{
		return CombatResult::InvalidObservation;
	}
	if (!target.isUsable())
	{
		return target.friendlyFireRisk ? CombatResult::FriendlyFireRisk :
			CombatResult::InvalidTarget;
	}
	if (target.confidence < config_.minimumTargetConfidence ||
		target.ageTicks > config_.maximumTargetAgeTicks)
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
		if (actor_.slot != context.actor.slot ||
				context.actor.generation <= actor_.generation)
		{
			return CombatResult::StaleGeneration;
		}
		actor_ = context.actor;
		lastFrame_ = {};
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
	lastFrame_ = context.frame;

	AimIntent aim = {};
	if (!buildAim(context, target, &aim))
	{
		return CombatResult::InvalidTarget;
	}

	const WeaponRecord *weapon = nullptr;
	if (inventory.selectReady(context.frame, &weapon) ==
			WeaponInventoryResult::Selected && weapon != nullptr)
	{
		decision->actor = context.actor;
		decision->frame = context.frame;
		decision->hasAim = true;
		decision->aim = aim;
		decision->hasFire = true;
		decision->fire.actor = context.actor;
		decision->fire.frame = context.frame;
		decision->fire.target = target.target;
		decision->fire.weapon = weapon->weapon;
		return CombatResult::FireIntentReady;
	}

	if (inventory.selectReloadable(context.frame, &weapon) ==
			WeaponInventoryResult::Reloadable && weapon != nullptr)
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

CombatResult CombatController::observeDamage(
	const DamageObservation &observation) const
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
	return observation.isConfirmedDeath() ? CombatResult::DeathObserved :
		CombatResult::DamageObserved;
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
