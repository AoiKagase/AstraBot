#include "astrabot/behavior/behavior_state.hpp"

#include <cmath>
#include <limits>

namespace astrabot
{
namespace behavior
{
BehaviorStateConfig::BehaviorStateConfig()
	: retreatHealth(25.0f),
	  maximumRecoveryTicks(64U),
	  maximumTargetMemoryTicks(128U)
{
}

bool BehaviorStateConfig::isValid() const
{
	return std::isfinite(retreatHealth) && retreatHealth >= 0.0f &&
		retreatHealth <= 100.0f &&
		maximumRecoveryTicks <= world::WorldLimits::kMaximumAgeTicks &&
		maximumTargetMemoryTicks <= world::WorldLimits::kMaximumAgeTicks;
}

bool ActorDecisionContext::isValid() const
{
	if (!actor.isValid() || !frame.isValid() ||
		!BehaviorStateMachine::isValidLifeStatus(life) ||
		!BehaviorStateMachine::isValidAvailability(roundAvailability) ||
		!BehaviorStateMachine::isValidAvailability(healthAvailability))
	{
		return false;
	}

	return healthAvailability != Availability::Available ||
		(std::isfinite(health) && health >= 0.0f && health <= 100.0f);
}

BehaviorStateMachine::BehaviorStateMachine()
	: config_(),
	  actor_(),
	  lastFrame_(),
	  rememberedTarget_(),
	  targetFrame_(),
	  state_(BehaviorState::Initial),
	  recoveryTicks_(0U),
	  hasRememberedTarget_(false),
	  initialized_(false)
{
}

BehaviorStateMachine::BehaviorStateMachine(
	const world::ActorKey &actor,
	const BehaviorStateConfig &config)
	: config_(config),
	  actor_(actor),
	  lastFrame_(),
	  rememberedTarget_(),
	  targetFrame_(),
	  state_(BehaviorState::Initial),
	  recoveryTicks_(0U),
	  hasRememberedTarget_(false),
	  initialized_(actor.isValid() && config.isValid())
{
}

bool BehaviorStateMachine::isValidAvailability(Availability availability)
{
	return availability == Availability::Unknown ||
		availability == Availability::Available ||
		availability == Availability::Unavailable;
}

bool BehaviorStateMachine::isValidLifeStatus(LifeStatus life)
{
	return life == LifeStatus::Unknown || life == LifeStatus::Alive ||
		life == LifeStatus::Dead;
}

bool BehaviorStateMachine::sameFrame(
	const world::FrameIdentity &left,
	const world::FrameIdentity &right)
{
	return left == right;
}

bool BehaviorStateMachine::isFrameAfter(
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

bool BehaviorStateMachine::findVisibleEnemy(
	const world::WorldSnapshot &snapshot,
	const world::ActorKey &observer,
	world::ActorKey *enemy)
{
	if (enemy == nullptr)
	{
		return false;
	}

	*enemy = {};
	for (std::size_t index = 0U; index < snapshot.actorCount(); ++index)
	{
		const world::ActorObservation *observation = snapshot.actorAt(index);
		if (observation == nullptr || observation->actor == observer ||
				observation->relation != world::TeamRelation::Hostile ||
				!observation->isConfirmed())
		{
			continue;
		}

		*enemy = observation->actor;
		return true;
	}

	return false;
}

bool BehaviorStateMachine::isTargetMemoryCurrent(
	const world::FrameIdentity &frame,
	const world::FrameIdentity &targetFrame,
	std::uint32_t maximumAgeTicks)
{
	return frame.mapGeneration == targetFrame.mapGeneration &&
		frame.roundGeneration == targetFrame.roundGeneration &&
		frame.tick >= targetFrame.tick &&
		frame.tick - targetFrame.tick <= maximumAgeTicks;
}

ObjectivePriority BehaviorStateMachine::priorityFor(BehaviorState state)
{
	switch (state)
	{
	case BehaviorState::Roam:
		return ObjectivePriority::Low;
	case BehaviorState::Seek:
		return ObjectivePriority::Normal;
	case BehaviorState::Engage:
		return ObjectivePriority::High;
	case BehaviorState::Retreat:
		return ObjectivePriority::Critical;
	case BehaviorState::Initial:
	case BehaviorState::Dead:
	case BehaviorState::Recovering:
		break;
	}

	return ObjectivePriority::Low;
}

ObjectiveKind BehaviorStateMachine::objectiveFor(BehaviorState state)
{
	switch (state)
	{
	case BehaviorState::Roam:
		return ObjectiveKind::Roam;
	case BehaviorState::Seek:
		return ObjectiveKind::Seek;
	case BehaviorState::Engage:
		return ObjectiveKind::Engage;
	case BehaviorState::Retreat:
		return ObjectiveKind::Retreat;
	case BehaviorState::Initial:
	case BehaviorState::Dead:
	case BehaviorState::Recovering:
		break;
	}

	return ObjectiveKind::None;
}

bool BehaviorStateMachine::makeProposal(
	const world::WorldSnapshot &snapshot,
	const world::ActorKey &actor,
	BehaviorState state,
	TransitionReason reason,
	const world::ActorKey &target,
	bool hasTarget,
	ObjectiveProposal *proposal)
{
	if (proposal == nullptr || !actor.isValid() ||
		!(snapshot.identity().observer == actor) ||
		objectiveFor(state) == ObjectiveKind::None)
	{
		return false;
	}

	std::uint32_t lifetime = state == BehaviorState::Engage ? 16U : 64U;
	if (state == BehaviorState::Seek)
	{
		lifetime = 32U;
	}

	if (snapshot.identity().frame.tick >
			std::numeric_limits<std::uint32_t>::max() - lifetime)
	{
		return false;
	}

	*proposal = {};
	proposal->actor = actor;
	proposal->sourceFrame = snapshot.identity().frame;
	proposal->sourceNavRevision = snapshot.identity().navRevision;
	proposal->objective = objectiveFor(state);
	proposal->priority = priorityFor(state);
	proposal->issuedTick = snapshot.identity().frame.tick;
	proposal->expiresAtTick = proposal->issuedTick + lifetime;
	proposal->hasTarget = hasTarget &&
		(state == BehaviorState::Seek || state == BehaviorState::Engage);
	proposal->target = proposal->hasTarget ? target : world::ActorKey{};
	(void)reason;
	return proposal->isValid();
}

BehaviorResult BehaviorStateMachine::transitionTo(
	const world::WorldSnapshot &snapshot,
	BehaviorState nextState,
	TransitionReason reason,
	const world::ActorKey &target,
	bool hasTarget,
	BehaviorTransition *transition)
{
	if (transition == nullptr)
	{
		return BehaviorResult::InvalidArgument;
	}

	const BehaviorState previous = state_;
	const bool forcedTransition =
		reason == TransitionReason::ActorGenerationChanged ||
		reason == TransitionReason::MapGenerationChanged ||
		reason == TransitionReason::RoundReset;
	const bool changed = previous != nextState || forcedTransition;
	state_ = nextState;
	transition->actor = actor_;
	transition->frame = snapshot.identity().frame;
	transition->from = previous;
	transition->to = nextState;
	transition->reason = reason;
	transition->hasObjectiveProposal = changed && makeProposal(
		snapshot,
		actor_,
		nextState,
		reason,
		target,
		hasTarget,
		&transition->proposal);
	if (!transition->hasObjectiveProposal)
	{
		transition->proposal = {};
	}

	return changed ? BehaviorResult::Transitioned : BehaviorResult::Unchanged;
}

BehaviorResult BehaviorStateMachine::update(
	const world::WorldSnapshot &snapshot,
	const ActorDecisionContext &context,
	BehaviorTransition *transition)
{
	if (transition == nullptr)
	{
		return BehaviorResult::InvalidArgument;
	}
	if (!config_.isValid())
	{
		return BehaviorResult::InvalidConfig;
	}
	if (!snapshot.isValid())
	{
		return BehaviorResult::InvalidSnapshot;
	}
	if (!context.isValid())
	{
		return BehaviorResult::InvalidObservation;
	}
	if (!(snapshot.identity().frame == context.frame) ||
		!(snapshot.identity().observer == context.actor))
	{
		return BehaviorResult::InvalidIdentity;
	}

	if (!initialized_)
	{
		actor_ = context.actor;
		initialized_ = true;
		state_ = BehaviorState::Initial;
		recoveryTicks_ = 0U;
		hasRememberedTarget_ = false;
	}
	else if (!(context.actor == actor_))
	{
		if (context.actor.slot != actor_.slot ||
				context.actor.generation <= actor_.generation)
		{
			return BehaviorResult::StaleGeneration;
		}

		actor_ = context.actor;
		lastFrame_ = context.frame;
		state_ = BehaviorState::Initial;
		recoveryTicks_ = 0U;
		hasRememberedTarget_ = false;
		return transitionTo(
			snapshot,
			BehaviorState::Initial,
			TransitionReason::ActorGenerationChanged,
			{},
			false,
			transition);
	}

	const bool firstFrame = !lastFrame_.isValid();
	if (!firstFrame && sameFrame(context.frame, lastFrame_))
	{
		return BehaviorResult::DuplicateFrame;
	}
	if (!firstFrame && !isFrameAfter(context.frame, lastFrame_))
	{
		return BehaviorResult::StaleFrame;
	}

	if (!firstFrame && context.frame.mapGeneration != lastFrame_.mapGeneration)
	{
		lastFrame_ = context.frame;
		hasRememberedTarget_ = false;
		recoveryTicks_ = 0U;
		return transitionTo(
			snapshot,
			BehaviorState::Initial,
			TransitionReason::MapGenerationChanged,
			{},
			false,
			transition);
	}

	if (!firstFrame &&
			context.frame.roundGeneration != lastFrame_.roundGeneration)
	{
		lastFrame_ = context.frame;
		hasRememberedTarget_ = false;
		recoveryTicks_ = 0U;
		return transitionTo(
			snapshot,
			BehaviorState::Initial,
			TransitionReason::RoundReset,
			{},
			false,
			transition);
	}

	lastFrame_ = context.frame;
	if (context.life == LifeStatus::Unknown ||
			context.roundAvailability != Availability::Available ||
			!context.roundActive)
	{
		++recoveryTicks_;
		if (recoveryTicks_ > config_.maximumRecoveryTicks)
		{
			recoveryTicks_ = config_.maximumRecoveryTicks;
			hasRememberedTarget_ = false;
			return transitionTo(
				snapshot,
				BehaviorState::Initial,
				TransitionReason::RecoveryTimeout,
				{},
				false,
				transition);
		}

		const BehaviorResult result = transitionTo(
			snapshot,
			BehaviorState::Recovering,
			TransitionReason::UnavailableObservation,
			{},
			false,
			transition);
		return result == BehaviorResult::InvalidArgument ? result :
			BehaviorResult::RecoveryPending;
	}

	recoveryTicks_ = 0U;
	if (context.life == LifeStatus::Dead)
	{
		hasRememberedTarget_ = false;
		return transitionTo(
			snapshot,
			BehaviorState::Dead,
			TransitionReason::DeathObserved,
			{},
			false,
			transition);
	}

	if (context.healthAvailability == Availability::Available &&
			context.health <= config_.retreatHealth)
	{
		return transitionTo(
			snapshot,
			BehaviorState::Retreat,
			TransitionReason::LowHealth,
			{},
			false,
			transition);
	}

	world::ActorKey visibleEnemy = {};
	const bool enemyVisible = findVisibleEnemy(
		snapshot,
		context.actor,
		&visibleEnemy);
	if (enemyVisible)
	{
		rememberedTarget_ = visibleEnemy;
		targetFrame_ = context.frame;
		hasRememberedTarget_ = true;
		return transitionTo(
			snapshot,
			BehaviorState::Engage,
			TransitionReason::EnemyVisible,
			visibleEnemy,
			true,
			transition);
	}

	if (hasRememberedTarget_ && isTargetMemoryCurrent(
			context.frame,
			targetFrame_,
			config_.maximumTargetMemoryTicks))
	{
		return transitionTo(
			snapshot,
			BehaviorState::Seek,
			TransitionReason::EnemyLost,
			rememberedTarget_,
			true,
			transition);
	}

	hasRememberedTarget_ = false;
	const TransitionReason noContactReason = firstFrame ?
		TransitionReason::Initial :
		(state_ == BehaviorState::Recovering ?
			TransitionReason::RecoveryComplete :
			TransitionReason::NoConfirmedContact);
	return transitionTo(
		snapshot,
		BehaviorState::Roam,
		noContactReason,
		{},
		false,
		transition);
}

BehaviorState BehaviorStateMachine::state() const
{
	return state_;
}

const world::ActorKey &BehaviorStateMachine::actor() const
{
	return actor_;
}

bool BehaviorStateMachine::isInitialized() const
{
	return initialized_;
}
}
}
