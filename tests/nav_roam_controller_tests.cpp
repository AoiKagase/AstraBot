#include "astrabot/runtime/nav_roam_controller.hpp"
#include "astrabot/compat/random_source.hpp"
#include "astrabot/team/round_objective_coordinator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

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

astrabot::nav::NavArea area(astrabot::nav::AreaId id, float lowX, float highX)
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

bool testGroundLookaheadRouteIsScopedToTheNextMatchingFrame()
{
	using namespace astrabot;
	runtime::GroundLookaheadRoute route = {};
	route.actor = {3U, 11U};
	route.frame = {5U, 7U, 100U};
	route.fullUpdateSequence = 8U;
	route.createdAt = 10.0f;
	route.targetArea = 42U;
	route.direction = {1.0f, 0.0f, 0.0f};
	route.running = true;
	route.active = true;
	const runtime::ActorId actor = {3U, 11U};
	bool passed = true;
	// Observed pre-ascent gaps were 2..14 server frames, never one.
	for (std::uint32_t gap = 1U; gap <= 14U; ++gap)
	{
		passed = check(route.matchesNextUpdate(actor, {5U, 7U, 100U + gap}, 9U, 10.1f),
					 "fresh route is usable at the next full update independent of server frame gap") &&
			passed;
	}
	passed = check(!route.matchesNextUpdate({3U, 12U}, {5U, 7U, 112U}, 9U, 10.1f) &&
					 !route.matchesNextUpdate({4U, 11U}, {5U, 7U, 112U}, 9U, 10.1f) &&
					 !route.matchesNextUpdate(actor, {6U, 7U, 112U}, 9U, 10.1f) &&
					 !route.matchesNextUpdate(actor, {5U, 8U, 112U}, 9U, 10.1f) &&
					 !route.matchesNextUpdate(actor, {5U, 7U, 100U}, 9U, 10.1f) &&
					 !route.matchesNextUpdate(actor, {5U, 7U, 112U}, 8U, 10.1f) &&
					 !route.matchesNextUpdate(actor, {5U, 7U, 112U}, 10U, 10.1f) &&
					 !route.matchesNextUpdate(actor, {5U, 7U, 112U}, 9U, 10.3f) &&
					 !route.matchesNextUpdate(actor, {5U, 7U, 112U}, 9U, 9.9f),
				 "scope rejects actor/map/round, duplicate frame/update, skipped update, old age and clock reversal") &&
		passed;
	passed = check(route.matchesNextUpdate(actor, {5U, 7U, 112U}, 9U, 10.25f) &&
					 !route.matchesNextUpdate(actor, {5U, 7U, 112U}, 9U, 10.2501f) &&
					 !route.matchesNextUpdate(actor, {5U, 7U, 112U}, 9U, std::numeric_limits<float>::quiet_NaN()) &&
					 !route.matchesNextUpdate(actor, {5U, 7U, 112U}, 9U, std::numeric_limits<float>::infinity()),
				 "route freshness includes .25 boundary and rejects non-finite clocks") &&
		passed;
	route.fullUpdateSequence = (std::numeric_limits<std::uint32_t>::max)();
	passed = check(route.matchesNextUpdate(actor, {5U, 7U, 112U}, 0U, 10.1f) &&
					 !route.matchesNextUpdate(actor, {5U, 7U, 112U}, 1U, 10.1f),
				 "full update wrap accepts only the immediate successor") &&
		passed;
	route.fullUpdateSequence = 8U;
	route.targetArea = 0U;
	passed =
		check(!route.matchesNextUpdate(actor, {5U, 7U, 112U}, 9U, 10.1f), "route without a target cannot be sampled") &&
		passed;
	route.targetArea = 42U;
	route.direction = {0.02f, 0.0f, -1.0f};
	passed = check(!route.matchesNextUpdate(actor, {5U, 7U, 112U}, 9U, 10.1f),
				 "off-mesh direction cannot become a ground-probe route") &&
		passed;
	route.reset();
	return check(!route.matchesNextUpdate(actor, {5U, 7U, 112U}, 9U, 10.1f), "reset expires the route") && passed;
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"jump traversal snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::compat::ScriptedRandomSource randomSource(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
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
	const auto firstResult = controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(firstResult == astrabot::runtime::NavRoamResult::IntentReady, "jump launch obtains a route intent"))
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
		check(randomSource.verifyComplete(), "small-area fallback consumes one reference goal draw");
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"run-speed snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::compat::ScriptedRandomSource randomSource(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
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
	return check(intent.speed >= 200.0f, "normal roam uses a run-speed movement intent");
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"drop portal snapshot is published"))
	{
		return false;
	}

	const auto landingPortal =
		astrabot::nav::portalSteeringPointForLink(launchArea, landingArea, {128.0f, 32.0f, 100.0f}, 1U);
	const auto launchPortal = astrabot::nav::portalSteeringPointForLink(landingArea, launchArea, landingPortal, 3U);
	if (!check(std::fabs(landingPortal.x - 272.0f) < 0.01f && std::fabs(launchPortal.x - 240.0f) < 0.01f,
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
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				intent.traversal == astrabot::nav::TraversalAction::Walk,
			"controller approaches safe drop portal before drop"))
	{
		return false;
	}

	observation.frame.tick = 2U;
	observation.locomotion.position = launchPortal;
	const auto dropResult = controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(dropResult == astrabot::runtime::NavRoamResult::IntentReady &&
				intent.traversal == astrabot::nav::TraversalAction::Drop && intent.targetArea == 2U &&
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
		if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
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
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
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
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"objective transition snapshot is published"))
	{
		return false;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavAreaVisitHistory visits;
	controller.setAreaVisitHistory(&visits);
	astrabot::compat::ScriptedRandomSource huntRandom(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
	controller.setRandomSource(&huntRandom);
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
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
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
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
			decision.goalKind == astrabot::runtime::NavGoalKind::Objective && decision.goalArea == 2U &&
			intent.traversal == astrabot::nav::TraversalAction::Drop && intent.targetArea == 2U,
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"failed objective link snapshot is published"))
	{
		return false;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.collectPathStats = true;
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
	const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
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
	const auto noAlternative = controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(noAlternative == astrabot::runtime::NavRoamResult::NoRoute,
			"objective route reports no available bypass for the failed link"))
	{
		return false;
	}
	if (!check(decision.linkFromArea == 1U && decision.linkToArea == 2U,
			"no-route diagnostics preserve the link that blocked the objective"))
		return false;
	// Coordinator handoff may temporarily replace the objective with Hunt.
	for (std::uint32_t tick = 3U; tick < 100U; ++tick)
	{
		observation.frame.tick = tick;
		observation.hasObjectiveTarget = (tick % 2U) == 0U;
		controller.update(publisher.snapshot(), observation, &intent, &decision);
		if (observation.hasObjectiveTarget &&
			!check(decision.pathSearchStats.searchCalls == 0U && intent.speed == 0.0f,
				"same unsafe objective does not restart A* across Hunt reassignment"))
			return false;
	}
	return true;
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"failed-roam-link snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamReservationBoard reservationBoard;
	reservationBoard.reset(1U, 1U);
	controller.setReservationBoard(&reservationBoard);
	astrabot::compat::ScriptedRandomSource huntRandom(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 2, 1),
			astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 2U}, 0, 2, 2)});
	controller.setRandomSource(&huntRandom);
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
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
					astrabot::runtime::NavRoamResult::ReplanRequired &&
				decision.failureReason == astrabot::runtime::NavFailureReason::UnsafeDrop && decision.goalArea == 2U &&
				!reservationBoard.goalReservedByOther(1U, {2U, 1U}, 2U),
			"an unsafe roam drop is classified against its selected goal"))
	{
		return false;
	}

	observation.frame.tick = 2U;
	observation.locomotion.safeDropHeightAvailable = true;
	observation.locomotion.maximumSafeDropHeight = 156.25f;
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
			decision.goalArea == 3U && intent.targetArea == 3U &&
			intent.traversal == astrabot::nav::TraversalAction::Walk &&
			!reservationBoard.goalReservedByOther(1U, {2U, 1U}, 3U),
		"path failure consumes the next goal draw and chooses its different route");
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"ladder traversal snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::compat::ScriptedRandomSource randomSource(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
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
	const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(result == astrabot::runtime::NavRoamResult::IntentReady, "ladder approach obtains a route intent"))
	{
		std::fprintf(stderr, "ladder result=%d failure=%d stage=%d path=%d goal=%u\\n", static_cast<int>(result),
			static_cast<int>(decision.failureReason), static_cast<int>(decision.stage),
			static_cast<int>(decision.pathResult), static_cast<unsigned int>(decision.goalArea));
		return false;
	}
	return check(intent.traversal == astrabot::nav::TraversalAction::Ladder,
			   "ladder approach emits ladder traversal intent") &&
		check(randomSource.verifyComplete(), "ladder approach consumes the all-small-area fallback draw");
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"stuck diagnostics snapshot is published"))
	{
		return false;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::compat::ScriptedRandomSource randomSource(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1),
			astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 2U}, 0, 1, 1)});
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
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				intent.targetArea == 2U,
			"stuck diagnostic route starts with a target"))
	{
		return false;
	}

	for (std::uint32_t tick = 2U; tick <= 9U; ++tick)
	{
		observation.frame.tick = tick;
		if (tick < 9U &&
			!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
					astrabot::runtime::NavRoamResult::IntentReady,
				"unchanged route remains active before the stuck limit"))
		{
			return false;
		}
		if (tick == 9U)
		{
			const astrabot::runtime::NavRoamResult result =
				controller.update(publisher.snapshot(), observation, &intent, &decision);
			if (!check(result == astrabot::runtime::NavRoamResult::IntentReady &&
						decision.locomotionResult == astrabot::nav::LocomotionResult::Stuck &&
						std::hypot(intent.direction.x, intent.direction.y) > 0.9f && decision.targetArea == 2U &&
						decision.targetPosition.x == 96.0f && decision.observationPosition.x == 32.0f &&
						decision.corridorAreaCount == 2U && decision.corridorIndex == 1U &&
						decision.linkFromArea == 1U && decision.linkToArea == 2U && controller.isActive(),
					"stuck emits bounded lateral recovery while retaining route diagnostics"))
			{
				return false;
			}
		}
		if (tick == 9U)
		{
			observation.frame.tick = 10U;
			if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
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

bool testFreezeMovementSuppressionPreservesObjectiveRoute()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f);
	first.connections[0U].push_back(2U);
	document.addArea(first);
	document.addArea(second);

	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"freeze-suppression NAV snapshot is published"))
	{
		return false;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {96.0f, 32.0f, 0.0f};
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				decision.goalKind == astrabot::runtime::NavGoalKind::Objective && decision.goalArea == 2U &&
				decision.linkFromArea == 1U && decision.linkToArea == 2U && controller.isActive(),
			"C4 carrier starts with its valid objective link"))
	{
		return false;
	}

	observation.movementSuppressed = true;
	for (std::uint32_t tick = 2U; tick < 160U; ++tick)
	{
		observation.frame.tick = tick;
		const astrabot::runtime::NavRoamResult result =
			controller.update(publisher.snapshot(), observation, &intent, &decision);
		if (!check(result == astrabot::runtime::NavRoamResult::MovementSuppressed &&
					decision.stage == astrabot::runtime::NavRoamStage::MovementSuppressed &&
					decision.failureReason == astrabot::runtime::NavFailureReason::None &&
					decision.goalKind == astrabot::runtime::NavGoalKind::Objective && decision.goalArea == 2U &&
					decision.linkFromArea == 1U && decision.linkToArea == 2U && controller.isActive(),
				"freeze preserves the C4 route without spending recovery frames"))
		{
			return false;
		}
	}

	observation.movementSuppressed = false;
	observation.frame.tick = 160U;
	const astrabot::runtime::NavRoamResult resumed =
		controller.update(publisher.snapshot(), observation, &intent, &decision);
	return check(resumed == astrabot::runtime::NavRoamResult::IntentReady &&
			decision.goalKind == astrabot::runtime::NavGoalKind::Objective && decision.goalArea == 2U &&
			decision.linkFromArea == 1U && decision.linkToArea == 2U && controller.isActive() &&
			std::hypot(intent.direction.x, intent.direction.y) > 0.5f,
		"C4 carrier resumes its saved route when movement is allowed");
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
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
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
			intent.targetArea == 3U && decision.targetArea == 3U && decision.linkToArea == 3U,
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"actor seed snapshot is published"))
	{
		return false;
	}
	const auto makeObservation = [](std::uint32_t slot)
	{
		astrabot::runtime::NavRoamObservation observation = {};
		observation.actor = {slot, 1U};
		observation.frame = {1U, 1U, 1U};
		observation.locomotion.position = {32.0f, 32.0f, 0.0f};
		observation.locomotion.standingClearance = 72.0f;
		observation.locomotion.crouchingClearance = 36.0f;
		observation.locomotion.grounded = true;
		return observation;
	};
	astrabot::runtime::NavRoamController firstController(astrabot::compat::RuntimeMode::Enhanced);
	astrabot::runtime::NavRoamController secondController(astrabot::compat::RuntimeMode::Enhanced);
	astrabot::nav::LocomotionIntent firstIntent = {};
	astrabot::nav::LocomotionIntent secondIntent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	astrabot::runtime::NavRoamDecision secondDecision = {};
	return check(firstController.update(publisher.snapshot(), makeObservation(1U), &firstIntent, &firstDecision) ==
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"normal roam goal snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller(astrabot::compat::RuntimeMode::Enhanced);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {3U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
			decision.corridorAreaCount == 3U && decision.linkToArea == 2U,
		"normal roam uses an actor-seeded A* goal instead of only the next link");
}

bool testCompatibilityGoalReroutesAroundFailedFirstLink()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	const auto setArea = [](astrabot::nav::AreaId id, float loX, float hiX, float loY, float hiY, float z)
	{
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"failed-first-link fixture publishes"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller(astrabot::compat::RuntimeMode::Compatibility);
	astrabot::compat::ScriptedRandomSource huntRandom(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 3, 1),
			astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 2U}, 0, 3, 3)});
	controller.setRandomSource(&huntRandom);
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
	const auto failure = controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(
			failure == astrabot::runtime::NavRoamResult::ReplanRequired, "unsafe first link requests a route replan"))
	{
		return false;
	}
	if (!check(decision.failureReason == astrabot::runtime::NavFailureReason::UnsafeDrop && decision.goalArea == 2U,
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
	const auto rerouted = controller.update(publisher.snapshot(), observation, &intent, &decision);
	return check(rerouted == astrabot::runtime::NavRoamResult::IntentReady,
			   "Compatibility patrol finds a route after a failed link") &&
		check(decision.goalArea == 4U, "failed goal is replaced by the next source-derived random choice") &&
		check(intent.targetArea == 3U && intent.traversal == astrabot::nav::TraversalAction::Walk,
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"compatibility route policy snapshot is published"))
	{
		return false;
	}
	const auto makeObservation = [](std::uint32_t slot)
	{
		astrabot::runtime::NavRoamObservation observation = {};
		observation.actor = {slot, 1U};
		observation.frame = {1U, 1U, 1U};
		observation.locomotion.position = {32.0f, 32.0f, 0.0f};
		observation.locomotion.standingClearance = 72.0f;
		observation.locomotion.crouchingClearance = 36.0f;
		observation.locomotion.grounded = true;
		return observation;
	};
	astrabot::runtime::NavRoamController firstController(astrabot::compat::RuntimeMode::Compatibility);
	astrabot::runtime::NavRoamController secondController(astrabot::compat::RuntimeMode::Compatibility);
	astrabot::compat::ScriptedRandomSource firstRandom(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 2, 1)});
	astrabot::compat::ScriptedRandomSource secondRandom(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {2U, 1U}, {0U, 0U, 1U}, 0, 2, 1)});
	firstController.setRandomSource(&firstRandom);
	secondController.setRandomSource(&secondRandom);
	astrabot::nav::LocomotionIntent firstIntent = {};
	astrabot::nav::LocomotionIntent secondIntent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	astrabot::runtime::NavRoamDecision secondDecision = {};
	if (!check(firstController.update(publisher.snapshot(), makeObservation(1U), &firstIntent, &firstDecision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				secondController.update(publisher.snapshot(), makeObservation(2U), &secondIntent, &secondDecision) ==
					astrabot::runtime::NavRoamResult::IntentReady,
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
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
	astrabot::runtime::NavRoamController controller(astrabot::compat::RuntimeMode::Compatibility);
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &firstDecision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				firstDecision.recomputeReason == astrabot::runtime::NavRecomputeReason::InitialGoal &&
				firstDecision.pathSequence == 1U,
			"initial goal computes one persistent route"))
	{
		return false;
	}
	observation.frame.tick = 2U;
	astrabot::runtime::NavRoamDecision persistentDecision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &persistentDecision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				persistentDecision.recomputeReason == astrabot::runtime::NavRecomputeReason::None &&
				persistentDecision.pathSequence == 1U,
			"unchanged goal keeps the existing route across updates"))
	{
		return false;
	}
	observation.frame.tick = 3U;
	observation.objectiveTarget = {224.0f, 32.0f, 0.0f};
	astrabot::runtime::NavRoamDecision changedDecision = {};
	return check(controller.update(publisher.snapshot(), observation, &intent, &changedDecision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
			changedDecision.recomputeReason == astrabot::runtime::NavRecomputeReason::GoalChanged &&
			changedDecision.pathSequence == 2U && changedDecision.targetArea == 4U,
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"map-change initial snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::compat::ScriptedRandomSource randomSource(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1),
			astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 2U}, 0, 1, 1)});
	controller.setRandomSource(&randomSource);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision initial = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &initial) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				initial.pathSequence == 1U,
			"map-change fixture starts one route"))
	{
		return false;
	}

	if (!check(publisher.publish(&document, 2U) == astrabot::nav::NavSnapshotResult::Published,
			"map-change replacement snapshot is published"))
	{
		return false;
	}
	observation.frame = {2U, 1U, 2U};
	astrabot::runtime::NavRoamDecision changed = {};
	return check(controller.update(publisher.snapshot(), observation, &intent, &changed) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
			changed.recomputeReason == astrabot::runtime::NavRecomputeReason::MapOrRoundChanged &&
			changed.pathSequence == 2U,
		"map generation change forces one route recompute");
}

bool testGoalAndPathFailuresAreTyped()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	document.addArea(area(1U, 0.0f, 64.0f));
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
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
	if (!check(invalidGoalController.update(publisher.snapshot(), observation, &intent, &decision) ==
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"temporary loss snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller;
	astrabot::compat::ScriptedRandomSource randomSource(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
	controller.setRandomSource(&randomSource);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision firstDecision = {};
	const astrabot::runtime::NavRoamResult firstResult =
		controller.update(publisher.snapshot(), observation, &intent, &firstDecision);
	if (!check(
			firstResult == astrabot::runtime::NavRoamResult::IntentReady, "temporary loss starts with an active route"))
	{
		return false;
	}
	if (!check(firstDecision.pathSequence == 1U, "temporary loss route sequence starts at one"))
	{
		return false;
	}
	observation.frame.tick = 2U;
	observation.locomotion.position = {50000.0f, 50000.0f, 0.0f};
	astrabot::runtime::NavRoamDecision lostDecision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &lostDecision) ==
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
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
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &firstDecision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				intent.currentArea == 1U && intent.targetArea == 2U,
			"multi-segment route starts on its first link"))
	{
		return false;
	}

	observation.frame.tick = 2U;
	observation.locomotion.position = {96.0f, 32.0f, 0.0f};
	astrabot::runtime::NavRoamDecision secondDecision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &secondDecision) ==
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
	const astrabot::runtime::NavRoamResult gapResult =
		controller.update(publisher.snapshot(), observation, &intent, &gapDecision);
	return check(gapResult == astrabot::runtime::NavRoamResult::IntentReady &&
			gapDecision.pathSequence == firstDecision.pathSequence && intent.currentArea == 2U &&
			intent.targetArea == 3U && intent.direction.x > 0.0f && intent.targetPosition.x > 132.0f,
		"temporary gap on a later segment preserves its current forward intent");
}

bool testResourceLimitFailureIsBackedOff()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	for (astrabot::nav::AreaId id = 1U; id <= 300U; ++id)
	{
		astrabot::nav::NavArea node = area(id, static_cast<float>((id - 1U) * 32U), static_cast<float>(id * 32U));
		if (id < 300U)
		{
			node.connections[0U].push_back(id + 1U);
		}
		document.addArea(node);
	}
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
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
	astrabot::compat::ScriptedRandomSource randomSource(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 3, 3)});
	astrabot::nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"goal selector snapshot is published"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller(astrabot::compat::RuntimeMode::Compatibility);
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
		decision.goalKind == astrabot::runtime::NavGoalKind::Roam && decision.goalArea == 4U &&
		randomSource.position() == 1U;
	if (!selected)
		return false;
	return check(randomSource.verifyComplete(), "Compatibility Goal selection consumes the expected RNG tape");
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"intermediate jump-link snapshot is published"))
	{
		return false;
	}
	astrabot::nav::NavQuery query(publisher.snapshot());
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 3U, &corridor) == astrabot::nav::NavQueryResult::Found &&
				corridor.links.size() == 2U && corridor.links[1U].how == 6U,
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
	const bool switched = controller.update(publisher.snapshot(), observation, &intent, &decision) ==
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
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
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				astrabot::runtime::NavRoamResult::IntentReady &&
			std::fabs(decision.targetPosition.x - 80.0f) < 0.01f &&
			std::fabs(decision.targetPosition.y - 32.0f) < 0.01f,
		"decision reports the active portal steering point instead of the final area center");
}

bool testCompatibilityHuntDrawsForEachActorEvenWithLargeAreas()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	for (std::uint32_t id = 1U; id <= 3U; ++id)
	{
		auto candidate = area(id, static_cast<float>(id - 1U) * 200.0f, static_cast<float>(id) * 200.0f);
		candidate.extent.hi.y = 200.0f;
		if (id == 1U)
			candidate.connections[1U] = {2U, 3U};
		document.addArea(candidate);
	}
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 5U);
	runtime::NavAreaVisitHistory visits;
	visits.record(5U, 1U, 2U, 1U);
	visits.record(5U, 1U, 3U, 20U);
	// Source-derived expectation: HuntState::OnUpdate's RANDOM_LONG is
	// unconditional, and its final loop overwrites the oldest-area candidate.
	// This is not a trace captured from an executing CSBot.
	for (std::uint32_t slot = 1U; slot <= 8U; ++slot)
	{
		const std::int32_t choice = slot % 2U == 0U ? 1 : 2;
		compat::ScriptedRandomSource random(
			{compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {slot, 1U}, {0U, 0U, 1U}, 0, 2, choice)});
		runtime::NavRoamController controller(compat::RuntimeMode::Compatibility);
		controller.setRandomSource(&random);
		controller.setAreaVisitHistory(&visits);
		runtime::NavRoamObservation obs{};
		obs.actor = {slot, 1U};
		obs.frame = {5U, 1U, 1U};
		obs.team = 1U;
		obs.locomotion.position = {100.0f, 100.0f, 0.0f};
		obs.locomotion.standingClearance = 72.0f;
		obs.locomotion.crouchingClearance = 36.0f;
		obs.locomotion.grounded = true;
		nav::LocomotionIntent intent{};
		runtime::NavRoamDecision decision{};
		if (!check(controller.update(publisher.snapshot(), obs, &intent, &decision) ==
						runtime::NavRoamResult::IntentReady &&
					decision.goalArea == static_cast<nav::AreaId>(choice + 1) &&
					decision.routeType == nav::NavRouteType::Safest && random.verifyComplete(),
				"each Compatibility Hunt actor consumes its draw despite shared oldest large-area history"))
			return false;
		obs.frame.tick = 2U;
		if (!check(controller.update(publisher.snapshot(), obs, &intent, &decision) ==
						runtime::NavRoamResult::IntentReady &&
					random.verifyComplete(),
				"continuing the same Hunt corridor does not redraw the destination"))
			return false;
	}
	return true;
}

bool testCompatibilityLargeAreaGoalUsesDrawAndSafestRoute()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto largeArea = [](astrabot::nav::AreaId id, float lowX, float highX, float lowY, float highY)
	{
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
	if (!check(publisher.publish(&document, 5U) == astrabot::nav::NavSnapshotResult::Published,
			"oldest-area fixture publishes"))
	{
		return false;
	}
	astrabot::runtime::NavRoamController controller(astrabot::compat::RuntimeMode::Compatibility);
	astrabot::runtime::NavAreaVisitHistory visitHistory;
	visitHistory.record(5U, 1U, 2U, 20U);
	visitHistory.record(5U, 1U, 3U, 5U);
	controller.setAreaVisitHistory(&visitHistory);
	astrabot::compat::ScriptedRandomSource huntRandom(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 10U}, 0, 2, 1)});
	controller.setRandomSource(&huntRandom);
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
	const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
	return check(result == astrabot::runtime::NavRoamResult::IntentReady, "Compatibility Hunt goal has a route") &&
		check(decision.goalArea == 2U, "random choice overrides the oldest eligible large area") &&
		check(decision.routeType == astrabot::nav::NavRouteType::Safest, "Compatibility Hunt uses the safest route") &&
		check(decision.goalSelectionStrategy == astrabot::runtime::GoalSelectionStrategyRandomFallback,
			"large NAV candidates also consume a bounded goal draw") &&
		check(huntRandom.verifyComplete(), "large-area goal consumes exactly its taped draw");
}

bool testTimedHuntDrawsBeforePathThrottle()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 64.0f);
	start.connections[1U].push_back(3U);
	document.addArea(start);
	document.addArea(area(2U, 128.0f, 192.0f));
	document.addArea(area(3U, 64.0f, 128.0f));
	nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) == nav::NavSnapshotResult::Published, "timed Hunt fixture publishes"))
		return false;
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 2, 1),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f),
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 2U}, 0, 2, 0),
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 3U}, 0, 2, 1),
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 4U}, 0, 2, 2),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 4U}, .4f, .6f, .5f),
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 6U}, 0, 2, 1)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.observedAt = 1.0f;
	observation.collectPathStats = true;
	nav::LocomotionIntent intent = {};
	runtime::NavRoamDecision decision = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				runtime::NavRoamResult::MovementSuppressed &&
				decision.goalArea == 2U && decision.targetArea == 3U && decision.pathSearchStats.searchCalls == 1U,
			"Hunt draws an unreachable goal and builds a corridor to the closest reachable area"))
		return false;
	controller.destroyPath();
	observation.frame.tick = 2U;
	observation.locomotion.observedAt = 1.1f;
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) == runtime::NavRoamResult::NoRoute &&
				decision.goalPresent && decision.goalArea == 1U && decision.pathSearchStats.searchCalls == 0U,
			"throttled Hunt still draws a fresh goal while ComputePath makes no search"))
		return false;
	observation.frame.tick = 3U;
	observation.locomotion.observedAt = 1.5f;
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) == runtime::NavRoamResult::NoRoute &&
				decision.goalArea == 2U && decision.pathSearchStats.searchCalls == 0U,
			"ComputePath remains throttled exactly at the retry deadline"))
		return false;
	observation.frame.tick = 4U;
	observation.locomotion.observedAt = 1.51f;
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				runtime::NavRoamResult::MovementSuppressed && decision.goalArea == 3U && intent.speed == 0.0f,
			"Hunt builds the newly selected route without following it on the same update"))
		return false;
	observation.frame.tick = 5U;
	observation.locomotion.observedAt = 1.61f;
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				runtime::NavRoamResult::IntentReady && intent.targetArea == 3U,
			"the next Hunt update follows the new route without another draw"))
		return false;
	controller.destroyPath();
	observation.frame.tick = 6U;
	observation.locomotion.observedAt = 1.71f;
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) == runtime::NavRoamResult::NoRoute &&
				decision.goalArea == 2U && decision.pathSearchStats.searchCalls == 0U && random.verifyComplete(),
			"a destroyed Hunt path causes a fresh goal draw even while ComputePath is throttled"))
		return false;

	class ResetDuringFloorTrace final : public nav::NavHuntPathProbe
	{
	public:
		explicit ResetDuringFloorTrace(runtime::NavRoamController &controller) : controller_(controller) {}
		bool clear(const nav::NavVector &, const nav::NavVector &, bool) const override { return true; }
		bool groundHeight(const nav::NavVector &, float *height) const override
		{
			controller_.destroyPath();
			*height = 0.0f;
			return true;
		}

	private:
		runtime::NavRoamController &controller_;
	} probe(controller);
	compat::ScriptedRandomSource retry({
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 7U}, 0, 2, 1),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 7U}, .4f, .6f, .5f)});
	controller.setRandomSource(&retry);
	observation.frame.tick = 7U;
	observation.locomotion.observedAt = 2.02f;
	observation.locomotion.huntPathProbe = &probe;
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
			runtime::NavRoamResult::ReplanRequired && intent.speed == 0.0f && retry.verifyComplete(),
		"a reset during the Hunt ground sample invalidates the outer route selection");
}

bool testTimedHuntFollowsSameAreaEndpoint()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	document.addArea(area(1U, 0.0f, 128.0f));
	document.addArea(area(2U, 160.0f, 224.0f));
	nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) == nav::NavSnapshotResult::Published, "same-area fixture publishes"))
		return false;
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 0),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f),
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 2U}, 0, 1, 1)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 2U;
	observation.locomotion.position = {12.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.observedAt = 1.0f;
	observation.collectPathStats = true;
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				runtime::NavRoamResult::MovementSuppressed &&
				decision.targetArea == 1U && decision.pathSearchStats.searchCalls == 0U && intent.speed == 0.0f,
			"Hunt builds a same-area path without an A* search or movement on the build update"))
		return false;
	observation.frame.tick = 2U;
	observation.locomotion.observedAt = 1.125f;
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				runtime::NavRoamResult::MovementSuppressed && decision.goalArea == 2U && intent.speed == 0.0f,
			"Hunt reselects the current area while the retry timer preserves the same-area route"))
		return false;
	observation.frame.tick = 3U;
	observation.locomotion.observedAt = 1.25f;
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				runtime::NavRoamResult::IntentReady && intent.targetArea == 1U && intent.targetPosition.x == 64.0f &&
				intent.direction.x > 0.0f && random.verifyComplete(),
		"Hunt follows the retained same-area endpoint after choosing a different area");
}

bool testTimedHuntRoutesAcrossNativeLadder()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	document.addArea(area(1U, 0.0f, 64.0f));
	auto top = area(2U, 0.0f, 64.0f);
	top.extent.lo.z = 200.0f;
	top.extent.hi.z = 200.0f;
	top.northEastZ = 200.0f;
	top.southWestZ = 200.0f;
	document.addArea(top);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	const auto snapshot = publisher.snapshot();
	nav::NavLadder ladder{};
	ladder.bottom = {32.0f, 32.0f, 0.0f};
	ladder.top = {32.0f, 32.0f, 200.0f};
	ladder.length = 200.0f;
	ladder.bottomArea = 1U;
	ladder.topAreas[0] = 2U;
	const nav::NavLadderTopology topology{snapshot.revision(), snapshot.mapGeneration(), {ladder}};
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.observedAt = 1.0f;
	observation.nativeLadders = &topology;
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	controller.update(snapshot, observation, &intent, &decision);
	if (!check(controller.hasNativeLadderPath() && decision.targetArea == 2U &&
				decision.linkHow == 4U && intent.speed == 0.0f && random.verifyComplete(),
			"Hunt prepares the native ladder corridor to its goal before moving"))
		return false;
	class LadderProbe final : public nav::INativeLadderProbe
	{
	public:
		bool current() const override { return true; }
		bool trace(const nav::NavLadder &, const nav::NavVector &, const nav::NavVector &, float *fraction) override
		{
			*fraction = 1.0f;
			return true;
		}
	} probe;
	observation.frame.tick = 2U;
	observation.locomotion.observedAt = 1.1f;
	observation.locomotion.viewYawAvailable = true;
	observation.nativeLadderProbe = &probe;
	return check(controller.update(snapshot, observation, &intent, &decision) == runtime::NavRoamResult::IntentReady &&
			controller.usingNativeLadder() && intent.traversal == nav::TraversalAction::Ladder && random.verifyComplete(),
		"Hunt starts following the ladder on the next update without selecting another goal");
}

bool testTimedHuntMovesAndJumpsOffCompletedLadder()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto bottom = area(1U, 0.0f, 64.0f);
	bottom.connections[1].push_back(3U);
	document.addArea(bottom);
	auto top = area(2U, 0.0f, 64.0f);
	top.extent.lo.z = top.extent.hi.z = top.northEastZ = top.southWestZ = 200.0f;
	document.addArea(top);
	auto goal = area(3U, 64.0f, 192.0f);
	goal.connections[3].push_back(1U);
	document.addArea(goal);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	const auto snapshot = publisher.snapshot();
	nav::NavLadder ladder{};
	ladder.bottom = {32.0f, 32.0f, 0.0f};
	ladder.top = {32.0f, 32.0f, 200.0f};
	ladder.length = 200.0f;
	ladder.bottomArea = 1U;
	ladder.topAreas[0] = 2U;
	const nav::NavLadderTopology topology{snapshot.revision(), snapshot.mapGeneration(), {ladder}};
	class LadderProbe final : public nav::INativeLadderProbe
	{
	public:
		bool current() const override { return true; }
		bool trace(const nav::NavLadder &, const nav::NavVector &, const nav::NavVector &, float *fraction) override
		{
			*fraction = 1.0f;
			return true;
		}
	} probe;
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 2, 2),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 200.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.viewYawAvailable = true;
	observation.locomotion.observedAt = 4.0f;
	observation.nativeLadders = &topology;
	observation.nativeLadderProbe = &probe;
	observation.huntKnownArea = 2U;
	observation.friendRayOriginAvailable = true;
	observation.friendRayOrigin = {32.0f, 32.0f, 237.0f};
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	controller.update(snapshot, observation, &intent, &decision);
	if (!check(controller.hasNativeLadderPath() && decision.goalArea == 3U, "Hunt plans past the bottom area"))
		return false;
	const auto update = [&](float feet)
	{
		++observation.frame.tick;
		observation.locomotion.observedAt += 0.1f;
		observation.locomotion.position.z = feet;
		observation.friendRayOrigin.z = feet + 37.0f;
		return controller.update(snapshot, observation, &intent, &decision);
	};
	update(200.0f);
	observation.locomotion.grounded = false;
	observation.locomotion.onLadder = true;
	update(100.0f);
	observation.huntKnownArea = 1U;
	update(35.5f);
	nav::NativeLadderObservation lookInput{};
	lookInput.actor = {1U, 1U};
	lookInput.frame = observation.frame;
	lookInput.now = observation.locomotion.observedAt;
	lookInput.origin = observation.friendRayOrigin;
	lookInput.feetZ = 36.5f;
	nav::NativeLadderLook look{};
	if (!check(controller.nativeLadderLook(snapshot, lookInput, &look) && look.phase == nav::NativeLadderPhase::Descend,
			"the hull minimum below the threshold does not end descent while CSBot feet remain above it"))
		return false;
	observation.locomotion.ducked = true;
	if (!check(update(24.0f) == runtime::NavRoamResult::IntentReady && intent.nativeLadderControl &&
				!intent.nativeMustJump, "active descent retains ladder steering without a jump"))
		return false;
	observation.locomotion.ducked = false;
	if (!check(update(0.0f) == runtime::NavRoamResult::IntentReady && !intent.nativeLadderControl &&
				intent.traversal == nav::TraversalAction::Walk && intent.direction.x > 0.0f && intent.speed > 0.0f,
			"the ladder's standing posture wins over a transient duck flag and floor steering starts on completion"))
		return false;
	if (!check(intent.nativeMustJump, "floor steering requests a forced jump while still attached to the ladder"))
		return false;
	return check(update(0.0f) == runtime::NavRoamResult::IntentReady && !intent.nativeMustJump &&
			intent.direction.x > 0.0f && random.verifyComplete(),
		"a subsequent attached update keeps floor movement and respects the in-progress jump");
}

enum class HuntRngCancellation { None, Goal, Retry };

bool testTimedHuntReselectsAtKnownLadderGoal(HuntRngCancellation cancellation = HuntRngCancellation::None)
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	document.addArea(area(1U, 0.0f, 64.0f));
	auto top = area(2U, 0.0f, 64.0f);
	top.extent.lo.z = top.extent.hi.z = top.northEastZ = top.southWestZ = 200.0f;
	document.addArea(top);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	const auto snapshot = publisher.snapshot();
	nav::NavLadder ladder{};
	ladder.bottom = {32.0f, 32.0f, 0.0f};
	ladder.top = {32.0f, 32.0f, 200.0f};
	ladder.length = 200.0f;
	ladder.bottomArea = 1U;
	ladder.topAreas[0] = 2U;
	const nav::NavLadderTopology topology{snapshot.revision(), snapshot.mapGeneration(), {ladder}};
	class LadderProbe final : public nav::INativeLadderProbe
	{
	public:
		bool current() const override { return true; }
		bool trace(const nav::NavLadder &, const nav::NavVector &, const nav::NavVector &, float *fraction) override
		{
			*fraction = 1.0f;
			return true;
		}
	} probe;
	class ReentrantRandom final : public compat::ScriptedRandomSource
	{
	public:
		using ScriptedRandomSource::ScriptedRandomSource;
		runtime::NavRoamController *controller = nullptr;
		HuntRngCancellation cancellation = HuntRngCancellation::None;
		compat::RandomLongResult nextLong(const compat::RandomRequest &request) override
		{
			const auto result = ScriptedRandomSource::nextLong(request);
			if (cancellation == HuntRngCancellation::Goal && position() == 4U)
				controller->destroyPath();
			return result;
		}
		compat::RandomFloatResult nextFloat(const compat::RandomRequest &request) override
		{
			const auto result = ScriptedRandomSource::nextFloat(request);
			if (cancellation == HuntRngCancellation::Retry && position() == 5U)
				controller->destroyPath();
			return result;
		}
	} random({
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 0),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f),
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 4U}, 0, 1, 0),
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 5U}, 0, 1, 0),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 5U}, .4f, .6f, .5f),
		compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 6U}, 0, 1, 0)});
	runtime::NavRoamController controller;
	random.controller = &controller;
	random.cancellation = cancellation;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 200.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.viewYawAvailable = true;
	observation.locomotion.observedAt = 1.0f;
	observation.nativeLadders = &topology;
	observation.nativeLadderProbe = &probe;
	observation.huntKnownArea = 2U;
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	controller.update(snapshot, observation, &intent, &decision);
	if (!check(controller.hasNativeLadderPath() && decision.linkHow == 5U, "Hunt plans the downward ladder"))
		return false;
	const auto update = [&](std::uint32_t tick, float now, float feet)
	{
		observation.frame.tick = tick;
		observation.locomotion.observedAt = now;
		observation.locomotion.position.z = feet;
		return controller.update(snapshot, observation, &intent, &decision);
	};
	update(2U, 1.1f, 200.0f);
	observation.locomotion.grounded = false;
	observation.locomotion.onLadder = true;
	if (!check(update(3U, 1.2f, 100.0f) == runtime::NavRoamResult::IntentReady &&
				controller.usingNativeLadder() && intent.nativeLadderControl,
			"Hunt keeps descending before the requested area is known"))
		return false;
	observation.huntKnownArea = 1U;
	const auto readLook = [&]()
	{
		nav::NativeLadderObservation in{};
		in.actor = {observation.actor.slot, observation.actor.actorGeneration};
		in.frame = observation.frame;
		in.origin = {32.0f, 32.0f, observation.locomotion.position.z + 36.0f};
		in.feetZ = observation.locomotion.position.z;
		in.now = observation.locomotion.observedAt;
		in.onLadder = true;
		nav::NativeLadderLook look{};
		controller.nativeLadderLook(snapshot, in, &look);
		return look;
	};
	if (!check(update(4U, 1.3f, 63.0f) == runtime::NavRoamResult::MovementSuppressed &&
				controller.usingNativeLadder() && !intent.nativeLadderControl && !intent.nativeMustJump &&
				readLook().active && readLook().pitch == 60.0f && random.position() == 3U,
			"Hunt reselects before executing the ladder step but retains the throttled route"))
		return false;
	const auto replacement = update(5U, 1.6f, 63.0f);
	if (cancellation != HuntRngCancellation::None)
		return check(replacement == runtime::NavRoamResult::ReplanRequired && !controller.hasPath() &&
				readLook().phase == nav::NativeLadderPhase::None,
			"cancellation inside either replan draw cannot restore the route or its look");
	if (!check(replacement == runtime::NavRoamResult::MovementSuppressed &&
				!controller.usingNativeLadder() && controller.hasPath() && !intent.nativeLadderControl,
			"Hunt replaces its ladder route from the known goal area once retry is allowed"))
		return false;
	const auto released = readLook();
	if (!check(!released.active && !released.pitchOverride && released.yaw == -90.0f,
			"successful replacement keeps the old ladder's lookahead without its pitch override"))
		return false;
	if (!check(update(6U, 1.7f, 63.0f) == runtime::NavRoamResult::MovementSuppressed &&
			!intent.nativeLadderControl && !intent.nativeMustJump && readLook().yaw == -90.0f && random.verifyComplete(),
			"the known-area check still wins while physically attached after replanning"))
		return false;
	controller.destroyPath();
	return check(readLook().phase == nav::NativeLadderPhase::None, "cancelling the route clears its retained look");
}

bool testFollowMovesBeforeReplacingItsRoute()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 64.0f);
	start.connections[1U].push_back(2U);
	start.connections[3U].push_back(3U);
	document.addArea(start);
	document.addArea(area(2U, 64.0f, 128.0f));
	document.addArea(area(3U, -64.0f, 0.0f));
	auto unreachable = area(4U, 128.0f, 192.0f);
	unreachable.extent.lo.y = 64.0f;
	unreachable.extent.hi.y = 128.0f;
	document.addArea(unreachable);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, .4f, .6f, .6f),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 3U}, .4f, .6f, .4f),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 5U}, .4f, .6f, .4f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.observedAt = 1.0f;
	observation.hasObjectiveTarget = true;
	observation.nativeSameAreaEndpoint = true;
	observation.followPath = true;
	observation.followGoalArea = 2U;
	observation.objectiveTarget = {96.0f, 32.0f, 0.0f};
	nav::LocomotionIntent intent{};
	controller.update(publisher.snapshot(), observation, &intent);
	if (!check(controller.hasPath() && intent.speed == 0.0f,
			"Follow prepares a path without moving along it on the same update"))
		return false;
	observation.followGoalArea = 3U;
	observation.objectiveTarget = {-32.0f, 32.0f, 0.0f};
	observation.frame.tick = 2U;
	observation.locomotion.observedAt = 1.55f;
	const nav::NavLadderTopology topology{publisher.snapshot().revision(), publisher.snapshot().mapGeneration(), {}};
	if (!check(!controller.tryFollowCoverPath(publisher.snapshot(), observation.actor, observation.frame,
				observation.locomotion.observedAt, 1U, 3U, observation.objectiveTarget, topology, [] { return true; }) &&
			controller.hasPath() && random.position() == 1U,
		"a throttled Follow cover attempt preserves the route and consumes no retry draw"))
		return false;
	if (!check(controller.update(publisher.snapshot(), observation, &intent) == runtime::NavRoamResult::IntentReady &&
				intent.direction.x > 0.0f,
			"the shared retry timer retains the current Follow path"))
		return false;
	observation.frame.tick = 3U;
	observation.locomotion.observedAt = 1.61f;
	bool movedBeforeSelection = false;
	const std::function<nav::AreaId()> selectReplacement = [&]
	{
		movedBeforeSelection = intent.speed > 0.0f && intent.direction.x > 0.0f;
		return 3U;
	};
	observation.followStartArea = 1U;
	observation.followGoalRequest = &selectReplacement;
	if (!check(controller.update(publisher.snapshot(), observation, &intent) == runtime::NavRoamResult::IntentReady &&
				intent.direction.x > 0.0f && movedBeforeSelection,
			"Follow moves along the current path before computing its replacement"))
		return false;
	observation.frame.tick = 4U;
	observation.locomotion.observedAt = 1.71f;
	observation.followGoalArea = 0U;
	observation.followGoalRequest = nullptr;
	if (!check(controller.update(publisher.snapshot(), observation, &intent) == runtime::NavRoamResult::IntentReady &&
				intent.direction.x < 0.0f,
			"the next Follow update moves along the replacement"))
		return false;
	class EndpointProbe final : public nav::NavHuntPathProbe
	{
	public:
		mutable nav::NavVector sampled{};
		bool clear(const nav::NavVector &, const nav::NavVector &, bool) const override { return true; }
		bool groundHeight(const nav::NavVector &point, float *height) const override
		{
			sampled = point;
			*height = 6.0f;
			return true;
		}
	} probe;
	observation.followPathProbe = &probe;
	observation.followGoalArea = 4U;
	observation.frame.tick = 5U;
	observation.locomotion.observedAt = 2.1f;
	controller.update(publisher.snapshot(), observation, &intent);
	observation.followGoalArea = 0U;
	observation.frame.tick = 6U;
	observation.locomotion.observedAt = 2.2f;
	return check(controller.update(publisher.snapshot(), observation, &intent) == runtime::NavRoamResult::IntentReady &&
			intent.direction.x > 0.0f && probe.sampled.x == 96.0f && probe.sampled.y == 32.0f && random.verifyComplete(),
		"Follow reaches the closest accessible area with a ground sample aligned to the final portal");
}

bool testCompatibilityPathRetryUsesEngineTime()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	document.addArea(area(1U, 0.0f, 64.0f));
	document.addArea(area(2U, 128.0f, 192.0f));
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	compat::ScriptedRandomSource random(
		{compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, 0.4f, 0.6f, 0.5f),
			compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 101U}, 0.4f, 0.6f, 0.5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {160.0f, 32.0f, 0.0f};
	observation.collectPathStats = true;
	nav::LocomotionIntent intent = {};
	runtime::NavRoamDecision decision = {};
	controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(decision.pathSearchStats.searchCalls == 1U, "first timed route attempts path search"))
		return false;
	observation.frame.tick = 100U;
	observation.locomotion.observedAt = 0.1f;
	controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(decision.pathSearchStats.searchCalls == 0U, "many server frames do not bypass engine-time repath delay"))
		return false;
	observation.frame.tick = 101U;
	observation.locomotion.observedAt = 0.6f;
	controller.update(publisher.snapshot(), observation, &intent, &decision);
	return check(decision.pathSearchStats.searchCalls == 1U && random.verifyComplete(),
		"elapsed engine time retries once with reference random interval");
}

bool testAirborneRouteDoesNotEnterStuckRecovery()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 256.0f);
	start.connections[1U].push_back(2U);
	document.addArea(start);
	document.addArea(area(2U, 256.0f, 512.0f));
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	compat::ScriptedRandomSource random(
		{compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, 0.4f, 0.6f, 0.5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {300.0f, 32.0f, 0.0f};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	nav::LocomotionIntent intent = {};
	if (!check(controller.update(publisher.snapshot(), observation, &intent) == runtime::NavRoamResult::IntentReady,
			"airborne regression route starts"))
		return false;
	observation.locomotion.grounded = false;
	observation.locomotion.position.z = 16.0f;
	observation.locomotion.velocity = {};
	observation.airborne = true;
	for (std::uint32_t tick = 2U; tick <= 21U; ++tick)
	{
		observation.frame.tick = tick;
		observation.locomotion.observedAt = (tick - 1U) * 0.1f;
		if (!check(
				controller.update(publisher.snapshot(), observation, &intent) == runtime::NavRoamResult::IntentReady &&
					intent.direction.x > 0.9f && std::fabs(intent.direction.y) < 0.01f &&
					intent.traversal == nav::TraversalAction::Walk,
				"airborne zero velocity preserves route heading without starting Wiggle"))
			return false;
	}
	return check(random.verifyComplete(), "airborne route consumes no stuck or Wiggle RNG");
}

bool testFriendWaitRepathsAroundCrowding(
	bool clears = false, bool attacking = false, int nonBlockingFriend = 0, bool onlyCorridor = false)
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 256.0f);
	auto crowded = area(2U, 256.0f, 512.0f);
	auto alternate = area(3U, 256.0f, 512.0f);
	alternate.extent.lo.y = 128.0f;
	alternate.extent.hi.y = 192.0f;
	auto goal = area(4U, 512.0f, 768.0f);
	start.connections[1U] = {2U, 3U};
	if (onlyCorridor)
		start.connections[1U] = {2U};
	crowded.connections[1U] = {4U};
	alternate.connections[1U] = {4U};
	document.addArea(start);
	document.addArea(crowded);
	document.addArea(alternate);
	document.addArea(goal);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	compat::ScriptedRandomSource random(
		{compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, 0.4f, 0.6f, 0.5f),
			compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 5U}, 0.4f, 0.6f, 0.5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	nav::NavPathCostContext cost{};
	cost.aggression = 0.5f;
	cost.attacking = attacking;
	controller.setPathCostContext(cost);
	runtime::NavRoamObservation obs{};
	obs.actor = {1U, 1U};
	obs.frame = {1U, 1U, 1U};
	obs.locomotion.position = {32.0f, 32.0f, 0.0f};
	obs.locomotion.standingClearance = 72.0f;
	obs.locomotion.crouchingClearance = 36.0f;
	obs.locomotion.grounded = true;
	obs.locomotion.timingAvailable = true;
	obs.hasObjectiveTarget = true;
	obs.objectiveTarget = {600.0f, 32.0f, 0.0f};
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	if (!check(
			controller.update(publisher.snapshot(), obs, &intent, &decision) == runtime::NavRoamResult::IntentReady &&
				decision.selectedPath == std::vector<nav::AreaId>{1U, 2U, 4U},
			"unoccupied reference path initially uses the direct branch"))
		return false;
	obs.frame.tick = 2U;
	obs.locomotion.observedAt = 0.1f;
	obs.nearbyFriends = {{82.0f, 32.0f, 0.0f}};
	if (nonBlockingFriend == 1)
		obs.nearbyFriends = {{0.0f, 32.0f, 0.0f}};
	if (nonBlockingFriend == 2)
		obs.nearbyFriends = {{82.0f, 80.0f, 0.0f}};
	if (nonBlockingFriend == 3)
		obs.nearbyFriends = {{82.0f, 32.0f, 100.0f}};
	const auto waiting = controller.update(publisher.snapshot(), obs, &intent, &decision);
	if (attacking || nonBlockingFriend != 0)
		return check(waiting == runtime::NavRoamResult::IntentReady && intent.speed > 0.0f,
			"attack or a friend behind, beside, or on another floor does not suppress movement");
	if (!check(waiting == runtime::NavRoamResult::MovementSuppressed && intent.speed == 0.0f &&
				intent.traversal != nav::TraversalAction::Jump && random.position() == 1U,
			"friend in the path suppresses pushing and Wiggle without consuming RNG"))
		return false;
	obs.frame.tick = 3U;
	obs.locomotion.observedAt = 3.59f;
	if (clears)
		obs.nearbyFriends.clear();
	const auto before = controller.update(publisher.snapshot(), obs, &intent, &decision);
	if (clears)
		return check(before == runtime::NavRoamResult::IntentReady && intent.speed > 0.0f && random.position() == 1U &&
				!random.failed(),
			"friend clearing resumes the route without a stale stuck jump");
	if (!check(before == runtime::NavRoamResult::MovementSuppressed, "half-aggression bot waits through 3.49 seconds"))
		return false;
	obs.frame.tick = 4U;
	obs.locomotion.observedAt = 3.61f;
	if (!check(
			controller.update(publisher.snapshot(), obs, &intent, &decision) == runtime::NavRoamResult::IntentReady &&
				intent.speed > 0.0f && intent.direction.x > 0.9f && intent.traversal != nav::TraversalAction::Jump &&
				decision.recomputeReason == runtime::NavRecomputeReason::PathInvalidated && !controller.isActive(),
			"CSBot patience expiry moves this update before destroying the path"))
		return false;
	cost.areas = {{2U, 0.0f, 3U}};
	controller.setPathCostContext(cost);
	obs.frame.tick = 5U;
	obs.locomotion.observedAt = 3.7f;
	return check(
		controller.update(publisher.snapshot(), obs, &intent, &decision) == runtime::NavRoamResult::IntentReady &&
			decision.selectedPath ==
				(onlyCorridor ? std::vector<nav::AreaId>{1U, 2U, 4U} : std::vector<nav::AreaId>{1U, 3U, 4U}) &&
			random.verifyComplete(),
		"repath selects a cheaper branch but keeps the only valid occupied corridor legal");
}

bool testCompatibilityCorridorWithoutProgressExpires(bool airborne = false, bool lateArrival = false)
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 256.0f);
	auto goal = area(2U, 256.0f, 512.0f);
	start.connections[1U].push_back(2U);
	document.addArea(start);
	document.addArea(goal);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	compat::ScriptedRandomSource random(
		{compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, 0.4f, 0.6f, 0.5f),
			compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 53U}, 0.4f, 0.6f, 0.5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = !airborne;
	observation.airborne = airborne;
	observation.locomotion.timingAvailable = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {300.0f, 32.0f, 0.0f};
	nav::LocomotionIntent intent = {};
	runtime::NavRoamDecision decision = {};
	for (std::uint32_t tick = 1U; tick <= 51U; ++tick)
	{
		observation.frame.tick = tick;
		observation.locomotion.observedAt = static_cast<float>(tick - 1U) * 0.1f;
		// Fast local motion prevents the average-velocity monitor from detecting stuck.
		observation.locomotion.position = {32.0f + (tick % 2U) * 10.0f, 32.0f, 0.0f};
		if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
					runtime::NavRoamResult::IntentReady,
				"corridor remains usable through the five-second progress boundary"))
			return false;
	}
	observation.frame.tick = 52U;
	observation.locomotion.observedAt = 5.1f;
	observation.locomotion.position.x = 32.0f;
	if (lateArrival)
		observation.locomotion.position.x = 300.0f;
	const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (lateArrival)
		return check(result == runtime::NavRoamResult::TargetReached && random.position() == 1U && !random.failed(),
			"arrival is processed before the five-second path deadline");
	if (airborne)
	{
		if (!check(result == runtime::NavRoamResult::IntentReady && intent.direction.x > 0.9f &&
					random.position() == 1U && !random.failed(),
				"airborne traversal is not interrupted by the ground progress deadline"))
			return false;
		observation.frame.tick = 53U;
		observation.locomotion.observedAt = 5.2f;
		observation.locomotion.grounded = true;
		observation.airborne = false;
		return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
					runtime::NavRoamResult::ReplanRequired &&
				intent.speed == 0.0f && decision.failureReason == runtime::NavFailureReason::MovementNotProduced,
			"landing without route progress cannot renew the stuck deadline");
	}
	if (!check(result == runtime::NavRoamResult::ReplanRequired && intent.speed == 0.0f &&
				decision.failureReason == runtime::NavFailureReason::MovementNotProduced &&
				decision.linkFromArea == 1U && decision.linkToArea == 2U,
			"moving without corridor progress fails the directed segment after five seconds"))
		return false;
	observation.frame.tick = 53U;
	observation.locomotion.observedAt = 5.2f;
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				runtime::NavRoamResult::IntentReady &&
			intent.speed > 0.0f && decision.linkFromArea == 1U && decision.linkToArea == 2U && random.verifyComplete(),
		"CSBot retry rebuilds the legal sole route after the old corridor expires");
}

bool testBsiteProgressFailureDoesNotBlacklistTheOnlyApproach()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	// Reduced from the deployed de_dust2 NAV. Area823 is the rising jump
	// approach to643; no alternative exists when157->823 is removed.
	auto start = area(157U, -1425.0f, -1350.0f);
	start.extent.lo = {-1425.0f, 2350.0f, 0.03125f};
	start.extent.hi = {-1350.0f, 2575.0f, 12.88872f};
	start.northEastZ = 0.03125f;
	start.southWestZ = 2.17444f;
	start.connections[2U] = {823U};
	auto approach = area(823U, -1375.0f, -1350.0f);
	approach.extent.lo = {-1375.0f, 2575.0f, 9.31729f};
	approach.extent.hi = {-1350.0f, 2625.0f, 112.03125f};
	approach.northEastZ = 12.88872f;
	approach.southWestZ = 112.03126f;
	approach.attributes = nav::NavArea::kJump;
	approach.connections[0U] = {157U};
	approach.connections[3U] = {643U};
	auto goal = area(643U, -1400.0f, -1375.0f);
	goal.extent.lo = {-1400.0f, 2600.0f, 64.03125f};
	goal.extent.hi = {-1375.0f, 2625.0f, 112.03126f};
	goal.northEastZ = 64.03127f;
	goal.southWestZ = 64.03126f;
	goal.connections[1U] = {823U};
	document.addArea(start);
	document.addArea(approach);
	document.addArea(goal);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	nav::NavQuery query(publisher.snapshot());
	nav::NavCorridor corridor{}, alternate{};
	if (!check(query.buildCorridor(157U, 643U, &corridor) == nav::NavQueryResult::Found &&
				query.buildAlternativeCorridor(corridor, corridor.links.front(), &alternate) ==
					nav::NavQueryResult::NoRoute,
			"recorded B-site approach has a legal corridor and no bypass"))
		return false;
	compat::ScriptedRandomSource random(
		{compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {7U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f),
			compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {7U, 1U}, {0U, 0U, 53U}, .4f, .6f, .5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation obs{};
	obs.actor = {7U, 1U};
	obs.frame = {1U, 1U, 1U};
	obs.locomotion.grounded = true;
	obs.locomotion.timingAvailable = true;
	obs.locomotion.standingClearance = 72.0f;
	obs.locomotion.crouchingClearance = 36.0f;
	obs.hasObjectiveTarget = true;
	obs.objectiveTarget = {-1387.5f, 2612.5f, 76.0f};
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	for (std::uint32_t tick = 1U; tick <= 52U; ++tick)
	{
		obs.frame.tick = tick;
		obs.locomotion.observedAt = 147.0f + static_cast<float>(tick - 1U) * .1f;
		// Local movement without area progress reproduces the reference five-second
		// give-up check without requiring random Wiggle decisions in this test.
		obs.locomotion.position = {-1361.3f - (tick % 2U) * 10.0f, 2447.0f, 1.9f};
		const auto result = controller.update(publisher.snapshot(), obs, &intent, &decision);
		if (!check(tick < 52U ? result == runtime::NavRoamResult::IntentReady
							  : (result == runtime::NavRoamResult::ReplanRequired &&
									decision.failureReason == runtime::NavFailureReason::MovementNotProduced),
				"B-site corridor expires after five seconds of no area progress"))
			return false;
	}
	obs.frame.tick = 53U;
	obs.locomotion.observedAt = 152.2f;
	obs.locomotion.position = {-1361.3f, 2447.0f, 1.9f};
	return check(
		controller.update(publisher.snapshot(), obs, &intent, &decision) == runtime::NavRoamResult::IntentReady &&
			intent.speed > 0.0f && decision.selectedPath == std::vector<nav::AreaId>({157U, 823U, 643U}) &&
			random.verifyComplete(),
		"CSBot B-site retry resumes the sole legal approach instead of latching PathSearchFailed");
}

bool testCompatibilityStuckJumpIsScheduledWithoutAPath()
{
	using namespace astrabot;
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::floatEntry("CSBOT-STUCK-JUMP-DELAY", {1U, 1U}, {0U, 0U, 6U},
			0.0f, 0.5f, 0.2f),
		compat::RandomTapeEntry::floatEntry("CSBOT-STUCK-JUMP-DELAY", {1U, 1U}, {0U, 0U, 13U},
			0.0f, 0.5f, 0.2f),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 14U}, 0.4f, 0.6f, 0.5f),
		compat::RandomTapeEntry::longEntry("CSBOT-WIGGLE-DIRECTION", {1U, 1U}, {0U, 0U, 14U}, 0, 3, 1),
		compat::RandomTapeEntry::floatEntry("CSBOT-WIGGLE-DURATION", {1U, 1U}, {0U, 0U, 14U}, 0.5f, 1.5f, 1.0f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	nav::LocomotionObservation sample{};
	sample.position = {32.0f, 32.0f, 0.0f};
	sample.grounded = true;
	sample.timingAvailable = true;
	for (std::uint32_t tick = 1U; tick <= 13U; ++tick)
	{
		if (tick == 8U)
			sample.position.x += 76.0f;
		sample.observedAt = static_cast<float>(tick) * 0.1f;
		controller.observeMotion({1U, 1U}, {1U, 1U, tick}, sample);
		controller.observeMotion({1U, 1U}, {1U, 1U, tick}, sample);
		const std::size_t expectedDraws = tick < 6U ? 0U : tick < 13U ? 1U : 2U;
		if (!check(!random.failed() && random.position() == expectedDraws,
				"stuck schedules the jump delay at detection, even without a route, once per latch"))
			return false;
	}
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 256.0f);
	start.connections[1U] = {2U};
	document.addArea(start);
	document.addArea(area(2U, 256.0f, 512.0f));
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	runtime::NavRoamObservation observation{};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 14U};
	observation.team = 1U;
	observation.locomotion = sample;
	observation.locomotion.observedAt = 1.4f;
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {300.0f, 32.0f, 0.0f};
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
			runtime::NavRoamResult::IntentReady &&
			decision.locomotionResult == nav::LocomotionResult::Stuck && random.verifyComplete(),
		"a later path reuses the current stuck episode's delay without another draw");
}

bool testCompatibilityWiggleUsesRandomDirectionAndDuration(bool withView = false, bool rewindClock = false,
	bool clearanceKnown = true, bool narrowArea = false, bool friendAfterWiggle = false,
	bool preObserve = false)
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 256.0f);
	auto goal = area(2U, 256.0f, 512.0f);
	if (narrowArea)
	{
		start.extent.hi.x = 64.0f;
		goal.extent.lo.x = 64.0f;
		goal.extent.hi.x = 128.0f;
	}
	start.connections[1U].push_back(2U);
	document.addArea(start);
	document.addArea(goal);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	std::vector<compat::RandomTapeEntry> tape = {
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, 0.4f, 0.6f, 0.5f),
		compat::RandomTapeEntry::floatEntry("CSBOT-STUCK-JUMP-DELAY", {1U, 1U}, {0U, 0U, 6U}, 0.0f, 0.5f, 0.2f),
		compat::RandomTapeEntry::longEntry("CSBOT-WIGGLE-DIRECTION", {1U, 1U}, {0U, 0U, 6U}, 0, 3, 1),
		compat::RandomTapeEntry::floatEntry("CSBOT-WIGGLE-DURATION", {1U, 1U}, {0U, 0U, 6U}, 0.5f, 1.5f, 1.0f),
		compat::RandomTapeEntry::floatEntry("CSBOT-WIGGLE-JUMP-INTERVAL", {1U, 1U}, {0U, 0U, 8U}, 1.0f, 2.0f, 1.0f)};
	if (rewindClock)
	{
		const auto repeated = tape;
		tape.insert(tape.end(), repeated.begin(), repeated.end());
	}
	if (friendAfterWiggle)
		tape.resize(4U);
	compat::ScriptedRandomSource random(tape);
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.viewYawAvailable = withView;
	observation.locomotion.clearanceAvailable = clearanceKnown;
	observation.locomotion.viewYaw = 180.0f;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {300.0f, 32.0f, 0.0f};
	if (narrowArea)
		observation.objectiveTarget.x = 96.0f;
	nav::LocomotionIntent intent = {};
	runtime::NavRoamDecision decision = {};
	bool jumpSeen = false;
	for (std::uint32_t tick = 1U; tick <= 10U; ++tick)
	{
		observation.frame.tick = tick;
		observation.locomotion.observedAt = static_cast<float>(tick - 1U) * 0.1f;
		if (friendAfterWiggle && tick == 7U)
		{
			observation.nearbyFriends = {{82.0f, 32.0f, 0.0f}};
			observation.locomotion.observedAt = 0.7f;
			if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
						runtime::NavRoamResult::MovementSuppressed,
					"a newly detected friend interrupts active Wiggle"))
				return false;
			observation.nearbyFriends.clear();
			observation.frame.tick = 8U;
			observation.locomotion.observedAt = 1.3f;
			return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
						runtime::NavRoamResult::ReplanRequired &&
					intent.speed == 0.0f && random.verifyComplete(),
				"clearing a friend invalidates stale Wiggle instead of resuming its delayed jump");
		}
		if (preObserve)
			controller.observeMotion(observation.actor, observation.frame, observation.locomotion);
		if (controller.update(publisher.snapshot(), observation, &intent, &decision) !=
			runtime::NavRoamResult::IntentReady)
			return false;
		jumpSeen = jumpSeen || intent.traversal == nav::TraversalAction::Jump;
	}
	if (!check((rewindClock ? random.position() == 5U && !random.failed() : random.verifyComplete()) && jumpSeen &&
				(withView ? intent.direction.y > 0.9f : intent.direction.y < -0.9f),
			"compatibility Wiggle uses random direction and persists for sampled seconds"))
		return false;
	observation.frame.tick = 11U;
	observation.locomotion.observedAt = 1.0f;
	observation.locomotion.position.x += 76.0f;
	if (narrowArea)
		observation.locomotion.position.x = 68.0f;
	controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(
			intent.direction.x > 0.9f, "displacement or grounded entry into the next narrow area ends stuck recovery"))
		return false;
	if (!rewindClock)
		return true;
	if (!check(publisher.publish(&document, 2U) == nav::NavSnapshotResult::Published,
			"new map publishes for clock rewind regression"))
		return false;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	bool jumpAfterRewind = false;
	for (std::uint32_t tick = 1U; tick <= 10U; ++tick)
	{
		observation.frame = {2U, 1U, tick};
		observation.locomotion.observedAt = static_cast<float>(tick - 1U) * 0.1f;
		if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
					runtime::NavRoamResult::IntentReady,
				"new-map timed route remains active"))
			return false;
		jumpAfterRewind = jumpAfterRewind || intent.traversal == nav::TraversalAction::Jump;
	}
	return check(jumpAfterRewind && random.verifyComplete(),
		"new-map clock rewind clears old jump cooldown and preserves RNG schedule");
}

bool testTeamReservationsChooseDistinctGoalsAndFirstLinks(bool compatibility = false)
{
	using astrabot::nav::NavArea;
	using astrabot::nav::NavDocument;
	using astrabot::runtime::NavRoamResult;
	using astrabot::nav::NavSnapshotPublisher;
	using astrabot::runtime::NavAreaVisitHistory;
	using astrabot::runtime::NavRoamController;
	using astrabot::runtime::NavRoamDecision;
	using astrabot::runtime::NavRoamObservation;
	using astrabot::runtime::NavRoamReservationBoard;

	auto largeArea = [](astrabot::nav::AreaId id, float lowX, float highX, float lowY, float highY)
	{
		NavArea result = area(id, lowX, highX);
		result.extent.lo.y = lowY;
		result.extent.hi.y = highY;
		return result;
	};
	NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	NavArea start = largeArea(1U, 0.0f, 200.0f, 0.0f, 200.0f);
	NavArea east = largeArea(2U, 300.0f, 500.0f, 0.0f, 200.0f);
	NavArea north = largeArea(3U, 0.0f, 200.0f, 300.0f, 500.0f);
	start.connections[0U].push_back(2U);
	start.connections[1U].push_back(3U);
	document.addArea(start);
	document.addArea(east);
	document.addArea(north);
	NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 5U) == astrabot::nav::NavSnapshotResult::Published,
			"team-reservation NAV fixture publishes"))
	{
		return false;
	}

	NavAreaVisitHistory visits;
	NavRoamReservationBoard board;
	board.reset(5U, 7U);
	const auto mode =
		compatibility ? astrabot::compat::RuntimeMode::Compatibility : astrabot::compat::RuntimeMode::Enhanced;
	NavRoamController first(mode);
	NavRoamController second(mode);
	first.setAreaVisitHistory(&visits);
	second.setAreaVisitHistory(&visits);
	first.setReservationBoard(&board);
	second.setReservationBoard(&board);
	astrabot::compat::ScriptedRandomSource firstRandom(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 2, 1)});
	astrabot::compat::ScriptedRandomSource secondRandom(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {2U, 1U}, {0U, 0U, 1U}, 0, 2, 1)});
	if (compatibility)
	{
		first.setRandomSource(&firstRandom);
		second.setRandomSource(&secondRandom);
	}
	NavRoamObservation firstObservation = {};
	firstObservation.actor = {1U, 1U};
	firstObservation.frame = {5U, 7U, 1U};
	firstObservation.team = 1U;
	firstObservation.locomotion.position = {100.0f, 100.0f, 0.0f};
	firstObservation.locomotion.standingClearance = 72.0f;
	firstObservation.locomotion.crouchingClearance = 36.0f;
	firstObservation.locomotion.grounded = true;
	NavRoamObservation secondObservation = firstObservation;
	secondObservation.actor = {2U, 1U};
	NavRoamDecision firstDecision = {};
	NavRoamDecision secondDecision = {};
	astrabot::nav::LocomotionIntent firstIntent = {};
	astrabot::nav::LocomotionIntent secondIntent = {};
	if (!check(first.update(publisher.snapshot(), firstObservation, &firstIntent, &firstDecision) ==
				NavRoamResult::IntentReady,
			"first teammate obtains a reserved roam route") ||
		!check(second.update(publisher.snapshot(), secondObservation, &secondIntent, &secondDecision) ==
				NavRoamResult::IntentReady,
			"second teammate obtains a distinct reserved roam route"))
	{
		return false;
	}
	if (compatibility)
	{
		return check(firstDecision.goalArea == secondDecision.goalArea && firstRandom.verifyComplete() &&
				secondRandom.verifyComplete() && !board.goalReservedByOther(1U, {3U, 1U}, firstDecision.goalArea),
			"compatibility ignores injected team reservations and creates no claims");
	}
	return check(firstDecision.goalArea != secondDecision.goalArea &&
			(firstDecision.linkFromArea != secondDecision.linkFromArea ||
				firstDecision.linkToArea != secondDecision.linkToArea),
		"shared oldest-visit history does not give teammates the same goal and first link");
}

bool testTeamReservationFallbackAndLifecycle()
{
	using astrabot::nav::NavArea;
	using astrabot::nav::NavDocument;
	using astrabot::nav::NavSnapshotPublisher;
	using astrabot::runtime::NavAreaVisitHistory;
	using astrabot::runtime::NavRoamController;
	using astrabot::runtime::NavRoamDecision;
	using astrabot::runtime::NavRoamObservation;
	using astrabot::runtime::NavRoamReservationBoard;

	auto largeArea = [](astrabot::nav::AreaId id, float lowX, float highX, float lowY, float highY)
	{
		NavArea result = area(id, lowX, highX);
		result.extent.lo.y = lowY;
		result.extent.hi.y = highY;
		return result;
	};
	NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	NavArea start = largeArea(1U, 0.0f, 200.0f, 0.0f, 200.0f);
	NavArea onlyGoal = largeArea(2U, 300.0f, 500.0f, 0.0f, 200.0f);
	start.connections[0U].push_back(2U);
	document.addArea(start);
	document.addArea(onlyGoal);
	NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 5U) == astrabot::nav::NavSnapshotResult::Published,
			"single-route reservation fixture publishes"))
	{
		return false;
	}

	NavRoamReservationBoard board;
	board.reset(5U, 7U);
	NavAreaVisitHistory visits;
	NavRoamController first(astrabot::compat::RuntimeMode::Enhanced);
	NavRoamController second(astrabot::compat::RuntimeMode::Enhanced);
	first.setAreaVisitHistory(&visits);
	second.setAreaVisitHistory(&visits);
	first.setReservationBoard(&board);
	second.setReservationBoard(&board);
	NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {5U, 7U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {100.0f, 100.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	NavRoamDecision firstDecision = {};
	NavRoamDecision secondDecision = {};
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(first.update(publisher.snapshot(), observation, &intent, &firstDecision) ==
				astrabot::runtime::NavRoamResult::IntentReady,
			"first teammate claims the only route"))
	{
		return false;
	}
	observation.actor = {2U, 1U};
	if (!check(second.update(publisher.snapshot(), observation, &intent, &secondDecision) ==
					astrabot::runtime::NavRoamResult::IntentReady &&
				secondDecision.goalArea == firstDecision.goalArea && secondDecision.reservationFallback &&
				secondDecision.reservationFallbackReason ==
					astrabot::runtime::NavRoamReservationFallbackReason::NoDistinctRouteFound,
			"shared route is an explicit fallback when no distinct goal exists"))
	{
		return false;
	}

	const astrabot::runtime::ActorId firstActor = {1U, 4U};
	const astrabot::runtime::ActorId reusedActor = {1U, 5U};
	if (!check(board.goalReservedByOther(1U, reusedActor, 2U),
			"a reused slot with a new generation cannot impersonate a claim owner"))
	{
		return false;
	}
	first.reset();
	if (!check(
			board.goalReservedByOther(1U, reusedActor, 2U), "another teammate's shared fallback claim remains active"))
	{
		return false;
	}
	second.reset();
	if (!check(!board.goalReservedByOther(1U, reusedActor, 2U), "controller resets release all shared goal claims"))
	{
		return false;
	}
	if (!check(board.reserve(1U, firstActor, 2U, 1U, 2U), "reservation board accepts a bounded actor claim"))
	{
		return false;
	}
	board.reset(5U, 7U);
	if (!check(board.goalReservedByOther(1U, reusedActor, 2U), "same-generation reset keeps valid claims"))
	{
		return false;
	}
	board.reset(5U, 8U);
	if (!check(!board.goalReservedByOther(1U, reusedActor, 2U), "round generation change releases claims") ||
		!check(board.reserve(1U, firstActor, 2U, 1U, 2U), "claim can be reserved after the round reset"))
	{
		return false;
	}
	board.reset(6U, 8U);
	return check(!board.goalReservedByOther(1U, reusedActor, 2U), "map generation change releases claims");
}

bool testReservationBudgetFallbackIsDiagnosed()
{
	using astrabot::nav::NavArea;
	using astrabot::nav::NavDocument;
	using astrabot::nav::NavSnapshotPublisher;
	using astrabot::runtime::NavAreaVisitHistory;
	using astrabot::runtime::NavRoamController;
	using astrabot::runtime::NavRoamDecision;
	using astrabot::runtime::NavRoamObservation;
	using astrabot::runtime::NavRoamReservationBoard;
	using astrabot::runtime::NavRoamReservationFallbackReason;

	auto makeArea = [](astrabot::nav::AreaId id, float lowX, float highX, float lowY, float highY)
	{
		NavArea result = area(id, lowX, highX);
		result.extent.lo.y = lowY;
		result.extent.hi.y = highY;
		return result;
	};
	NavArea start = makeArea(1U, 0.0f, 200.0f, 0.0f, 200.0f);
	NavArea hub = makeArea(2U, 220.0f, 280.0f, 0.0f, 200.0f);
	NavArea distinctGoal = makeArea(12U, 0.0f, 200.0f, 300.0f, 500.0f);
	NavDocument orderedDocument;
	orderedDocument.setSourceIdentity({5U, 100U, 200U});
	start.connections[0U].push_back(2U);
	start.connections[1U].push_back(12U);
	for (astrabot::nav::AreaId id = 3U; id <= 11U; ++id)
	{
		hub.connections[0U].push_back(id);
	}
	orderedDocument.addArea(start);
	orderedDocument.addArea(hub);
	for (astrabot::nav::AreaId id = 3U; id <= 11U; ++id)
	{
		orderedDocument.addArea(makeArea(id, 300.0f + static_cast<float>(id - 3U) * 220.0f,
			500.0f + static_cast<float>(id - 3U) * 220.0f, 0.0f, 200.0f));
	}
	orderedDocument.addArea(distinctGoal);
	NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&orderedDocument, 5U) == astrabot::nav::NavSnapshotResult::Published,
			"reservation-budget NAV fixture publishes"))
	{
		return false;
	}

	NavRoamReservationBoard board;
	board.reset(5U, 7U);
	if (!check(board.reserve(1U, {1U, 1U}, 3U, 1U, 2U), "first Bot claims shared first-link branch"))
	{
		return false;
	}
	NavAreaVisitHistory visits;
	NavRoamController controller(astrabot::compat::RuntimeMode::Enhanced);
	controller.setAreaVisitHistory(&visits);
	controller.setReservationBoard(&board);
	NavRoamObservation observation = {};
	observation.actor = {2U, 1U};
	observation.frame = {5U, 7U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {100.0f, 100.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	NavRoamDecision decision = {};
	const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
	return check(result == astrabot::runtime::NavRoamResult::IntentReady && decision.reservationFallback &&
			decision.reservationFallbackReason == NavRoamReservationFallbackReason::CandidateBudgetExhausted,
		"shared-route fallback records that the distinct-route budget ended");
}

bool testOffMeshCompatibilityKeepsTryingAfterBlockedCandidate()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	document.addArea(area(1U, 0.0f, 64.0f));
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	compat::ScriptedRandomSource random({compat::RandomTapeEntry::floatEntry(
											 "CSBOT-STUCK-JUMP-DELAY", {3U, 1U}, {0U, 0U, 21U}, 0.0f, 0.5f, 0.0f),
		compat::RandomTapeEntry::longEntry("CSBOT-WIGGLE-DIRECTION", {3U, 1U}, {0U, 0U, 21U}, 0, 3, 1),
		compat::RandomTapeEntry::floatEntry("CSBOT-WIGGLE-DURATION", {3U, 1U}, {0U, 0U, 21U}, 0.5f, 1.5f, 0.5f),
		compat::RandomTapeEntry::floatEntry("CSBOT-WIGGLE-JUMP-INTERVAL", {3U, 1U}, {0U, 0U, 21U}, 1.0f, 2.0f, 1.0f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation obs = {};
	obs.actor = {3U, 1U};
	obs.frame = {1U, 1U, 1U};
	obs.locomotion.position = {-8.0f, 32.0f, 0.0f};
	obs.locomotion.grounded = true;
	obs.locomotion.timingAvailable = true;
	nav::LocomotionIntent intent = {};
	runtime::NavRoamDecision decision = {};
	for (std::uint32_t tick = 1U; tick <= 21U; ++tick)
	{
		obs.frame.tick = tick;
		obs.locomotion.observedAt = 0.1f * static_cast<float>(tick - 1U);
		controller.update(publisher.snapshot(), obs, &intent, &decision);
	}
	if (!check(intent.speed > 0.0f && std::fabs(intent.direction.y) > 0.9f &&
				intent.traversal == nav::TraversalAction::Jump && random.verifyComplete(),
			"off-mesh compatibility failure performs timed Wiggle instead of latching neutral forever"))
		return false;
	obs.frame.tick = 22U;
	obs.locomotion.observedAt = 2.1f;
	obs.movementSuppressed = true;
	if (!check(controller.update(publisher.snapshot(), obs, &intent, &decision) ==
					runtime::NavRoamResult::MovementSuppressed &&
				intent.speed == 0.0f,
			"freeze still suppresses off-mesh Wiggle"))
		return false;
	return true;
}

bool test1734DropLaunchAcceptsRecordedGroundedFeet(unsigned completion)
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto ledge = area(1734U, -500.0f, -425.0f);
	ledge.extent.lo = {-500.0f, -650.0f, 125.03221f};
	ledge.extent.hi = {-425.0f, -625.0f, 133.48207f};
	ledge.northEastZ = 106.28207f;
	ledge.southWestZ = 157.03221f;
	ledge.attributes = nav::NavArea::kJump;
	ledge.connections[2U] = {96U};
	auto landing = area(96U, -500.0f, -375.0f);
	landing.extent.lo = {-500.0f, -600.0f, .03125f};
	landing.extent.hi = {-375.0f, -475.0f, .03125f};
	landing.northEastZ = landing.southWestZ = .03125f;
	if (completion == 5U)
		landing.connections[1U] = {999U};
	document.addArea(ledge);
	document.addArea(landing);
	if (completion >= 4U)
	{
		// A synthetic neighboring floor checks corridor membership at landing.
		auto neighbor = area(999U, -375.0f, -250.0f);
		neighbor.extent.lo = {-375.0f, -600.0f, .03125f};
		neighbor.extent.hi = {-250.0f, -475.0f, .03125f};
		neighbor.northEastZ = neighbor.southWestZ = .03125f;
		document.addArea(neighbor);
	}
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	compat::ScriptedRandomSource random(
		{compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {2U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {2U, 1U};
	observation.frame = {1U, 2U, 1U};
	// Saved stationary origin (-443.9,-632.1,178.9), standing mins.z=-36.
	// The actual grounded feet are about11.55 above the interpolated NAV surface.
	observation.locomotion.position = {-443.9f, -632.1f, 142.9f};
	observation.locomotion.grounded = true;
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.safeDropHeightAvailable = true;
	observation.locomotion.maximumSafeDropHeight = 156.25f;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.observedAt = 1.0f;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {-437.5f, -537.5f, .03125f};
	if (completion == 5U)
		observation.objectiveTarget.x = -300.0f;
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (!check(result == runtime::NavRoamResult::IntentReady && intent.traversal == nav::TraversalAction::Drop &&
				intent.currentArea == 1734U && intent.targetArea == 96U && intent.speed > 0.0f &&
				intent.direction.y > 0.0f && decision.failureReason == runtime::NavFailureReason::None &&
				random.verifyComplete(),
			"recorded1734->96 grounded feet start safe descent without NAV launch mismatch"))
		return false;
	observation.frame.tick = 501U;
	observation.locomotion.observedAt = completion == 1U ? 3.1f : 1.8f;
	observation.locomotion.position = {-443.9f, -520.0f, 20.0f};
	observation.locomotion.grounded = false;
	observation.airborne = true;
	const auto flying = controller.update(publisher.snapshot(), observation, &intent, &decision);
	if (completion == 1U)
		return check(flying == runtime::NavRoamResult::ReplanRequired && intent.speed == 0.0f,
			"timed descent still expires after its two-second compatibility budget");
	if (!check(flying == runtime::NavRoamResult::IntentReady,
			"1734 descent survives high server FPS until physical landing feedback"))
		return false;
	observation.frame.tick = 601U;
	observation.locomotion.observedAt = 1.9f;
	observation.locomotion.position = {-443.9f, -500.0f, .03125f};
	if (completion >= 4U)
		observation.locomotion.position.x = -370.0f;
	observation.locomotion.grounded = true;
	observation.airborne = false;
	observation.landingConfirmed = true;
	observation.hasLandingDamage = completion != 2U;
	observation.landingDamage = completion == 3U ? 1.0f : 0.0f;
	const auto landed = controller.update(publisher.snapshot(), observation, &intent, &decision);
	return check((completion == 0U || completion == 5U)
			? (landed == runtime::NavRoamResult::TargetReached &&
				  decision.failureReason == runtime::NavFailureReason::None)
			: (landed == runtime::NavRoamResult::ReplanRequired &&
				  decision.failureReason != runtime::NavFailureReason::None),
		"safe landing in corridor completes descent despite overshoot; unknown/damaged landing stays rejected");
}

bool testRepeated1773DropFailureRearmsDirectedBypass()
{
	using namespace astrabot;
	// Reduced de_dust2 NAV: the saved 1773->243 route begins 1773->139;
	// excluding that edge gives 1773->78->1772->139. Keep actual corner heights.
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto ledge = area(1773U, -250.0f, -225.0f);
	ledge.extent.lo = {-250.0f, 1525.0f, 32.03125f};
	ledge.extent.hi = {-225.0f, 1575.0f, 0.03125f};
	ledge.northEastZ = 0.03125f;
	ledge.southWestZ = 32.03125f;
	ledge.attributes = nav::NavArea::kJump;
	ledge.connections[0U] = {1772U};
	ledge.connections[1U] = {78U};
	ledge.connections[3U] = {139U};
	auto lowerLedge = ledge;
	lowerLedge.id = 1772U;
	lowerLedge.extent.lo.y = 1475.0f;
	lowerLedge.extent.hi.y = 1525.0f;
	lowerLedge.connections[0U].clear();
	lowerLedge.connections[2U] = {1773U};
	auto approach = area(78U, -225.0f, -75.0f);
	approach.extent.lo = {-225.0f, 1400.0f, 0.03125f};
	approach.extent.hi = {-75.0f, 1550.0f, 0.03125f};
	approach.northEastZ = approach.southWestZ = 0.03125f;
	approach.connections[3U] = {1772U, 1773U};
	auto landing = area(139U, -375.0f, -275.0f);
	landing.extent.lo = {-375.0f, 1475.0f, -127.96874f};
	landing.extent.hi = {-275.0f, 1575.0f, -127.96874f};
	landing.northEastZ = landing.southWestZ = -127.96874f;
	for (const auto &entry : {ledge, lowerLedge, approach, landing})
		document.addArea(entry);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	compat::ScriptedRandomSource random(
		{compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {7U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f),
			compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {7U, 1U}, {0U, 0U, 7U}, .4f, .6f, .5f),
			compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {7U, 1U}, {0U, 0U, 13U}, .4f, .6f, .5f),
			compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {7U, 1U}, {0U, 0U, 19U}, .4f, .6f, .5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {7U, 1U};
	observation.frame = {1U, 5U, 1U};
	// Saved standing origin (-236.7,1561,68), mins.z=-36: feet.z=32.
	observation.locomotion.position = {-236.7f, 1561.0f, 32.0f};
	observation.locomotion.grounded = true;
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.safeDropHeightAvailable = true;
	observation.locomotion.maximumSafeDropHeight = 156.25f;
	observation.locomotion.timingAvailable = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {-325.0f, 1525.0f, -127.96874f};
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	for (unsigned update = 0U; update < 4U; ++update)
	{
		observation.frame.tick = 1U + 6U * update;
		observation.locomotion.observedAt = 1.0f + .6f * update;
		if (update == 2U)
		{
			controller.destroyPath();
			if (!check(!controller.hasPath(), "destroyed corridor has no authoritative path despite prior decision"))
				return false;
		}
		const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
		if ((update % 2U) == 0U)
		{
			if (!check(result == runtime::NavRoamResult::ReplanRequired &&
						decision.failureReason == runtime::NavFailureReason::UnsafeDrop && intent.speed == 0.0f,
					"recorded1773->139 unsafe drop remains rejected after a discarded bypass"))
				return false;
		}
		else if (!check(result == runtime::NavRoamResult::IntentReady && intent.targetArea == 78U &&
						 intent.speed > 0.0f && intent.direction.x > 0.0f,
					 "repeated1773->139 failure retries directed bypass instead of latching neutral"))
			return false;
		if ((update % 2U) == 0U)
		{
			const auto consumed = random.position();
			// Actual full updates continue while the 0.5-second retry is pending.
			// They must neither consume the bypass nor request another random delay.
			for (unsigned waiting = 1U; waiting <= 4U; ++waiting)
			{
				observation.frame.tick = 1U + 6U * update + waiting;
				observation.locomotion.observedAt = 1.0f + .6f * update + .1f * waiting;
				if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
								runtime::NavRoamResult::NoRoute &&
							intent.speed == 0.0f && random.position() == consumed,
						"retry cooldown remains neutral and consumes no bypass search RNG"))
					return false;
			}
		}
	}
	return check(random.verifyComplete(), "repeated drop preserves path retry RNG timing and consumption");
}

bool testSafeDescentDoesNotReverseTowardNeighboringLedge()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto source = area(1U, 0.0f, 64.0f);
	source.extent.lo.z = source.extent.hi.z = source.northEastZ = source.southWestZ = 120.0f;
	auto landing = area(2U, 0.0f, 64.0f);
	landing.extent.lo.y = 89.0f;
	landing.extent.hi.y = 189.0f;
	auto neighbor = source;
	neighbor.id = 3U;
	neighbor.extent.lo.x = -64.0f;
	neighbor.extent.hi.x = 0.0f;
	source.connections[2U].push_back(2U);
	neighbor.connections[2U].push_back(2U);
	document.addArea(source);
	document.addArea(landing);
	document.addArea(neighbor);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	runtime::NavRoamController controller;
	runtime::NavRoamObservation obs = {};
	obs.actor = {1U, 1U};
	obs.frame = {1U, 1U, 1U};
	obs.locomotion.position = {32.0f, 10.0f, 120.0f};
	obs.locomotion.grounded = true;
	obs.locomotion.standingClearance = 72.0f;
	obs.locomotion.crouchingClearance = 36.0f;
	obs.locomotion.safeDropHeightAvailable = true;
	obs.locomotion.maximumSafeDropHeight = 156.25f;
	obs.hasObjectiveTarget = true;
	obs.objectiveTarget = {32.0f, 150.0f, 0.0f};
	nav::LocomotionIntent intent = {};
	runtime::NavRoamDecision decision = {};
	controller.update(publisher.snapshot(), obs, &intent, &decision);
	obs.frame.tick = 2U;
	obs.locomotion.position = {-20.0f, 70.0f, 120.0f};
	const auto result = controller.update(publisher.snapshot(), obs, &intent, &decision);
	return check(result == runtime::NavRoamResult::IntentReady && intent.direction.y > 0.0f &&
			decision.stage != runtime::NavRoamStage::OffMeshRecovery,
		"safe descent across adjacent connected ledge retains forward intent across the NAV gap");
}

bool testOffMeshRecoveryHasHorizontalProgressAndBounds()
{
	using namespace astrabot;
	bool passed = true;
	for (int scenario = 0; scenario < 4; ++scenario)
	{
		nav::NavDocument document;
		document.setSourceIdentity({5U, 100U, 200U});
		document.addArea(area(1U, 0.0f, 64.0f));
		if (scenario == 1)
		{
			nav::NavArea upper = area(2U, 0.0f, 64.0f);
			upper.extent.lo.z = upper.extent.hi.z = 96.0f;
			upper.northEastZ = upper.southWestZ = 96.0f;
			document.addArea(upper);
		}
		nav::NavSnapshotPublisher publisher;
		publisher.publish(&document, 1U);
		runtime::NavRoamController controller;
		runtime::NavRoamObservation observation = {};
		observation.actor = {3U, 1U};
		observation.frame = {1U, 8U, 40158U};
		observation.locomotion.position = {-0.25f, 32.0f, 40.0f};
		observation.locomotion.grounded = true;
		if (scenario == 1)
			observation.locomotion.position.z = 96.0f;
		if (scenario == 2)
			observation.locomotion.position = {32.0f, 32.0f, 200.0f};
		nav::LocomotionIntent intent = {};
		runtime::NavRoamDecision decision = {};
		auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
		if (scenario == 2)
		{
			passed = check(result == runtime::NavRoamResult::NoRoute && intent.speed == 0.0f,
						 "recovery does not steer vertically toward a remote stacked floor") &&
				passed;
			continue;
		}
		passed =
			check(result == runtime::NavRoamResult::IntentReady && std::fabs(intent.direction.x - 1.0f) < 0.001f &&
					intent.direction.z == 0.0f && intent.targetPosition.x > 0.0f && intent.targetPosition.x < 64.0f &&
					decision.targetPosition.x == intent.targetPosition.x && intent.speed == 240.0f,
				"small XY / large Z recovery moves horizontally into the area and reports its target") &&
			passed;
		if (scenario == 1)
		{
			passed = check(intent.targetArea == 2U,
						 "stacked NAV recovery prefers the compatible floor over the lower area id") &&
				passed;
		}
		if (scenario != 3)
			continue;
		// Replay the observed 12-server-frame gap, with 100 suppressed updates.
		observation.movementSuppressed = true;
		for (int update = 0; update < 100; ++update)
		{
			observation.frame.tick += 12U;
			result = controller.update(publisher.snapshot(), observation, &intent, &decision);
		}
		passed = check(result == runtime::NavRoamResult::MovementSuppressed,
					 "freeze suppresses off-mesh recovery without spending its progress budget") &&
			passed;
		observation.movementSuppressed = false;
		observation.frame.tick += 12U;
		result = controller.update(publisher.snapshot(), observation, &intent, &decision);
		passed =
			check(result == runtime::NavRoamResult::IntentReady, "off-mesh recovery resumes after a long freeze") &&
			passed;
		for (int update = 0; update < 35; ++update)
		{
			observation.frame.tick += 12U;
			result = controller.update(publisher.snapshot(), observation, &intent, &decision);
		}
		passed = check(result == runtime::NavRoamResult::NoRoute && intent.speed == 0.0f &&
						 decision.failureReason != runtime::NavFailureReason::CurrentAreaMissing,
					 "stationary off-mesh recovery ends with an explicit bounded no-progress failure") &&
			passed;
		observation.actor.actorGeneration = 2U;
		observation.frame.tick += 12U;
		result = controller.update(publisher.snapshot(), observation, &intent, &decision);
		passed = check(result == runtime::NavRoamResult::IntentReady,
					 "new actor generation does not inherit a blocked recovery") &&
			passed;
	}
	return passed;
}

bool testRecoveryBudgetAndStateTransitions()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	document.addArea(area(1U, 0.0f, 64.0f));
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	runtime::NavRoamObservation observation = {};
	observation.actor = {3U, 1U};
	observation.frame = {1U, 8U, 40158U};
	observation.locomotion.position = {-8.0f, 32.0f, 40.0f};
	observation.locomotion.grounded = true;
	bool passed = true;
	for (int transition = 0; transition < 8; ++transition)
	{
		runtime::NavRoamController controller;
		auto obs = observation;
		nav::LocomotionIntent intent = {};
		runtime::NavRoamDecision decision = {};
		controller.update(publisher.snapshot(), obs, &intent, &decision);
		for (int update = 1; update <= 20; ++update)
		{
			obs.frame.tick += 12U;
			const auto result = controller.update(publisher.snapshot(), obs, &intent, &decision);
			passed = check(update < 20 ? result == runtime::NavRoamResult::IntentReady
									   : (result == runtime::NavRoamResult::NoRoute && intent.speed == 0.0f &&
											 intent.direction.x == 0.0f && intent.direction.y == 0.0f &&
											 decision.failureReason == runtime::NavFailureReason::RecoveryNoProgress),
						 "recovery fails with neutral intent exactly at twenty no-progress updates") &&
				passed;
		}
		obs.frame.tick += 12U;
		passed =
			check(controller.update(publisher.snapshot(), obs, &intent, &decision) == runtime::NavRoamResult::NoRoute &&
					intent.speed == 0.0f,
				"blocked recovery stays neutral at the same position") &&
			passed;
		if (transition == 0)
			obs.actor.actorGeneration++;
		if (transition == 1)
			obs.frame.roundGeneration++;
		if (transition == 2)
		{
			obs.frame.mapGeneration = 2U;
			publisher.publish(&document, 2U);
		}
		if (transition == 3)
		{
			obs.hasObjectiveTarget = true;
			obs.objectiveTarget = {32.0f, 32.0f, 0.0f};
		}
		if (transition == 4)
			controller.reset();
		if (transition == 5)
			obs.locomotion.position.y += 8.0f;
		if (transition == 6 || transition == 7)
		{
			if (transition == 7)
			{
				obs.hasObjectiveTarget = true;
				obs.objectiveTarget = {1000000.0f, 1000000.0f, 0.0f};
			}
			obs.frame.tick += 12U;
			obs.locomotion.position = {16.0f, 32.0f, 0.0f};
			controller.update(publisher.snapshot(), obs, &intent, &decision);
			obs.locomotion.position = observation.locomotion.position;
			obs.hasObjectiveTarget = false;
		}
		obs.frame.tick += 12U;
		passed =
			check(controller.update(publisher.snapshot(), obs, &intent, &decision) ==
						(transition == 3 ? runtime::NavRoamResult::NoRoute : runtime::NavRoamResult::IntentReady) &&
					(transition == 3 || decision.recoveryNoProgressUpdates == 0U),
				"lifecycle/push/mesh entry resets recovery; objective change retains failed candidates") &&
			passed;
		publisher.publish(&document, 1U);
	}
	for (int moving = 0; moving < 2; ++moving)
	{
		runtime::NavRoamController controller;
		auto obs = observation;
		nav::LocomotionIntent intent = {};
		runtime::NavRoamDecision decision = {};
		controller.update(publisher.snapshot(), obs, &intent, &decision);
		runtime::NavRoamResult result = runtime::NavRoamResult::InvalidArgument;
		for (int update = 1; update <= 25; ++update)
		{
			obs.frame.tick += 12U;
			obs.locomotion.position.x = moving ? -8.0f + update * 0.2f : -8.0f + (update % 2 ? 0.25f : -0.25f);
			result = controller.update(publisher.snapshot(), obs, &intent, &decision);
		}
		passed =
			check(moving ? result == runtime::NavRoamResult::IntentReady : result == runtime::NavRoamResult::NoRoute,
				"accumulated forward progress resets budget while sub-unit oscillation does not") &&
			passed;
	}
	nav::NavDocument narrow;
	narrow.setSourceIdentity({5U, 100U, 200U});
	auto thin = area(2U, 0.0f, 6.0f);
	thin.extent.hi.y = 6.0f;
	narrow.addArea(thin);
	thin.id = 1U;
	narrow.addArea(thin);
	publisher.publish(&narrow, 1U);
	auto obs = observation;
	obs.locomotion.position = {-0.25f, 3.0f, 64.0f};
	runtime::NavRoamController controller;
	nav::LocomotionIntent intent = {};
	runtime::NavRoamDecision decision = {};
	passed =
		check(controller.update(publisher.snapshot(), obs, &intent, &decision) == runtime::NavRoamResult::IntentReady &&
				intent.targetArea == 1U && intent.targetPosition.x == 3.0f && intent.targetPosition.y == 3.0f,
			"narrow-area inset remains inside, Z64 is accepted, and ties use stable area id") &&
		passed;
	obs.frame.tick += 12U;
	obs.locomotion.position.z = 64.01f;
	return check(controller.update(publisher.snapshot(), obs, &intent, &decision) == runtime::NavRoamResult::NoRoute,
			   "incompatible layer beyond Z64 is rejected") &&
		passed;
}

bool testRecoveryReselectsBeforeExhaustion()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	for (nav::AreaId id = 1U; id <= 5U; ++id)
	{
		auto candidate = area(id, (id - 1U) * 64.0f, (id - 1U) * 64.0f + 64.0f);
		if (id < 5U)
			candidate.connections[1].push_back(id + 1U);
		document.addArea(candidate);
	}
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	runtime::NavRoamObservation obs = {};
	obs.actor = {3U, 1U};
	obs.frame = {1U, 8U, 100U};
	obs.locomotion.position = {-8.0f, 32.0f, 40.0f};
	obs.locomotion.grounded = true;
	obs.locomotion.standingClearance = 72.0f;
	obs.locomotion.crouchingClearance = 36.0f;
	obs.collectPathStats = true;
	obs.hasObjectiveTarget = true;
	obs.objectiveTarget = {288.0f, 32.0f, 0.0f};
	bool passed = true;
	for (int enterMesh = 0; enterMesh < 2; ++enterMesh)
	{
		runtime::NavRoamController controller;
		auto observation = obs;
		nav::LocomotionIntent intent = {};
		runtime::NavRoamDecision decision = {};
		for (nav::AreaId attempted = 1U; attempted <= (enterMesh ? 1U : 4U); ++attempted)
		{
			observation.frame.tick++;
			passed = check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
								 runtime::NavRoamResult::IntentReady &&
							 intent.targetArea == attempted,
						 "no-progress recovery selects a different untried candidate") &&
				passed;
			for (int update = 0; update < 20; ++update)
			{
				observation.frame.tick++;
				controller.update(publisher.snapshot(), observation, &intent, &decision);
			}
			passed = check(decision.failureReason == runtime::NavFailureReason::RecoveryNoProgress &&
							 intent.speed == 0.0f && decision.recoveryArea == attempted,
						 "candidate failure is reported with the failed area before reselection") &&
				passed;
		}
		observation.frame.tick++;
		// A higher-level goal replacement must not restart the failed-candidate loop.
		observation.objectiveTarget = {224.0f, 32.0f, 0.0f};
		const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
		passed = check(enterMesh ? result == runtime::NavRoamResult::IntentReady && intent.targetArea == 2U
								 : result == runtime::NavRoamResult::NoRoute && intent.speed == 0.0f,
					 "goal replacement retains exclusions and four-attempt limit") &&
			passed;
		if (enterMesh)
		{
			observation.frame.tick++;
			observation.locomotion.position = {96.0f, 32.0f, 0.0f};
			const auto entered = controller.update(publisher.snapshot(), observation, &intent, &decision);
			passed = check(entered == runtime::NavRoamResult::IntentReady &&
							 decision.pathSearchStats.searchCalls > 0U && decision.currentArea == 2U,
						 "successful alternate NAV entry resumes objective path search") &&
				passed;
		}
		else
		{
			for (int update = 0; update < 100; ++update)
			{
				observation.frame.tick++;
				passed = check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
									 runtime::NavRoamResult::NoRoute &&
								 intent.speed == 0.0f,
							 "exhausted attempt budget never cycles into an unbounded retry") &&
					passed;
			}
		}
	}
	return passed;
}

bool testUnsafeDropRetryPolicy(
	std::uint32_t cadence, int release, bool bypass, bool prefix = false, bool launch = false, bool handoff = false)
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 256.0f);
	auto goal = area(2U, prefix ? 512.0f : 256.0f, prefix ? 768.0f : 512.0f);
	start.extent.hi.y = goal.extent.hi.y = 256.0f;
	start.extent.lo.z = start.extent.hi.z = start.northEastZ = start.southWestZ = 100.0f;
	start.connections[1U].push_back(prefix ? 3U : 2U);
	if (bypass)
		start.connections[2U].push_back(3U);
	document.addArea(start);
	document.addArea(goal);
	if (handoff)
	{
		auto farStart = area(5U, -512.0f, -448.0f);
		farStart.connections[1U].push_back(2U);
		document.addArea(farStart);
	}
	if (prefix)
	{
		auto middle = area(3U, 256.0f, 512.0f);
		middle.extent.hi.y = 256.0f;
		middle.extent.lo.z = middle.extent.hi.z = middle.northEastZ = middle.southWestZ = 100.0f;
		middle.connections[1U].push_back(2U);
		document.addArea(middle);
	}
	if (bypass)
	{
		auto side = area(3U, 0.0f, 256.0f);
		auto corner = area(4U, 256.0f, 512.0f);
		side.extent.lo.y = corner.extent.lo.y = 256.0f;
		side.extent.hi.y = corner.extent.hi.y = 512.0f;
		side.extent.lo.z = side.extent.hi.z = side.northEastZ = side.southWestZ = 60.0f;
		corner.extent.lo.z = corner.extent.hi.z = corner.northEastZ = corner.southWestZ = 30.0f;
		side.connections[1U].push_back(4U);
		corner.connections[0U].push_back(2U);
		document.addArea(side);
		document.addArea(corner);
	}
	nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) == nav::NavSnapshotResult::Published,
			"unsafe retry policy snapshot published"))
		return false;
	runtime::NavRoamController controller;
	runtime::NavRoamObservation obs = {};
	obs.actor = {1U, 1U};
	obs.frame = {1U, 1U, 1U};
	obs.collectPathStats = true;
	obs.hasObjectiveTarget = true;
	obs.objectiveTarget = {prefix ? 640.0f : 384.0f, 128.0f, 0.0f};
	obs.objectiveTargetEntity = {31U, 4U};
	// Outside the 32-unit launch tolerance, inside locomotion's 80-unit lookahead.
	obs.locomotion.position = {190.0f, 128.0f, 100.0f};
	obs.locomotion.grounded = true;
	obs.locomotion.standingClearance = 72.0f;
	obs.locomotion.crouchingClearance = 36.0f;
	obs.locomotion.safeDropHeightAvailable = true;
	obs.locomotion.maximumSafeDropHeight = 25.0f;
	nav::LocomotionIntent intent = {};
	runtime::NavRoamDecision decision = {};
	team::RoundObjectiveCoordinator coordinator;
	team::TeamObjectiveInput teamInput = {};
	team::TeamObjectiveAssignmentSet assignments = {};
	team::TeamObjectiveAssignment failedAssignment = {};
	if (handoff)
	{
		teamInput.mapGeneration = teamInput.roundGeneration = teamInput.frameSequence = 1U;
		teamInput.actorCount = 2U;
		teamInput.actors[0] = {
			{1U, 1U}, objectives::TeamRole::CounterTerrorist, true, false, true, {190.0f, 128.0f, 100.0f}, 1U};
		teamInput.actors[1] = {
			{2U, 1U}, objectives::TeamRole::CounterTerrorist, true, false, true, {-480.0f, 32.0f, 0.0f}, 5U};
		teamInput.plantedC4 = {true, obs.objectiveTargetEntity, {384.0f, 128.0f, 0.0f}, 2U};
		if (!check(coordinator.assign(teamInput, publisher.snapshot(), &assignments) ==
						team::TeamObjectiveResult::Assigned &&
					assignments.assignments[0].actor.slot == 1U,
				"nearer CT receives bomb before actual UnsafeDrop failure"))
			return false;
		failedAssignment = assignments.assignments[0];
	}
	if (prefix)
	{
		if (!check(
				controller.update(publisher.snapshot(), obs, &intent, &decision) == runtime::NavRoamResult::IntentReady,
				"safe walk prefix starts before unsafe next link"))
			return false;
		obs.frame.tick = 2U;
		obs.locomotion.position.x = launch ? 466.0f : 446.0f;
		const auto rejected = controller.update(publisher.snapshot(), obs, &intent, &decision);
		return check(rejected == runtime::NavRoamResult::ReplanRequired &&
				decision.failureReason == runtime::NavFailureReason::UnsafeDrop && decision.linkFromArea == 3U &&
				decision.linkToArea == 2U,
			"locomotion unsafe failure identifies current dangerous link after a safe prefix");
	}
	if (!check(controller.update(publisher.snapshot(), obs, &intent, &decision) ==
					runtime::NavRoamResult::ReplanRequired &&
				decision.failureReason == runtime::NavFailureReason::UnsafeDrop,
			"unsafe objective is rejected before retry policy"))
		return false;
	if (handoff)
	{
		coordinator.reportPathFailure(failedAssignment.actor, failedAssignment.generation, 0U, 1U);
		teamInput.frameSequence = 2U;
		if (!check(coordinator.assign(teamInput, publisher.snapshot(), &assignments) ==
						team::TeamObjectiveResult::Assigned &&
					assignments.assignments[0].actor.slot == 2U,
				"actual controller UnsafeDrop hands bomb to another CT through existing coordinator"))
			return false;
		const std::uint32_t replacementGeneration = assignments.assignments[0].generation;
		obs.frame.tick = 2U;
		controller.update(publisher.snapshot(), obs, &intent, &decision);
		for (std::uint32_t tick = 3U; tick < 100U; ++tick)
		{
			obs.frame.tick = teamInput.frameSequence = tick;
			obs.hasObjectiveTarget = tick % 2U == 0U;
			controller.update(publisher.snapshot(), obs, &intent, &decision);
			if (!check(decision.pathSearchStats.searchCalls == 0U,
					"failed CT Hunt/objective toggles do not restart movement A* after handoff"))
				return false;
			coordinator.reportPathFailure(failedAssignment.actor, failedAssignment.generation, 0U, tick);
			if (!check(coordinator.assign(teamInput, publisher.snapshot(), &assignments) ==
							team::TeamObjectiveResult::Assigned &&
						assignments.assignments[0].actor.slot == 2U &&
						assignments.assignments[0].generation == replacementGeneration,
					"repeated old CT failure cannot evict cached replacement or rescore candidates"))
				return false;
		}
		// Adapter physical UnsafeDrop reports startArea=0: a neighboring NAV
		// area identity alone cannot establish that the physical veto is gone.
		runtime::NavRoamController replacementController;
		auto replacementObs = obs;
		replacementObs.actor = {2U, 1U};
		replacementObs.hasObjectiveTarget = true;
		replacementObs.frame.tick = 100U;
		if (!check(replacementController.update(publisher.snapshot(), replacementObs, &intent, &decision) ==
						runtime::NavRoamResult::ReplanRequired &&
					decision.failureReason == runtime::NavFailureReason::UnsafeDrop,
				"replacement CT also emits an actual physical UnsafeDrop"))
			return false;
		coordinator.reportPathFailure(assignments.assignments[0].actor, replacementGeneration, 0U, 100U);
		teamInput.actors[1].currentArea = 1U;
		for (std::uint32_t tick = 101U; tick < 129U; ++tick)
		{
			teamInput.frameSequence = tick;
			teamInput.actors[0].currentArea = tick % 2U == 0U ? 1U : 5U;
			teamInput.actors[1].currentArea = tick % 2U == 0U ? 5U : 1U;
			if (!check(coordinator.assign(teamInput, publisher.snapshot(), &assignments) ==
							team::TeamObjectiveResult::NoObjective &&
						assignments.count == 0U,
					"all physically failed CTs stay excluded through NAV area jitter without candidate rescoring"))
				return false;
		}
		teamInput.frameSequence = 129U;
		teamInput.actors[0].currentArea = teamInput.actors[1].currentArea = 1U;
		if (!check(coordinator.assign(teamInput, publisher.snapshot(), &assignments) ==
						team::TeamObjectiveResult::Assigned &&
					assignments.assignments[0].actor.slot == 1U,
				"physical exclusion still expires at 128 server frames rather than permanently blocking all CTs"))
			return false;
		return true;
	}
	obs.frame.tick = 2U;
	const auto alternative = controller.update(publisher.snapshot(), obs, &intent, &decision);
	if (bypass)
	{
		return check(alternative == runtime::NavRoamResult::IntentReady &&
				decision.selectedPath == std::vector<nav::AreaId>({1U, 3U, 4U, 2U}) && intent.speed > 0.0f &&
				intent.traversal == nav::TraversalAction::Walk,
			"existing directed alternative reaches same objective through safe descending steps");
	}
	if (!check(alternative == runtime::NavRoamResult::NoRoute, "no bypass enters bounded retry interval"))
		return false;
	// Freeze time must not spend the budget, including the first resume update.
	obs.movementSuppressed = true;
	obs.frame.tick = 3U;
	controller.update(publisher.snapshot(), obs, &intent, &decision);
	obs.frame.tick = 1000U;
	controller.update(publisher.snapshot(), obs, &intent, &decision);
	obs.movementSuppressed = false;
	obs.frame.tick = 1001U;
	if (!check(controller.update(publisher.snapshot(), obs, &intent, &decision) == runtime::NavRoamResult::NoRoute &&
				decision.pathSearchStats.searchCalls == 0U &&
				decision.failureReason == runtime::NavFailureReason::UnsafeDrop,
			"freeze and resume preserve unsafe retry budget and typed failure"))
		return false;
	if (release != 0)
	{
		obs.frame.tick++;
		if (release == 1)
			obs.objectiveTargetEntity.id++;
		if (release == 2)
			obs.objectiveTargetEntity.generation++;
		if (release == 3)
			obs.locomotion.position.x -= 80.0f;
		if (release == 7)
		{
			obs.frame.tick += 128U;
			obs.locomotion.maximumSafeDropHeight = 156.25f;
		}
		if (release == 4)
			obs.actor.actorGeneration++;
		if (release == 5)
			obs.frame.roundGeneration++;
		if (release == 6)
		{
			obs.frame.mapGeneration++;
			if (!check(publisher.publish(&document, 2U) == nav::NavSnapshotResult::Published,
					"new map snapshot published"))
				return false;
		}
		controller.update(publisher.snapshot(), obs, &intent, &decision);
		return check(decision.pathSearchStats.searchCalls > 0U,
				   "new bomb id or generation, grounded progress, lifecycle and expiry release old failure") &&
			check(release != 7 || (decision.failureReason == runtime::NavFailureReason::None && intent.speed > 0.0f),
				"expiry reevaluates the original directed link when safe physics becomes available");
	}
	// Full updates spaced as 0.1s at 60, 90 and approximately 128 FPS.
	std::uint32_t lastSearchElapsed = 0U;
	std::uint32_t expectedDelay = 128U;
	std::uint32_t retries = 0U;
	for (std::uint32_t elapsed = cadence; elapsed <= 1400U; elapsed += cadence)
	{
		obs.frame.tick = 1001U + elapsed;
		obs.locomotion.position.y = 128.0f + ((elapsed / cadence) % 2U == 0U ? 16.0f : -16.0f);
		const auto result = controller.update(publisher.snapshot(), obs, &intent, &decision);
		if (!check((result == runtime::NavRoamResult::NoRoute || result == runtime::NavRoamResult::ReplanRequired) &&
					intent.speed == 0.0f,
				"retry remains safe and bounded without a reachable bypass"))
			return false;
		if (decision.pathSearchStats.searchCalls > 0U)
		{
			const std::uint32_t delta = elapsed - lastSearchElapsed;
			if (!check(delta >= expectedDelay && delta < expectedDelay + cadence,
					"128/256/512 server-frame retry delay independent of full-update spacing"))
				return false;
			lastSearchElapsed = elapsed;
			expectedDelay = (std::min)(512U, expectedDelay * 2U);
			++retries;
		}
	}
	return check(retries == 3U, "repeated unsafe search loop replaced by capped active-frame retries");
}

bool testInvestigationSafestRoute()
{
	using namespace astrabot;
	nav::NavDocument doc;
	doc.setSourceIdentity({1, 0, 0});
	nav::NavArea first{};
	first.id = 1;
	first.extent.lo = {0, 0, 0};
	first.extent.hi = {100, 100, 0};
	first.connections[1].push_back(2);
	auto second = first;
	second.id = 2;
	second.extent.lo.x = 100;
	second.extent.hi.x = 200;
	second.connections[1].clear();
	doc.addArea(first);
	doc.addArea(second);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&doc, 1);
	runtime::NavRoamController controller;
	runtime::NavRoamObservation observation{};
	observation.actor = {1, 1};
	observation.frame = {1, 1, 1};
	observation.locomotion.position = {16, 50, 0};
	observation.locomotion.standingClearance = 72;
	observation.locomotion.crouchingClearance = 36;
	observation.locomotion.grounded = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {180, 50, 0};
	observation.objectiveRouteType = nav::NavRouteType::Safest;
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
	return check(result == runtime::NavRoamResult::IntentReady && decision.routeType == nav::NavRouteType::Safest,
		"investigation objective requests SAFEST route through movement controller");
}

bool testInvestigationRetainsPathWhenThrottled()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	for (nav::AreaId id = 1; id <= 3; ++id)
	{
		auto piece = area(id, (id - 1U) * 128.0f, id * 128.0f);
		if (id < 3)
			piece.connections[1U] = {id + 1U};
		document.addArea(piece);
	}
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	runtime::NavRoamController controller;
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, 0.4f, 0.6f, 0.5f),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 4U}, 0.4f, 0.6f, 0.5f),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 7U}, 0.4f, 0.6f, 0.5f),
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 8U}, 0.4f, 0.6f, 0.5f)});
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 0U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {192.0f, 32.0f, 0.0f};
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	const auto update = [&](float now, runtime::NoiseRouteUpdate noise)
	{
		++observation.frame.tick;
		observation.locomotion.observedAt = now;
		observation.noiseRoute = noise;
		return controller.update(publisher.snapshot(), observation, &intent, &decision);
	};
	if (!check(update(1.0f, runtime::NoiseRouteUpdate::None) == runtime::NavRoamResult::IntentReady &&
			decision.selectedPath == std::vector<nav::AreaId>{1U, 2U},
			"the bot starts with an existing route and a pending path retry deadline"))
		return false;
	const auto originalPath = decision.pathSequence;
	observation.objectiveTarget.x = 320.0f;
	observation.objectiveRouteType = nav::NavRouteType::Safest;
	if (!check(update(1.5f, runtime::NoiseRouteUpdate::Enter) == runtime::NavRoamResult::MovementSuppressed &&
			decision.pathSequence == originalPath && intent.speed == 0.0f && random.position() == 1U,
			"noise entry at the retry deadline keeps the old route and does not roll another delay"))
		return false;
	if (!check(update(1.6f, runtime::NoiseRouteUpdate::Follow) == runtime::NavRoamResult::IntentReady &&
			decision.pathSequence == originalPath && decision.selectedPath == std::vector<nav::AreaId>{1U, 2U} &&
			random.position() == 1U,
			"the investigation follows the old route without retrying the consumed sound"))
		return false;
	observation.objectiveTarget.x = 96.0f;
	if (!check(update(1.7f, runtime::NoiseRouteUpdate::Redirect) == runtime::NavRoamResult::IntentReady &&
			decision.pathSequence != originalPath && decision.selectedPath == std::vector<nav::AreaId>{1U} &&
			intent.targetPosition.x == 96.0f && decision.routeType == nav::NavRouteType::Safest &&
			random.position() == 2U,
			"a newly attended sound replaces the path with a same-area SAFEST route"))
		return false;
	observation.locomotion.position.x = 90.0f;
	if (!check(update(1.8f, runtime::NoiseRouteUpdate::Follow) == runtime::NavRoamResult::TargetReached &&
			update(2.4f, runtime::NoiseRouteUpdate::Follow) == runtime::NavRoamResult::NoRoute &&
			random.position() == 2U,
			"the state cannot rebuild the completed path for a sound it already consumed"))
		return false;
	observation.locomotion.position.x = -50.0f;
	observation.noisePathStartArea = 1U;
	if (!check(update(2.6f, runtime::NoiseRouteUpdate::Enter) == runtime::NavRoamResult::MovementSuppressed &&
			decision.selectedPath == std::vector<nav::AreaId>{1U} && random.position() == 3U,
			"noise entry starts from the last known area when the bot is temporarily outside the mesh"))
		return false;
	observation.locomotion.position.x = 32.0f;
	observation.objectiveTarget.x = 100.0f;
	observation.noiseTargetCleared = true;
	return check(update(3.2f, runtime::NoiseRouteUpdate::Redirect) == runtime::NavRoamResult::TargetReached &&
			intent.speed == 0.0f && !controller.hasPath() && random.verifyComplete(),
		"a visible redirected sound consumes its route attempt before completing the investigation");
}

bool testInvestigationKeepsClosestReachablePath()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, -64.0f, 64.0f);
	start.connections[1U] = {3U, 2U};
	auto far = area(3U, 550.0f, 650.0f);
	far.extent.lo.y = 64.0f;
	far.extent.hi.y = 128.0f;
	auto goal = area(4U, 300.0f, 950.0f);
	goal.extent.lo.z = goal.extent.hi.z = goal.northEastZ = goal.southWestZ = 12.0f;
	document.addArea(start);
	document.addArea(area(2U, 170.0f, 270.0f));
	document.addArea(far);
	document.addArea(goal);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, 0.4f, 0.6f, 0.5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 1U;
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.observedAt = 1.0f;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {320.0f, 32.0f, 12.0f};
	observation.objectiveRouteType = nav::NavRouteType::Safest;
	observation.noiseRoute = runtime::NoiseRouteUpdate::Enter;
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				runtime::NavRoamResult::MovementSuppressed && decision.selectedPath == std::vector<nav::AreaId>{1U, 2U} &&
			decision.huntEndpoint.x == 320.0f && decision.huntEndpoint.z == 12.0f,
			"disconnected noise keeps the corridor nearest its position and the original endpoint"))
		return false;
	++observation.frame.tick;
	observation.locomotion.observedAt = 1.12f;
	observation.noiseRoute = runtime::NoiseRouteUpdate::Follow;
	if (!check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
				runtime::NavRoamResult::IntentReady && intent.targetArea == 2U && random.verifyComplete(),
			"investigation follows the partial corridor without another route attempt"))
		return false;
	nav::NavCorridor bypass{};
	const nav::NavDirectedLink rejected = {1U, 2U, 1U, 1U};
	if (!check(nav::NavQuery(publisher.snapshot()).buildInvestigationCorridor(1U, 4U, observation.objectiveTarget,
			nullptr, &bypass, nullptr, &rejected) == nav::NavQueryResult::Found &&
			bypass.areas == std::vector<nav::AreaId>{1U, 3U} && bypass.endpoint.x == 320.0f && bypass.endpoint.z == 12.0f,
			"excluding an unsafe first link lets the investigation fall back to the next closest reachable area"))
		return false;

	controller.reset();
	compat::ScriptedRandomSource isolatedRandom({
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, 0.4f, 0.6f, 0.5f)});
	controller.setRandomSource(&isolatedRandom);
	observation.frame.tick = 1U;
	observation.locomotion.position = {600.0f, 96.0f, 0.0f};
	observation.noiseRoute = runtime::NoiseRouteUpdate::Enter;
	return check(controller.update(publisher.snapshot(), observation, &intent, &decision) ==
			runtime::NavRoamResult::MovementSuppressed && decision.selectedPath == std::vector<nav::AreaId>{3U} &&
		decision.huntEndpoint.x == 320.0f && decision.huntEndpoint.z == 0.0f && isolatedRandom.verifyComplete(),
		"an isolated start retains the noise XY and projects the trivial path onto its own floor");
}

bool testInvestigationSearchKeepsPositionWhenImprovingPath()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	const auto add = [&](nav::AreaId id, float x, float y, std::initializer_list<nav::AreaId> neighbors)
	{
		const float radius = id == 5U ? 180.0f : 2.0f;
		auto piece = area(id, x - radius, x + radius);
		piece.extent.lo.y = y - radius;
		piece.extent.hi.y = y + radius;
		piece.connections[1U] = neighbors;
		document.addArea(piece);
	};
	add(1U, 32.0f, 256.0f, {2U, 3U, 4U});
	add(2U, 320.0f, 176.0f, {5U});
	add(3U, 224.0f, 416.0f, {5U});
	add(4U, 80.0f, 432.0f, {5U});
	add(5U, 224.0f, 208.0f, {});
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1U);
	nav::NavCorridor corridor{};
	return check(nav::NavQuery(publisher.snapshot()).buildInvestigationCorridor(
			1U, 5U, {240.0f, 384.0f, 0.0f}, nullptr, &corridor) == nav::NavQueryResult::Found &&
			corridor.areas == std::vector<nav::AreaId>{1U, 2U, 5U},
		"improving a queued path still estimates the distance to the explicit noise position");
}

bool testRemainingPathDistance()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	for (nav::AreaId id = 1; id <= 4; ++id)
	{
		auto piece = area(id, (id - 1) * 64.0f, id * 64.0f);
		if (id < 4)
			piece.connections[1] = {id + 1};
		if (id > 1)
			piece.connections[3] = {id - 1};
		document.addArea(piece);
	}
	nav::NavSnapshotPublisher publisher;
	if (!check(
			publisher.publish(&document, 1U) == nav::NavSnapshotResult::Published, "remaining path NAV is published"))
		return false;
	const auto snapshot = publisher.snapshot();
	runtime::NavRoamController controller;
	runtime::NavRoamObservation observation{};
	observation.actor = {1, 1};
	observation.frame = {1, 1, 1};
	observation.locomotion.position = {32, 32, 0};
	observation.locomotion.grounded = true;
	observation.locomotion.standingClearance = 72;
	observation.locomotion.crouchingClearance = 36;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {224, 32, 0};
	nav::LocomotionIntent intent{};
	const bool noPath = controller.pathDistanceRemaining(snapshot) == -1;
	const auto result = controller.update(snapshot, observation, &intent);
	const bool distance =
		result == runtime::NavRoamResult::IntentReady && controller.pathDistanceRemaining(snapshot) == 128;
	const bool invalid = std::isinf(controller.pathDistanceRemaining({}));
	controller.destroyPath();
	return check(noPath && distance && invalid && controller.pathDistanceRemaining(snapshot) == -1,
		"remaining path sums the area centers after the active node and respects path/snapshot invalidation");
}

bool testDefuseMoveToPreparesPathBeforeMoving()
{
	using namespace astrabot;
	nav::NavDocument document;
	document.addArea(area(1U, 0.0f, 128.0f));
	nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) == nav::NavSnapshotResult::Published, "C4 endpoint NAV publishes"))
		return false;
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 2U;
	observation.locomotion.position = {16.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	observation.locomotion.grounded = true;
	observation.locomotion.timingAvailable = true;
	observation.locomotion.observedAt = 1.0f;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {96.0f, 32.0f, 0.0f};
	observation.nativeSameAreaEndpoint = true;
	observation.prepareObjectivePathOnly = true;
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	const auto snapshot = publisher.snapshot();
	if (!check(controller.update(snapshot, observation, &intent, &decision) ==
				runtime::NavRoamResult::ObjectivePathPrepared &&
				controller.hasPath() && decision.pathSequence != 0U && intent.speed == 0.0f && random.verifyComplete(),
			"Defuse MoveTo entry draws the retry and prepares a path without movement"))
		return false;
	const auto path = decision.pathSequence;
	observation.frame.tick = 2U;
	observation.locomotion.observedAt = 1.125f;
	observation.prepareObjectivePathOnly = false;
	if (!check(controller.update(snapshot, observation, &intent, &decision) == runtime::NavRoamResult::IntentReady &&
			decision.pathSequence == path && intent.speed > 0.0f && random.verifyComplete(),
			"Defuse MoveTo follows the path on the next update without another draw"))
		return false;
	controller.destroyPath();
	observation.frame.tick = 3U;
	observation.locomotion.observedAt = 1.25f;
	observation.prepareObjectivePathOnly = true;
	return check(controller.update(snapshot, observation, &intent, &decision) ==
			runtime::NavRoamResult::ObjectivePathPrepared &&
			!controller.hasPath() && decision.pathSequence == 0U && intent.speed == 0.0f && random.verifyComplete(),
		"a throttled entry reports the missing path without drawing or moving");
}

bool testDefuseMoveToRetainsFirstPointAfterDrift(bool clear)
{
	using namespace astrabot;
	constexpr float floor = 0.03125f;
	const auto makeArea = [&](nav::AreaId id, float xLow, float yLow, float xHigh, float yHigh) {
		auto value = area(id, xLow, xHigh);
		value.extent.lo = {xLow, yLow, floor};
		value.extent.hi = {xHigh, yHigh, floor};
		value.northEastZ = value.southWestZ = floor;
		return value;
	};
	auto outside = makeArea(54U, -1600.0f, 2325.0f, -1425.0f, 2575.0f);
	auto start = makeArea(293U, -1600.0f, 2575.0f, -1475.0f, 2625.0f);
	auto goal = makeArea(53U, -1650.0f, 2625.0f, -1475.0f, 2825.0f);
	outside.connections[2] = {start.id};
	start.connections[0] = {outside.id};
	start.connections[2] = {goal.id};
	goal.connections[0] = {start.id};
	nav::NavDocument document;
	document.addArea(outside);
	document.addArea(start);
	document.addArea(goal);
	nav::NavSnapshotPublisher publisher;
	if (!check(publisher.publish(&document, 1U) == nav::NavSnapshotResult::Published, "C4 drift NAV publishes"))
		return false;

	struct Probe final : nav::NavHuntPathProbe
	{
		explicit Probe(bool clear) : clear_(clear) {}
		bool clear(const nav::NavVector &, const nav::NavVector &, bool) const override { return clear_; }
		bool clear_;
	} probe(clear);
	compat::ScriptedRandomSource random({
		compat::RandomTapeEntry::floatEntry("CSBOT-PATH-RETRY", {1U, 1U}, {0U, 0U, 1U}, .4f, .6f, .5f)});
	runtime::NavRoamController controller;
	controller.setRandomSource(&random);
	runtime::NavRoamObservation observation{};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.team = 2U;
	observation.hasObjectiveTarget = true;
	observation.objectiveTarget = {-1536.0f, 2688.0f, floor};
	observation.nativeSameAreaEndpoint = true;
	observation.defuseApproach = true;
	observation.prepareObjectivePathOnly = true;
	auto &physics = observation.locomotion;
	physics.position = {-1492.5020f, 2577.0173f, floor};
	physics.velocity = {-6.8838f, -147.9226f, 0.0f};
	physics.grounded = true;
	physics.standingClearance = 72.0f;
	physics.crouchingClearance = 36.0f;
	physics.timingAvailable = true;
	physics.observedAt = 1.0f;
	physics.viewYawAvailable = true;
	physics.viewYaw = -92.63f;
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	const auto snapshot = publisher.snapshot();
	if (!check(controller.update(snapshot, observation, &intent, &decision) ==
				runtime::NavRoamResult::ObjectivePathPrepared && decision.linkFromArea == 293 &&
				decision.linkToArea == 53 && decision.pathSequence != 0 && intent.speed == 0.0f,
			"C4 MoveTo prepares 293 to 53 with zero input"))
		return false;
	const auto path = decision.pathSequence;
	observation.prepareObjectivePathOnly = false;
	physics.huntPathProbe = &probe;
	physics.position = {-1493.1123f, 2563.9057f, floor};
	physics.velocity = {-3.9455f, -84.7835f, 0.0f};
	observation.frame.tick++;
	physics.observedAt += .131f;
	const auto result = controller.update(snapshot, observation, &intent, &decision);
	if (!clear)
		return check(result == runtime::NavRoamResult::ReplanRequired && intent.speed == 0.0f && random.verifyComplete(),
			"a blocked ground approach does not bypass path rejection");
	return check(result == runtime::NavRoamResult::IntentReady && controller.hasPath() && decision.pathSequence == path &&
			decision.currentArea == 54 && decision.linkFromArea == 293 && decision.linkToArea == 53 &&
			intent.currentArea == 54 && intent.targetArea == 293 && intent.speed > 0.0f &&
			intent.targetPosition.x == -1537.5f && intent.targetPosition.y == 2630.0f &&
			decision.defuseLookPoint.x == -1536.0f && decision.defuseLookPoint.y == 2688.0f &&
			intent.direction.x < 0.0f && intent.direction.y > 0.0f && random.verifyComplete(),
		"C4 MoveTo steers toward the retained portal after drifting into area 54 without another draw");
}

int main()
{
	if (!testRemainingPathDistance() || !testDefuseMoveToPreparesPathBeforeMoving() ||
		!testDefuseMoveToRetainsFirstPointAfterDrift(true) || !testDefuseMoveToRetainsFirstPointAfterDrift(false))
		return 1;
	if (!testInvestigationSafestRoute() || !testInvestigationRetainsPathWhenThrottled() ||
		!testInvestigationKeepsClosestReachablePath() || !testInvestigationSearchKeepsPositionWhenImprovingPath())
		return 1;
	for (unsigned completion = 0U; completion < 6U; ++completion)
		if (!test1734DropLaunchAcceptsRecordedGroundedFeet(completion))
			return 1;
	if (!testRepeated1773DropFailureRearmsDirectedBypass())
		return 1;
	{
		using namespace astrabot;
		nav::NavDocument document;
		document.setSourceIdentity({5U, 100U, 200U});
		auto start = area(1U, 0, 64);
		auto goal = area(2U, 64, 128);
		start.connections[1U] = {2U};
		start.extent.lo.z = start.extent.hi.z = 42.0f;
		start.northEastZ = start.southWestZ = 42.0f;
		goal.northEastZ = goal.southWestZ = 42.0f;
		document.addArea(start);
		document.addArea(goal);
		nav::NavSnapshotPublisher publisher;
		publisher.publish(&document, 1U);
		compat::ScriptedRandomSource random(
			{compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
		runtime::NavRoamController controller;
		controller.setRandomSource(&random);
		runtime::NavRoamObservation observation = {};
		observation.actor = {1U, 1U};
		observation.frame = {1U, 1U, 1U};
		observation.locomotion.position = {32, 32, 42};
		observation.locomotion.grounded = true;
		observation.locomotion.standingClearance = 72;
		observation.locomotion.crouchingClearance = 36;
		nav::LocomotionIntent intent = {};
		runtime::NavRoamDecision decision = {};
		const auto initial = controller.update(publisher.snapshot(), observation, &intent, &decision);
		if (!check(initial == runtime::NavRoamResult::IntentReady, "nonplanar Hunt test starts a traversable route"))
			return 1;
		observation.frame.tick = 2;
		observation.locomotion.position = {96, 32, 21};
		const auto result = controller.update(publisher.snapshot(), observation, &intent, &decision);
		if (!check(result == runtime::NavRoamResult::TargetReached && random.verifyComplete(),
				"Hunt endpoint uses the four-corner NAV floor height on a nonplanar area"))
			return 1;
	}
	{
		using namespace astrabot;
		nav::NavDocument d;
		d.setSourceIdentity({5U, 100U, 200U});
		auto a = area(1251U, 1300, 1375);
		a.extent.lo = {1300, 2300, .03125f};
		a.extent.hi = {1375, 2350, 11.53222f};
		a.northEastZ = .03125f;
		a.southWestZ = 11.53222f;
		a.connections[2U] = {481U};
		auto b = area(481U, 1350, 1375);
		b.extent.lo = {1350, 2350, 11.53222f};
		b.extent.hi = {1375, 2400, 24.03226f};
		b.northEastZ = 11.53222f;
		b.southWestZ = 24.03227f;
		b.attributes = nav::NavArea::kCrouch;
		d.addArea(a);
		d.addArea(b);
		nav::NavSnapshotPublisher p;
		p.publish(&d, 1U);
		compat::ScriptedRandomSource rng(
			{compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
		runtime::NavRoamController c;
		c.setRandomSource(&rng);
		runtime::NavRoamObservation o{};
		o.actor = {1U, 1U};
		o.frame = {1U, 1U, 1U};
		// Recorded duck origin(1359.5,2349.7,29.9), public mins.z=-18.
		// BSP hull3 floor at29.8577 proves a valid duck stance, not a wall.
		o.locomotion.position = {1359.5f, 2349.7f, 11.9f};
		o.locomotion.grounded = true;
		o.locomotion.ducked = true;
		o.locomotion.standingClearance = 72;
		o.locomotion.crouchingClearance = 36;
		nav::LocomotionIntent intent{};
		runtime::NavRoamDecision decision{};
		c.update(p.snapshot(), o, &intent, &decision);
		o.frame.tick = 2U;
		if (!check(c.update(p.snapshot(), o, &intent, &decision) == runtime::NavRoamResult::IntentReady &&
					intent.speed > 0 && intent.direction.y > 0 && intent.posture == nav::LocomotionPosture::Crouching &&
					rng.verifyComplete(),
				"BSP-backed1251->481 ramp keeps moving to CSBot endpoint instead of arriving before boundary"))
			return 1;
	}
	if (!testBsiteProgressFailureDoesNotBlacklistTheOnlyApproach())
		return 1;
	{
		using namespace astrabot;
		nav::NavDocument d;
		d.setSourceIdentity({5U, 100U, 200U});
		auto a = area(1U, 0, 64), b = area(2U, 64, 84);
		a.connections[1U] = {2U};
		d.addArea(a);
		d.addArea(b);
		nav::NavSnapshotPublisher p;
		p.publish(&d, 1U);
		compat::ScriptedRandomSource rng(
			{compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 1, 1)});
		runtime::NavRoamController c;
		c.setRandomSource(&rng);
		runtime::NavRoamObservation o{};
		o.actor = {1U, 1U};
		o.frame = {1U, 1U, 1U};
		o.locomotion.position = {63, 32, 0};
		o.locomotion.grounded = true;
		o.locomotion.standingClearance = 72;
		o.locomotion.crouchingClearance = 36;
		nav::LocomotionIntent intent{};
		runtime::NavRoamDecision decision{};
		c.update(p.snapshot(), o, &intent, &decision);
		o.frame.tick = 2;
		if (!check(c.update(p.snapshot(), o, &intent, &decision) == runtime::NavRoamResult::TargetReached &&
					rng.verifyComplete(),
				"CSBot endpoint completion retires Hunt goal even just outside its NAV polygon"))
			return 1;
	}
	if (!testCompatibilityHuntDrawsForEachActorEvenWithLargeAreas())
		return 1;
	for (std::uint32_t cadence : {6U, 9U, 13U})
	{
		if (!testUnsafeDropRetryPolicy(cadence, 0, false))
			return 1;
	}
	for (int release = 1; release <= 7; ++release)
	{
		if (!testUnsafeDropRetryPolicy(6U, release, false))
			return 1;
	}
	if (!testUnsafeDropRetryPolicy(6U, 0, true))
		return 1;
	if (!testUnsafeDropRetryPolicy(6U, 0, false, true))
		return 1;
	if (!testUnsafeDropRetryPolicy(6U, 0, false, true, true))
		return 1;
	if (!testUnsafeDropRetryPolicy(6U, 0, false, false, false, true))
		return 1;
	const bool offMeshWiggle = testOffMeshCompatibilityKeepsTryingAfterBlockedCandidate();
	const bool ledgeDescent = testSafeDescentDoesNotReverseTowardNeighboringLedge();
	if (!offMeshWiggle || !ledgeDescent || !testOffMeshRecoveryHasHorizontalProgressAndBounds() ||
		!testRecoveryBudgetAndStateTransitions() || !testRecoveryReselectsBeforeExhaustion())
		return 1;
	if (!testGroundLookaheadRouteIsScopedToTheNextMatchingFrame())
	{
		return 1;
	}
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
	if (!testGoalAndPathFailuresAreTyped() || !testTemporaryCurrentAreaLossRetainsActiveRoute() ||
		!testPlannedGapAfterEarlierSegmentKeepsCurrentLink())
	{
		return 1;
	}
	if (!testFreezeMovementSuppressionPreservesObjectiveRoute())
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
	if (!testCompatibilityLargeAreaGoalUsesDrawAndSafestRoute())
	{
		return 1;
	}
	if (!testTimedHuntDrawsBeforePathThrottle() || !testTimedHuntFollowsSameAreaEndpoint() ||
		!testTimedHuntRoutesAcrossNativeLadder() || !testTimedHuntMovesAndJumpsOffCompletedLadder() ||
		!testTimedHuntReselectsAtKnownLadderGoal() ||
		!testTimedHuntReselectsAtKnownLadderGoal(HuntRngCancellation::Goal) ||
		!testTimedHuntReselectsAtKnownLadderGoal(HuntRngCancellation::Retry) ||
		!testFollowMovesBeforeReplacingItsRoute() ||
		!testCompatibilityPathRetryUsesEngineTime())
	{
		return 1;
	}
	if (!testFriendWaitRepathsAroundCrowding() || !testFriendWaitRepathsAroundCrowding(true) ||
		!testFriendWaitRepathsAroundCrowding(false, false, 0, true) ||
		!testFriendWaitRepathsAroundCrowding(false, true) || !testFriendWaitRepathsAroundCrowding(false, false, 1) ||
		!testFriendWaitRepathsAroundCrowding(false, false, 2) ||
		!testFriendWaitRepathsAroundCrowding(false, false, 3) || !testCompatibilityCorridorWithoutProgressExpires() ||
		!testCompatibilityCorridorWithoutProgressExpires(true) ||
		!testCompatibilityCorridorWithoutProgressExpires(false, true) ||
		!testCompatibilityStuckJumpIsScheduledWithoutAPath() ||
		!testCompatibilityWiggleUsesRandomDirectionAndDuration() ||
		!testCompatibilityWiggleUsesRandomDirectionAndDuration(true) ||
		!testCompatibilityWiggleUsesRandomDirectionAndDuration(false, true) ||
		!testCompatibilityWiggleUsesRandomDirectionAndDuration(false, false, false) ||
		!testCompatibilityWiggleUsesRandomDirectionAndDuration(false, false, true, true) ||
		!testCompatibilityWiggleUsesRandomDirectionAndDuration(false, false, true, false, false, true) ||
		!testCompatibilityWiggleUsesRandomDirectionAndDuration(false, false, true, false, true))
	{
		return 1;
	}
	if (!testAirborneRouteDoesNotEnterStuckRecovery())
	{
		return 1;
	}
	if (!testTeamReservationsChooseDistinctGoalsAndFirstLinks())
	{
		return 1;
	}
	if (!testTeamReservationsChooseDistinctGoalsAndFirstLinks(true))
	{
		return 1;
	}
	if (!testTeamReservationFallbackAndLifecycle())
	{
		return 1;
	}
	if (!testReservationBudgetFallbackIsDiagnosed())
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
	if (!check(publisher.publish(&document, 1U) == astrabot::nav::NavSnapshotResult::Published,
			"roam snapshot is published"))
	{
		return 1;
	}

	astrabot::runtime::NavRoamController controller;
	astrabot::compat::ScriptedRandomSource randomSource(
		{astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 1U}, 0, 2, 1),
			astrabot::compat::RandomTapeEntry::longEntry("CSBOT-HUNT-GOAL", {1U, 1U}, {0U, 0U, 3U}, 0, 2, 2)});
	controller.setRandomSource(&randomSource);
	astrabot::runtime::NavRoamObservation observation = {};
	observation.actor = {1U, 1U};
	observation.frame = {1U, 1U, 1U};
	observation.locomotion.position = {32.0f, 32.0f, 0.0f};
	observation.locomotion.standingClearance = 72.0f;
	observation.locomotion.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	astrabot::runtime::NavRoamDecision decision = {};
	const astrabot::runtime::NavRoamResult result =
		controller.update(publisher.snapshot(), observation, &intent, &decision);
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
	if (!check(decision.goalGeneration == 2U, "goal arrival increments goal generation"))
	{
		return 1;
	}
	if (!check(decision.goalSelectionReason == astrabot::runtime::GoalSelectionReached,
			"goal arrival reports the reselection reason"))
	{
		return 1;
	}
	if (!check(decision.goalSelectionStrategy == astrabot::runtime::GoalSelectionStrategyRandomFallback,
			"small-area selection reports random fallback strategy"))
	{
		return 1;
	}
	if (!check(intent.targetArea == 3U, "roam controller replans from the new area"))
	{
		return 1;
	}

	astrabot::runtime::NavRoamController recoveryController;
	astrabot::runtime::NavRoamObservation recoveryObservation = observation;
	recoveryObservation.frame.tick = 1U;
	recoveryObservation.locomotion.position = {-512.0f, 32.0f, 0.0f};
	astrabot::runtime::NavRoamDecision recoveryDecision = {};
	if (!check(recoveryController.update(publisher.snapshot(), recoveryObservation, &intent, &recoveryDecision) ==
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
