#include "astrabot/objectives/objective_state.hpp"

namespace astrabot
{
namespace objectives
{
namespace
{
bool isValidScenarioKind(ScenarioKind kind)
{
	return kind == ScenarioKind::Bomb || kind == ScenarioKind::Hostage;
}

bool isValidTeamRole(TeamRole team)
{
	return team == TeamRole::Terrorist ||
		team == TeamRole::CounterTerrorist;
}

bool isValidObjectiveKind(ObjectiveKind kind)
{
	return kind == ObjectiveKind::Attack ||
		kind == ObjectiveKind::Defend || kind == ObjectiveKind::Retake ||
		kind == ObjectiveKind::Save || kind == ObjectiveKind::Rescue ||
		kind == ObjectiveKind::Escort || kind == ObjectiveKind::Buy ||
		kind == ObjectiveKind::Plant || kind == ObjectiveKind::Defuse;
}

bool sameRound(
	const world::FrameIdentity &left,
	const world::FrameIdentity &right)
{
	return left.mapGeneration == right.mapGeneration &&
		left.roundGeneration == right.roundGeneration;
}

bool sameScenarioVersion(
	const ScenarioIdentity &left,
	const ScenarioIdentity &right)
{
	return sameRound(left.frame, right.frame) &&
		left.scenarioGeneration == right.scenarioGeneration &&
		left.kind == right.kind && left.team == right.team;
}
}

bool ScenarioIdentity::isValid() const
{
	return frame.isValid() && scenarioGeneration != 0U &&
		isValidScenarioKind(kind) && isValidTeamRole(team);
}

bool ScenarioIdentity::operator==(const ScenarioIdentity &other) const
{
	return frame == other.frame &&
		scenarioGeneration == other.scenarioGeneration &&
		kind == other.kind && team == other.team;
}

bool ObjectiveIdentity::isValid() const
{
	return scenario.isValid() && isValidObjectiveKind(kind);
}

bool ObjectiveIdentity::operator==(const ObjectiveIdentity &other) const
{
	return scenario == other.scenario && kind == other.kind &&
		subjectId == other.subjectId;
}

bool ObjectiveProposal::isValid() const
{
	return actor.isValid() && scenario.isValid() && objective.isValid() &&
		objective.scenario == scenario && issuedTick == scenario.frame.tick &&
		issuedTick < expiresAtTick &&
		expiresAtTick - issuedTick <= ObjectiveLimits::kMaximumLifetimeTicks &&
		(priority == ObjectivePriority::Low ||
		 priority == ObjectivePriority::Normal ||
		 priority == ObjectivePriority::High ||
		 priority == ObjectivePriority::Critical);
}

bool ObjectiveProposal::isExpired(const world::FrameIdentity &frame) const
{
	return !frame.isValid() || !sameRound(frame, scenario.frame) ||
		frame.tick >= expiresAtTick;
}

bool ObjectiveProposal::isProposal() const
{
	return isValid();
}

bool ObjectiveProposal::isCompletionFeedback() const
{
	return false;
}

bool ObjectiveCompletionFeedback::isValid() const
{
	return actor.isValid() && scenario.isValid() && objective.isValid() &&
		objective.scenario == scenario &&
		(status == ObjectiveStatus::Unknown ||
		 status == ObjectiveStatus::Active ||
		 status == ObjectiveStatus::Completed ||
		 status == ObjectiveStatus::Failed ||
		 status == ObjectiveStatus::Expired);
}

bool ObjectiveCompletionFeedback::isConfirmed() const
{
	return isValid() && status == ObjectiveStatus::Completed;
}

ObjectiveState::ObjectiveState()
	: actor_(),
	  record_(),
	  hasRecord_(false)
{
}

ObjectiveState::ObjectiveState(const world::ActorKey &actor)
	: actor_(actor),
	  record_(),
	  hasRecord_(false)
{
}

ObjectiveStateResult ObjectiveState::acceptProposal(
	const ObjectiveProposal &proposal)
{
	if (!actor_.isValid() || !proposal.actor.isValid())
	{
		return ObjectiveStateResult::InvalidIdentity;
	}
	if (proposal.actor.slot != actor_.slot)
	{
		return ObjectiveStateResult::InvalidIdentity;
	}
	if (proposal.actor.generation != actor_.generation)
	{
		return ObjectiveStateResult::StaleGeneration;
	}
	if (!proposal.isValid())
	{
		return ObjectiveStateResult::InvalidProposal;
	}
	if (hasRecord_ && !sameRound(
			proposal.scenario.frame,
			record_.objective.scenario.frame))
	{
		return ObjectiveStateResult::StaleFrame;
	}

	record_.actor = proposal.actor;
	record_.objective = proposal.objective;
	record_.priority = proposal.priority;
	record_.status = ObjectiveStatus::Proposed;
	record_.issuedTick = proposal.issuedTick;
	record_.expiresAtTick = proposal.expiresAtTick;
	hasRecord_ = true;
	return ObjectiveStateResult::Accepted;
}

ObjectiveStateResult ObjectiveState::applyFeedback(
	const ObjectiveCompletionFeedback &feedback)
{
	if (!actor_.isValid() || !feedback.actor.isValid())
	{
		return ObjectiveStateResult::InvalidIdentity;
	}
	if (feedback.actor.slot != actor_.slot)
	{
		return ObjectiveStateResult::InvalidIdentity;
	}
	if (feedback.actor.generation != actor_.generation)
	{
		return ObjectiveStateResult::StaleGeneration;
	}
	if (!feedback.isValid())
	{
		return ObjectiveStateResult::InvalidArgument;
	}
	if (!hasRecord_)
	{
		return ObjectiveStateResult::NotFound;
	}
	if (!sameScenarioVersion(feedback.scenario, record_.objective.scenario) ||
			feedback.objective.kind != record_.objective.kind ||
			feedback.objective.subjectId != record_.objective.subjectId)
	{
		return ObjectiveStateResult::StaleFrame;
	}
	if (feedback.status == ObjectiveStatus::Unknown)
	{
		return ObjectiveStateResult::FeedbackUnavailable;
	}

	record_.status = feedback.status;
	return feedback.status == ObjectiveStatus::Completed ?
		ObjectiveStateResult::Completed : ObjectiveStateResult::Accepted;
}

ObjectiveStateResult ObjectiveState::expire(const world::FrameIdentity &frame)
{
	if (!frame.isValid())
	{
		return ObjectiveStateResult::InvalidArgument;
	}
	if (!hasRecord_)
	{
		return ObjectiveStateResult::NotFound;
	}
	if (!sameRound(frame, record_.objective.scenario.frame))
	{
		return ObjectiveStateResult::StaleFrame;
	}
	if (frame.tick < record_.expiresAtTick)
	{
		return ObjectiveStateResult::Accepted;
	}

	record_.status = ObjectiveStatus::Expired;
	return ObjectiveStateResult::Expired;
}

ObjectiveStateResult ObjectiveState::current(ObjectiveStateRecord *record) const
{
	if (record == nullptr)
	{
		return ObjectiveStateResult::InvalidArgument;
	}
	if (!hasRecord_)
	{
		return ObjectiveStateResult::NotFound;
	}
	*record = record_;
	return ObjectiveStateResult::Found;
}

bool ObjectiveState::isComplete() const
{
	return hasRecord_ && record_.status == ObjectiveStatus::Completed;
}

const world::ActorKey &ObjectiveState::actor() const
{
	return actor_;
}
}
}
