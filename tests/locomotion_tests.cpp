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
		64.0f
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
		64.0f
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
	astrabot::nav::NavDocument highStepDocument = routeDocument(40.0f);
	const astrabot::nav::NavSnapshot highStepSnapshot =
		snapshotFor(&highStepDocument, 1U);
	astrabot::nav::LocomotionConfig stepConfig = config();
	stepConfig.maximumStepHeight = 16.0f;
	astrabot::nav::LocomotionController stepController(stepConfig);
	stepController.start(corridorFor(highStepSnapshot));
	astrabot::nav::LocomotionObservation observation = {
		{32.0f, 32.0f, 0.0f},
		64.0f,
		64.0f
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

bool testCompletionAndInvalidation()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::LocomotionController controller(config());
	controller.start(corridorFor(snapshot));
	astrabot::nav::LocomotionObservation observation = {
		{32.0f, 32.0f, 0.0f},
		64.0f,
		64.0f
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
		64.0f
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
			{{0.0f, 0.0f, 0.0f}, 64.0f, 64.0f},
			nullptr) == astrabot::nav::LocomotionResult::InvalidArgument,
		"null intent output is rejected");
}
}

int main()
{
	if (!testWalkAndStepIntent() ||
			!testCrouchSelection() ||
			!testStepAndStuckRecovery() ||
			!testCompletionAndInvalidation() ||
			!testInvalidInputs())
	{
		return 1;
	}

	return 0;
}
