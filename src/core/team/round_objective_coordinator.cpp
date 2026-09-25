#include "astrabot/team/round_objective_coordinator.hpp"

#include <cmath>
#include <limits>

namespace astrabot
{
namespace team
{
namespace
{
constexpr std::uint32_t kPathFailureRetryFrames = 128U;

bool isFinitePosition(const world::WorldVector &position)
{
	return std::isfinite(position.x) && std::isfinite(position.y) &&
		std::isfinite(position.z) &&
		std::fabs(position.x) <= world::WorldLimits::kMaximumCoordinate &&
		std::fabs(position.y) <= world::WorldLimits::kMaximumCoordinate &&
		std::fabs(position.z) <= world::WorldLimits::kMaximumCoordinate;
}

bool actorKeyBefore(const world::ActorKey &left, const world::ActorKey &right)
{
	return left.slot < right.slot ||
		(left.slot == right.slot && left.generation < right.generation);
}

const TeamObjectiveActorObservation *findActor(
	const TeamObjectiveInput &input, const world::ActorKey &actor)
{
	for (std::size_t index = 0U; index < input.actorCount; ++index)
	{
		if (input.actors[index].actor == actor)
		{
			return &input.actors[index];
		}
	}
	return nullptr;
}

bool isEligible(const TeamObjectiveActorObservation &actor, objectives::TeamRole team)
{
	return actor.actor.isValid() && actor.alive && actor.team == team;
}

bool sameTarget(const TeamObjectiveAssignment &assignment, const TeamBombTargetObservation &target)
{
	return assignment.targetEntity == target.entity;
}

bool actorCanKeepAssignment(
	const TeamObjectiveActorObservation &actor, TeamObjectiveKind kind)
{
	switch (kind)
	{
	case TeamObjectiveKind::RetrieveDroppedC4:
		return isEligible(actor, objectives::TeamRole::Terrorist) && !actor.carryingC4;
	case TeamObjectiveKind::PlantC4:
		return isEligible(actor, objectives::TeamRole::Terrorist) && actor.carryingC4;
	case TeamObjectiveKind::DefuseC4:
		return isEligible(actor, objectives::TeamRole::CounterTerrorist);
	case TeamObjectiveKind::None:
	default:
		return false;
	}
}

bool routeCost(
	const TeamObjectiveActorObservation &actor,
	const TeamBombTargetObservation &target,
	const nav::NavSnapshot &navigation,
	const nav::NavQuery *query,
	float *cost)
{
	if (cost == nullptr || !target.available || !isFinitePosition(target.position))
	{
		return false;
	}

	if (!navigation.isValid())
	{
		if (!actor.positionAvailable || !isFinitePosition(actor.position))
		{
			return false;
		}
		const float dx = actor.position.x - target.position.x;
		const float dy = actor.position.y - target.position.y;
		const float dz = actor.position.z - target.position.z;
		const float geometricCost = dx * dx + dy * dy + dz * dz;
		if (!std::isfinite(geometricCost))
		{
			return false;
		}
		*cost = geometricCost;
		return true;
	}

	if (query == nullptr)
	{
		return false;
	}

	nav::AreaId startArea = actor.currentArea;
	if (startArea == 0U)
	{
		if (!actor.positionAvailable || !isFinitePosition(actor.position))
		{
			return false;
		}
		nav::NavAreaMatch match = {};
		if (query->findNearest3D(
				{actor.position.x, actor.position.y, actor.position.z},
				world::WorldLimits::kMaximumCoordinate,
				&match) != nav::NavQueryResult::Found)
		{
			return false;
		}
		startArea = match.area;
	}

	nav::AreaId targetArea = target.area;
	if (targetArea == 0U)
	{
		nav::NavAreaMatch match = {};
		if (query->findNearest3D(
				{target.position.x, target.position.y, target.position.z},
				world::WorldLimits::kMaximumCoordinate,
				&match) != nav::NavQueryResult::Found)
		{
			return false;
		}
		targetArea = match.area;
	}

	nav::NavCorridor corridor = {};
	if (query->buildCorridor(
			startArea, targetArea, nav::NavRouteType::Fastest, &corridor) !=
			nav::NavQueryResult::Found ||
		!corridor.isValid() || !std::isfinite(corridor.cost) || corridor.cost < 0.0f)
	{
		return false;
	}
	*cost = corridor.cost;
	return true;
}
}

RoundObjectiveCoordinator::RoundObjectiveCoordinator()
: hasAssignment_(false),
  mapGeneration_(0U),
  roundGeneration_(0U),
  nextAssignmentGeneration_(0U),
	assignment_{},
	hasFailedPathAssignment_(false),
	failedPathKind_(TeamObjectiveKind::None),
	failedPathTarget_{},
	failedPathActorCount_(0U),
	failedPathActors_{}
{
}

void RoundObjectiveCoordinator::reportPathFailure(
	const world::ActorKey &actor,
	std::uint32_t assignmentGeneration,
	nav::AreaId startArea,
	std::uint32_t frameSequence)
{
	if (!hasAssignment_ || !actor.isValid() ||
		!(assignment_.actor == actor) ||
		assignment_.generation != assignmentGeneration)
	{
		return;
	}
	if (assignment_.kind != TeamObjectiveKind::PlantC4)
	{
		if (!hasFailedPathAssignment_ ||
			failedPathKind_ != assignment_.kind ||
			!(failedPathTarget_ == assignment_.targetEntity))
		{
			hasFailedPathAssignment_ = true;
			failedPathKind_ = assignment_.kind;
			failedPathTarget_ = assignment_.targetEntity;
			failedPathActorCount_ = 0U;
		}
		bool alreadyRecorded = false;
		for (std::size_t index = 0U;
			index < failedPathActorCount_; ++index)
		{
			if (failedPathActors_[index].actor == actor)
			{
				failedPathActors_[index].startArea = startArea;
				failedPathActors_[index].frameSequence = frameSequence;
				alreadyRecorded = true;
				break;
			}
		}
		if (!alreadyRecorded &&
			failedPathActorCount_ < kMaximumTeamObjectiveActors)
		{
			failedPathActors_[failedPathActorCount_++] = {
				actor, startArea, frameSequence};
		}
	}
	hasAssignment_ = false;
	assignment_ = {};
}

TeamObjectiveResult RoundObjectiveCoordinator::assign(
	const TeamObjectiveInput &input,
	const nav::NavSnapshot &navigation,
	TeamObjectiveAssignmentSet *assignments)
{
	if (assignments == nullptr || input.actorCount > kMaximumTeamObjectiveActors ||
		input.mapGeneration == 0U || input.roundGeneration == 0U)
	{
		return TeamObjectiveResult::InvalidInput;
	}
	*assignments = {};

	if (mapGeneration_ != input.mapGeneration || roundGeneration_ != input.roundGeneration)
	{
		reset(input.mapGeneration, input.roundGeneration);
	}

	const TeamObjectiveActorObservation *carrier = nullptr;
	for (std::size_t index = 0U; index < input.actorCount; ++index)
	{
		const TeamObjectiveActorObservation &candidate = input.actors[index];
		if (!candidate.carryingC4 || !isEligible(candidate, objectives::TeamRole::Terrorist))
		{
			continue;
		}
		if (carrier == nullptr || actorKeyBefore(candidate.actor, carrier->actor))
		{
			carrier = &candidate;
		}
	}

	TeamObjectiveKind wantedKind = TeamObjectiveKind::None;
	objectives::TeamRole wantedTeam = objectives::TeamRole::Unknown;
	const TeamBombTargetObservation *target = nullptr;
	const TeamObjectiveActorObservation *fixedOwner = nullptr;
	if (input.plantedC4.available)
	{
		if (!input.plantedC4.entity.isValid() || !isFinitePosition(input.plantedC4.position))
		{
			return TeamObjectiveResult::InvalidInput;
		}
		wantedKind = TeamObjectiveKind::DefuseC4;
		wantedTeam = objectives::TeamRole::CounterTerrorist;
		target = &input.plantedC4;
	}
	else if (carrier != nullptr)
	{
		wantedKind = TeamObjectiveKind::PlantC4;
		fixedOwner = carrier;
	}
	else if (input.droppedC4.available)
	{
		if (!input.droppedC4.entity.isValid() || !isFinitePosition(input.droppedC4.position))
		{
			return TeamObjectiveResult::InvalidInput;
		}
		wantedKind = TeamObjectiveKind::RetrieveDroppedC4;
		wantedTeam = objectives::TeamRole::Terrorist;
		target = &input.droppedC4;
	}
	else
	{
		hasAssignment_ = false;
		assignment_ = {};
		hasFailedPathAssignment_ = false;
		failedPathActorCount_ = 0U;
		return TeamObjectiveResult::NoObjective;
	}

	const bool failedObjectiveStillCurrent =
		hasFailedPathAssignment_ && failedPathKind_ == wantedKind &&
		target != nullptr && failedPathTarget_ == target->entity;
	if (!failedObjectiveStillCurrent)
	{
		hasFailedPathAssignment_ = false;
		failedPathActorCount_ = 0U;
	}
	else
	{
		std::size_t failedIndex = 0U;
		while (failedIndex < failedPathActorCount_)
		{
			const FailedPathActor &failed = failedPathActors_[failedIndex];
			const TeamObjectiveActorObservation *actor =
				findActor(input, failed.actor);
			const bool actorInvalid = actor == nullptr ||
				!actorCanKeepAssignment(*actor, wantedKind);
			const bool routeStartChanged = actor != nullptr &&
				failed.startArea != 0U && actor->currentArea != 0U &&
				actor->currentArea != failed.startArea;
			const bool retryWindowElapsed =
				input.frameSequence != 0U && failed.frameSequence != 0U &&
				static_cast<std::uint32_t>(
					input.frameSequence - failed.frameSequence) >=
					kPathFailureRetryFrames;
			if (!actorInvalid && !routeStartChanged && !retryWindowElapsed)
			{
				++failedIndex;
				continue;
			}
			for (std::size_t moveIndex = failedIndex + 1U;
				moveIndex < failedPathActorCount_; ++moveIndex)
			{
				failedPathActors_[moveIndex - 1U] =
					failedPathActors_[moveIndex];
			}
			--failedPathActorCount_;
		}
		hasFailedPathAssignment_ = failedPathActorCount_ != 0U;
	}

	if (hasAssignment_ && assignment_.kind == wantedKind &&
		(assignment_.kind == TeamObjectiveKind::PlantC4 ||
			(target != nullptr && sameTarget(assignment_, *target))))
	{
		const TeamObjectiveActorObservation *assignedActor =
			findActor(input, assignment_.actor);
		if (assignedActor != nullptr && actorCanKeepAssignment(*assignedActor, wantedKind) &&
			(fixedOwner == nullptr || fixedOwner->actor == assignedActor->actor))
		{
			if (target != nullptr)
			{
				assignment_.targetPosition = target->position;
				assignment_.targetArea = target->area;
			}
			assignments->count = 1U;
			assignments->assignments[0U] = assignment_;
			return TeamObjectiveResult::Assigned;
		}
	}

	const TeamObjectiveActorObservation *selectedActor = fixedOwner;
	float selectedCost = 0.0f;
	bool selectedCostAvailable = false;
	if (selectedActor == nullptr && target != nullptr)
	{
		nav::NavQuery query(navigation);
		const nav::NavQuery *queryPointer = navigation.isValid() ? &query : nullptr;
		float bestCost = (std::numeric_limits<float>::max)();
		for (std::size_t index = 0U; index < input.actorCount; ++index)
		{
		const TeamObjectiveActorObservation &candidate = input.actors[index];
		bool actorPathFailed = false;
		if (failedObjectiveStillCurrent)
		{
			for (std::size_t failedIndex = 0U;
				failedIndex < failedPathActorCount_; ++failedIndex)
			{
				if (failedPathActors_[failedIndex].actor == candidate.actor)
				{
					actorPathFailed = true;
					break;
				}
			}
		}
		if (!isEligible(candidate, wantedTeam) || actorPathFailed ||
			(wantedKind == TeamObjectiveKind::RetrieveDroppedC4 && candidate.carryingC4))
			{
				continue;
			}
			float candidateCost = 0.0f;
			if (!routeCost(candidate, *target, navigation, queryPointer, &candidateCost))
			{
				continue;
			}
			if (selectedActor == nullptr || candidateCost < bestCost ||
				(candidateCost == bestCost && actorKeyBefore(candidate.actor, selectedActor->actor)))
			{
			selectedActor = &candidate;
			bestCost = candidateCost;
			selectedCost = candidateCost;
			selectedCostAvailable = true;
			}
		}
	}

	if (selectedActor == nullptr)
	{
		hasAssignment_ = false;
		assignment_ = {};
		return TeamObjectiveResult::NoObjective;
	}

	if (nextAssignmentGeneration_ == (std::numeric_limits<std::uint32_t>::max)())
	{
		nextAssignmentGeneration_ = 1U;
	}
	else
	{
		++nextAssignmentGeneration_;
		if (nextAssignmentGeneration_ == 0U)
		{
			nextAssignmentGeneration_ = 1U;
		}
	}

	assignment_ = {};
	assignment_.actor = selectedActor->actor;
	assignment_.kind = wantedKind;
	if (target != nullptr)
	{
		assignment_.targetEntity = target->entity;
		assignment_.targetPosition = target->position;
		assignment_.targetArea = target->area;
	}
	assignment_.generation = nextAssignmentGeneration_;
	assignment_.routeCostAvailable = selectedCostAvailable;
	assignment_.routeCost = selectedCost;
	assignment_.routeCostGeometricFallback =
		selectedCostAvailable && !navigation.isValid();
	mapGeneration_ = input.mapGeneration;
	roundGeneration_ = input.roundGeneration;
	hasAssignment_ = true;
	assignments->count = 1U;
	assignments->assignments[0U] = assignment_;
	return TeamObjectiveResult::Assigned;
}

void RoundObjectiveCoordinator::reset(
	std::uint32_t mapGeneration, std::uint32_t roundGeneration)
{
	hasAssignment_ = false;
	mapGeneration_ = mapGeneration;
	roundGeneration_ = roundGeneration;
	nextAssignmentGeneration_ = 0U;
	assignment_ = {};
	hasFailedPathAssignment_ = false;
	failedPathKind_ = TeamObjectiveKind::None;
	failedPathTarget_ = {};
	failedPathActorCount_ = 0U;
	for (FailedPathActor &actor : failedPathActors_)
	{
		actor = {};
	}
}
}
}
