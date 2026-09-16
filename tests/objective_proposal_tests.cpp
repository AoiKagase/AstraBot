#include "astrabot/behavior/objective_proposal.hpp"

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

	astrabot::world::FrameIdentity frame(std::uint32_t tick)
	{
		return {4U, 2U, tick};
	}

	astrabot::behavior::ObjectiveProposal proposal(
		astrabot::behavior::ObjectiveKind objective,
		astrabot::behavior::ObjectivePriority priority,
		std::uint32_t issuedTick,
		std::uint32_t expiresAtTick)
	{
		astrabot::behavior::ObjectiveProposal value = {};
		value.actor = {1U, 5U};
		value.sourceFrame = frame(issuedTick);
		value.objective = objective;
		value.priority = priority;
		value.issuedTick = issuedTick;
		value.expiresAtTick = expiresAtTick;
		return value;
	}
}

bool testPriorityTieBreakingAndExpiry()
{
	using astrabot::behavior::ObjectiveKind;
	using astrabot::behavior::ObjectivePriority;
	using astrabot::behavior::ObjectiveProposal;
	using astrabot::behavior::ObjectiveProposalSet;
	using astrabot::behavior::ProposalResult;

	ObjectiveProposalSet proposals({1U, 5U});
	if (!check(proposals.add(proposal(
			ObjectiveKind::Roam,
			ObjectivePriority::Low,
			10U,
			100U)) == ProposalResult::Accepted,
			"low-priority proposal is accepted"))
	{
		return false;
	}

	if (!check(proposals.add(proposal(
			ObjectiveKind::Engage,
			ObjectivePriority::High,
			11U,
			80U)) == ProposalResult::Accepted &&
			proposals.add(proposal(
				ObjectiveKind::Retreat,
				ObjectivePriority::High,
				12U,
				60U)) == ProposalResult::Accepted,
			"high-priority proposals are accepted"))
	{
		return false;
	}

	ObjectiveProposal selected = {};
	if (!check(proposals.select(frame(20U), &selected) ==
			ProposalResult::Selected &&
			selected.objective == ObjectiveKind::Retreat &&
			selected.priority == ObjectivePriority::High,
			"priority and earliest expiry tie-break deterministically"))
	{
		return false;
	}

	if (!check(proposals.select(frame(60U), &selected) ==
			ProposalResult::Selected &&
			selected.objective == ObjectiveKind::Engage,
			"expired proposal is excluded at its expiry tick"))
	{
		return false;
	}

	return check(proposals.select(frame(100U), &selected) ==
			ProposalResult::NoProposal,
		"all expired proposals produce no objective completion claim");
}

bool testGenerationOwnershipAndProposalBoundary()
{
	using astrabot::behavior::ObjectiveKind;
	using astrabot::behavior::ObjectivePriority;
	using astrabot::behavior::ObjectiveProposal;
	using astrabot::behavior::ObjectiveProposalSet;
	using astrabot::behavior::ProposalResult;

	ObjectiveProposalSet proposals({1U, 5U});
	ObjectiveProposal wrongActor = proposal(
		ObjectiveKind::Seek,
		ObjectivePriority::Normal,
		20U,
		40U);
	wrongActor.actor = {2U, 5U};
	if (!check(proposals.add(wrongActor) == ProposalResult::InvalidActor,
			"proposal from another actor is rejected"))
	{
		return false;
	}

	ObjectiveProposal oldGeneration = proposal(
		ObjectiveKind::Seek,
		ObjectivePriority::Normal,
		20U,
		40U);
	oldGeneration.actor.generation = 4U;
	if (!check(proposals.add(oldGeneration) == ProposalResult::StaleGeneration,
			"stale actor generation cannot enter proposal set"))
	{
		return false;
	}

	ObjectiveProposal valid = proposal(
		ObjectiveKind::Seek,
		ObjectivePriority::Normal,
		20U,
		40U);
	if (!check(proposals.add(valid) == ProposalResult::Accepted &&
			proposals.add(valid) == ProposalResult::DuplicateProposal,
			"duplicate proposal is rejected without mutation"))
	{
		return false;
	}

	ObjectiveProposal selected = {};
	if (!check(proposals.select({4U, 3U, 21U}, &selected) ==
			ProposalResult::StaleFrame,
			"round generation invalidates old proposals"))
	{
		return false;
	}

	return check(valid.isProposal() && !valid.isActionIntent() &&
			!valid.isCompletionFeedback(),
		"proposal type is distinct from action intent and completion feedback");
}

int main()
{
	if (!testPriorityTieBreakingAndExpiry() ||
			!testGenerationOwnershipAndProposalBoundary())
	{
		return 1;
	}

	return 0;
}
