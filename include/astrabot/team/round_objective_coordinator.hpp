#ifndef ASTRABOT_TEAM_ROUND_OBJECTIVE_COORDINATOR_HPP
#define ASTRABOT_TEAM_ROUND_OBJECTIVE_COORDINATOR_HPP

#include "astrabot/nav/nav_query.hpp"
#include "astrabot/objectives/objective_state.hpp"
#include "astrabot/world/world_snapshot.hpp"

#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace team
{
	static constexpr std::size_t kMaximumTeamObjectiveActors = 32U;

	enum class TeamObjectiveKind : std::uint8_t
	{
		None,
		RetrieveDroppedC4,
		PlantC4,
		DefuseC4,
		GuardBombDefuser
	};

	enum class TeamObjectiveResult : std::uint8_t
	{
		Assigned,
		NoObjective,
		InvalidInput
	};

	struct TeamObjectiveActorObservation
	{
		world::ActorKey actor;
		objectives::TeamRole team;
		bool alive;
		bool carryingC4;
		bool positionAvailable;
		world::WorldVector position;
		nav::AreaId currentArea;
	};

	struct TeamBombTargetObservation
	{
		bool available;
		world::EntityKey entity;
		world::WorldVector position;
		nav::AreaId area;
	};

	struct TeamObjectiveInput
	{
		std::uint32_t mapGeneration;
		std::uint32_t roundGeneration;
	std::size_t actorCount;
	TeamObjectiveActorObservation actors[kMaximumTeamObjectiveActors];
	TeamBombTargetObservation droppedC4;
	TeamBombTargetObservation plantedC4;
	bool externalDefuserActive;
	std::uint32_t frameSequence;
	};

	struct TeamObjectiveAssignment
	{
		world::ActorKey actor;
		TeamObjectiveKind kind;
		world::EntityKey targetEntity;
		world::WorldVector targetPosition;
	nav::AreaId targetArea;
	std::uint32_t generation;
	bool routeCostAvailable;
	float routeCost;
	bool routeCostGeometricFallback;
};

	struct TeamObjectiveAssignmentSet
	{
		std::size_t count;
		TeamObjectiveAssignment assignments[kMaximumTeamObjectiveActors];
	};

	class RoundObjectiveCoordinator
	{
	public:
		RoundObjectiveCoordinator();

	TeamObjectiveResult assign(
		const TeamObjectiveInput &input,
		const nav::NavSnapshot &navigation,
		TeamObjectiveAssignmentSet *assignments);
	void reportPathFailure(
		const world::ActorKey &actor,
		std::uint32_t assignmentGeneration,
		nav::AreaId startArea,
		std::uint32_t frameSequence);
	void reset(std::uint32_t mapGeneration, std::uint32_t roundGeneration);

	private:
		bool hasAssignment_;
		std::uint32_t mapGeneration_;
		std::uint32_t roundGeneration_;
	std::uint32_t nextAssignmentGeneration_;
	TeamObjectiveAssignment assignment_;
	bool hasFailedPathAssignment_;
	TeamObjectiveKind failedPathKind_;
	world::EntityKey failedPathTarget_;
	std::size_t failedPathActorCount_;
	struct FailedPathActor
	{
		world::ActorKey actor;
		nav::AreaId startArea;
		std::uint32_t frameSequence;
	};
	FailedPathActor failedPathActors_[kMaximumTeamObjectiveActors];
};
}
}

#endif
