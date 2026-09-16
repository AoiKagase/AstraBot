#include "astrabot/objectives/round_objectives.hpp"

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

	astrabot::objectives::ScenarioIdentity identity(
		astrabot::objectives::ScenarioKind kind,
		astrabot::objectives::TeamRole team,
		std::uint32_t roundGeneration,
		std::uint32_t tick)
	{
		astrabot::objectives::ScenarioIdentity value = {};
		value.frame = {3U, roundGeneration, tick};
		value.scenarioGeneration = 1U;
		value.kind = kind;
		value.team = team;
		return value;
	}

	astrabot::world::WorldSnapshot snapshot(
		const astrabot::objectives::ScenarioIdentity &scenario)
	{
		astrabot::world::SnapshotIdentity snapshotIdentity = {};
		snapshotIdentity.frame = scenario.frame;
		snapshotIdentity.observer = {1U, 5U};
		astrabot::perception::PerceptionInput input(snapshotIdentity);
		astrabot::perception::PerceptionAssembler assembler;
		astrabot::world::WorldSnapshot value;
		assembler.publish(input, &value);
		return value;
	}

	astrabot::objectives::ScenarioObservation observation(
		const astrabot::objectives::ScenarioIdentity &scenario)
	{
		astrabot::objectives::ScenarioObservation value = {};
		value.scenario = scenario;
		value.phase = astrabot::objectives::RoundPhase::Live;
		value.buyAvailability = astrabot::objectives::Availability::Unavailable;
		value.eventCount = 0U;
		return value;
	}

	astrabot::objectives::ScenarioEvent event(
		astrabot::objectives::ScenarioEventKind kind,
		const astrabot::objectives::ScenarioIdentity &scenario)
	{
		astrabot::objectives::ScenarioEvent value = {};
		value.id = scenario.frame.tick;
		value.frame = scenario.frame;
		value.kind = kind;
		value.state = astrabot::objectives::EventState::Observed;
		value.actor = {2U, 9U};
		value.team = astrabot::objectives::TeamRole::Terrorist;
		return value;
	}
}

bool testBombHostageAndBuyProposals()
{
	using astrabot::behavior::BehaviorState;
	using astrabot::objectives::ObjectiveKind;
	using astrabot::objectives::ObjectiveResult;
	using astrabot::objectives::RoundObjectivePlanner;

	const auto bomb = identity(
		astrabot::objectives::ScenarioKind::Bomb,
		astrabot::objectives::TeamRole::Terrorist,
		4U,
		10U);
	RoundObjectivePlanner planner({1U, 5U});
	astrabot::objectives::ObjectiveProposal proposal = {};
	if (!check(planner.plan(
			snapshot(bomb),
			BehaviorState::Roam,
			observation(bomb),
			nullptr,
			&proposal) == ObjectiveResult::Proposed &&
			proposal.objective.kind == ObjectiveKind::Attack,
			"terrorist live bomb round proposes attack"))
	{
		return false;
	}

	auto planted = observation(identity(
		astrabot::objectives::ScenarioKind::Bomb,
		astrabot::objectives::TeamRole::CounterTerrorist,
		4U,
		11U));
	planted.eventCount = 1U;
	planted.events[0] = event(
		astrabot::objectives::ScenarioEventKind::BombPlanted,
		planted.scenario);
	if (!check(planner.plan(
			snapshot(planted.scenario),
			BehaviorState::Roam,
			planted,
			nullptr,
			&proposal) == ObjectiveResult::Proposed &&
			proposal.objective.kind == ObjectiveKind::Defuse,
			"counter-terrorist observes planted bomb and proposes defuse"))
	{
		return false;
	}

	auto hostage = observation(identity(
		astrabot::objectives::ScenarioKind::Hostage,
		astrabot::objectives::TeamRole::CounterTerrorist,
		4U,
		12U));
	hostage.eventCount = 1U;
	hostage.events[0] = event(
		astrabot::objectives::ScenarioEventKind::HostageLocated,
		hostage.scenario);
	if (!check(planner.plan(
			snapshot(hostage.scenario),
			BehaviorState::Roam,
			hostage,
			nullptr,
			&proposal) == ObjectiveResult::Proposed &&
			proposal.objective.kind == ObjectiveKind::Rescue,
			"hostage scenario proposes rescue"))
	{
		return false;
	}

	auto buy = observation(identity(
		astrabot::objectives::ScenarioKind::Bomb,
		astrabot::objectives::TeamRole::Terrorist,
		4U,
		13U));
	buy.phase = astrabot::objectives::RoundPhase::Freeze;
	buy.buyAvailability = astrabot::objectives::Availability::Available;
	return check(planner.plan(
			snapshot(buy.scenario),
			BehaviorState::Roam,
			buy,
			nullptr,
			&proposal) == ObjectiveResult::Proposed &&
			proposal.objective.kind == ObjectiveKind::Buy,
		"available freeze-period buy state proposes buy objective");
}

bool testUnknownFeedbackAndRoundStaleness()
{
	using astrabot::behavior::BehaviorState;
	using astrabot::objectives::ObjectiveResult;
	using astrabot::objectives::RoundObjectivePlanner;

	const auto unknown = identity(
		astrabot::objectives::ScenarioKind::Unknown,
		astrabot::objectives::TeamRole::Unknown,
		4U,
		20U);
	RoundObjectivePlanner planner({1U, 5U});
	astrabot::objectives::ObjectiveProposal proposal = {};
	if (!check(planner.plan(
			snapshot(unknown),
			BehaviorState::Roam,
			observation(unknown),
			nullptr,
			&proposal) == ObjectiveResult::RecoveryPending &&
			!proposal.isProposal(),
			"unknown scenario enters recovery without omniscient objective"))
	{
		return false;
	}

	auto stale = observation(identity(
		astrabot::objectives::ScenarioKind::Bomb,
		astrabot::objectives::TeamRole::Terrorist,
		3U,
		21U));
	return check(planner.plan(
			snapshot(stale.scenario),
			BehaviorState::Roam,
			stale,
			nullptr,
			&proposal) == ObjectiveResult::StaleFrame,
		"stale round generation cannot reuse objective planner state");
}

int main()
{
	if (!testBombHostageAndBuyProposals() ||
			!testUnknownFeedbackAndRoundStaleness())
	{
		return 1;
	}

	return 0;
}
