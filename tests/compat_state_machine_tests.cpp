#include "astrabot/compat/state_machine.hpp"

#include <cstdio>
#include <vector>

namespace
{
using namespace astrabot::compat;

bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}

	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

struct TraceCollector : public IStateTraceSink
{
	std::vector<StateTraceRecord> records;

	void record(const StateTraceRecord &record) override
	{
		records.push_back(record);
	}
};

StateUpdateContext context(
	std::uint32_t tick,
	float timestamp,
	std::uint32_t fullUpdateSequence,
	astrabot::world::ActorKey actor = {1U, 7U})
{
	StateUpdateContext value = {};
	value.actor = actor;
	value.frame = {2U, 3U, tick};
	value.timestamp = timestamp;
	value.fullUpdateSequence = fullUpdateSequence;
	value.fullUpdate = true;
	value.lifeAvailability = ObservationAvailability::Available;
	value.alive = true;
	value.roundAvailability = ObservationAvailability::Available;
	value.roundActive = true;
	value.transitionRequest = ObservationAvailability::Available;
	value.attackObservation = ObservationAvailability::Available;
	value.rngAvailability = ObservationAvailability::Available;
	return value;
}

bool initialize(
	CompatibilityStateMachine *machine,
	TraceCollector *trace,
	const StateUpdateContext &value)
{
	machine->setTraceSink(trace);
	return machine->initialize(value) == StateResult::Transitioned;
}

bool testInitialStateAndOwnership()
{
	TraceCollector trace;
	CompatibilityStateMachine machine({1U, 7U});
	const StateUpdateContext value = context(10U, 1.0f, 1U);
	if (!check(initialize(&machine, &trace, value), "initialization enters Idle"))
	{
		return false;
	}

	return check(machine.state() == CompatibilityStateId::Idle,
			"initial state is Idle") &&
		check(!machine.attackOverlayActive(), "initial attack overlay is inactive") &&
		check(machine.stateTimestamp() == 1.0f, "initial timestamp is injected time") &&
		check(trace.records.size() >= 2U &&
			trace.records[0].event == StateTraceEvent::OnEnter &&
			trace.records[0].callbackState == CompatibilityStateId::Idle &&
			trace.records[0].machineState == CompatibilityStateId::None,
			"state instance owns initial OnEnter before pointer publication");
}

bool testReferenceSetStateOrderingAndSameState()
{
	TraceCollector trace;
	CompatibilityStateMachine machine({1U, 7U});
	if (!check(initialize(&machine, &trace, context(10U, 1.0f, 1U)),
			"ordering setup initializes"))
	{
		return false;
	}
	trace.records.clear();
	const StateUpdateContext value = context(11U, 2.0f, 2U);
	if (!check(machine.transition(
			CompatibilityStateId::MoveTo,
			StateTransitionReason::ExplicitRequest,
			StateTransitionId::IdleToMoveTo,
			value) == StateResult::Transitioned,
			"Idle to MoveTo transitions"))
	{
		return false;
	}
	if (!check(trace.records.size() >= 4U,
			"transition emits lifecycle records"))
	{
		return false;
	}
	std::size_t published = trace.records.size();
	std::size_t timestamp = trace.records.size();
	for (std::size_t index = 0U; index < trace.records.size(); ++index)
	{
		if (trace.records[index].event == StateTraceEvent::StatePublished)
		{
			published = index;
		}
		if (trace.records[index].event == StateTraceEvent::TimestampUpdated)
		{
			timestamp = index;
		}
	}
	if (!check(trace.records[0].event == StateTraceEvent::OnExit &&
			trace.records[0].callbackState == CompatibilityStateId::Idle &&
			trace.records[1].event == StateTraceEvent::OnEnter &&
			trace.records[1].callbackState == CompatibilityStateId::MoveTo &&
			trace.records[1].machineState == CompatibilityStateId::Idle &&
			published > 1U && timestamp == published + 1U,
			"SetState ordering is OnExit, OnEnter, side effects, publish, timestamp"))
	{
		return false;
	}

	trace.records.clear();
	return check(machine.transition(
			CompatibilityStateId::MoveTo,
			StateTransitionReason::ExplicitRequest,
			StateTransitionId::ExplicitStateChange,
			context(12U, 3.0f, 3U)) == StateResult::Transitioned,
			"same-state request runs reference lifecycle") &&
		check(trace.records.size() >= 4U &&
			trace.records[0].callbackState == CompatibilityStateId::MoveTo &&
			trace.records[1].callbackState == CompatibilityStateId::MoveTo &&
			machine.stateTimestamp() == 3.0f,
			"same-state transition refreshes lifecycle and timestamp");
}

bool testFullUpdateAndTransitionTrace()
{
	TraceCollector trace;
	CompatibilityStateMachine machine({1U, 7U});
	if (!check(initialize(&machine, &trace, context(10U, 1.0f, 1U)),
			"update setup initializes"))
	{
		return false;
	}
	trace.records.clear();
	StateUpdateContext notFull = context(11U, 1.1f, 1U);
	notFull.fullUpdate = false;
	if (!check(machine.update(notFull) == StateResult::SkippedNotFullUpdate,
			"non-full update does not update state"))
	{
		return false;
	}
	StateUpdateContext step = context(12U, 2.0f, 2U);
	step.requestTransition = true;
	step.requestedState = CompatibilityStateId::Hunt;
	step.transitionReason = StateTransitionReason::EnemyVisible;
	step.transitionId = StateTransitionId::MoveToHunt;
	if (!check(machine.update(step) == StateResult::Transitioned,
			"full update dispatches transition"))
	{
		return false;
	}
	return check(machine.state() == CompatibilityStateId::Hunt,
			"multi-step update publishes requested state") &&
		check(trace.records.size() >= 5U &&
			trace.records[0].event == StateTraceEvent::OnUpdate &&
			trace.records[1].event == StateTraceEvent::OnExit,
			"update trace contains update before transition lifecycle");
}

bool testAttackOverlayLifecycleAndInteraction()
{
	TraceCollector trace;
	CompatibilityStateMachine machine({1U, 7U});
	if (!check(initialize(&machine, &trace, context(10U, 1.0f, 1U)),
			"attack setup initializes"))
	{
		return false;
	}
	if (!check(machine.transition(
			CompatibilityStateId::Hide,
			StateTransitionReason::ExplicitRequest,
			StateTransitionId::ExplicitStateChange,
			context(11U, 2.0f, 2U)) == StateResult::Transitioned,
			"attack overlay setup enters Hide"))
	{
		return false;
	}
	trace.records.clear();
	const StateUpdateContext attackContext = context(12U, 3.0f, 3U);
	if (!check(machine.beginAttack(attackContext) == StateResult::Transitioned,
			"attack overlay activates"))
	{
		return false;
	}
	if (!check(machine.state() == CompatibilityStateId::Hide &&
			machine.attackOverlayActive() && machine.stateTimestamp() == 2.0f,
			"attack retains underlying state and timestamp"))
	{
		return false;
	}
	if (!check(machine.update(context(13U, 4.0f, 4U)) == StateResult::Updated,
			"attack update is dispatched"))
	{
		return false;
	}
	if (!check(machine.stopAttack(context(14U, 5.0f, 5U)) == StateResult::Transitioned,
			"attack overlay deactivates"))
	{
		return false;
	}
	std::size_t attackEnter = trace.records.size();
	std::size_t attackUpdate = trace.records.size();
	std::size_t attackExit = trace.records.size();
	for (std::size_t index = 0U; index < trace.records.size(); ++index)
	{
		if (trace.records[index].event == StateTraceEvent::AttackOnEnter)
		{
			attackEnter = index;
		}
		else if (trace.records[index].event == StateTraceEvent::AttackOnUpdate)
		{
			attackUpdate = index;
		}
		else if (trace.records[index].event == StateTraceEvent::AttackOnExit)
		{
			attackExit = index;
		}
	}
	return check(machine.state() == CompatibilityStateId::Hide &&
			!machine.attackOverlayActive(),
			"underlying state continues after attack") &&
		check(attackEnter < attackUpdate && attackUpdate < attackExit,
			"attack trace records activation update deactivation");
}

bool testMultiStepAndTransitionDuringAttack()
{
	TraceCollector trace;
	CompatibilityStateMachine machine({1U, 7U});
	if (!check(initialize(&machine, &trace, context(20U, 1.0f, 1U)),
			"multi-step setup initializes"))
	{
		return false;
	}
	if (!check(machine.transition(
			CompatibilityStateId::MoveTo,
			StateTransitionReason::ExplicitRequest,
			StateTransitionId::IdleToMoveTo,
			context(21U, 2.0f, 2U)) == StateResult::Transitioned &&
			machine.state() == CompatibilityStateId::MoveTo,
			"multi-step first transition enters MoveTo"))
	{
		return false;
	}
	StateUpdateContext hunt = context(22U, 3.0f, 3U);
	hunt.requestTransition = true;
	hunt.requestedState = CompatibilityStateId::Hunt;
	hunt.transitionId = StateTransitionId::MoveToHunt;
	hunt.transitionReason = StateTransitionReason::EnemyVisible;
	if (!check(machine.update(hunt) == StateResult::Transitioned &&
			machine.state() == CompatibilityStateId::Hunt,
			"multi-step second transition enters Hunt"))
	{
		return false;
	}
	if (!check(machine.beginAttack(context(23U, 4.0f, 4U)) == StateResult::Transitioned,
			"attack transition setup activates overlay"))
	{
		return false;
	}
	if (!check(machine.transition(
			CompatibilityStateId::Hide,
			StateTransitionReason::PathFailure,
			StateTransitionId::ExplicitStateChange,
			context(24U, 5.0f, 5U)) == StateResult::Transitioned,
			"state transition during attack is accepted"))
	{
		return false;
	}
	return check(machine.state() == CompatibilityStateId::Hide &&
			!machine.attackOverlayActive(),
			"state transition stops overlay and publishes destination");
}

bool testBlockedRngObservationAndLifecycleIsolation()
{
	TraceCollector trace;
	CompatibilityStateMachine machineA({1U, 7U});
	CompatibilityStateMachine machineB({2U, 8U});
	if (!check(initialize(&machineA, &trace, context(10U, 1.0f, 1U)) &&
			initialize(&machineB, &trace, context(10U, 1.0f, 1U, {2U, 8U})),
			"two machines initialize independently"))
	{
		return false;
	}
	StateUpdateContext blocked = context(11U, 2.0f, 2U);
	blocked.transitionRequest = ObservationAvailability::Unavailable;
	blocked.requestTransition = true;
	blocked.requestedState = CompatibilityStateId::Buy;
	if (!check(machineA.update(blocked) == StateResult::BlockedByObservation &&
			machineA.state() == CompatibilityStateId::Idle,
			"unavailable observation blocks transition"))
	{
		return false;
	}
	StateUpdateContext rngBlocked = context(12U, 3.0f, 3U);
	rngBlocked.rngAvailability = ObservationAvailability::Unavailable;
	rngBlocked.requestTransition = true;
	rngBlocked.requestedState = CompatibilityStateId::Follow;
	rngBlocked.transitionId = StateTransitionId::RngDependent;
	if (!check(machineA.update(rngBlocked) == StateResult::BlockedByRng,
			"unavailable RNG blocks random transition"))
	{
		return false;
	}
	if (!check(machineB.transition(
			CompatibilityStateId::MoveTo,
			StateTransitionReason::ExplicitRequest,
			StateTransitionId::ExplicitStateChange,
			context(13U, 4.0f, 4U, {2U, 8U})) == StateResult::Transitioned &&
			machineA.state() == CompatibilityStateId::Idle &&
			machineB.state() == CompatibilityStateId::MoveTo,
			"two actors keep independent state") ||
		!check(machineA.onDeath(context(14U, 5.0f, 5U)) == StateResult::Transitioned &&
			machineA.state() == CompatibilityStateId::Idle,
			"death reset is per actor"))
	{
		return false;
	}
	return check(machineA.onRespawn(context(15U, 6.0f, 6U)) == StateResult::Transitioned,
			"respawn re-enters Idle lifecycle") &&
		check(!(machineA.actor() == machineB.actor()), "actor identities remain isolated");
}
}

int main()
{
	return testInitialStateAndOwnership() &&
		testReferenceSetStateOrderingAndSameState() &&
		testFullUpdateAndTransitionTrace() &&
		testAttackOverlayLifecycleAndInteraction() &&
		testMultiStepAndTransitionDuringAttack() &&
		testBlockedRngObservationAndLifecycleIsolation() ? 0 : 1;
}
