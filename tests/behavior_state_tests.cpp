#include "astrabot/behavior/behavior_state.hpp"

#include "astrabot/perception/perception.hpp"

#include <cstdio>

namespace
{
	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	astrabot::world::SnapshotIdentity identity(
		std::uint32_t mapGeneration,
		std::uint32_t roundGeneration,
		std::uint32_t tick,
		astrabot::world::ActorKey observer = {1U, 5U})
	{
		astrabot::world::SnapshotIdentity value = {};
		value.frame = {mapGeneration, roundGeneration, tick};
		value.observer = observer;
		return value;
	}

	astrabot::world::WorldSnapshot snapshot(
		std::uint32_t mapGeneration,
		std::uint32_t roundGeneration,
		std::uint32_t tick,
		bool enemyVisible,
		astrabot::world::ActorKey observer = {1U, 5U})
	{
		astrabot::perception::PerceptionInput input(identity(
			mapGeneration,
			roundGeneration,
			tick,
			observer));
		if (enemyVisible)
		{
			astrabot::world::ActorObservation enemy = {};
			enemy.actor = {2U, 9U};
			enemy.state = astrabot::world::ObservationState::ObservedPresent;
			enemy.relation = astrabot::world::TeamRelation::Hostile;
			enemy.position = {128.0f, 0.0f, 0.0f};
			enemy.confidence = {0.9f, 0U};
			input.addActor(enemy);
		}

		astrabot::perception::PerceptionAssembler assembler;
		astrabot::world::WorldSnapshot value;
		assembler.publish(input, &value);
		return value;
	}

	astrabot::behavior::ActorDecisionContext context(
		const astrabot::world::SnapshotIdentity &snapshotIdentity)
	{
		astrabot::behavior::ActorDecisionContext value = {};
		value.actor = snapshotIdentity.observer;
		value.frame = snapshotIdentity.frame;
		value.life = astrabot::behavior::LifeStatus::Alive;
		value.roundAvailability = astrabot::behavior::Availability::Available;
		value.roundActive = true;
		value.healthAvailability = astrabot::behavior::Availability::Available;
		value.health = 100.0f;
		return value;
	}
}

bool testInitialRoamEngageAndMemorySeek()
{
	astrabot::behavior::BehaviorStateMachine machine;
	astrabot::behavior::BehaviorTransition transition = {};
	const auto firstSnapshot = snapshot(3U, 4U, 10U, false);
	if (!check(machine.update(
			firstSnapshot,
			context(firstSnapshot.identity()),
			&transition) == astrabot::behavior::BehaviorResult::Transitioned &&
			machine.state() == astrabot::behavior::BehaviorState::Roam &&
			transition.reason == astrabot::behavior::TransitionReason::Initial &&
			transition.hasObjectiveProposal &&
			transition.proposal.objective ==
				astrabot::behavior::ObjectiveKind::Roam,
			"initial alive frame enters roam with a proposal"))
	{
		return false;
	}

	const auto enemySnapshot = snapshot(3U, 4U, 11U, true);
	if (!check(machine.update(
			enemySnapshot,
			context(enemySnapshot.identity()),
			&transition) == astrabot::behavior::BehaviorResult::Transitioned &&
			machine.state() == astrabot::behavior::BehaviorState::Engage &&
			transition.reason == astrabot::behavior::TransitionReason::EnemyVisible &&
			transition.proposal.hasTarget &&
			transition.proposal.target.slot == 2U,
			"confirmed visible hostile enters engage with target proposal"))
	{
		return false;
	}

	const auto hiddenSnapshot = snapshot(3U, 4U, 12U, false);
	return check(machine.update(
			hiddenSnapshot,
			context(hiddenSnapshot.identity()),
			&transition) == astrabot::behavior::BehaviorResult::Transitioned &&
			machine.state() == astrabot::behavior::BehaviorState::Seek &&
			transition.reason == astrabot::behavior::TransitionReason::EnemyLost &&
			transition.proposal.hasTarget,
		"usable generation-scoped memory enters seek without omniscient contact");
}

bool testRetreatDeadRecoveryAndBoundedTransitions()
{
	astrabot::behavior::BehaviorStateConfig config = {};
	config.retreatHealth = 30.0f;
	config.maximumRecoveryTicks = 2U;
	astrabot::behavior::BehaviorStateMachine machine({1U, 5U}, config);
	astrabot::behavior::BehaviorTransition transition = {};

	const auto firstSnapshot = snapshot(3U, 4U, 20U, false);
	astrabot::behavior::ActorDecisionContext firstContext =
		context(firstSnapshot.identity());
	if (!check(machine.update(firstSnapshot, firstContext, &transition) ==
			astrabot::behavior::BehaviorResult::Transitioned,
			"configured machine accepts initial frame"))
	{
		return false;
	}

	const auto retreatSnapshot = snapshot(3U, 4U, 21U, false);
	astrabot::behavior::ActorDecisionContext retreatContext =
		context(retreatSnapshot.identity());
	retreatContext.health = 20.0f;
	if (!check(machine.update(retreatSnapshot, retreatContext, &transition) ==
			astrabot::behavior::BehaviorResult::Transitioned &&
			machine.state() == astrabot::behavior::BehaviorState::Retreat &&
			transition.reason == astrabot::behavior::TransitionReason::LowHealth,
			"low observed health enters retreat"))
	{
		return false;
	}

	const auto deadSnapshot = snapshot(3U, 4U, 22U, false);
	astrabot::behavior::ActorDecisionContext deadContext =
		context(deadSnapshot.identity());
	deadContext.life = astrabot::behavior::LifeStatus::Dead;
	if (!check(machine.update(deadSnapshot, deadContext, &transition) ==
			astrabot::behavior::BehaviorResult::Transitioned &&
			machine.state() == astrabot::behavior::BehaviorState::Dead &&
			transition.reason == astrabot::behavior::TransitionReason::DeathObserved,
			"observed death enters dead state"))
	{
		return false;
	}

	const auto unavailableSnapshot = snapshot(3U, 4U, 23U, false);
	astrabot::behavior::ActorDecisionContext unavailableContext =
		context(unavailableSnapshot.identity());
	unavailableContext.life = astrabot::behavior::LifeStatus::Unknown;
	unavailableContext.roundAvailability =
		astrabot::behavior::Availability::Unavailable;
	if (!check(machine.update(
			unavailableSnapshot,
			unavailableContext,
			&transition) == astrabot::behavior::BehaviorResult::RecoveryPending &&
			machine.state() == astrabot::behavior::BehaviorState::Recovering &&
			!transition.hasObjectiveProposal,
			"unavailable observations enter bounded recovery without objective success"))
	{
		return false;
	}

	const auto resetSnapshot = snapshot(3U, 5U, 1U, false);
	return check(machine.update(
			resetSnapshot,
			context(resetSnapshot.identity()),
			&transition) == astrabot::behavior::BehaviorResult::Transitioned &&
			machine.state() == astrabot::behavior::BehaviorState::Initial &&
			transition.reason == astrabot::behavior::TransitionReason::RoundReset,
		"round generation change resets state before new decisions");
}

bool testGenerationAndFrameValidation()
{
	astrabot::behavior::BehaviorStateMachine machine;
	astrabot::behavior::BehaviorTransition transition = {};
	const auto firstSnapshot = snapshot(3U, 4U, 30U, false);
	machine.update(firstSnapshot, context(firstSnapshot.identity()), &transition);

	const auto oldActorSnapshot = snapshot(3U, 4U, 31U, false, {1U, 4U});
	astrabot::behavior::ActorDecisionContext oldContext =
		context(oldActorSnapshot.identity());
	if (!check(machine.update(oldActorSnapshot, oldContext, &transition) ==
			astrabot::behavior::BehaviorResult::StaleGeneration,
			"older actor generation is rejected"))
	{
		return false;
	}

	const auto newActorSnapshot = snapshot(3U, 4U, 32U, false, {1U, 6U});
	astrabot::behavior::ActorDecisionContext newContext =
		context(newActorSnapshot.identity());
	if (!check(machine.update(newActorSnapshot, newContext, &transition) ==
			astrabot::behavior::BehaviorResult::Transitioned &&
			transition.reason ==
				astrabot::behavior::TransitionReason::ActorGenerationChanged &&
			machine.state() == astrabot::behavior::BehaviorState::Initial,
			"new actor generation starts an isolated state machine"))
	{
		return false;
	}

	return check(machine.update(
			newActorSnapshot,
			newContext,
			&transition) == astrabot::behavior::BehaviorResult::DuplicateFrame,
		"duplicate frame is rejected without a second transition");
}

int main()
{
	if (!testInitialRoamEngageAndMemorySeek() ||
			!testRetreatDeadRecoveryAndBoundedTransitions() ||
			!testGenerationAndFrameValidation())
	{
		return 1;
	}

	return 0;
}
