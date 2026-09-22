#include "astrabot/nav/locomotion.hpp"

#include <cmath>
#include <cstdio>
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

astrabot::nav::NavArea area(
	astrabot::nav::AreaId id,
	float lowX,
	float highX,
	float floorZ)
{
	astrabot::nav::NavArea result = {};
	result.id = id;
	result.extent.lo = {lowX, 0.0f, floorZ};
	result.extent.hi = {highX, 64.0f, floorZ};
	result.northEastZ = floorZ;
	result.southWestZ = floorZ;
	return result;
}

astrabot::nav::NavSnapshot snapshotFor(
	astrabot::nav::NavDocument *document,
	std::uint32_t mapGeneration)
{
	astrabot::nav::NavSnapshotPublisher publisher;
	publisher.publish(document, mapGeneration);
	return publisher.snapshot();
}

astrabot::nav::NavDocument routeDocument(float destinationFloor)
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f, destinationFloor);
	first.connections[0U].push_back(2U);
	document.addArea(first);
	document.addArea(second);
	return document;
}

astrabot::nav::NavCorridor corridorFor(
	const astrabot::nav::NavSnapshot &snapshot)
{
	astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	query.buildCorridor(1U, 2U, &corridor);
	return corridor;
}

astrabot::nav::LocomotionConfig config()
{
	return {
		32.0f,
		1.0f,
		1.0f,
		16.0f,
		100.0f,
		2U
	};
}

bool testWalkAndStepIntent()
{
	astrabot::nav::NavDocument document = routeDocument(8.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionController controller(config());
	if (!check(controller.start(corridorFor(snapshot)) ==
			astrabot::nav::LocomotionResult::Started,
			"locomotion accepts a valid corridor"))
	{
		return false;
	}

	astrabot::nav::LocomotionObservation observation = {
		{32.0f, 32.0f, 0.0f},
		64.0f,
		64.0f,
		{0.0f, 0.0f, 0.0f},
		false,
		false,
		false
	};
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
			intent.currentArea == 1U &&
			intent.targetArea == 2U &&
			intent.posture == astrabot::nav::LocomotionPosture::Standing &&
			intent.stepUp &&
			intent.direction.x > 0.9f &&
			intent.speed == 100.0f,
			"walk step intent is bounded and directed"))
	{
		return false;
	}

	return check(controller.isActive(),
		"intent emission does not claim movement completion");
}

bool testCrouchSelection()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionController controller(config());
	controller.start(corridorFor(snapshot));
	const astrabot::nav::LocomotionObservation observation = {
		{16.0f, 32.0f, 0.0f},
		24.0f,
		64.0f,
		{0.0f, 0.0f, 0.0f},
		false,
		false,
		false
	};
	astrabot::nav::LocomotionIntent intent = {};
	return check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
			intent.posture == astrabot::nav::LocomotionPosture::Crouching &&
			!intent.stepUp,
		"insufficient standing clearance selects crouch");
}

bool testStepAndStuckRecovery()
{
	astrabot::nav::NavDocument highStepDocument = routeDocument(64.0f);
	const astrabot::nav::NavSnapshot highStepSnapshot =
		snapshotFor(&highStepDocument, 1U);
	astrabot::nav::LocomotionConfig stepConfig = config();
	stepConfig.maximumStepHeight = 16.0f;
	astrabot::nav::LocomotionController stepController(stepConfig);
	stepController.start(corridorFor(highStepSnapshot));
	astrabot::nav::LocomotionObservation observation = {
		{32.0f, 32.0f, 0.0f},
		64.0f,
		64.0f,
		{0.0f, 0.0f, 0.0f},
		false,
		false,
		false
	};
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(stepController.update(
			highStepSnapshot,
			observation,
			&intent) == astrabot::nav::LocomotionResult::StepTooHigh &&
			!stepController.isActive(),
			"excessive step requests recovery"))
	{
		return false;
	}

	astrabot::nav::NavDocument stuckDocument = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot stuckSnapshot =
		snapshotFor(&stuckDocument, 1U);
	astrabot::nav::LocomotionController stuckController(config());
	stuckController.start(corridorFor(stuckSnapshot));
	observation.position = {16.0f, 32.0f, 0.0f};
	if (!check(stuckController.update(
			stuckSnapshot,
			observation,
			&intent) == astrabot::nav::LocomotionResult::IntentReady &&
			stuckController.update(
				stuckSnapshot,
				observation,
				&intent) == astrabot::nav::LocomotionResult::IntentReady &&
			stuckController.update(
				stuckSnapshot,
				observation,
				&intent) == astrabot::nav::LocomotionResult::Stuck &&
			!stuckController.isActive(),
			"unchanged position reaches bounded stuck recovery"))
	{
		return false;
	}

	return true;
}

bool testTerrainDiscontinuityRequestsJump()
{
	astrabot::nav::NavDocument document = routeDocument(32.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionConfig jumpConfig = config();
	jumpConfig.maximumStepHeight = 16.0f;
	astrabot::nav::LocomotionController controller(jumpConfig);
	if (!check(controller.start(corridorFor(snapshot)) ==
				astrabot::nav::LocomotionResult::Started,
				"terrain discontinuity jump controller starts"))
	{
		return false;
	}
	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {16.0f, 32.0f, 0.0f};
	observation.standingClearance = 72.0f;
	observation.crouchingClearance = 36.0f;
	observation.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
			intent.traversal == astrabot::nav::TraversalAction::Jump,
			"bounded terrain discontinuity requests Jump before StepTooHigh"))
	{
		return false;
	}

	if (!check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
			intent.traversal == astrabot::nav::TraversalAction::Walk,
			"the same terrain transition does not hold Jump after launch"))
	{
		return false;
	}

	observation.position.x = 17.0f;
	observation.grounded = false;
	return check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
			intent.traversal == astrabot::nav::TraversalAction::Walk,
			"an airborne terrain jump keeps forward movement without re-pressing Jump");
}

bool testContinuousRampUsesPortalTransitionHeight()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea ramp = area(1U, 0.0f, 100.0f, 0.0f);
	ramp.extent.hi.y = 64.0f;
	ramp.extent.hi.z = 60.0f;
	ramp.northEastZ = 60.0f;
	ramp.southWestZ = 0.0f;
	astrabot::nav::NavArea landing = area(2U, 100.0f, 200.0f, 60.0f);
	landing.extent.hi.y = 64.0f;
	ramp.connections[1U].push_back(2U);
	document.addArea(ramp);
	document.addArea(landing);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionConfig locomotionConfig = config();
	locomotionConfig.maximumStepHeight = 16.0f;
	astrabot::nav::LocomotionController controller(locomotionConfig);
	if (!check(controller.start(corridorFor(snapshot)) ==
				astrabot::nav::LocomotionResult::Started,
				"continuous ramp controller starts"))
	{
		return false;
	}
	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {80.0f, 32.0f, 48.0f};
	observation.standingClearance = 72.0f;
	observation.crouchingClearance = 36.0f;
	observation.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	return check(controller.update(snapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::IntentReady &&
				intent.targetArea == 2U && !intent.stepUp &&
				intent.traversal == astrabot::nav::TraversalAction::Walk,
				"continuous ramp does not become a center-height step");
}

bool testLateralJitterDoesNotResetForwardProgress()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionConfig jitterConfig = config();
	jitterConfig.stuckFrameLimit = 2U;
	astrabot::nav::LocomotionController controller(jitterConfig);
	if (!check(controller.start(corridorFor(snapshot)) ==
				astrabot::nav::LocomotionResult::Started,
				"jitter progress controller starts"))
	{
		return false;
	}
	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {16.0f, 32.0f, 0.0f};
	observation.standingClearance = 72.0f;
	observation.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(controller.update(snapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::IntentReady,
				"jitter progress establishes baseline"))
	{
		return false;
	}
	observation.position.y = 32.02f;
	if (!check(controller.update(snapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::IntentReady,
				"small lateral jitter remains a live intent"))
	{
		return false;
	}
	observation.position.y = 31.98f;
	return check(controller.update(snapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::Stuck,
				"lateral jitter does not reset forward no-progress window");
}

bool testUnavailableClearanceIsExplicit()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea crouch = area(2U, 64.0f, 128.0f, 0.0f);
	start.connections[0U].push_back(2U);
	crouch.attributes = astrabot::nav::NavArea::kCrouch;
	document.addArea(start);
	document.addArea(crouch);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionController controller(config());
	controller.start(corridorFor(snapshot));
	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {16.0f, 32.0f, 0.0f};
	observation.clearanceAvailable = false;
	astrabot::nav::LocomotionIntent intent = {};
	return check(controller.update(snapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::InvalidClearance,
				"unavailable clearance is not replaced by guessed hull values");
}

bool testCompletionAndInvalidation()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionController controller(config());
	controller.start(corridorFor(snapshot));
	astrabot::nav::LocomotionObservation observation = {
		{32.0f, 32.0f, 0.0f},
		64.0f,
		64.0f,
		{0.0f, 0.0f, 0.0f},
		false,
		false,
		false
	};
	astrabot::nav::LocomotionIntent intent = {};
	controller.update(snapshot, observation, &intent);
	observation.position = {96.0f, 32.0f, 0.0f};
	if (!check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::TargetReached &&
			!controller.isActive(),
			"final position feedback completes the route"))
	{
		return false;
	}

	astrabot::nav::LocomotionController staleController(config());
	staleController.start(corridorFor(snapshot));
	astrabot::nav::NavDocument replacement = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot changedSnapshot =
		snapshotFor(&replacement, 2U);
	return check(staleController.update(
			changedSnapshot,
			observation,
			&intent) == astrabot::nav::LocomotionResult::StaleSnapshot &&
			!staleController.isActive(),
		"changed map generation invalidates locomotion");
}

bool testCorridorIndexTracksPortalProgress()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionController controller(config());
	if (!check(controller.start(corridorFor(snapshot)) ==
			astrabot::nav::LocomotionResult::Started,
			"portal progress test starts"))
	{
		return false;
	}
	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {16.0f, 32.0f, 0.0f};
	observation.standingClearance = 72.0f;
	observation.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
			controller.currentCorridorIndex() == 1U,
			"locomotion exposes the advanced portal index"))
	{
		return false;
	}
	return check(controller.currentCorridorIndex() == 1U,
			"portal index remains tied to the active route");
}

bool testInvalidInputs()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionConfig invalidConfig = config();
	invalidConfig.maximumSpeed =
		astrabot::nav::LocomotionConfig::kMaximumSpeed + 1.0f;
	astrabot::nav::LocomotionController invalidController(invalidConfig);
	if (!check(invalidController.start(corridorFor(snapshot)) ==
			astrabot::nav::LocomotionResult::InvalidConfig,
			"unbounded speed configuration is rejected"))
	{
		return false;
	}

	astrabot::nav::LocomotionController controller(config());
	astrabot::nav::LocomotionObservation observation = {
		{std::numeric_limits<float>::quiet_NaN(), 32.0f, 0.0f},
		64.0f,
		64.0f,
		{0.0f, 0.0f, 0.0f},
		false,
		false,
		false
	};
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(controller.start({}) ==
			astrabot::nav::LocomotionResult::InvalidCorridor,
			"empty corridor is rejected"))
	{
		return false;
	}
	controller.start(corridorFor(snapshot));
	if (!check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::InvalidObservation,
			"non-finite observation is rejected"))
	{
		return false;
	}
	return check(controller.update(
			snapshot,
		{{0.0f, 0.0f, 0.0f}, 64.0f, 64.0f,
			{0.0f, 0.0f, 0.0f}, false, false, false},
			nullptr) == astrabot::nav::LocomotionResult::InvalidArgument,
		"null intent output is rejected");
}
}

bool testReferenceArrivalTolerance()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionController controller;
	if (!check(controller.start(corridorFor(snapshot)) ==
				astrabot::nav::LocomotionResult::Started,
			"reference arrival controller starts"))
	{
		return false;
	}
	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {16.0f, 32.0f, 0.0f};
	observation.standingClearance = 72.0f;
	observation.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(controller.update(snapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::IntentReady,
			"reference arrival emits an approach intent"))
	{
		return false;
	}
	observation.position = {60.0f, 32.0f, 0.0f};
	return check(controller.update(snapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::TargetReached,
			"reference arrival accepts a point within 20 units of the next area");
}

bool testNavAttributesSelectTraversal()
{
	astrabot::nav::NavDocument crouchDocument;
	crouchDocument.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea crouchStart = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea crouchGoal = area(2U, 64.0f, 128.0f, 0.0f);
	crouchStart.connections[0U].push_back(2U);
	crouchGoal.attributes = astrabot::nav::NavArea::kCrouch;
	crouchDocument.addArea(crouchStart);
	crouchDocument.addArea(crouchGoal);
	const astrabot::nav::NavSnapshot crouchSnapshot =
			snapshotFor(&crouchDocument, 1U);
	astrabot::nav::LocomotionController crouchController;
	crouchController.start(corridorFor(crouchSnapshot));
	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {16.0f, 32.0f, 0.0f};
	observation.standingClearance = 72.0f;
	observation.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(crouchController.update(crouchSnapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::IntentReady &&
				intent.traversal == astrabot::nav::TraversalAction::Crouch,
			"NAV_CROUCH forces crouch traversal"))
	{
		return false;
	}

	astrabot::nav::NavDocument jumpDocument;
	jumpDocument.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea jumpStart = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea jumpGoal = area(2U, 64.0f, 128.0f, 0.0f);
	jumpStart.connections[0U].push_back(2U);
	jumpGoal.attributes = astrabot::nav::NavArea::kJump;
	jumpDocument.addArea(jumpStart);
	jumpDocument.addArea(jumpGoal);
	const astrabot::nav::NavSnapshot jumpSnapshot = snapshotFor(&jumpDocument, 1U);
	astrabot::nav::LocomotionController jumpController;
	jumpController.start(corridorFor(jumpSnapshot));
	return check(jumpController.update(jumpSnapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::IntentReady &&
				intent.traversal == astrabot::nav::TraversalAction::Jump,
			"NAV_JUMP selects jump traversal before ordinary walking");
}

bool testJumpAttributesOverrideStepHeight()
{
	astrabot::nav::NavDocument document = routeDocument(32.0f);
	astrabot::nav::NavArea jumpGoal = area(2U, 64.0f, 128.0f, 32.0f);
	jumpGoal.attributes = astrabot::nav::NavArea::kJump;
	astrabot::nav::NavDocument jumpDocument;
	jumpDocument.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 64.0f, 0.0f);
	start.connections[0U].push_back(2U);
	jumpDocument.addArea(start);
	jumpDocument.addArea(jumpGoal);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&jumpDocument, 1U);
	astrabot::nav::LocomotionController controller(config());
	if (!check(controller.start(corridorFor(snapshot)) ==
				astrabot::nav::LocomotionResult::Started,
				"jump step-height fixture starts"))
	{
		return false;
	}
	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {16.0f, 32.0f, 0.0f};
	observation.standingClearance = 72.0f;
	observation.crouchingClearance = 36.0f;
	astrabot::nav::LocomotionIntent intent = {};
	return check(controller.update(snapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::IntentReady &&
				intent.traversal == astrabot::nav::TraversalAction::Jump,
				"NAV_JUMP takes precedence over ordinary step rejection");
}

bool testBoundedDescendingGapRequestsJumpNearTransition()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea launch = area(1U, 0.0f, 200.0f, 128.0f);
	astrabot::nav::NavArea landing = area(2U, 225.0f, 300.0f, 30.0f);
	launch.connections[1U].push_back(2U);
	document.addArea(launch);
	document.addArea(landing);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionController controller(config());
	if (!check(controller.start(corridorFor(snapshot)) ==
			astrabot::nav::LocomotionResult::Started,
			"descending gap fixture starts"))
	{
		return false;
	}
	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {32.0f, 32.0f, 128.0f};
	observation.standingClearance = 72.0f;
	observation.crouchingClearance = 36.0f;
	observation.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
			intent.traversal == astrabot::nav::TraversalAction::Walk,
			"a distant descending gap remains an approach walk"))
	{
		return false;
	}
	observation.position = {190.0f, 32.0f, 128.0f};
	if (!check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
		intent.traversal == astrabot::nav::TraversalAction::Jump,
		"a nearby 25-unit descending gap requests Jump"))
	{
		return false;
	}

	observation.position = {198.0f, 32.0f, 150.0f};
	observation.grounded = false;
	return check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
		intent.traversal == astrabot::nav::TraversalAction::Walk &&
		intent.posture == astrabot::nav::LocomotionPosture::Crouching,
		"an airborne descending-gap jump crouches while continuing forward");
}

bool testDescendingGapHonorsNoJumpAndSafeDropBounds()
{
	auto runCase = [](std::uint8_t landingAttributes, float landingFloor) {
		astrabot::nav::NavDocument document;
		document.setSourceIdentity({5U, 100U, 200U});
		astrabot::nav::NavArea launch = area(1U, 0.0f, 200.0f, 128.0f);
		astrabot::nav::NavArea landing = area(2U, 225.0f, 300.0f, landingFloor);
		landing.attributes = landingAttributes;
		launch.connections[1U].push_back(2U);
		document.addArea(launch);
		document.addArea(landing);
		const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
		astrabot::nav::LocomotionController controller(config());
		if (controller.start(corridorFor(snapshot)) !=
				astrabot::nav::LocomotionResult::Started)
		{
			return false;
		}
		astrabot::nav::LocomotionObservation observation = {};
		observation.position = {190.0f, 32.0f, 128.0f};
		observation.standingClearance = 72.0f;
		observation.crouchingClearance = 36.0f;
		observation.grounded = true;
		astrabot::nav::LocomotionIntent intent = {};
		return controller.update(snapshot, observation, &intent) ==
				astrabot::nav::LocomotionResult::IntentReady &&
			intent.traversal == astrabot::nav::TraversalAction::Walk;
	};
	return check(runCase(astrabot::nav::NavArea::kNoJump, 30.0f),
			"NAV_NO_JUMP suppresses descending gap Jump") &&
		check(runCase(0U, -128.0f),
			"an excessive descending gap remains non-Jump");
}

bool testOffPathAreaCannotTriggerJumpToNonAdjacentCorridorTarget()
{
	astrabot::nav::NavDocument document = routeDocument(32.0f);
	document.addArea(area(3U, 192.0f, 256.0f, 0.0f));
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionController controller(config());
	if (!check(controller.start(corridorFor(snapshot)) ==
			astrabot::nav::LocomotionResult::Started,
			"off-path fixture starts its valid corridor"))
	{
		return false;
	}

	astrabot::nav::LocomotionObservation observation = {};
	observation.position = {32.0f, 32.0f, 0.0f};
	observation.standingClearance = 72.0f;
	observation.crouchingClearance = 36.0f;
	observation.grounded = true;
	astrabot::nav::LocomotionIntent intent = {};
	if (!check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::IntentReady &&
			controller.currentCorridorIndex() == 1U,
			"off-path fixture advances to the connected target segment"))
	{
		return false;
	}

	observation.position = {224.0f, 32.0f, 0.0f};
	return check(controller.update(snapshot, observation, &intent) ==
			astrabot::nav::LocomotionResult::InvalidCorridor,
			"an unrelated current area invalidates the segment instead of fabricating Jump");
}

int main()
{
	if (!testBoundedDescendingGapRequestsJumpNearTransition() ||
		!testDescendingGapHonorsNoJumpAndSafeDropBounds())
	{
		return 1;
	}
	if (!testOffPathAreaCannotTriggerJumpToNonAdjacentCorridorTarget())
	{
		return 1;
	}
	if (!testWalkAndStepIntent() ||
		!testCrouchSelection() ||
		!testStepAndStuckRecovery() ||
		!testTerrainDiscontinuityRequestsJump() ||
		!testContinuousRampUsesPortalTransitionHeight() ||
			!testCompletionAndInvalidation() ||
			!testCorridorIndexTracksPortalProgress() ||
			!testInvalidInputs() ||
			!testReferenceArrivalTolerance() ||
			!testNavAttributesSelectTraversal() ||
			!testLateralJitterDoesNotResetForwardProgress() ||
			!testUnavailableClearanceIsExplicit() ||
			!testJumpAttributesOverrideStepHeight())
	{
		return 1;
	}

	return 0;
}
