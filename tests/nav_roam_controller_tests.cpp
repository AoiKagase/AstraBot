#include "astrabot/runtime/nav_roam_controller.hpp"
#include "astrabot/compat/random_source.hpp"

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

astrabot::nav::NavArea area(
	astrabot::nav::AreaId id,
	float lowX,
	float highX)
{
	astrabot::nav::NavArea result = {};
	result.id = id;
	result.extent.lo = {lowX, 0.0f, 0.0f};
	result.extent.hi = {highX, 64.0f, 0.0f};
	result.northEastZ = 0.0f;
	result.southWestZ = 0.0f;
	return result;
}
}

bool testJumpTraversalAction()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	first.connections[0U].push_back(2U);
	first.approaches.push_back({1U, 0U, 2U, 0U, 6U});
	document.addArea(first);
	document.addArea(second);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				   astrabot::nav::NavSnapshotResult::Published,
			   "jump traversal snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.landingConfirmed = false;
	astrabot::nav::LocomotionIntent intent = {};
	return check(controller.update(publisher.snapshot(), observation, &intent) ==
					 astrabot::runtime::NavRoamResult::IntentReady &&
					 intent.traversal == astrabot::nav::TraversalAction::Jump,
				 "jump approach emits jump traversal intent");
}

bool testLadderTraversalAction()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	first.connections[0U].push_back(2U);
	first.approaches.push_back({1U, 0U, 2U, 0U, 4U});
	document.addArea(first);
	document.addArea(second);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				   astrabot::nav::NavSnapshotResult::Published,
			   "ladder traversal snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.ladderContact = true;
	observation.entryConfirmed = true;
	astrabot::nav::LocomotionIntent intent = {};
	return check(controller.update(publisher.snapshot(), observation, &intent) ==
					 astrabot::runtime::NavRoamResult::IntentReady &&
					 intent.traversal == astrabot::nav::TraversalAction::Ladder,
				 "ladder approach emits ladder traversal intent");
}

bool testStuckDecisionRetainsSelectedRoute()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	first.connections[0U].push_back(2U);
	document.addArea(first);
	document.addArea(second);

	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				   astrabot::nav::NavSnapshotResult::Published,
			   "stuck diagnostics snapshot is published"))
	{
		return false;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &decision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
				intent.targetArea == 2U,
				"stuck diagnostic route starts with a target"))
	{
		return false;
	}

	for (std::uint32_t tick = 2U; tick <= 9U; ++tick)
	{
		observation.frame.tick = tick;
		if (tick < 9U && !check(controller.update(
				publisher.snapshot(), observation, &intent, &decision) ==
					astrabot::runtime::NavRoamResult::IntentReady,
				"unchanged route remains active before the stuck limit"))
		{
			return false;
		}
		if (tick == 9U)
		{
			const astrabot::runtime::NavRoamResult result = controller.update(
				publisher.snapshot(), observation, &intent, &decision);
			if (!check(result == astrabot::runtime::NavRoamResult::IntentReady &&
					decision.locomotionResult ==
					astrabot::nav::LocomotionResult::Stuck &&
					std::hypot(intent.direction.x, intent.direction.y) > 0.9f &&
					decision.targetArea == 2U &&
					decision.targetPosition.x == 96.0f &&
					decision.observationPosition.x == 32.0f &&
					decision.corridorAreaCount == 2U &&
			decision.corridorIndex == 1U &&
					decision.linkFromArea == 1U &&
					decision.linkToArea == 2U &&
					controller.isActive(),
				"stuck emits bounded lateral recovery while retaining route diagnostics"))
			{
				return false;
			}
		}
		if (tick == 9U)
		{
		observation.frame.tick = 10U;
		if (!check(controller.update(
				publisher.snapshot(), observation, &intent, &decision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
					intent.targetArea == 2U,
				"following frame selects a route after stuck replan"))
		{
			return false;
		}
		}
	}
	return true;
}

bool testObjectiveTargetSelectsGoalCorridor()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea distractor = area(2U, 64.0f, 128.0f);
	astrabot::nav::NavArea goal = area(3U, 128.0f, 192.0f);
	first.connections[0U].push_back(2U);
	first.connections[0U].push_back(3U);
	document.addArea(first);
	document.addArea(distractor);
	document.addArea(goal);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				astrabot::nav::NavSnapshotResult::Published,
			"objective target snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {160.0f, 32.0f, 0.0f};
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	return check(controller.update(
				publisher.snapshot(), observation, &intent, &decision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				intent.targetArea == 3U && decision.targetArea == 3U &&
				decision.linkToArea == 3U,
			"objective target selects its goal corridor instead of random roam");
}

bool testActorSeedDistributesInitialLinks()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	first.connections[0U].push_back(2U);
	first.connections[0U].push_back(3U);
	first.connections[0U].push_back(4U);
	document.addArea(first);
	document.addArea(area(2U, 64.0f, 128.0f));
	document.addArea(area(3U, 128.0f, 192.0f));
	document.addArea(area(4U, 192.0f, 256.0f));
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				astrabot::nav::NavSnapshotResult::Published,
			"actor seed snapshot is published"))
	{
		return false;
	}
	const auto makeObservation = [](std::uint32_t slot) {
		astrabot::runtime::NavRoamObservation observation = {};
		observation.actor = {slot, 1U};
		observation.frame = {1U, 1U, 1U};
		observation.locomotion.position = {32.0f, 32.0f, 0.0f};
		observation.locomotion.standingClearance = 72.0f;
		observation.locomotion.crouchingClearance = 36.0f;
		observation.locomotion.grounded = true;
		return observation;
	};
	astrabot::runtime::NavRoamController firstController(
			astrabot::compat::RuntimeMode::Enhanced);
	astrabot::runtime::NavRoamController secondController(
			astrabot::compat::RuntimeMode::Enhanced);
	astrabot::nav::LocomotionIntent firstIntent = {};
	astrabot::nav::LocomotionIntent secondIntent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	astrabot::runtime::NavRoamDecision secondDecision = {};
	return check(
				firstController.update(publisher.snapshot(), makeObservation(1U), &firstIntent, &firstDecision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				secondController.update(publisher.snapshot(), makeObservation(2U), &secondIntent, &secondDecision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
					firstDecision.targetPosition.x != secondDecision.targetPosition.x,
				"actor identity distributes initial outgoing links");
}

bool testNormalRoamUsesActorSeededAStarGoal()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	astrabot::nav::NavArea distractor = area(3U, -192.0f, -128.0f);
	astrabot::nav::NavArea goal = area(4U, 128.0f, 192.0f);
	first.connections[0U].push_back(2U);
	second.connections[0U].push_back(4U);
	document.addArea(first);
	document.addArea(second);
	document.addArea(distractor);
	document.addArea(goal);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				astrabot::nav::NavSnapshotResult::Published,
			"normal roam goal snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller(
			astrabot::compat::RuntimeMode::Enhanced);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {3U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	return check(controller.update(
				publisher.snapshot(), observation, &intent, &decision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				decision.corridorAreaCount == 3U && decision.linkToArea == 2U,
				"normal roam uses an actor-seeded A* goal instead of only the next link");
}

bool testCompatibilityIgnoresActorSeededRouteRotation()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 64.0f);
	start.connections[0U].push_back(2U);
	start.connections[0U].push_back(3U);
	document.addArea(start);
	document.addArea(area(2U, 64.0f, 128.0f));
	document.addArea(area(3U, 128.0f, 192.0f));

	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				astrabot::nav::NavSnapshotResult::Published,
			"compatibility route policy snapshot is published"))
	{
		return false;
	}
	const auto makeObservation = [](std::uint32_t slot) {
		astrabot::runtime::NavRoamObservation observation = {};
		observation.actor = {slot, 1U};
		observation.frame = {1U, 1U, 1U};
		observation.locomotion.position = {32.0f, 32.0f, 0.0f};
		observation.locomotion.standingClearance = 72.0f;
		observation.locomotion.crouchingClearance = 36.0f;
		observation.locomotion.grounded = true;
		return observation;
	};
	astrabot::runtime::NavRoamController firstController(
			astrabot::compat::RuntimeMode::Compatibility);
	astrabot::runtime::NavRoamController secondController(
			astrabot::compat::RuntimeMode::Compatibility);
	astrabot::nav::LocomotionIntent firstIntent = {};
	astrabot::nav::LocomotionIntent secondIntent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	astrabot::runtime::NavRoamDecision secondDecision = {};
	if (!check(firstController.update(
				publisher.snapshot(), makeObservation(1U), &firstIntent, &firstDecision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
				secondController.update(
						publisher.snapshot(), makeObservation(2U), &secondIntent,
						&secondDecision) == astrabot::runtime::NavRoamResult::IntentReady,
				"compatibility controllers emit route intents"))
	{
		return false;
	}
	return check(firstDecision.linkToArea == 2U && secondDecision.linkToArea == 2U,
			"compatibility route selection is independent of actor-derived rotation");
}

bool testRoutePersistsUntilGoalChange()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea middle = area(2U, 64.0f, 128.0f);
	astrabot::nav::NavArea firstGoal = area(3U, 128.0f, 192.0f);
	astrabot::nav::NavArea secondGoal = area(4U, 192.0f, 256.0f);
	start.connections[0U].push_back(2U);
	start.connections[0U].push_back(4U);
	middle.connections[0U].push_back(3U);
	document.addArea(start);
	document.addArea(middle);
	document.addArea(firstGoal);
	document.addArea(secondGoal);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				astrabot::nav::NavSnapshotResult::Published,
			"route persistence snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {160.0f, 32.0f, 0.0f};
	astrabot::runtime::NavRoamController controller(
			astrabot::compat::RuntimeMode::Compatibility);
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &firstDecision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
				firstDecision.recomputeReason ==
						astrabot::runtime::NavRecomputeReason::InitialGoal &&
				firstDecision.pathSequence == 1U,
				"initial goal computes one persistent route"))
	{
		return false;
	}
	observation.frame.tick = 2U;
	astrabot::runtime::NavRoamDecision persistentDecision = {};
	if (!check(controller.update(
				publisher.snapshot(), observation, &intent, &persistentDecision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
				persistentDecision.recomputeReason ==
						astrabot::runtime::NavRecomputeReason::None &&
				persistentDecision.pathSequence == 1U,
				"unchanged goal keeps the existing route across updates"))
	{
		return false;
	}
	observation.frame.tick = 3U;
	observation.objectiveTarget = {224.0f, 32.0f, 0.0f};
	astrabot::runtime::NavRoamDecision changedDecision = {};
	return check(controller.update(
				publisher.snapshot(), observation, &intent, &changedDecision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
				changedDecision.recomputeReason ==
						astrabot::runtime::NavRecomputeReason::GoalChanged &&
				changedDecision.pathSequence == 2U &&
				changedDecision.targetArea == 4U,
			"goal change is the explicit route recompute trigger");
}

bool testRouteRecomputesOnMapGenerationChange()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	first.connections[0U].push_back(2U);
	document.addArea(first);
	document.addArea(second);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"map-change initial snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision initial = {};
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &initial) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			initial.pathSequence == 1U,
			"map-change fixture starts one route"))
	{
		return false;
	}

	if (!check(publisher.publish(&document, 2U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"map-change replacement snapshot is published"))
	{
		return false;
	}
	observation.frame = {2U, 1U, 2U};
	astrabot::runtime::NavRoamDecision changed = {};
	return check(controller.update(
			publisher.snapshot(), observation, &intent, &changed) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			changed.recomputeReason ==
				astrabot::runtime::NavRecomputeReason::MapOrRoundChanged &&
			changed.pathSequence == 2U,
			"map generation change forces one route recompute");
}

bool testGoalAndPathFailuresAreTyped()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	document.addArea(area(1U, 0.0f, 64.0f));
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"failure reason snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::NoRoute &&
			decision.failureReason == astrabot::runtime::NavFailureReason::NoGoal,
			"no outgoing goal is not reported as path search failure"))
	{
		return false;
	}

	astrabot::runtime::NavRoamController invalidGoalController;
	observation.frame.tick = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {100000.0f, 100000.0f, 0.0f};
	decision = {};
	if (!check(invalidGoalController.update(
			publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::NoRoute &&
			decision.failureReason == astrabot::runtime::NavFailureReason::GoalAreaMissing,
			"unresolvable objective goal is distinguished from path failure"))
	{
		return false;
	}

	return true;
}

bool testTemporaryCurrentAreaLossRetainsActiveRoute()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	first.connections[0U].push_back(2U);
	document.addArea(first);
	document.addArea(area(2U, 64.0f, 128.0f));
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"temporary loss snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &firstDecision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			firstDecision.pathSequence == 1U,
			"temporary loss starts with an active route"))
	{
		return false;
	}
	observation.frame.tick = 2U;
	observation.locomotion.position = {50000.0f, 50000.0f, 0.0f};
	astrabot::runtime::NavRoamDecision lostDecision = {};
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &lostDecision) ==
			astrabot::runtime::NavRoamResult::NoRoute &&
			lostDecision.failureReason == astrabot::runtime::NavFailureReason::CurrentAreaMissing &&
			lostDecision.pathSequence == 1U,
			"temporary current-area loss retains the active route identity"))
	{
		return false;
	}
	return true;
}

bool testResourceLimitFailureIsBackedOff()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	for (astrabot::nav::AreaId id = 1U; id <= 300U; ++id)
	{
		astrabot::nav::NavArea node = area(
			id, static_cast<float>((id - 1U) * 32U), static_cast<float>(id * 32U));
		if (id < 300U)
		{
			node.connections[0U].push_back(id + 1U);
		}
		document.addArea(node);
	}
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
		"ResourceLimit backoff snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {16.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {9584.0f, 32.0f, 0.0f};
	observation.collectPathStats = true;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision first = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &first) ==
			astrabot::runtime::NavRoamResult::NoRoute &&
		first.pathResult == astrabot::nav::NavQueryResult::ResourceLimit &&
		first.pathSearchStats.searchCalls > 0U,
		"ResourceLimit request is measured on first search"))
	{
		return false;
	}
	observation.frame.tick = 2U;
	astrabot::runtime::NavRoamDecision second = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &second) ==
			astrabot::runtime::NavRoamResult::NoRoute &&
		second.failureReason == astrabot::runtime::NavFailureReason::PathSearchFailed &&
		second.pathSearchStats.searchCalls == 0U,
		"identical ResourceLimit request is backed off"))
	{
		return false;
	}
	return true;
}

bool testCompatibilityGoalSelectionUsesRngBoundary()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea branchA = area(2U, 64.0f, 128.0f);
	astrabot::nav::NavArea branchB = area(3U, 128.0f, 192.0f);
	astrabot::nav::NavArea goal = area(4U, 192.0f, 256.0f);
	start.connections[0U].push_back(2U);
	start.connections[0U].push_back(3U);
	branchA.connections[0U].push_back(4U);
	branchB.connections[0U].push_back(4U);
	document.addArea(start);
	document.addArea(branchA);
	document.addArea(branchB);
	document.addArea(goal);
	astrabot::compat::ScriptedRandomSource randomSource({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 2, 2)});
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				astrabot::nav::NavSnapshotResult::Published,
				"goal selector snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller(
		astrabot::compat::RuntimeMode::Compatibility);
	controller.setRandomSource(&randomSource);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	const bool selected = controller.update(publisher.snapshot(), observation, &intent, &decision) ==
		astrabot::runtime::NavRoamResult::IntentReady &&
		decision.goalKind == astrabot::runtime::NavGoalKind::Roam &&
		decision.goalArea == 4U && randomSource.position() == 1U;
	if (!selected) return false;
	return check(randomSource.verifyComplete(),
				"Compatibility Goal selection consumes the expected RNG tape");
}

bool testTraversalSwitchesOnIntermediateJumpLink()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea middle = area(2U, 64.0f, 128.0f);
	astrabot::nav::NavArea goal = area(3U, 128.0f, 192.0f);
	first.connections[0U].push_back(2U);
	middle.connections[0U].push_back(3U);
	middle.approaches.push_back({2U, 1U, 3U, 0U, 6U});
	document.addArea(first);
	document.addArea(middle);
	document.addArea(goal);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				astrabot::nav::NavSnapshotResult::Published,
				"intermediate jump-link snapshot is published"))
	{
		return false;
	}
	astrabot::nav::NavQuery query(publisher.snapshot());
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 3U, &corridor) ==
				astrabot::nav::NavQueryResult::Found && corridor.links.size() == 2U &&
				corridor.links[1U].how == 6U,
				"intermediate jump-link metadata is retained"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {160.0f, 32.0f, 0.0f};
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
				intent.traversal == astrabot::nav::TraversalAction::Walk,
				"initial corridor link remains ordinary walking"))
	{
		return false;
	}
	observation.frame.tick = 2U;
	observation.locomotion.position = {96.0f, 32.0f, 0.0f};
	const bool switched = controller.update(
		publisher.snapshot(), observation, &intent, &decision) ==
		astrabot::runtime::NavRoamResult::IntentReady &&
		intent.traversal == astrabot::nav::TraversalAction::Jump;
	return check(switched, "intermediate how=6 link starts Jump traversal");
}

int main()
{
	if (!testJumpTraversalAction())
	{
		return 1;
	}
	if (!testLadderTraversalAction())
	{
		return 1;
	}
	if (!testObjectiveTargetSelectsGoalCorridor())
	{
		return 1;
	}
	if (!testActorSeedDistributesInitialLinks())
	{
		return 1;
	}
	if (!testNormalRoamUsesActorSeededAStarGoal())
	{
		return 1;
	}
	if (!testCompatibilityIgnoresActorSeededRouteRotation())
	{
		return 1;
	}
	if (!testRoutePersistsUntilGoalChange())
	{
		return 1;
	}
	if (!testRouteRecomputesOnMapGenerationChange())
	{
		return 1;
	}
	if (!testGoalAndPathFailuresAreTyped() ||
			!testTemporaryCurrentAreaLossRetainsActiveRoute())
	{
		return 1;
	}
	if (!testStuckDecisionRetainsSelectedRoute())
	{
		return 1;
	}
	if (!testResourceLimitFailureIsBackedOff())
	{
		return 1;
	}
	if (!testCompatibilityGoalSelectionUsesRngBoundary())
	{
		return 1;
	}
	if (!testTraversalSwitchesOnIntermediateJumpLink())
	{
		return 1;
	}
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	astrabot::nav::NavArea third = area(3U, 128.0f, 192.0f);
	first.connections[0U].push_back(2U);
	second.connections[0U].push_back(3U);
	document.addArea(first);
	document.addArea(second);
	document.addArea(third);

	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"roam snapshot is published"))
	{
		return 1;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			decision.stage == astrabot::runtime::NavRoamStage::LocomotionReady &&
			intent.targetArea == 2U && intent.direction.x > 0.0f,
			"roam controller produces the first directed movement intent"))
	{
		return 1;
	}

	if (!check(controller.update(publisher.snapshot(), observation, &intent) ==
			astrabot::runtime::NavRoamResult::DuplicateFrame,
			"duplicate roam frame is rejected"))
	{
		return 1;
	}

	observation.frame.tick = 2U;
	observation.locomotion.position = {96.0f, 32.0f, 0.0f};
	if (!check(controller.update(publisher.snapshot(), observation, &intent) ==
			astrabot::runtime::NavRoamResult::TargetReached,
			"reaching the directed target retires the route"))
	{
		return 1;
	}

	observation.frame.tick = 3U;
	if (!check(controller.update(publisher.snapshot(), observation, &intent) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			intent.targetArea == 3U,
			"roam controller replans from the new area"))
	{
		return 1;
	}

	astrabot::runtime::NavRoamController recoveryController;
	astrabot::runtime::NavRoamObservation recoveryObservation = observation;
	recoveryObservation.frame.tick = 1U;
	recoveryObservation.locomotion.position = {-512.0f, 32.0f, 0.0f};
	astrabot::runtime::NavRoamDecision recoveryDecision = {};
	if (!check(recoveryController.update(
			publisher.snapshot(), recoveryObservation, &intent, &recoveryDecision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			recoveryDecision.stage == astrabot::runtime::NavRoamStage::OffMeshRecovery &&
			recoveryDecision.recoveryArea == 1U && intent.targetArea == 1U,
			"nearby position recovers to the nearest roam area"))
	{
		return 1;
	}

	observation.frame.tick = 2U;
	if (!check(controller.update(publisher.snapshot(), observation, &intent) ==
			astrabot::runtime::NavRoamResult::StaleFrame,
			"older roam frame is rejected"))
	{
		return 1;
	}
	return 0;
}
