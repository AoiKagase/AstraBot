#include "astrabot/team/round_objective_coordinator.hpp"
#include "astrabot/runtime/nav_roam_controller.hpp"

#include <cmath>
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

astrabot::nav::NavArea area(astrabot::nav::AreaId id, float loX, float hiX)
{
	astrabot::nav::NavArea result = {};
	result.id = id;
	result.extent.lo = {loX, 0.0f, 0.0f};
	result.extent.hi = {hiX, 64.0f, 0.0f};
	result.northEastZ = 0.0f;
	result.southWestZ = 0.0f;
	return result;
}

struct NavFixture
{
	astrabot::nav::NavDocument document;
	astrabot::nav::NavSnapshotPublisher publisher;
	bool ready;

	NavFixture(bool connectBothStarts = true)
		: document(), publisher(), ready(false)
	{
		document.setSourceIdentity({5U, 100U, 200U});
		auto farStart = area(1U, 0.0f, 64.0f);
		auto nearStart = area(2U, 200.0f, 264.0f);
		auto goal = area(3U, 300.0f, 364.0f);
		if (connectBothStarts)
		{
			farStart.connections[0U].push_back(3U);
			nearStart.connections[0U].push_back(3U);
		}
		document.addArea(farStart);
		document.addArea(nearStart);
		document.addArea(goal);
		ready = publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published;
	}

	astrabot::nav::NavSnapshot snapshot() const
	{
		return publisher.snapshot();
	}
};

astrabot::team::TeamObjectiveActorObservation actor(
	std::uint32_t slot,
	astrabot::objectives::TeamRole team,
	bool alive,
	bool carryingC4,
	astrabot::nav::AreaId currentArea,
	float x)
{
	return {{slot, 1U}, team, alive, carryingC4, true, {x, 32.0f, 0.0f}, currentArea};
}

astrabot::team::TeamBombTargetObservation bombTarget(
	std::uint32_t id,
	astrabot::nav::AreaId areaId,
	float x)
{
	return {true, {id, 1U}, {x, 32.0f, 0.0f}, areaId};
}

bool testDroppedBombSelectsNearestReachableTerrorist()
{
	NavFixture nav;
	if (!check(nav.ready, "dropped C4 NAV fixture publishes"))
	{
		return false;
	}
	astrabot::team::TeamObjectiveInput input = {};
	input.mapGeneration = 1U;
	input.roundGeneration = 4U;
	input.actorCount = 3U;
	input.actors[0] = actor(1U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 1U, 32.0f);
	input.actors[1] = actor(2U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 2U, 232.0f);
	input.actors[2] = actor(3U, astrabot::objectives::TeamRole::CounterTerrorist,
		true, false, 2U, 232.0f);
	input.droppedC4 = bombTarget(40U, 3U, 332.0f);

	astrabot::team::RoundObjectiveCoordinator coordinator;
	astrabot::team::TeamObjectiveAssignmentSet assignments = {};
	const auto result = coordinator.assign(input, nav.snapshot(), &assignments);
	return check(result == astrabot::team::TeamObjectiveResult::Assigned &&
		assignments.count == 1U && assignments.assignments[0U].actor.slot == 2U &&
		assignments.assignments[0U].kind ==
			astrabot::team::TeamObjectiveKind::RetrieveDroppedC4 &&
		assignments.assignments[0U].routeCostAvailable &&
		assignments.assignments[0U].routeCost > 0.0f &&
		!assignments.assignments[0U].routeCostGeometricFallback,
			"nearest reachable Terrorist receives the dropped C4 assignment");
}

bool testPlantedBombSelectsNearestReachableCounterTerrorist()
{
	NavFixture nav;
	if (!check(nav.ready, "planted C4 NAV fixture publishes"))
	{
		return false;
	}
	astrabot::team::TeamObjectiveInput input = {};
	input.mapGeneration = 1U;
	input.roundGeneration = 4U;
	input.actorCount = 3U;
	input.actors[0] = actor(1U, astrabot::objectives::TeamRole::CounterTerrorist,
		true, false, 1U, 32.0f);
	input.actors[1] = actor(2U, astrabot::objectives::TeamRole::CounterTerrorist,
		true, false, 2U, 232.0f);
	input.actors[2] = actor(3U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 2U, 232.0f);
	input.plantedC4 = bombTarget(41U, 3U, 332.0f);

	astrabot::team::RoundObjectiveCoordinator coordinator;
	astrabot::team::TeamObjectiveAssignmentSet assignments = {};
	const auto result = coordinator.assign(input, nav.snapshot(), &assignments);
	return check(result == astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.count == 1U && assignments.assignments[0U].actor.slot == 2U &&
			assignments.assignments[0U].kind == astrabot::team::TeamObjectiveKind::DefuseC4,
			"nearest reachable Counter-Terrorist receives the defuse assignment");
}

bool testExternalDefuserAssignsBombAreaGuardThenRestoresDefuse()
{
	NavFixture nav;
	if (!check(nav.ready, "external defuser NAV fixture publishes"))
	{
		return false;
	}
	astrabot::team::TeamObjectiveInput input = {};
	input.mapGeneration = 1U;
	input.roundGeneration = 4U;
	input.frameSequence = 1U;
	input.actorCount = 2U;
	input.actors[0] = actor(1U,
		astrabot::objectives::TeamRole::CounterTerrorist, true, false, 1U, 32.0f);
	input.actors[1] = actor(2U,
		astrabot::objectives::TeamRole::CounterTerrorist, true, false, 2U, 232.0f);
	input.plantedC4 = bombTarget(47U, 3U, 310.0f);
	input.externalDefuserActive = true;
	astrabot::team::RoundObjectiveCoordinator coordinator;
	astrabot::team::TeamObjectiveAssignmentSet assignments = {};
	if (!check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.count == 1U &&
			assignments.assignments[0U].actor.slot == 2U &&
			assignments.assignments[0U].kind ==
				astrabot::team::TeamObjectiveKind::GuardBombDefuser &&
			assignments.assignments[0U].targetArea == 3U &&
			std::fabs(assignments.assignments[0U].targetPosition.x - 332.0f) < 0.001f,
			"active external defuser gives one CT bot a guard point in the bomb NAV area"))
	{
		return false;
	}
	input.frameSequence = 2U;
	input.externalDefuserActive = false;
	return check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.count == 1U &&
			assignments.assignments[0U].actor.slot == 2U &&
			assignments.assignments[0U].kind ==
				astrabot::team::TeamObjectiveKind::DefuseC4,
			"ending external defuse returns the nearest CT bot to the defuse assignment");
}

bool testDistanceTieUsesStableActorKeyAndOneOwner()
{
	NavFixture nav;
	if (!check(nav.ready, "tie-break NAV fixture publishes"))
	{
		return false;
	}
	astrabot::team::TeamObjectiveInput input = {};
	input.mapGeneration = 1U;
	input.roundGeneration = 4U;
	input.actorCount = 2U;
	input.actors[0] = actor(5U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 2U, 232.0f);
	input.actors[1] = actor(3U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 2U, 232.0f);
	input.droppedC4 = bombTarget(42U, 3U, 332.0f);

	astrabot::team::RoundObjectiveCoordinator coordinator;
	astrabot::team::TeamObjectiveAssignmentSet assignments = {};
	return check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned && assignments.count == 1U &&
			assignments.assignments[0U].actor.slot == 3U,
			"equal NAV costs use stable actor slot order and assign one owner");
}

bool testDeadAndWrongTeamActorsCannotRetrieveDroppedBomb()
{
	NavFixture nav;
	if (!check(nav.ready, "eligibility NAV fixture publishes"))
	{
		return false;
	}
	astrabot::team::TeamObjectiveInput input = {};
	input.mapGeneration = 1U;
	input.roundGeneration = 4U;
	input.actorCount = 2U;
	input.actors[0] = actor(1U, astrabot::objectives::TeamRole::Terrorist,
		false, false, 2U, 232.0f);
	input.actors[1] = actor(2U, astrabot::objectives::TeamRole::CounterTerrorist,
		true, false, 2U, 232.0f);
	input.droppedC4 = bombTarget(43U, 3U, 332.0f);

	astrabot::team::RoundObjectiveCoordinator coordinator;
	astrabot::team::TeamObjectiveAssignmentSet assignments = {};
	return check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::NoObjective && assignments.count == 0U,
			"dead and wrong-team actors are not assigned to dropped C4");
}

bool testCarryingTerroristOwnsPlantInsteadOfDroppedPickup()
{
	NavFixture nav;
	if (!check(nav.ready, "carrier NAV fixture publishes"))
	{
		return false;
	}
	astrabot::team::TeamObjectiveInput input = {};
	input.mapGeneration = 1U;
	input.roundGeneration = 4U;
	input.actorCount = 2U;
	input.actors[0] = actor(1U, astrabot::objectives::TeamRole::Terrorist,
		true, true, 1U, 32.0f);
	input.actors[1] = actor(2U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 2U, 232.0f);
	input.droppedC4 = bombTarget(44U, 3U, 332.0f);

	astrabot::team::RoundObjectiveCoordinator coordinator;
	astrabot::team::TeamObjectiveAssignmentSet assignments = {};
	return check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned && assignments.count == 1U &&
			assignments.assignments[0U].actor.slot == 1U &&
			assignments.assignments[0U].kind == astrabot::team::TeamObjectiveKind::PlantC4,
			"observed carrier keeps the Plant assignment while a stale drop observation exists");
}

bool testOwnerIsRetainedUntilItBecomesInvalid()
{
	NavFixture nav;
	if (!check(nav.ready, "retention NAV fixture publishes"))
	{
		return false;
	}
	astrabot::team::TeamObjectiveInput input = {};
	input.mapGeneration = 1U;
	input.roundGeneration = 4U;
	input.actorCount = 2U;
	input.actors[0] = actor(1U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 1U, 32.0f);
	input.actors[1] = actor(2U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 2U, 232.0f);
	input.droppedC4 = bombTarget(45U, 3U, 332.0f);

	astrabot::team::RoundObjectiveCoordinator coordinator;
	astrabot::team::TeamObjectiveAssignmentSet assignments = {};
	if (!check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.assignments[0U].actor.slot == 2U,
			"initial closest Bot receives dropped C4"))
	{
		return false;
	}
	input.actors[0] = actor(1U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 2U, 232.0f);
	input.actors[1] = actor(2U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 1U, 32.0f);
	if (!check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.assignments[0U].actor.slot == 2U,
			"active pickup owner is retained while still eligible"))
	{
		return false;
	}
	input.actors[1].alive = false;
	return check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.assignments[0U].actor.slot == 1U,
			"dead pickup owner is released and the next reachable Bot is assigned");
}

bool testUnavailablePositionCannotBecomeOriginDistance()
{
	astrabot::nav::NavSnapshot unavailableNavigation{};
	astrabot::team::TeamObjectiveInput input = {};
	input.mapGeneration = 1U;
	input.roundGeneration = 4U;
	input.actorCount = 2U;
	input.actors[0] = actor(1U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 0U, 0.0f);
	input.actors[0].positionAvailable = false;
	input.actors[1] = actor(2U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 0U, 500.0f);
	input.droppedC4 = bombTarget(46U, 0U, 520.0f);

	astrabot::team::RoundObjectiveCoordinator coordinator;
	astrabot::team::TeamObjectiveAssignmentSet assignments = {};
	return check(coordinator.assign(input, unavailableNavigation, &assignments) ==
		astrabot::team::TeamObjectiveResult::Assigned && assignments.count == 1U &&
		assignments.assignments[0U].actor.slot == 2U &&
		assignments.assignments[0U].routeCostAvailable &&
		assignments.assignments[0U].routeCostGeometricFallback,
			"unavailable actor origin is not treated as a geometric candidate");
}
}

bool testPathFailureReassignsDroppedC4ToAnotherReachableActor()
{
	NavFixture nav;
	if (!check(nav.ready, "path-failure NAV fixture publishes"))
	{
		return false;
	}
	astrabot::team::TeamObjectiveInput input = {};
	input.mapGeneration = 1U;
	input.roundGeneration = 4U;
	input.frameSequence = 100U;
	input.actorCount = 2U;
	input.actors[0] = actor(
		1U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 2U, 232.0f);
	input.actors[1] = actor(
		2U, astrabot::objectives::TeamRole::Terrorist,
		true, false, 1U, 32.0f);
	input.droppedC4 = bombTarget(47U, 3U, 332.0f);

	astrabot::team::RoundObjectiveCoordinator coordinator;
	astrabot::team::TeamObjectiveAssignmentSet assignments = {};
	if (!check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.assignments[0U].actor.slot == 1U,
			"nearest Bot receives dropped-C4 assignment before route failure"))
	{
		return false;
	}
	const auto failedAssignment = assignments.assignments[0U];
	astrabot::runtime::NavRoamController movement;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {failedAssignment.actor.slot, failedAssignment.actor.generation};
	observation.frame = {1U, 4U, 100U};
	observation.locomotion.position = {232.0f, -8.0f, 40.0f};
	observation.locomotion.grounded = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {332.0f, 32.0f, 0.0f};
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	movement.update(nav.snapshot(), observation, &intent, &decision);
	for (int update = 0; update < 20; ++update)
	{
		observation.frame.tick++;
		movement.update(nav.snapshot(), observation, &intent, &decision);
	}
	if (!check(decision.recoveryCandidateFailed && decision.recoveryArea == 2U &&
		decision.failureReason == astrabot::runtime::NavFailureReason::RecoveryNoProgress,
		"off-mesh no-progress emits one candidate failure for objective reassignment")) return false;
	coordinator.reportPathFailure(
		failedAssignment.actor, failedAssignment.generation, decision.recoveryArea,
		input.frameSequence);
	if (!check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.assignments[0U].actor.slot == 2U,
			"route failure releases current owner and selects next reachable Bot"))
	{
		return false;
	}
	observation.frame.tick++;
	observation.hasObjectiveTarget = false;
	if (!check(movement.update(nav.snapshot(), observation, &intent, &decision) ==
		astrabot::runtime::NavRoamResult::IntentReady && intent.targetArea != 2U &&
		!decision.recoveryCandidateFailed,
		"released objective owner selects another recovery candidate without repeating failure")) return false;
	coordinator.reportPathFailure(
		failedAssignment.actor, failedAssignment.generation, 2U,
		input.frameSequence);
	if (!check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.assignments[0U].actor.slot == 2U,
			"stale path-failure generation cannot evict replacement owner"))
	{
		return false;
	}
	const auto replacementAssignment = assignments.assignments[0U];
	input.frameSequence = 101U;
	coordinator.reportPathFailure(
		replacementAssignment.actor, replacementAssignment.generation, 1U,
		input.frameSequence);
	if (!check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::NoObjective &&
			assignments.count == 0U,
			"two failed owners cannot cycle back to the first Bot"))
	{
		return false;
	}
	input.droppedC4 = bombTarget(48U, 3U, 332.0f);
	input.frameSequence = 102U;
	if (!check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.assignments[0U].actor.slot == 1U,
			"new dropped-C4 identity clears prior path-failure exclusions"))
	{
		return false;
	}
	const auto renewedAssignment = assignments.assignments[0U];
	coordinator.reportPathFailure(
		renewedAssignment.actor, renewedAssignment.generation, 2U,
		input.frameSequence);
	input.frameSequence = 230U;
	return check(coordinator.assign(input, nav.snapshot(), &assignments) ==
			astrabot::team::TeamObjectiveResult::Assigned &&
			assignments.assignments[0U].actor.slot == 1U,
			"path-failure exclusion expires after its retry window");
}

int main()
{
	if (!testDroppedBombSelectsNearestReachableTerrorist() ||
		!testPlantedBombSelectsNearestReachableCounterTerrorist() ||
		!testExternalDefuserAssignsBombAreaGuardThenRestoresDefuse() ||
		!testDistanceTieUsesStableActorKeyAndOneOwner() ||
		!testDeadAndWrongTeamActorsCannotRetrieveDroppedBomb() ||
		!testCarryingTerroristOwnsPlantInsteadOfDroppedPickup() ||
		!testOwnerIsRetainedUntilItBecomesInvalid() ||
		!testUnavailablePositionCannotBecomeOriginDistance() ||
		!testPathFailureReassignsDroppedC4ToAnotherReachableActor())
	{
		return 1;
	}
	std::puts("round objective coordinator: PASS");
	return 0;
}
