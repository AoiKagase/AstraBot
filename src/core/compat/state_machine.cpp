#include "astrabot/compat/state_machine.hpp"

#include <cmath>
#include <limits>

namespace astrabot
{
namespace compat
{
namespace
{
// State residence begins on entry, independently of delivered plant inputs.
constexpr float kPlantStateTimeoutSeconds = 5.0f;
const std::array<CompatibilityStateId, CompatibilityStateMachine::kNormalStateCount> kNormalStates = {
	{CompatibilityStateId::Idle, CompatibilityStateId::Buy, CompatibilityStateId::DefuseBomb,
		CompatibilityStateId::EscapeFromBomb, CompatibilityStateId::FetchBomb, CompatibilityStateId::Follow,
		CompatibilityStateId::Hide, CompatibilityStateId::Hunt, CompatibilityStateId::InvestigateNoise,
		CompatibilityStateId::MoveTo, CompatibilityStateId::PlantBomb, CompatibilityStateId::UseEntity}};
}

bool StateUpdateContext::isValid() const
{
	return actor.isValid() && frame.isValid() && std::isfinite(timestamp) &&
		(lifeAvailability == ObservationAvailability::Available ||
			lifeAvailability == ObservationAvailability::Unavailable) &&
		(roundAvailability == ObservationAvailability::Available ||
			roundAvailability == ObservationAvailability::Unavailable) &&
		(transitionRequest == ObservationAvailability::Available ||
			transitionRequest == ObservationAvailability::Unavailable) &&
		(attackObservation == ObservationAvailability::Available ||
			attackObservation == ObservationAvailability::Unavailable) &&
		(rngAvailability == ObservationAvailability::Available ||
			rngAvailability == ObservationAvailability::Unavailable);
}

const char *toString(CompatibilityStateId state)
{
	switch (state)
	{
	case CompatibilityStateId::None:
		return "STATE-NONE";
	case CompatibilityStateId::Idle:
		return "STATE-IDLE";
	case CompatibilityStateId::AttackOverlay:
		return "STATE-ATTACK-OVERLAY";
	case CompatibilityStateId::Buy:
		return "STATE-BUY";
	case CompatibilityStateId::DefuseBomb:
		return "STATE-DEFUSE-BOMB";
	case CompatibilityStateId::EscapeFromBomb:
		return "STATE-ESCAPE-BOMB";
	case CompatibilityStateId::FetchBomb:
		return "STATE-FETCH-BOMB";
	case CompatibilityStateId::Follow:
		return "STATE-FOLLOW";
	case CompatibilityStateId::Hide:
		return "STATE-HIDE";
	case CompatibilityStateId::Hunt:
		return "STATE-HUNT";
	case CompatibilityStateId::InvestigateNoise:
		return "STATE-INVESTIGATE-NOISE";
	case CompatibilityStateId::MoveTo:
		return "STATE-MOVE-TO";
	case CompatibilityStateId::PlantBomb:
		return "STATE-PLANT-BOMB";
	case CompatibilityStateId::UseEntity:
		return "STATE-USE-ENTITY";
	default:
		return "STATE-NONE";
	}
}

const char *toString(CompatibilityTaskId task)
{
	switch (task)
	{
	case CompatibilityTaskId::None:
		return "TASK-NONE";
	case CompatibilityTaskId::SeekAndDestroy:
		return "TASK-SEEK-AND-DESTROY";
	case CompatibilityTaskId::Buy:
		return "TASK-BUY";
	case CompatibilityTaskId::DefuseBomb:
		return "TASK-DEFUSE-BOMB";
	case CompatibilityTaskId::EscapeFromBomb:
		return "TASK-ESCAPE-FROM-BOMB";
	case CompatibilityTaskId::FetchBomb:
		return "TASK-FETCH-BOMB";
	case CompatibilityTaskId::Follow:
		return "TASK-FOLLOW";
	case CompatibilityTaskId::InvestigateNoise:
		return "TASK-INVESTIGATE-NOISE";
	case CompatibilityTaskId::MoveTo:
		return "TASK-MOVE-TO";
	case CompatibilityTaskId::PlantBomb:
		return "TASK-PLANT-BOMB";
	case CompatibilityTaskId::GuardTickingBomb:
		return "TASK-GUARD-TICKING-BOMB";
	case CompatibilityTaskId::MoveToLastKnownEnemyPosition:
		return "TASK-MOVE-TO-LAST-KNOWN-ENEMY-POSITION";
	case CompatibilityTaskId::FindTickingBomb:
		return "TASK-FIND-TICKING-BOMB";
	case CompatibilityTaskId::CollectHostages:
		return "TASK-COLLECT-HOSTAGES";
	case CompatibilityTaskId::RescueHostages:
		return "TASK-RESCUE-HOSTAGES";
	case CompatibilityTaskId::MoveToSniperSpot:
		return "TASK-MOVE-TO-SNIPER-SPOT";
	case CompatibilityTaskId::Sniping:
		return "TASK-SNIPING";
	default:
		return "TASK-NONE";
	}
}

const char *toString(StateTransitionId transition)
{
	switch (transition)
	{
	case StateTransitionId::None:
		return "TRANS-NONE";
	case StateTransitionId::InitialState:
		return "TRANS-INITIAL-IDLE";
	case StateTransitionId::ExplicitStateChange:
		return "TRANS-EXPLICIT-STATE-CHANGE";
	case StateTransitionId::IdleToMoveTo:
		return "TRANS-IDLE-TO-MOVE-TO";
	case StateTransitionId::MoveToHunt:
		return "TRANS-MOVE-TO-HUNT";
	case StateTransitionId::AttackOverlayStart:
		return "TRANS-ANY-TO-ATTACK-OVERLAY";
	case StateTransitionId::AttackOverlayStop:
		return "TRANS-ATTACK-STOP";
	case StateTransitionId::LifecycleDeath:
		return "TRANS-DEATH-TO-IDLE";
	case StateTransitionId::LifecycleRespawn:
		return "TRANS-RESPAWN-TO-IDLE";
	case StateTransitionId::LifecycleRoundReset:
		return "TRANS-ROUND-RESET-TO-IDLE";
	case StateTransitionId::RngDependent:
		return "TRANS-RNG-DEPENDENT";
	default:
		return "TRANS-NONE";
	}
}

const char *toString(StateTransitionReason reason)
{
	switch (reason)
	{
	case StateTransitionReason::None:
		return "none";
	case StateTransitionReason::Initial:
		return "initial";
	case StateTransitionReason::ExplicitRequest:
		return "explicit_request";
	case StateTransitionReason::EnemyVisible:
		return "enemy_visible";
	case StateTransitionReason::EnemyLost:
		return "enemy_lost";
	case StateTransitionReason::ObjectiveChanged:
		return "objective_changed";
	case StateTransitionReason::PathFailure:
		return "path_failure";
	case StateTransitionReason::LeaderInvalid:
		return "leader_invalid";
	case StateTransitionReason::DeathObserved:
		return "death_observed";
	case StateTransitionReason::RespawnObserved:
		return "respawn_observed";
	case StateTransitionReason::RoundReset:
		return "round_reset";
	case StateTransitionReason::AttackStarted:
		return "attack_started";
	case StateTransitionReason::AttackEnded:
		return "attack_ended";
	case StateTransitionReason::ObservationUnavailable:
		return "observation_unavailable";
	case StateTransitionReason::RngUnavailable:
		return "rng_unavailable";
	default:
		return "none";
	}
}

const char *toString(StateResult result)
{
	switch (result)
	{
	case StateResult::Transitioned:
		return "transitioned";
	case StateResult::Updated:
		return "updated";
	case StateResult::NoOp:
		return "no_op";
	case StateResult::SkippedNotFullUpdate:
		return "skipped_not_full_update";
	case StateResult::BlockedByObservation:
		return "blocked_by_observation";
	case StateResult::BlockedByRng:
		return "blocked_by_rng";
	case StateResult::InvalidContext:
		return "invalid_context";
	case StateResult::InvalidArgument:
		return "invalid_argument";
	case StateResult::NotInitialized:
		return "not_initialized";
	case StateResult::DuplicateUpdate:
		return "duplicate_update";
	default:
		return "invalid_context";
	}
}

CompatibilityStateMachine::CompatibilityStateMachine()
	: stateInstances_(), actor_(), state_(CompatibilityStateId::None), task_(CompatibilityTaskId::None),
	  stateTimestamp_(0.0f), lastFullUpdateSequence_(0U), traceSequence_(0U), stateSequence_(0U),
	  hasLastFullUpdate_(false), attackOverlayActive_(false), initialized_(false), traceSink_(nullptr)
{
	for (std::size_t index = 0U; index < stateInstances_.size(); ++index)
	{
		stateInstances_[index] = {kNormalStates[index], 0U, 0U, 0U};
	}
}

CompatibilityStateMachine::CompatibilityStateMachine(const world::ActorKey &actor) : CompatibilityStateMachine()
{
	actor_ = actor;
}

void CompatibilityStateMachine::setTraceSink(IStateTraceSink *sink)
{
	traceSink_ = sink;
}

bool CompatibilityStateMachine::isNormalState(CompatibilityStateId state)
{
	return state != CompatibilityStateId::None && state != CompatibilityStateId::AttackOverlay;
}

bool CompatibilityStateMachine::isValidTransition(CompatibilityStateId state)
{
	return isNormalState(state);
}

bool CompatibilityStateMachine::isAvailable(ObservationAvailability availability)
{
	return availability == ObservationAvailability::Available;
}

std::size_t CompatibilityStateMachine::stateIndex(CompatibilityStateId state)
{
	for (std::size_t index = 0U; index < kNormalStates.size(); ++index)
	{
		if (kNormalStates[index] == state)
		{
			return index;
		}
	}
	return kNormalStates.size();
}

StateUpdateContext CompatibilityStateMachine::resetContextFor(
	const StateUpdateContext &context, StateTransitionReason reason, StateTransitionId transition)
{
	StateUpdateContext value = context;
	value.requestTransition = false;
	value.requestedState = CompatibilityStateId::None;
	value.transitionId = transition;
	value.transitionReason = reason;
	value.transitionRequest = ObservationAvailability::Available;
	return value;
}

bool CompatibilityStateMachine::validateContext(const StateUpdateContext &context) const
{
	return context.isValid() && context.actor == actor_ && context.frame.isValid() && std::isfinite(context.timestamp);
}

CompatibilityStateMachine::StateInstance *CompatibilityStateMachine::instanceFor(CompatibilityStateId state)
{
	const std::size_t index = stateIndex(state);
	return index < stateInstances_.size() ? &stateInstances_[index] : nullptr;
}

StateResult CompatibilityStateMachine::initialize(const StateUpdateContext &context)
{
	if (!context.isValid() || !context.fullUpdate || !actor_.isValid() || !(context.actor == actor_))
	{
		return StateResult::InvalidContext;
	}
	if (initialized_)
	{
		return StateResult::NoOp;
	}
	const StateResult result = transitionInternal(
		CompatibilityStateId::Idle, StateTransitionReason::Initial, StateTransitionId::InitialState, context, true);
	if (result == StateResult::Transitioned)
	{
		initialized_ = true;
		hasLastFullUpdate_ = true;
		lastFullUpdateSequence_ = context.fullUpdateSequence;
	}
	return result;
}

StateResult CompatibilityStateMachine::update(const StateUpdateContext &context)
{
	behaviorDecision_ = {};
	if (!initialized_)
	{
		return StateResult::NotInitialized;
	}
	if (!context.isValid() || !(context.actor == actor_))
	{
		return StateResult::InvalidContext;
	}
	if (!context.fullUpdate)
	{
		return StateResult::SkippedNotFullUpdate;
	}
	if (hasLastFullUpdate_ && context.fullUpdateSequence <= lastFullUpdateSequence_)
	{
		return StateResult::DuplicateUpdate;
	}
	if (!isAvailable(context.lifeAvailability) || !isAvailable(context.roundAvailability))
	{
		return StateResult::BlockedByObservation;
	}
	if (context.requestTransition && !isAvailable(context.transitionRequest))
	{
		return StateResult::BlockedByObservation;
	}
	if (context.requestTransition && context.transitionId == StateTransitionId::RngDependent &&
		!isAvailable(context.rngAvailability))
	{
		return StateResult::BlockedByRng;
	}

	lastFullUpdateSequence_ = context.fullUpdateSequence;
	hasLastFullUpdate_ = true;
	invokeUpdate(context);
	if (!context.requestTransition)
	{
		return StateResult::Updated;
	}
	return transitionInternal(context.requestedState, context.transitionReason, context.transitionId, context, false);
}

StateResult CompatibilityStateMachine::transition(CompatibilityStateId nextState, StateTransitionReason reason,
	StateTransitionId transitionId, const StateUpdateContext &context)
{
	if (!initialized_)
	{
		return StateResult::NotInitialized;
	}
	if (!validateContext(context) || !isValidTransition(nextState))
	{
		return StateResult::InvalidContext;
	}
	return transitionInternal(nextState, reason, transitionId, context, false);
}

StateResult CompatibilityStateMachine::finishNavigation(const StateUpdateContext &context)
{
	if (!initialized_)
		return StateResult::NotInitialized;
	if (!validateContext(context) || context.behavior == nullptr || !context.behavior->navigationOwned)
		return StateResult::InvalidContext;
	const auto &step = context.behavior->navigation;
	if (!(step.actor == context.actor) || step.frame.mapGeneration != context.frame.mapGeneration ||
		step.frame.roundGeneration != context.frame.roundGeneration || step.frame.tick != context.frame.tick ||
		step.fullUpdateSequence != context.fullUpdateSequence)
		return StateResult::InvalidContext;
	if (navigationCompletedFrame_.mapGeneration == context.frame.mapGeneration &&
		navigationCompletedFrame_.roundGeneration == context.frame.roundGeneration &&
		navigationCompletedFrame_.tick >= context.frame.tick)
		return StateResult::NoOp;
	// Only these states consume a path result here. Other normal states were
	// already updated before navigation; do not run their OnUpdate twice.
	if (state_ != CompatibilityStateId::MoveTo && state_ != CompatibilityStateId::InvestigateNoise)
		return StateResult::NoOp;
	navigationCompletedFrame_ = context.frame;
	auto value = context;
	value.requestTransition = false;
	const auto before = state_;
	updateBehavior(value);
	return before == state_ ? StateResult::Updated : StateResult::Transitioned;
}

StateResult CompatibilityStateMachine::beginAttack(const StateUpdateContext &context)
{
	if (!initialized_)
	{
		return StateResult::NotInitialized;
	}
	if (!validateContext(context))
	{
		return StateResult::InvalidContext;
	}
	if (!isAvailable(context.attackObservation))
	{
		return StateResult::BlockedByObservation;
	}
	if (attackOverlayActive_)
	{
		return StateResult::NoOp;
	}
	attackOverlayActive_ = true;
	++stateSequence_;
	emit(StateTraceEvent::AttackOnEnter, CompatibilityStateId::AttackOverlay, state_, state_,
		StateTransitionReason::AttackStarted, StateTransitionId::AttackOverlayStart, context);
	emitSideEffect(CompatibilityStateId::AttackOverlay, state_, state_, StateTransitionReason::AttackStarted,
		StateTransitionId::AttackOverlayStart, context, StateSideEffect::ResetPath);
	return StateResult::Transitioned;
}

StateResult CompatibilityStateMachine::stopAttack(const StateUpdateContext &context)
{
	if (!initialized_)
	{
		return StateResult::NotInitialized;
	}
	if (!validateContext(context))
	{
		return StateResult::InvalidContext;
	}
	return stopAttackInternal(context);
}

StateResult CompatibilityStateMachine::stopAttackInternal(const StateUpdateContext &context)
{
	if (!attackOverlayActive_)
	{
		return StateResult::NoOp;
	}
	emit(StateTraceEvent::AttackOnExit, CompatibilityStateId::AttackOverlay, state_, state_,
		StateTransitionReason::AttackEnded, StateTransitionId::AttackOverlayStop, context);
	attackOverlayActive_ = false;
	if (state_ == CompatibilityStateId::Follow)
	{
		return transitionInternal(CompatibilityStateId::Idle, StateTransitionReason::AttackEnded,
			StateTransitionId::AttackOverlayStop, context, false);
	}
	return StateResult::Transitioned;
}

StateResult CompatibilityStateMachine::onSpawn(const StateUpdateContext &context)
{
	return transition(CompatibilityStateId::Idle, StateTransitionReason::RespawnObserved,
		StateTransitionId::LifecycleRespawn,
		resetContextFor(context, StateTransitionReason::RespawnObserved, StateTransitionId::LifecycleRespawn));
}

StateResult CompatibilityStateMachine::onDeath(const StateUpdateContext &context)
{
	return transitionInternal(CompatibilityStateId::Idle, StateTransitionReason::DeathObserved,
		StateTransitionId::LifecycleDeath,
		resetContextFor(context, StateTransitionReason::DeathObserved, StateTransitionId::LifecycleDeath), true);
}

StateResult CompatibilityStateMachine::onRespawn(const StateUpdateContext &context)
{
	return onSpawn(context);
}

StateResult CompatibilityStateMachine::onRoundReset(const StateUpdateContext &context)
{
	return transitionInternal(CompatibilityStateId::Idle, StateTransitionReason::RoundReset,
		StateTransitionId::LifecycleRoundReset,
		resetContextFor(context, StateTransitionReason::RoundReset, StateTransitionId::LifecycleRoundReset), true);
}

StateResult CompatibilityStateMachine::transitionInternal(CompatibilityStateId nextState, StateTransitionReason reason,
	StateTransitionId transitionId, const StateUpdateContext &context, bool forceLifecycle)
{
	if (!isValidTransition(nextState))
	{
		return StateResult::InvalidArgument;
	}
	if (attackOverlayActive_)
	{
		(void)stopAttackInternal(context);
	}
	const CompatibilityStateId previousState = state_;
	if (!forceLifecycle && previousState == nextState)
	{
		// ReGameDLL SetState does not short-circuit same-state requests.
	}
	if (previousState != CompatibilityStateId::None)
	{
		invokeExit(previousState, nextState, reason, transitionId, context);
	}
	invokeEnter(nextState, previousState, reason, transitionId, context);
	state_ = nextState;
	++stateSequence_;
	if (++normalStateEpoch_ == 0)
		++normalStateEpoch_;
	emit(StateTraceEvent::StatePublished, CompatibilityStateId::None, previousState, nextState, reason, transitionId,
		context);
	stateTimestamp_ = context.timestamp;
	emit(StateTraceEvent::TimestampUpdated, CompatibilityStateId::None, previousState, nextState, reason, transitionId,
		context);
	return StateResult::Transitioned;
}

void CompatibilityStateMachine::invokeEnter(CompatibilityStateId state, CompatibilityStateId previousState,
	StateTransitionReason reason, StateTransitionId transitionId, const StateUpdateContext &context)
{
	StateInstance *instance = instanceFor(state);
	if (instance != nullptr)
	{
		++instance->enterCount;
	}
	emit(StateTraceEvent::OnEnter, state, previousState, state, reason, transitionId, context);
	applyEnterEffects(state, previousState, reason, transitionId, context);
}

void CompatibilityStateMachine::invokeUpdate(const StateUpdateContext &context)
{
	if (attackOverlayActive_)
	{
		updateBehavior(context);
		emit(StateTraceEvent::AttackOnUpdate, CompatibilityStateId::AttackOverlay, state_, state_,
			StateTransitionReason::AttackStarted, StateTransitionId::AttackOverlayStart, context);
		return;
	}
	StateInstance *instance = instanceFor(state_);
	if (instance != nullptr)
	{
		++instance->updateCount;
	}
	emit(StateTraceEvent::OnUpdate, state_, state_, state_, StateTransitionReason::None, StateTransitionId::None,
		context);
	updateBehavior(context);
}

void CompatibilityStateMachine::updateBehavior(const StateUpdateContext &context)
{
	constexpr float kHuntTimeoutSeconds = 30.0f;
	constexpr float kDefuseReceiptTimeoutSeconds = 1.0f;
	if (!context.behavior)
		return;
	const StateBehaviorInput &input = *context.behavior;
	if (!context.alive || !context.roundActive)
	{
		behaviorDecision_.clearMovement = true;
		return;
	}
	if (attackOverlayActive_)
	{
		behaviorDecision_.action = StateAction::Attack;
		return;
	}
	CompatibilityStateId next = state_;
	switch (state_)
	{
	case CompatibilityStateId::Idle:
		behaviorDecision_.action = StateAction::Hunt;
		if (input.navigationOwned && input.navigationReady)
			next = CompatibilityStateId::Hunt;
		break;
	case CompatibilityStateId::Hunt:
		behaviorDecision_.action = StateAction::Hunt;
		if (input.navigationOwned && context.timestamp - stateTimestamp_ > kHuntTimeoutSeconds)
			next = CompatibilityStateId::Idle;
		break;
	case CompatibilityStateId::MoveTo:
		behaviorDecision_.action = StateAction::Move;
		if (input.navigationOwned)
		{
			if (input.navigation.outcome == NavigationStepOutcome::EndOfPath ||
				input.navigation.outcome == NavigationStepOutcome::PathFailure)
				next = task_ == CompatibilityTaskId::DefuseBomb && input.defuseGoalKnown && input.withinDefuseRange &&
						(input.reloadKnown || input.reloadInferred) && !input.reloading
					? CompatibilityStateId::DefuseBomb
					: CompatibilityStateId::Idle;
		}
		else if (input.pathFailed || input.goalReached || !input.goalAvailable)
			next = CompatibilityStateId::Idle;
		break;
	case CompatibilityStateId::Follow:
		behaviorDecision_.action = StateAction::Follow;
		if (!input.leaderAvailable)
			next = CompatibilityStateId::Idle;
		break;
	case CompatibilityStateId::InvestigateNoise:
		behaviorDecision_.action = StateAction::Investigate;
		if (!input.noiseAvailable || input.goalReached || input.pathFailed ||
			(input.navigationOwned &&
				(input.navigation.outcome == NavigationStepOutcome::EndOfPath ||
					input.navigation.outcome == NavigationStepOutcome::PathFailure)))
			next = CompatibilityStateId::Idle;
		break;
	case CompatibilityStateId::Buy:
		behaviorDecision_.action = StateAction::Buy;
		break;
	case CompatibilityStateId::Hide:
		behaviorDecision_.action = StateAction::Hide;
		if (input.navigationOwned && input.guardObjectiveEnded)
			next = CompatibilityStateId::Idle;
		break;
	case CompatibilityStateId::EscapeFromBomb:
		behaviorDecision_.action = StateAction::Escape;
		if (!input.objectiveActive)
			next = CompatibilityStateId::Idle;
		break;
	case CompatibilityStateId::FetchBomb:
		behaviorDecision_.action = StateAction::Fetch;
		if (!input.objectiveActive || !input.goalAvailable)
			next = CompatibilityStateId::Idle;
		break;
	case CompatibilityStateId::DefuseBomb:
		if (input.navigationOwned)
		{
			behaviorDecision_.action = StateAction::Defuse;
			behaviorDecision_.clearMovement = true;
			if (!input.objectiveActive ||
				(context.timestamp - stateTimestamp_ > kDefuseReceiptTimeoutSeconds &&
					input.defuseReceipt != DefuseReceipt::Self))
				next = CompatibilityStateId::Idle;
			break;
		}
		// Legacy geometry-only callers retain their original interaction contract.
		// fall through
	case CompatibilityStateId::PlantBomb:
		if (state_ == CompatibilityStateId::PlantBomb && input.navigationOwned &&
			context.timestamp - stateTimestamp_ > kPlantStateTimeoutSeconds)
		{
			next = CompatibilityStateId::Idle;
			break;
		}
		// fall through
	case CompatibilityStateId::UseEntity:
		if (!input.objectiveActive || input.pathFailed)
		{
			next = CompatibilityStateId::Idle;
			break;
		}
		behaviorDecision_.action = input.inInteractionRange
			? (state_ == CompatibilityStateId::DefuseBomb		  ? StateAction::Defuse
					  : state_ == CompatibilityStateId::PlantBomb ? StateAction::Plant
																  : StateAction::Use)
			: StateAction::Move;
		behaviorDecision_.clearMovement = input.inInteractionRange;
		break;
	default:
		break;
	}
	if (next != state_ && !context.requestTransition)
	{
		behaviorDecision_.clearPath = true;
		behaviorDecision_.clearMovement = true;
		behaviorDecision_.action = StateAction::None;
		transitionInternal(next,
			input.pathFailed ? StateTransitionReason::PathFailure : StateTransitionReason::ObjectiveChanged,
			StateTransitionId::ExplicitStateChange, context, false);
	}
}

void CompatibilityStateMachine::invokeExit(CompatibilityStateId state, CompatibilityStateId nextState,
	StateTransitionReason reason, StateTransitionId transitionId, const StateUpdateContext &context)
{
	StateInstance *instance = instanceFor(state);
	if (instance != nullptr)
	{
		++instance->exitCount;
	}
	emit(StateTraceEvent::OnExit, state, state, nextState, reason, transitionId, context);
	applyExitEffects(state, nextState, reason, transitionId, context);
}

void CompatibilityStateMachine::emit(StateTraceEvent event, CompatibilityStateId callbackState,
	CompatibilityStateId previousState, CompatibilityStateId currentState, StateTransitionReason reason,
	StateTransitionId transitionId, const StateUpdateContext &context, StateSideEffect sideEffect)
{
	if (traceSink_ == nullptr)
	{
		return;
	}
	StateTraceRecord record = {};
	record.sequence = ++traceSequence_;
	record.stateSequence = stateSequence_;
	record.event = event;
	record.actor = actor_;
	record.frame = context.frame;
	record.previousState = previousState;
	record.currentState = currentState;
	record.callbackState = callbackState;
	record.machineState = state_;
	record.transitionId = transitionId;
	record.reason = reason;
	record.timestamp = context.timestamp;
	record.fullUpdateSequence = context.fullUpdateSequence;
	record.attackOverlayActive = attackOverlayActive_;
	record.task = task_;
	record.sideEffect = sideEffect;
	traceSink_->record(record);
}

void CompatibilityStateMachine::emitSideEffect(CompatibilityStateId callbackState, CompatibilityStateId previousState,
	CompatibilityStateId currentState, StateTransitionReason reason, StateTransitionId transitionId,
	const StateUpdateContext &context, StateSideEffect sideEffect)
{
	emit(StateTraceEvent::SideEffect, callbackState, previousState, currentState, reason, transitionId, context,
		sideEffect);
}

void CompatibilityStateMachine::applyEnterEffects(CompatibilityStateId state, CompatibilityStateId previousState,
	StateTransitionReason reason, StateTransitionId transitionId, const StateUpdateContext &context)
{
	if (state == CompatibilityStateId::Idle || state == CompatibilityStateId::EscapeFromBomb ||
		state == CompatibilityStateId::FetchBomb || state == CompatibilityStateId::Follow ||
		state == CompatibilityStateId::Hunt)
	{
		emitSideEffect(state, previousState, state, reason, transitionId, context, StateSideEffect::ResetPath);
	}
	switch (state)
	{
	case CompatibilityStateId::Idle:
		task_ = CompatibilityTaskId::SeekAndDestroy;
		break;
	case CompatibilityStateId::Buy:
		task_ = CompatibilityTaskId::Buy;
		break;
	case CompatibilityStateId::DefuseBomb:
		task_ = CompatibilityTaskId::DefuseBomb;
		break;
	case CompatibilityStateId::EscapeFromBomb:
		task_ = CompatibilityTaskId::EscapeFromBomb;
		break;
	case CompatibilityStateId::FetchBomb:
		task_ = CompatibilityTaskId::FetchBomb;
		break;
	case CompatibilityStateId::Follow:
		task_ = CompatibilityTaskId::Follow;
		break;
	case CompatibilityStateId::Hunt:
		task_ = CompatibilityTaskId::SeekAndDestroy;
		break;
	case CompatibilityStateId::InvestigateNoise:
		task_ = CompatibilityTaskId::InvestigateNoise;
		break;
	case CompatibilityStateId::MoveTo:
		if (context.preserveMoveToTask)
			return;
		task_ = context.moveToTask != CompatibilityTaskId::None ? context.moveToTask : CompatibilityTaskId::MoveTo;
		break;
	case CompatibilityStateId::PlantBomb:
		task_ = CompatibilityTaskId::PlantBomb;
		break;
	case CompatibilityStateId::UseEntity:
		task_ = CompatibilityTaskId::MoveTo;
		break;
	case CompatibilityStateId::Hide:
	case CompatibilityStateId::None:
	case CompatibilityStateId::AttackOverlay:
		return;
	}
	emitSideEffect(state, previousState, state, reason, transitionId, context, StateSideEffect::SetTask);
}

void CompatibilityStateMachine::applyExitEffects(CompatibilityStateId state, CompatibilityStateId nextState,
	StateTransitionReason reason, StateTransitionId transitionId, const StateUpdateContext &context)
{
	if (state == CompatibilityStateId::DefuseBomb || state == CompatibilityStateId::Hide ||
		state == CompatibilityStateId::PlantBomb || state == CompatibilityStateId::UseEntity)
	{
		emitSideEffect(state, state, nextState, reason, transitionId, context, StateSideEffect::ClearLookTarget);
	}
	if (state == CompatibilityStateId::PlantBomb)
	{
		task_ = CompatibilityTaskId::GuardTickingBomb;
		emitSideEffect(state, state, nextState, reason, transitionId, context, StateSideEffect::SetTask);
	}
}

CompatibilityStateId CompatibilityStateMachine::state() const
{
	return state_;
}

CompatibilityTaskId CompatibilityStateMachine::task() const
{
	return task_;
}

const world::ActorKey &CompatibilityStateMachine::actor() const
{
	return actor_;
}

float CompatibilityStateMachine::stateTimestamp() const
{
	return stateTimestamp_;
}

bool CompatibilityStateMachine::attackOverlayActive() const
{
	return attackOverlayActive_;
}

bool CompatibilityStateMachine::isInitialized() const
{
	return initialized_;
}
}
}
