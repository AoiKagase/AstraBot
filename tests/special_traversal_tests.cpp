#include "astrabot/nav/special_traversal.hpp"

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

astrabot::nav::NavSnapshot snapshotFor(
	astrabot::nav::NavDocument *document,
	std::uint32_t mapGeneration)
{
	astrabot::nav::NavSnapshotPublisher publisher;
	publisher.publish(document, mapGeneration);
	return publisher.snapshot();
}

astrabot::nav::NavCorridor corridorFor(
	const astrabot::nav::NavSnapshot &snapshot,
	astrabot::nav::AreaId start = 1U,
	astrabot::nav::AreaId goal = 2U)
{
	astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	query.buildCorridor(start, goal, &corridor);
	return corridor;
}

astrabot::nav::SpecialTraversalConfig config()
{
	return {8.0f, 100.0f, 4U, 3U, 0.5f};
}

astrabot::nav::SpecialTraversalCapability capability(
	astrabot::nav::SpecialTraversalKind kind,
	astrabot::nav::SpecialTraversalPosture minimumPosture =
		astrabot::nav::SpecialTraversalPosture::Standing,
	float requiredClearance = 32.0f)
{
	astrabot::nav::SpecialTraversalCapability result = {};
	result.kind = kind;
	result.entry = {{32.0f, 32.0f, 0.0f}, 1U};
	result.exit = {{96.0f, 32.0f, 0.0f}, 2U};
	result.sourceDirection = 0U;
	result.requiredClearance = requiredClearance;
	result.minimumPosture = minimumPosture;
	return result;
}

astrabot::nav::SpecialTraversalObservation observation(
	const astrabot::nav::NavVector &position,
	std::uint32_t actorGeneration,
	std::uint32_t frame,
	float standingClearance,
	float crouchingClearance,
	astrabot::nav::SpecialTraversalAvailability availability,
	bool ladderContact,
	bool entryConfirmed,
	bool exitConfirmed)
{
	return {
		position,
		frame,
		actorGeneration,
		standingClearance,
		crouchingClearance,
		availability,
		ladderContact,
		entryConfirmed,
		exitConfirmed
	};
}

bool testLadderEntryExitIdentity()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::SpecialTraversalController controller(config());
	const astrabot::nav::SpecialTraversalCapability ladder = capability(
		astrabot::nav::SpecialTraversalKind::Ladder);
	if (!check(controller.start(corridorFor(snapshot), ladder, 7U) ==
			astrabot::nav::SpecialTraversalResult::Ready,
			"ladder traversal is ready"))
	{
		return false;
	}

	astrabot::nav::SpecialTraversalIntent intent = {};
	if (!check(controller.update(snapshot,
			observation(
				{32.0f, 32.0f, 0.0f},
				7U,
				1U,
				64.0f,
				64.0f,
				astrabot::nav::SpecialTraversalAvailability::Unknown,
				true,
				false,
				false),
			&intent) == astrabot::nav::SpecialTraversalResult::EnterIntent &&
			intent.kind == astrabot::nav::SpecialTraversalKind::Ladder &&
			intent.phase == astrabot::nav::SpecialTraversalIntentPhase::Enter &&
			intent.entryArea == 1U &&
			intent.exitArea == 2U &&
			intent.sourceDirection == 0U,
			"ladder entry intent preserves identity"))
	{
		return false;
	}

	if (!check(controller.update(snapshot,
			observation(
				{32.0f, 32.0f, 0.0f},
				7U,
				2U,
				64.0f,
				64.0f,
				astrabot::nav::SpecialTraversalAvailability::Unknown,
				true,
				true,
				false),
			&intent) == astrabot::nav::SpecialTraversalResult::MaintainIntent &&
			intent.phase == astrabot::nav::SpecialTraversalIntentPhase::Maintain,
			"ladder contact enters maintain state"))
	{
		return false;
	}

	if (!check(controller.update(snapshot,
			observation(
				{96.0f, 32.0f, 0.0f},
				7U,
				3U,
				64.0f,
				64.0f,
				astrabot::nav::SpecialTraversalAvailability::Unknown,
				true,
				true,
				true),
			&intent) == astrabot::nav::SpecialTraversalResult::ExitIntent &&
			intent.phase == astrabot::nav::SpecialTraversalIntentPhase::Exit,
			"ladder exit feedback emits exit intent"))
	{
		return false;
	}

	return check(controller.update(snapshot,
		observation(
			{96.0f, 32.0f, 0.0f},
			7U,
			4U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			true,
			true,
			true),
		&intent) == astrabot::nav::SpecialTraversalResult::Completed &&
		!controller.isActive(),
		"ladder completes only after explicit exit feedback");
}

bool testDoorAvailabilityIsExplicit()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::SpecialTraversalCapability door = capability(
		astrabot::nav::SpecialTraversalKind::Door);
	astrabot::nav::SpecialTraversalIntent intent = {};
	astrabot::nav::SpecialTraversalController unknownController(config());
	unknownController.start(corridorFor(snapshot), door, 7U);
	if (!check(unknownController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			1U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::RecoverableFailure &&
		unknownController.failureReason() ==
			astrabot::nav::SpecialTraversalFailureReason::UnknownAvailability &&
		!unknownController.isActive(),
		"unknown door availability is recoverable failure"))
	{
		return false;
	}

	astrabot::nav::SpecialTraversalController closedController(config());
	closedController.start(corridorFor(snapshot), door, 7U);
	if (!check(closedController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			1U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Closed,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::RecoverableFailure &&
		closedController.failureReason() ==
			astrabot::nav::SpecialTraversalFailureReason::Unavailable,
		"closed door is not treated as open"))
	{
		return false;
	}

	astrabot::nav::SpecialTraversalController openController(config());
	openController.start(corridorFor(snapshot), door, 7U);
	if (!check(openController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			1U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Open,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::EnterIntent,
		"open door emits entry intent"))
	{
		return false;
	}

	openController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			2U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Open,
			false,
			true,
			false),
		&intent);
	return check(openController.update(snapshot,
		observation(
			{96.0f, 32.0f, 0.0f},
			7U,
			3U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Open,
			false,
			true,
			true),
		&intent) == astrabot::nav::SpecialTraversalResult::ExitIntent,
		"open door accepts explicit exit feedback");
}

bool testNarrowPassageClearanceAndCrouch()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::SpecialTraversalCapability narrow = capability(
		astrabot::nav::SpecialTraversalKind::NarrowPassage,
		astrabot::nav::SpecialTraversalPosture::Crouching,
		48.0f);
	astrabot::nav::SpecialTraversalIntent intent = {};
	astrabot::nav::SpecialTraversalController insufficientController(config());
	insufficientController.start(corridorFor(snapshot), narrow, 7U);
	if (!check(insufficientController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			1U,
			64.0f,
			32.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::RecoverableFailure &&
		insufficientController.failureReason() ==
			astrabot::nav::SpecialTraversalFailureReason::InsufficientClearance,
		"narrow passage rejects insufficient crouch clearance"))
	{
		return false;
	}

	astrabot::nav::SpecialTraversalController crouchController(config());
	crouchController.start(corridorFor(snapshot), narrow, 7U);
	return check(crouchController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			1U,
			64.0f,
			48.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::EnterIntent &&
		intent.posture == astrabot::nav::SpecialTraversalPosture::Crouching,
		"narrow passage selects crouch posture");
}

bool testDirectedRouteTimeoutAndRecovery()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor reverse = {};
	if (!check(query.buildCorridor(2U, 1U, &reverse) ==
			astrabot::nav::NavQueryResult::NoRoute,
			"reverse special traversal route is not fabricated"))
	{
		return false;
	}

	astrabot::nav::SpecialTraversalConfig shortConfig = config();
	shortConfig.maximumTraversalFrames = 2U;
	astrabot::nav::SpecialTraversalController timeoutController(shortConfig);
	const astrabot::nav::SpecialTraversalCapability narrow = capability(
		astrabot::nav::SpecialTraversalKind::NarrowPassage);
	if (!check(timeoutController.start(corridorFor(snapshot), narrow, 7U) ==
			astrabot::nav::SpecialTraversalResult::Ready,
			"directed special traversal starts"))
	{
		return false;
	}

	astrabot::nav::SpecialTraversalIntent intent = {};
	timeoutController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			10U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent);
	if (!check(timeoutController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			13U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::TimedOut &&
		!timeoutController.isActive(),
		"special traversal timeout terminates stale progress"))
	{
		return false;
	}

	astrabot::nav::SpecialTraversalConfig stuckConfig = config();
	stuckConfig.maximumNoProgressSamples = 2U;
	astrabot::nav::SpecialTraversalController stuckController(stuckConfig);
	stuckController.start(corridorFor(snapshot), narrow, 7U);
	stuckController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			1U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent);
	return check(stuckController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			2U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::RecoverableFailure &&
		stuckController.failureReason() ==
			astrabot::nav::SpecialTraversalFailureReason::NoProgress,
		"bounded no-progress recovery is explicit");
}

bool testActorAndSnapshotInvalidation()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::SpecialTraversalCapability narrow = capability(
		astrabot::nav::SpecialTraversalKind::NarrowPassage);
	astrabot::nav::SpecialTraversalIntent intent = {};
	astrabot::nav::SpecialTraversalController actorController(config());
	actorController.start(corridorFor(snapshot), narrow, 7U);
	if (!check(actorController.update(snapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			8U,
			1U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::Invalidated &&
		!actorController.isActive(),
		"actor generation invalidates special traversal"))
	{
		return false;
	}

	astrabot::nav::SpecialTraversalController staleController(config());
	staleController.start(corridorFor(snapshot), narrow, 7U);
	astrabot::nav::NavDocument replacement = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot changedSnapshot =
		snapshotFor(&replacement, 2U);
	return check(staleController.update(changedSnapshot,
		observation(
			{32.0f, 32.0f, 0.0f},
			7U,
			1U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::Invalidated &&
		!staleController.isActive(),
		"changed map generation invalidates special traversal");
}

bool testInvalidObservations()
{
	astrabot::nav::NavDocument document = routeDocument(0.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::SpecialTraversalController controller(config());
	controller.start(
		corridorFor(snapshot),
		capability(astrabot::nav::SpecialTraversalKind::NarrowPassage),
		7U);
	astrabot::nav::SpecialTraversalIntent intent = {};
	return check(controller.update(snapshot,
		observation(
			{std::numeric_limits<float>::quiet_NaN(), 32.0f, 0.0f},
			7U,
			1U,
			64.0f,
			64.0f,
			astrabot::nav::SpecialTraversalAvailability::Unknown,
			false,
			false,
			false),
		&intent) == astrabot::nav::SpecialTraversalResult::InvalidObservation &&
		!controller.isActive(),
		"non-finite traversal observation fails closed");
}
}

int main()
{
	if (!testLadderEntryExitIdentity() ||
		!testDoorAvailabilityIsExplicit() ||
		!testNarrowPassageClearanceAndCrouch() ||
		!testDirectedRouteTimeoutAndRecovery() ||
		!testActorAndSnapshotInvalidation() ||
		!testInvalidObservations())
	{
		return 1;
	}

	return 0;
}
