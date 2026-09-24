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
	astrabot::nav::NavArea first = area(1U, 0.0f, 128.0f);
	astrabot::nav::NavArea second = area(2U, 128.0f, 256.0f);
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
	astrabot::compat::ScriptedRandomSource randomSource({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
	controller.setRandomSource(&randomSource);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {64.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.landingConfirmed = false;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	const auto firstResult = controller.update(
		publisher.snapshot(), observation, &intent, &decision);
	if (!check(firstResult == astrabot::runtime::NavRoamResult::IntentReady,
			"jump launch obtains a route intent"))
	{
		return false;
	}
	if (!check(intent.traversal == astrabot::nav::TraversalAction::Walk,
			"controller walks to the launch portal before jumping"))
	{
		return false;
	}

	observation.frame.tick = 2U;
	observation.locomotion.position = {112.0f, 32.0f, 0.0f};
	if (!check(controller.update(publisher.snapshot(), observation, &intent) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			intent.traversal == astrabot::nav::TraversalAction::Jump,
			"jump is emitted after reaching the launch portal"))
	{
		return false;
	}

	observation.frame.tick = 3U;
	return check(controller.update(publisher.snapshot(), observation, &intent) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			intent.traversal == astrabot::nav::TraversalAction::Walk,
			"jump button is released while traversal feedback is pending") &&
		check(randomSource.verifyComplete(),
			"small-area fallback consumes one reference goal draw");
}

bool testNormalRoamUsesRunSpeed()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	first.connections[1U].push_back(2U);
	document.addArea(first);
	document.addArea(second);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
				astrabot::nav::NavSnapshotResult::Published,
				"run-speed snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::compat::ScriptedRandomSource randomSource({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
	controller.setRandomSource(&randomSource);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {96.0f, 32.0f, 0.0f};
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				astrabot::runtime::NavRoamResult::IntentReady,
				"run-speed roam produces an intent"))
	{
		return false;
	}
	return check(intent.speed >= 200.0f,
				"normal roam uses a run-speed movement intent");
}

bool testDropTraversalWaitsForLaunchPortal()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto launchArea = area(1U, 0.0f, 256.0f);
	auto landingArea = area(2U, 256.0f, 512.0f);
	auto goalArea = area(3U, 512.0f, 768.0f);
	launchArea.extent.lo.z = 100.0f;
	launchArea.extent.hi.z = 100.0f;
	launchArea.northEastZ = 100.0f;
	launchArea.southWestZ = 100.0f;
	launchArea.connections[1U].push_back(2U);
	landingArea.connections[1U].push_back(3U);
	document.addArea(launchArea);
	document.addArea(landingArea);
	document.addArea(goalArea);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"drop portal snapshot is published"))
	{
		return false;
	}

	const auto landingPortal = astrabot::nav::portalSteeringPointForLink(
		launchArea, landingArea, {128.0f, 32.0f, 100.0f}, 1U);
	const auto launchPortal = astrabot::nav::portalSteeringPointForLink(
		landingArea, launchArea, landingPortal, 3U);
	if (!check(std::fabs(landingPortal.x - 272.0f) < 0.01f &&
			std::fabs(launchPortal.x - 240.0f) < 0.01f,
			"directed portal points retain 16 units on each side of the link"))
	{
		return false;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {128.0f, 32.0f, 100.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.safeDropHeightAvailable = true;
	observation.locomotion.maximumSafeDropHeight = 156.25f;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {640.0f, 32.0f, 0.0f};
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			intent.traversal == astrabot::nav::TraversalAction::Walk,
			"controller approaches safe drop portal before drop"))
	{
		return false;
	}

	observation.frame.tick = 2U;
	observation.locomotion.position = launchPortal;
	const auto dropResult = controller.update(
			publisher.snapshot(), observation, &intent, &decision);
	if (!check(dropResult == astrabot::runtime::NavRoamResult::IntentReady &&
			intent.traversal == astrabot::nav::TraversalAction::Drop &&
			intent.targetArea == 2U &&
			std::fabs(intent.speed - 240.0f) < 0.01f,
			"safe drop starts at the portal without Jump at run speed"))
	{
		return false;
	}

	for (std::uint32_t tick = 3U; tick < 15U; ++tick)
	{
		observation.frame.tick = tick;
		observation.airborne = true;
		observation.locomotion.grounded = false;
		if (!check(controller.update(
				publisher.snapshot(), observation, &intent, &decision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
				intent.traversal == astrabot::nav::TraversalAction::Walk &&
				std::fabs(intent.speed - 240.0f) < 0.01f,
				"safe drop remains active through the fall without Jump"))
		{
			return false;
		}
	}

	observation.frame.tick = 15U;
	observation.locomotion.position = landingPortal;
	observation.locomotion.grounded = true;
	observation.airborne = false;
	observation.landingConfirmed = true;
	observation.hasLandingDamage = true;
	observation.landingDamage = 0.0f;
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::ReplanRequired &&
			decision.locomotionResult == astrabot::nav::LocomotionResult::TargetReached &&
			decision.currentArea == 2U,
			"no-damage landing confirms the destination area"))
	{
		return false;
	}

	observation.frame.tick = 16U;
	observation.landingConfirmed = false;
	observation.hasLandingDamage = false;
	return check(controller.update(
			publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			decision.currentArea == 2U && intent.targetArea == 3U,
			"corridor advances from the landed area toward the retained goal");
}
bool testObjectiveChangeClearsFailedRoamLink()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto launchArea = area(1U, 0.0f, 256.0f);
	auto landingArea = area(2U, 256.0f, 512.0f);
	for (astrabot::nav::NavArea *candidate : {&launchArea, &landingArea})
	{
		candidate->extent.lo.y = 0.0f;
		candidate->extent.hi.y = 256.0f;
	}
	launchArea.extent.lo.z = 100.0f;
	launchArea.extent.hi.z = 100.0f;
	launchArea.northEastZ = 100.0f;
	launchArea.southWestZ = 100.0f;
	launchArea.connections[1U].push_back(2U);
	document.addArea(launchArea);
	document.addArea(landingArea);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"objective transition snapshot is published"))
	{
		return false;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavAreaVisitHistory visits;
	controller.setAreaVisitHistory(&visits);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 0U;
	observation.locomotion.position = {240.0f, 128.0f, 100.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::ReplanRequired &&
			decision.failureReason == astrabot::runtime::NavFailureReason::UnsafeDrop,
			"unknown-height roam drop fails with a typed unsafe result"))
	{
		return false;
	}

	observation.frame.tick = 2U;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {384.0f, 128.0f, 0.0f};
	observation.locomotion.safeDropHeightAvailable = true;
	observation.locomotion.maximumSafeDropHeight = 156.25f;
	return check(controller.update(
			publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			decision.goalKind == astrabot::runtime::NavGoalKind::Objective &&
			decision.goalArea == 2U &&
			intent.traversal == astrabot::nav::TraversalAction::Drop &&
			intent.targetArea == 2U,
			"a new BombTarget goal can reuse a link rejected by the previous roam goal");
}

bool testObjectiveStartFailureRetainsAttemptedLink()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 256.0f);
	auto landing = area(2U, 256.0f, 512.0f);
	for (astrabot::nav::NavArea *candidate : {&start, &landing})
	{
		candidate->extent.lo.y = 0.0f;
		candidate->extent.hi.y = 256.0f;
	}
	start.extent.lo.z = 100.0f;
	start.extent.hi.z = 100.0f;
	start.northEastZ = 100.0f;
	start.southWestZ = 100.0f;
	start.connections[1U].push_back(2U);
	document.addArea(start);
	document.addArea(landing);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"failed objective link snapshot is published"))
	{
		return false;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {240.0f, 128.0f, 100.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.safeDropHeightAvailable = true;
	observation.locomotion.maximumSafeDropHeight = 25.0f;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {384.0f, 128.0f, 0.0f};
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	const auto result = controller.update(
		publisher.snapshot(), observation, &intent, &decision);
	if (!check(result == astrabot::runtime::NavRoamResult::ReplanRequired &&
			decision.goalKind == astrabot::runtime::NavGoalKind::Objective &&
			decision.failureReason == astrabot::runtime::NavFailureReason::UnsafeDrop,
			"unsafe objective drop is rejected before movement"))
	{
		return false;
	}
	if (!check(decision.linkFromArea == 1U && decision.linkToArea == 2U,
			"objective start failure retains the rejected directed link"))
	{
		return false;
	}
	observation.frame.tick = 2U;
	const auto noAlternative = controller.update(
		publisher.snapshot(), observation, &intent, &decision);
	if (!check(noAlternative == astrabot::runtime::NavRoamResult::NoRoute,
			"objective route reports no available bypass for the failed link"))
	{
		return false;
	}
	return check(decision.linkFromArea == 1U && decision.linkToArea == 2U,
			"no-route diagnostics preserve the link that blocked the objective");
}

bool testPathFailureChoosesAnotherCompatibilityGoal()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 256.0f);
	auto failedGoal = area(2U, 256.0f, 512.0f);
	auto alternateGoal = area(3U, 0.0f, 256.0f);
	for (astrabot::nav::NavArea *candidate : {&start, &failedGoal, &alternateGoal})
	{
		candidate->extent.lo.y = 0.0f;
		candidate->extent.hi.y = 256.0f;
	}
	start.extent.lo.z = 100.0f;
	start.extent.hi.z = 100.0f;
	start.northEastZ = 100.0f;
	start.southWestZ = 100.0f;
	alternateGoal.extent.lo.z = 100.0f;
	alternateGoal.extent.hi.z = 100.0f;
	alternateGoal.northEastZ = 100.0f;
	alternateGoal.southWestZ = 100.0f;
	start.connections[1U].push_back(2U);
	start.connections[0U].push_back(3U);
	document.addArea(start);
	document.addArea(failedGoal);
	document.addArea(alternateGoal);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"failed-roam-link snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {240.0f, 128.0f, 100.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::ReplanRequired &&
			decision.failureReason == astrabot::runtime::NavFailureReason::UnsafeDrop &&
			decision.goalArea == 2U,
			"an unsafe roam drop is classified against its selected goal"))
	{
		return false;
	}

	observation.frame.tick = 2U;
	observation.locomotion.safeDropHeightAvailable = true;
	observation.locomotion.maximumSafeDropHeight = 156.25f;
	return check(controller.update(
			publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			decision.goalArea == 3U && intent.targetArea == 3U &&
			intent.traversal == astrabot::nav::TraversalAction::Walk,
			"path failure skips the failed oldest goal and chooses a different route");
}

bool testLadderTraversalAction()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto first = area(1U, 0.0f, 64.0f);
	auto second = area(2U, 64.0f, 128.0f);
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
	astrabot::compat::ScriptedRandomSource randomSource({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
	controller.setRandomSource(&randomSource);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {48.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.ladderContact = true;
	observation.entryConfirmed = true;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	const auto result = controller.update(
		publisher.snapshot(), observation, &intent, &decision);
	if (!check(result == astrabot::runtime::NavRoamResult::IntentReady,
			"ladder approach obtains a route intent"))
	{
		std::fprintf(stderr, "ladder result=%d failure=%d stage=%d path=%d goal=%u\\n",
			static_cast<int>(result), static_cast<int>(decision.failureReason),
			static_cast<int>(decision.stage), static_cast<int>(decision.pathResult),
			static_cast<unsigned int>(decision.goalArea));
		return false;
	}
	return check(intent.traversal == astrabot::nav::TraversalAction::Ladder,
			"ladder approach emits ladder traversal intent") &&
		check(randomSource.verifyComplete(),
			"ladder approach consumes the all-small-area fallback draw");
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
	astrabot::compat::ScriptedRandomSource randomSource({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1),
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 2U}, 0, 1, 1)});
	controller.setRandomSource(&randomSource);
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

bool testCompatibilityGoalReroutesAroundFailedFirstLink()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	const auto setArea = [](astrabot::nav::AreaId id, float loX, float hiX,
			float loY, float hiY, float z) {
		auto result = area(id, loX, hiX);
		result.extent.lo.y = loY;
		result.extent.hi.y = hiY;
		result.extent.lo.z = z;
		result.extent.hi.z = z;
		result.northEastZ = z;
		result.southWestZ = z;
		return result;
	};
	auto start = setArea(1U, 0.0f, 256.0f, 0.0f, 256.0f, 100.0f);
	auto failedBranch = setArea(2U, 256.0f, 512.0f, 0.0f, 256.0f, 0.0f);
	auto alternateBranch = setArea(3U, 0.0f, 64.0f, -64.0f, 0.0f, 100.0f);
	auto goal = setArea(4U, 512.0f, 768.0f, 0.0f, 256.0f, 0.0f);
	start.connections[1U].push_back(2U);
	start.connections[0U].push_back(3U);
	failedBranch.connections[0U].push_back(4U);
	alternateBranch.connections[0U].push_back(4U);
	for (const auto &candidate : {start, failedBranch, alternateBranch, goal})
	{
		document.addArea(candidate);
	}
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"failed-first-link fixture publishes"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller(
		astrabot::compat::RuntimeMode::Compatibility);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {240.0f, 128.0f, 100.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	const auto failure = controller.update(
		publisher.snapshot(), observation, &intent, &decision);
	if (!check(failure == astrabot::runtime::NavRoamResult::ReplanRequired,
			"unsafe first link requests a route replan"))
	{
		return false;
	}
	if (!check(decision.failureReason == astrabot::runtime::NavFailureReason::UnsafeDrop &&
			decision.goalArea == 2U,
			"unsafe first link is recorded as the failed roam link"))
	{
		return false;
	}
	if (!check(decision.linkFromArea == 1U && decision.linkToArea == 2U,
			"failed traversal diagnostics retain the attempted directed link"))
	{
		return false;
	}

	observation.frame.tick = 2U;
	observation.locomotion.safeDropHeightAvailable = true;
	observation.locomotion.maximumSafeDropHeight = 156.25f;
	const auto rerouted = controller.update(
		publisher.snapshot(), observation, &intent, &decision);
	return check(rerouted == astrabot::runtime::NavRoamResult::IntentReady,
			"Compatibility patrol finds a route after a failed link") &&
		check(decision.goalArea == 4U,
			"failed goal is replaced while retaining the oldest remaining goal") &&
		check(intent.targetArea == 3U &&
			intent.traversal == astrabot::nav::TraversalAction::Walk,
			"alternate route avoids the failed 1-to-2 link instead of retrying it");
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
	astrabot::compat::ScriptedRandomSource firstRandom({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 2, 1)});
	astrabot::compat::ScriptedRandomSource secondRandom({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {2U, 1U}, {0U, 0U, 1U}, 0, 2, 1)});
	firstController.setRandomSource(&firstRandom);
	secondController.setRandomSource(&secondRandom);
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
			"compatibility route selection is independent of actor-derived rotation") &&
		check(firstRandom.verifyComplete() && secondRandom.verifyComplete(),
			"small-area fallback consumes one bounded goal draw per actor");
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
	astrabot::compat::ScriptedRandomSource randomSource({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1),
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 2U}, 0, 1, 1)});
	controller.setRandomSource(&randomSource);
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
	astrabot::compat::ScriptedRandomSource randomSource({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
	controller.setRandomSource(&randomSource);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	const astrabot::runtime::NavRoamResult firstResult = controller.update(
		publisher.snapshot(), observation, &intent, &firstDecision);
	if (!check(
			firstResult == astrabot::runtime::NavRoamResult::IntentReady,
			"temporary loss starts with an active route"))
	{
		return false;
	}
	if (!check(firstDecision.pathSequence == 1U,
			"temporary loss route sequence starts at one"))
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

bool testPlannedGapAfterEarlierSegmentKeepsCurrentLink()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	astrabot::nav::NavArea third = area(3U, 153.0f, 217.0f);
	first.connections[1U].push_back(2U);
	second.connections[1U].push_back(3U);
	document.addArea(first);
	document.addArea(second);
	document.addArea(third);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
		"multi-segment gap snapshot is published"))
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
	observation.objectiveTarget = {185.0f, 32.0f, 0.0f};
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &firstDecision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			intent.currentArea == 1U && intent.targetArea == 2U,
		"multi-segment route starts on its first link"))
	{
		return false;
	}

	observation.frame.tick = 2U;
	observation.locomotion.position = {96.0f, 32.0f, 0.0f};
	astrabot::runtime::NavRoamDecision secondDecision = {};
	if (!check(controller.update(
			publisher.snapshot(), observation, &intent, &secondDecision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			intent.currentArea == 2U && intent.targetArea == 3U &&
			secondDecision.pathSequence == firstDecision.pathSequence,
		"locomotion advances to the later planned link without replanning"))
	{
		return false;
	}

	observation.frame.tick = 3U;
	observation.locomotion.position = {132.0f, 32.0f, 0.0f};
	observation.locomotion.grounded = false;
	observation.airborne = true;
	intent = {};
	astrabot::runtime::NavRoamDecision gapDecision = {};
	const astrabot::runtime::NavRoamResult gapResult = controller.update(
		publisher.snapshot(), observation, &intent, &gapDecision);
	return check(gapResult ==
			astrabot::runtime::NavRoamResult::IntentReady &&
		gapDecision.pathSequence == firstDecision.pathSequence &&
		intent.currentArea == 2U && intent.targetArea == 3U &&
		intent.direction.x > 0.0f && intent.targetPosition.x > 132.0f,
		"temporary gap on a later segment preserves its current forward intent");
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
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 3, 3)});
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
	observation.locomotion.position = {112.0f, 32.0f, 0.0f};
	const bool switched = controller.update(
		publisher.snapshot(), observation, &intent, &decision) ==
		astrabot::runtime::NavRoamResult::IntentReady &&
		intent.traversal == astrabot::nav::TraversalAction::Jump;
	return check(switched, "intermediate how=6 link starts Jump traversal");
}

bool testDecisionReportsActualLocalSteeringTarget()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	first.connections[1U].push_back(2U);
	document.addArea(first);
	document.addArea(second);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"local target snapshot is published"))
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
	observation.objectiveTarget = {96.0f, 32.0f, 0.0f};
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	return check(controller.update(
			publisher.snapshot(), observation, &intent, &decision) ==
			astrabot::runtime::NavRoamResult::IntentReady &&
			std::fabs(decision.targetPosition.x - 80.0f) < 0.01f &&
			std::fabs(decision.targetPosition.y - 32.0f) < 0.01f,
			"decision reports the active portal steering point instead of the final area center");
}

bool testCompatibilityGoalPrefersOldestLargeAreaAndSafestRoute()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto largeArea = [](astrabot::nav::AreaId id, float lowX, float highX,
			float lowY, float highY) {
		auto result = area(id, lowX, highX);
		result.extent.lo.y = lowY;
		result.extent.hi.y = highY;
		return result;
	};
	auto start = largeArea(1U, 0.0f, 200.0f, 0.0f, 200.0f);
	auto firstCandidate = largeArea(2U, 300.0f, 500.0f, 0.0f, 200.0f);
	auto firstLinkCandidate = largeArea(3U, 0.0f, 200.0f, 300.0f, 500.0f);
	start.connections[0U].push_back(2U);
	start.connections[1U].push_back(3U);
	document.addArea(start);
	document.addArea(firstCandidate);
	document.addArea(firstLinkCandidate);
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 5U) ==
			astrabot::nav::NavSnapshotResult::Published,
			"oldest-area fixture publishes"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller(
		astrabot::compat::RuntimeMode::Compatibility);
	astrabot::runtime::NavAreaVisitHistory visitHistory;
	visitHistory.record(5U, 1U, 2U, 20U);
	visitHistory.record(5U, 1U, 3U, 5U);
	controller.setAreaVisitHistory(&visitHistory);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {5U, 1U, 10U};
	observation.team = 1U;
	observation.locomotion.position = {10.0f, 10.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	const auto result = controller.update(
		publisher.snapshot(), observation, &intent, &decision);
	return check(result == astrabot::runtime::NavRoamResult::IntentReady,
			"Compatibility Hunt goal has a route") &&
		check(decision.goalArea == 3U,
			"oldest eligible area beats the first outgoing link") &&
		check(decision.routeType == astrabot::nav::NavRouteType::Safest,
			"Compatibility Hunt uses the safest route") &&
		check(decision.goalSelectionStrategy ==
				astrabot::runtime::GoalSelectionStrategyOldestVisitedArea,
			"large NAV candidates use oldest-visited selection");
}

int main()
{
	if (!testDecisionReportsActualLocalSteeringTarget())
	{
		return 1;
	}
	if (!testJumpTraversalAction())
	{
		return 1;
	}
	if (!testDropTraversalWaitsForLaunchPortal())
	{
		return 1;
	}
	if (!testObjectiveChangeClearsFailedRoamLink())
	{
		return 1;
	}
	if (!testObjectiveStartFailureRetainsAttemptedLink())
	{
		return 1;
	}
	if (!testPathFailureChoosesAnotherCompatibilityGoal())
	{
		return 1;
	}
	if (!testCompatibilityGoalReroutesAroundFailedFirstLink())
	{
		return 1;
	}
	if (!testNormalRoamUsesRunSpeed())
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
		!testTemporaryCurrentAreaLossRetainsActiveRoute() ||
		!testPlannedGapAfterEarlierSegmentKeepsCurrentLink())
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
	if (!testCompatibilityGoalPrefersOldestLargeAreaAndSafestRoute())
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
	astrabot::compat::ScriptedRandomSource randomSource({
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 2, 1),
		astrabot::compat::RandomTapeEntry::longEntry(
			"CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 3U}, 0, 2, 2)});
	controller.setRandomSource(&randomSource);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	const astrabot::runtime::NavRoamResult result = controller.update(
		publisher.snapshot(), observation, &intent, &decision);
	if (!check(result == astrabot::runtime::NavRoamResult::IntentReady,
			"roam controller produces an intent for its selected goal"))
	{
		return 1;
	}
	if (!check(decision.stage == astrabot::runtime::NavRoamStage::LocomotionReady,
			"roam controller reaches the locomotion stage"))
	{
		return 1;
	}
	if (!check(intent.targetArea == 2U && intent.direction.x > 0.0f,
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
	const astrabot::runtime::NavRoamResult nextResult =
		controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(nextResult == astrabot::runtime::NavRoamResult::IntentReady,
			"roam controller produces a new intent after target arrival"))
	{
		return 1;
	}
	if (!check(decision.goalGeneration == 2U,
			"goal arrival increments goal generation"))
	{
		return 1;
	}
	if (!check(decision.goalSelectionReason ==
			astrabot::runtime::GoalSelectionReached,
			"goal arrival reports the reselection reason"))
	{
		return 1;
	}
	if (!check(decision.goalSelectionStrategy ==
			astrabot::runtime::GoalSelectionStrategyRandomFallback,
			"small-area selection reports random fallback strategy"))
	{
		return 1;
	}
	if (!check(intent.targetArea == 3U,
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
