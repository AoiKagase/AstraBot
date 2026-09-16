#include "astrabot/objectives/round_objectives.hpp"

#include <algorithm>
#include <limits>

namespace astrabot
{
namespace objectives
{
namespace
{
bool isValidScenarioKind(ScenarioKind kind)
{
	return kind == ScenarioKind::Unknown || kind == ScenarioKind::Bomb ||
		kind == ScenarioKind::Hostage;
}

bool isValidTeamRole(TeamRole team)
{
	return team == TeamRole::Unknown || team == TeamRole::Terrorist ||
		team == TeamRole::CounterTerrorist;
}
}

bool ScenarioObservation::isValid() const
{
	return scenario.frame.isValid() && scenario.scenarioGeneration != 0U &&
		isValidScenarioKind(scenario.kind) && isValidTeamRole(scenario.team) &&
		(eventCount <= kMaximumEvents);
}

RoundObjectiveConfig::RoundObjectiveConfig()
	: proposalLifetimeTicks(64U)
{
}

bool RoundObjectiveConfig::isValid() const
{
	return proposalLifetimeTicks > 0U &&
		proposalLifetimeTicks <= ObjectiveLimits::kMaximumLifetimeTicks;
}

RoundObjectivePlanner::RoundObjectivePlanner()
	: config_(),
	  actor_(),
	  lastFrame_(),
	  state_(),
	  initialized_(false)
{
}

RoundObjectivePlanner::RoundObjectivePlanner(const world::ActorKey &actor)
	: config_(),
	  actor_(actor),
	  lastFrame_(),
	  state_(actor),
	  initialized_(actor.isValid())
{
}

RoundObjectivePlanner::RoundObjectivePlanner(
	const world::ActorKey &actor,
	const RoundObjectiveConfig &config)
	: config_(config),
	  actor_(actor),
	  lastFrame_(),
	  state_(actor),
	  initialized_(actor.isValid() && config.isValid())
{
}

bool RoundObjectivePlanner::isValidPhase(RoundPhase phase)
{
	return phase == RoundPhase::Unknown || phase == RoundPhase::Freeze ||
		phase == RoundPhase::Live || phase == RoundPhase::PostRound;
}

bool RoundObjectivePlanner::isValidAvailability(Availability availability)
{
	return availability == Availability::Unknown ||
		availability == Availability::Unavailable ||
		availability == Availability::Available;
}

bool RoundObjectivePlanner::isValidEventKind(ScenarioEventKind kind)
{
	return kind != ScenarioEventKind::Unknown;
}

bool RoundObjectivePlanner::isValidEventState(EventState state)
{
	return state == EventState::Unknown || state == EventState::Observed ||
		state == EventState::Unavailable;
}

bool RoundObjectivePlanner::sameRound(
	const world::FrameIdentity &left,
	const world::FrameIdentity &right)
{
	return left.mapGeneration == right.mapGeneration &&
		left.roundGeneration == right.roundGeneration;
}

bool RoundObjectivePlanner::isFrameAfter(
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

bool RoundObjectivePlanner::makeProposal(
	const ScenarioObservation &observation,
	ObjectiveKind kind,
	ObjectivePriority priority,
	std::uint32_t subjectId,
	ObjectiveProposal *proposal,
	std::uint32_t lifetimeTicks)
{
	if (proposal == nullptr || kind == ObjectiveKind::None ||
			lifetimeTicks == 0U ||
			lifetimeTicks > ObjectiveLimits::kMaximumLifetimeTicks)
	{
		return false;
	}
	const std::uint32_t tick = observation.scenario.frame.tick;
	if (tick > std::numeric_limits<std::uint32_t>::max() - lifetimeTicks)
	{
		return false;
	}

	*proposal = {};
	proposal->actor = {};
	proposal->scenario = observation.scenario;
	proposal->objective.scenario = observation.scenario;
	proposal->objective.kind = kind;
	proposal->objective.subjectId = subjectId;
	proposal->priority = priority;
	proposal->issuedTick = tick;
	proposal->expiresAtTick = tick + lifetimeTicks;
	return true;
}

bool RoundObjectivePlanner::findEvent(
	const ScenarioObservation &observation,
	ScenarioEventKind kind,
	ScenarioEvent *event)
{
	if (event == nullptr)
	{
		return false;
	}
	for (std::size_t index = 0U; index < observation.eventCount; ++index)
	{
		if (observation.events[index].kind == kind &&
				observation.events[index].state == EventState::Observed)
		{
			*event = observation.events[index];
			return true;
		}
	}
	return false;
}

ObjectiveResult RoundObjectivePlanner::proposalForScenario(
	const world::WorldSnapshot &snapshot,
	behavior::BehaviorState behaviorState,
	const ScenarioObservation &observation,
	ObjectiveProposal *proposal)
{
	ObjectiveKind kind = ObjectiveKind::None;
	ObjectivePriority priority = ObjectivePriority::Normal;
	std::uint32_t subjectId = 0U;
	ScenarioEvent event = {};

	if (behaviorState == behavior::BehaviorState::Retreat)
	{
		kind = ObjectiveKind::Save;
		priority = ObjectivePriority::Critical;
	}
	else if (observation.buyAvailability == Availability::Available &&
			observation.phase == RoundPhase::Freeze)
	{
		kind = ObjectiveKind::Buy;
		priority = ObjectivePriority::High;
	}
	else if (observation.scenario.kind == ScenarioKind::Bomb)
	{
		if (findEvent(observation, ScenarioEventKind::BombDefused, &event) ||
				findEvent(observation, ScenarioEventKind::BombExploded, &event))
		{
			(void)snapshot;
			return ObjectiveResult::NoObjective;
		}
		if (findEvent(observation, ScenarioEventKind::BombPlanted, &event))
		{
			subjectId = event.id;
			if (observation.scenario.team == TeamRole::CounterTerrorist)
			{
				kind = ObjectiveKind::Defuse;
				priority = ObjectivePriority::Critical;
			}
			else
			{
				kind = ObjectiveKind::Defend;
				priority = ObjectivePriority::High;
			}
		}
		else if (findEvent(observation, ScenarioEventKind::BombCarried, &event) &&
				observation.scenario.team == TeamRole::Terrorist)
		{
			kind = ObjectiveKind::Plant;
			subjectId = event.id;
		}
		else if (observation.scenario.team == TeamRole::Terrorist)
		{
			kind = ObjectiveKind::Attack;
		}
		else
		{
			kind = ObjectiveKind::Defend;
		}
	}
	else if (observation.scenario.kind == ScenarioKind::Hostage)
	{
		if (findEvent(
				observation,
				ScenarioEventKind::HostageRescued,
				&event) ||
				findEvent(observation, ScenarioEventKind::HostageKilled, &event))
		{
			(void)snapshot;
			return ObjectiveResult::NoObjective;
		}
		if (findEvent(
				observation,
				ScenarioEventKind::HostagePickedUp,
				&event))
		{
			kind = ObjectiveKind::Escort;
			 subjectId = event.id;
		}
		else
		{
			kind = ObjectiveKind::Rescue;
		}
	}

	if (kind == ObjectiveKind::None)
	{
		(void)snapshot;
		return ObjectiveResult::NoObjective;
	}
	if (!makeProposal(
			observation,
			kind,
			priority,
			subjectId,
			proposal,
			config_.proposalLifetimeTicks))
	{
		return ObjectiveResult::InvalidObservation;
	}
	proposal->actor = actor_;
	if (!proposal->isValid() ||
			state_.acceptProposal(*proposal) != ObjectiveStateResult::Accepted)
	{
		return ObjectiveResult::InvalidObservation;
	}
	return ObjectiveResult::Proposed;
}

ObjectiveResult RoundObjectivePlanner::plan(
	const world::WorldSnapshot &snapshot,
	behavior::BehaviorState behaviorState,
	const ScenarioObservation &observation,
	const ObjectiveCompletionFeedback *feedback,
	ObjectiveProposal *proposal)
{
	if (proposal == nullptr)
	{
		return ObjectiveResult::InvalidArgument;
	}
	*proposal = {};
	if (!config_.isValid())
	{
		return ObjectiveResult::InvalidConfig;
	}
	if (!snapshot.isValid())
	{
		return ObjectiveResult::InvalidSnapshot;
	}
	if (!observation.isValid())
	{
		return ObjectiveResult::InvalidObservation;
	}
	if (!(snapshot.identity().observer == actor_) ||
		!(snapshot.identity().frame == observation.scenario.frame))
	{
		return ObjectiveResult::InvalidObservation;
	}
	if (!initialized_)
	{
		actor_ = snapshot.identity().observer;
		state_ = ObjectiveState(actor_);
		initialized_ = true;
	}
	else if (!(actor_ == snapshot.identity().observer))
	{
		return snapshot.identity().observer.slot == actor_.slot ?
			ObjectiveResult::StaleGeneration : ObjectiveResult::InvalidObservation;
	}

	if (lastFrame_.isValid())
	{
		if (sameRound(observation.scenario.frame, lastFrame_) &&
				!isFrameAfter(observation.scenario.frame, lastFrame_))
		{
			return ObjectiveResult::StaleFrame;
		}
		if (!isFrameAfter(observation.scenario.frame, lastFrame_))
		{
			return ObjectiveResult::StaleFrame;
		}
		if (!sameRound(observation.scenario.frame, lastFrame_))
		{
			state_ = ObjectiveState(actor_);
		}
	}
	lastFrame_ = observation.scenario.frame;

	if (!isValidPhase(observation.phase) ||
			!isValidAvailability(observation.buyAvailability) ||
			observation.phase == RoundPhase::Unknown ||
			observation.scenario.kind == ScenarioKind::Unknown ||
			observation.scenario.team == TeamRole::Unknown)
	{
		return ObjectiveResult::RecoveryPending;
	}
	if (observation.phase == RoundPhase::PostRound)
	{
		return ObjectiveResult::NoObjective;
	}
	for (std::size_t index = 0U; index < observation.eventCount; ++index)
	{
		if (!isValidEventKind(observation.events[index].kind) ||
				!isValidEventState(observation.events[index].state) ||
				!observation.events[index].frame.isValid())
		{
			return ObjectiveResult::InvalidObservation;
		}
		if (observation.events[index].state != EventState::Observed)
		{
			return ObjectiveResult::RecoveryPending;
		}
		if (observation.events[index].frame.mapGeneration !=
				observation.scenario.frame.mapGeneration ||
			observation.events[index].frame.roundGeneration !=
				observation.scenario.frame.roundGeneration ||
			observation.events[index].frame.tick >
				observation.scenario.frame.tick)
		{
			return ObjectiveResult::StaleFrame;
		}
	}

	if (feedback != nullptr)
	{
		const ObjectiveStateResult feedbackResult = state_.applyFeedback(*feedback);
		if (feedbackResult == ObjectiveStateResult::Completed)
		{
			return ObjectiveResult::Completed;
		}
		if (feedbackResult == ObjectiveStateResult::FeedbackUnavailable)
		{
			return ObjectiveResult::RecoveryPending;
		}
		if (feedbackResult == ObjectiveStateResult::StaleGeneration)
		{
			return ObjectiveResult::StaleGeneration;
		}
		if (feedbackResult == ObjectiveStateResult::StaleFrame)
		{
			return ObjectiveResult::StaleFrame;
		}
	}

	return proposalForScenario(snapshot, behaviorState, observation, proposal);
}

const world::ActorKey &RoundObjectivePlanner::actor() const
{
	return actor_;
}

bool RoundObjectivePlanner::isInitialized() const
{
	return initialized_;
}
}
}
