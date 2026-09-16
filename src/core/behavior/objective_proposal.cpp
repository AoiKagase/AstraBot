#include "astrabot/behavior/objective_proposal.hpp"

namespace astrabot
{
namespace behavior
{
namespace
{
bool sameFrameGeneration(
	const world::FrameIdentity &left,
	const world::FrameIdentity &right)
{
	return left.mapGeneration == right.mapGeneration &&
		left.roundGeneration == right.roundGeneration;
}

}

bool ObjectiveProposal::isValid() const
{
	if (!actor.isValid() || !sourceFrame.isValid() ||
			objective == ObjectiveKind::None ||
			(priority != ObjectivePriority::Low &&
			 priority != ObjectivePriority::Normal &&
			 priority != ObjectivePriority::High &&
			 priority != ObjectivePriority::Critical) ||
			issuedTick != sourceFrame.tick || issuedTick >= expiresAtTick ||
			expiresAtTick - issuedTick > ObjectiveLimits::kMaximumLifetimeTicks)
	{
		return false;
	}

	if (hasTarget && !target.isValid())
	{
		return false;
	}

	return objective == ObjectiveKind::Roam ||
		objective == ObjectiveKind::Seek ||
		objective == ObjectiveKind::Engage ||
		objective == ObjectiveKind::Retreat;
}

bool ObjectiveProposal::isExpired(const world::FrameIdentity &frame) const
{
	return !frame.isValid() || !sourceFrame.isValid() ||
		!sameFrameGeneration(frame, sourceFrame) ||
		frame.tick >= expiresAtTick;
}

bool ObjectiveProposal::isProposal() const
{
	return isValid();
}

bool ObjectiveProposal::isActionIntent() const
{
	return false;
}

bool ObjectiveProposal::isCompletionFeedback() const
{
	return false;
}

ObjectiveProposalSet::ObjectiveProposalSet()
	: owner_(),
	  proposals_(),
	  proposalCount_(0U)
{
}

ObjectiveProposalSet::ObjectiveProposalSet(const world::ActorKey &owner)
	: owner_(),
	  proposals_(),
	  proposalCount_(0U)
{
	setOwner(owner);
}

ProposalResult ObjectiveProposalSet::setOwner(const world::ActorKey &owner)
{
	if (!owner.isValid())
	{
		return ProposalResult::InvalidActor;
	}

	owner_ = owner;
	clear();
	return ProposalResult::Accepted;
}

ProposalResult ObjectiveProposalSet::add(
	const ObjectiveProposal &proposal)
{
	if (!owner_.isValid() || !proposal.actor.isValid())
	{
		return ProposalResult::InvalidActor;
	}

	if (proposal.actor.slot != owner_.slot)
	{
		return ProposalResult::InvalidActor;
	}

	if (proposal.actor.generation != owner_.generation)
	{
		return ProposalResult::StaleGeneration;
	}

	if (!proposal.isValid())
	{
		return ProposalResult::InvalidProposal;
	}

	for (std::size_t index = 0U; index < proposalCount_; ++index)
	{
		if (sameProposal(proposals_[index], proposal))
		{
			return ProposalResult::DuplicateProposal;
		}
	}

	if (proposalCount_ >= proposals_.size())
	{
		return ProposalResult::ResourceLimit;
	}

	proposals_[proposalCount_] = proposal;
	++proposalCount_;
	return ProposalResult::Accepted;
}

ProposalResult ObjectiveProposalSet::select(
	const world::FrameIdentity &frame,
	ObjectiveProposal *proposal) const
{
	if (proposal == nullptr || !frame.isValid())
	{
		return ProposalResult::InvalidArgument;
	}
	if (!owner_.isValid())
	{
		return ProposalResult::InvalidActor;
	}

	bool found = false;
	bool sawStaleFrame = false;
	ObjectiveProposal selected = {};
	for (std::size_t index = 0U; index < proposalCount_; ++index)
	{
		if (!sameFrameGeneration(frame, proposals_[index].sourceFrame))
		{
			sawStaleFrame = true;
			continue;
		}
		if (proposals_[index].isExpired(frame))
		{
			continue;
		}

		if (!found || isBetter(proposals_[index], selected))
		{
			selected = proposals_[index];
			found = true;
		}
	}

	if (!found)
	{
		return sawStaleFrame ? ProposalResult::StaleFrame :
			ProposalResult::NoProposal;
	}

	*proposal = selected;
	return ProposalResult::Selected;
}

void ObjectiveProposalSet::clear()
{
	proposalCount_ = 0U;
}

std::size_t ObjectiveProposalSet::size() const
{
	return proposalCount_;
}

const world::ActorKey &ObjectiveProposalSet::owner() const
{
	return owner_;
}

bool ObjectiveProposalSet::isValidObjective(ObjectiveKind objective)
{
	return objective == ObjectiveKind::Roam ||
		objective == ObjectiveKind::Seek ||
		objective == ObjectiveKind::Engage ||
		objective == ObjectiveKind::Retreat;
}

bool ObjectiveProposalSet::isValidPriority(ObjectivePriority priority)
{
	return priority == ObjectivePriority::Low ||
		priority == ObjectivePriority::Normal ||
		priority == ObjectivePriority::High ||
		priority == ObjectivePriority::Critical;
}

bool ObjectiveProposalSet::sameProposal(
	const ObjectiveProposal &left,
	const ObjectiveProposal &right)
{
	return left.actor == right.actor && left.sourceFrame == right.sourceFrame &&
		left.sourceNavRevision == right.sourceNavRevision &&
		left.objective == right.objective && left.priority == right.priority &&
		left.issuedTick == right.issuedTick &&
		left.expiresAtTick == right.expiresAtTick &&
		left.hasTarget == right.hasTarget &&
		(!left.hasTarget || left.target == right.target);
}

int ObjectiveProposalSet::priorityValue(ObjectivePriority priority)
{
	return static_cast<int>(priority);
}

int ObjectiveProposalSet::objectiveValue(ObjectiveKind objective)
{
	return static_cast<int>(objective);
}

bool ObjectiveProposalSet::isBetter(
	const ObjectiveProposal &candidate,
	const ObjectiveProposal &current)
{
	if (priorityValue(candidate.priority) != priorityValue(current.priority))
	{
		return priorityValue(candidate.priority) >
			priorityValue(current.priority);
	}

	if (candidate.expiresAtTick != current.expiresAtTick)
	{
		return candidate.expiresAtTick < current.expiresAtTick;
	}

	if (candidate.issuedTick != current.issuedTick)
	{
		return candidate.issuedTick > current.issuedTick;
	}

	if (objectiveValue(candidate.objective) !=
			objectiveValue(current.objective))
	{
		return objectiveValue(candidate.objective) <
			objectiveValue(current.objective);
	}

	if (candidate.hasTarget != current.hasTarget)
	{
		return candidate.hasTarget;
	}

	if (!candidate.hasTarget)
	{
		return false;
	}

	if (candidate.target.slot != current.target.slot)
	{
		return candidate.target.slot < current.target.slot;
	}

	return candidate.target.generation < current.target.generation;
}
}
}
