#include "astrabot/objectives/objective_state.hpp"

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

	astrabot::objectives::ScenarioIdentity scenario(
		std::uint32_t roundGeneration,
		std::uint32_t tick)
	{
		astrabot::objectives::ScenarioIdentity value = {};
		value.frame = {3U, roundGeneration, tick};
		value.scenarioGeneration = 8U;
		value.kind = astrabot::objectives::ScenarioKind::Bomb;
		value.team = astrabot::objectives::TeamRole::CounterTerrorist;
		return value;
	}

	astrabot::objectives::ObjectiveProposal proposal(
		const astrabot::objectives::ScenarioIdentity &identity)
	{
		astrabot::objectives::ObjectiveProposal value = {};
		value.actor = {1U, 5U};
		value.scenario = identity;
		value.objective.scenario = identity;
		value.objective.kind = astrabot::objectives::ObjectiveKind::Defend;
		value.objective.subjectId = 2U;
		value.priority = astrabot::objectives::ObjectivePriority::High;
		value.issuedTick = identity.frame.tick;
		value.expiresAtTick = identity.frame.tick + 32U;
		return value;
	}
}

bool testProposalLifecycleNeedsExplicitFeedback()
{
	using astrabot::objectives::ObjectiveCompletionFeedback;
	using astrabot::objectives::ObjectiveState;
	using astrabot::objectives::ObjectiveStateRecord;
	using astrabot::objectives::ObjectiveStatus;
	using astrabot::objectives::ObjectiveStateResult;

	const auto identity = scenario(4U, 10U);
	ObjectiveState state({1U, 5U});
	const auto candidate = proposal(identity);
	if (!check(candidate.isProposal() && !candidate.isCompletionFeedback() &&
			state.acceptProposal(candidate) == ObjectiveStateResult::Accepted,
			"actor-owned objective proposal is accepted"))
	{
		return false;
	}

	ObjectiveStateRecord record = {};
	if (!check(state.current(&record) == ObjectiveStateResult::Found &&
			record.status == ObjectiveStatus::Proposed &&
			!state.isComplete(),
			"proposal does not imply objective completion"))
	{
		return false;
	}

	ObjectiveCompletionFeedback unknown = {};
	unknown.actor = {1U, 5U};
	unknown.objective = record.objective;
	unknown.scenario = identity;
	unknown.status = ObjectiveStatus::Unknown;
	if (!check(state.applyFeedback(unknown) ==
			ObjectiveStateResult::FeedbackUnavailable && !state.isComplete(),
			"unknown feedback cannot complete objective"))
	{
		return false;
	}

	ObjectiveCompletionFeedback completed = unknown;
	completed.status = ObjectiveStatus::Completed;
	completed.scenario.frame.tick = 11U;
	completed.objective.scenario = completed.scenario;
	if (!check(state.applyFeedback(completed) ==
			ObjectiveStateResult::Completed && state.isComplete(),
			"explicit completion feedback completes objective"))
	{
		return false;
	}

	ObjectiveCompletionFeedback stale = completed;
	stale.actor.generation = 4U;
	return check(state.applyFeedback(stale) ==
			ObjectiveStateResult::StaleGeneration,
		"stale actor feedback is rejected");
}

bool testExpiryAndRoundGeneration()
{
	using astrabot::objectives::ObjectiveState;
	using astrabot::objectives::ObjectiveStateResult;

	ObjectiveState state({1U, 5U});
	const auto candidate = proposal(scenario(4U, 20U));
	if (!check(state.acceptProposal(candidate) == ObjectiveStateResult::Accepted,
			"expiring objective proposal is accepted"))
	{
		return false;
	}

	if (!check(state.expire({3U, 4U, 52U}) ==
			ObjectiveStateResult::Expired,
			"objective expiry is explicit"))
	{
		return false;
	}

	const auto staleProposal = proposal(scenario(3U, 10U));
	if (!check(state.acceptProposal(staleProposal) ==
			ObjectiveStateResult::StaleFrame,
			"proposal from another round is rejected"))
	{
		return false;
	}

	return check(state.current(nullptr) == ObjectiveStateResult::InvalidArgument,
		"null objective state output is rejected");
}

int main()
{
	if (!testProposalLifecycleNeedsExplicitFeedback() ||
			!testExpiryAndRoundGeneration())
	{
		return 1;
	}

	return 0;
}
