#include "astrabot/nav/jump_drop.hpp"

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
	const astrabot::nav::NavSnapshot &snapshot)
{
	astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	query.buildCorridor(1U, 2U, &corridor);
	return corridor;
}

astrabot::nav::JumpDropConfig config()
{
	return {
		8.0f,
		8.0f,
		8.0f,
		100.0f,
		4U
	};
}

astrabot::nav::JumpDropEnvelope envelope(
	astrabot::nav::JumpDropKind kind,
	float landingZ)
{
	astrabot::nav::JumpDropEnvelope result = {};
	result.kind = kind;
	result.launch = {{32.0f, 32.0f, 0.0f}, 1U};
	result.landing = {{96.0f, 32.0f, landingZ}, 2U};
	result.maximumRise = 64.0f;
	result.maximumDrop = 64.0f;
	result.horizontalReach = 64.0f;
	result.damageRisk = {48.0f, 25.0f};
	return result;
}

astrabot::nav::JumpDropObservation observation(
	const astrabot::nav::NavVector &position,
	std::uint32_t actorGeneration,
	std::uint32_t frame,
	bool airborne,
	bool landingConfirmed,
	bool hasLandingDamage,
	float landingDamage)
{
	return {
		position,
		frame,
		actorGeneration,
		airborne,
		landingConfirmed,
		hasLandingDamage,
		landingDamage
	};
}

bool testValidLaunchAndLandingFeedback()
{
	astrabot::nav::NavDocument document = routeDocument(32.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::JumpDropController controller(config());
	const astrabot::nav::JumpDropEnvelope jump =
		envelope(astrabot::nav::JumpDropKind::Jump, 32.0f);
	if (!check(controller.start(corridorFor(snapshot), jump, 7U) ==
			astrabot::nav::JumpDropResult::Ready,
			"valid jump envelope is ready"))
	{
		return false;
	}

	astrabot::nav::JumpDropIntent intent = {};
	const astrabot::nav::JumpDropObservation launch = observation(
		{32.0f, 32.0f, 0.0f}, 7U, 100U, false, false, false, 0.0f);
	if (!check(controller.update(snapshot, launch, &intent) ==
			astrabot::nav::JumpDropResult::Emitted &&
			controller.isActive() &&
			intent.kind == astrabot::nav::JumpDropKind::Jump &&
			intent.launchArea == 1U &&
			intent.landingArea == 2U &&
			intent.direction.x > 0.9f,
			"launch emits bounded intent without completion"))
	{
		return false;
	}

	const astrabot::nav::JumpDropObservation unconfirmedLanding =
		observation({96.0f, 32.0f, 32.0f}, 7U, 101U, false, false, false, 0.0f);
	if (!check(controller.update(snapshot, unconfirmedLanding, &intent) ==
			astrabot::nav::JumpDropResult::Emitted &&
			controller.isActive() &&
			intent.speed == 0.0f,
			"landing position alone does not claim success"))
	{
		return false;
	}

	const astrabot::nav::JumpDropObservation confirmedLanding = observation(
		{96.0f, 32.0f, 32.0f}, 7U, 102U, false, true, false, 0.0f);
	return check(controller.update(snapshot, confirmedLanding, &intent) ==
			astrabot::nav::JumpDropResult::Landed &&
			!controller.isActive(),
			"explicit landing feedback completes the jump");
}

bool testHeightAndHorizontalReachBounds()
{
	astrabot::nav::NavDocument document = routeDocument(32.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::JumpDropEnvelope tooHigh =
		envelope(astrabot::nav::JumpDropKind::Jump, 32.0f);
	tooHigh.maximumRise = 31.0f;
	astrabot::nav::JumpDropController highController(config());
	if (!check(highController.start(corridorFor(snapshot), tooHigh, 7U) ==
			astrabot::nav::JumpDropResult::Unsafe &&
			!highController.isActive(),
			"jump above maximum rise is rejected"))
	{
		return false;
	}

	astrabot::nav::JumpDropEnvelope tooFar =
		envelope(astrabot::nav::JumpDropKind::Jump, 32.0f);
	tooFar.horizontalReach = 63.0f;
	astrabot::nav::JumpDropController reachController(config());
	return check(reachController.start(corridorFor(snapshot), tooFar, 7U) ==
			astrabot::nav::JumpDropResult::Unsafe &&
			!reachController.isActive(),
		"horizontal reach beyond the envelope is rejected");
}

bool testDropDamageRiskAndUnknownLanding()
{
	astrabot::nav::NavDocument document = routeDocument(-40.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::JumpDropEnvelope unsafeDrop =
		envelope(astrabot::nav::JumpDropKind::Drop, -40.0f);
	unsafeDrop.damageRisk.maximumSafeDropHeight = 32.0f;
	astrabot::nav::JumpDropController unsafeController(config());
	if (!check(unsafeController.start(
			corridorFor(snapshot), unsafeDrop, 7U) ==
			astrabot::nav::JumpDropResult::Unsafe &&
			!unsafeController.isActive(),
			"drop damage height policy rejects unsafe traversal"))
	{
		return false;
	}

	astrabot::nav::JumpDropController unknownController(config());
	unknownController.start(
		corridorFor(snapshot),
		envelope(astrabot::nav::JumpDropKind::Drop, -40.0f),
		7U);
	astrabot::nav::JumpDropIntent unknownIntent = {};
	unknownController.update(snapshot,
		observation({32.0f, 32.0f, 0.0f}, 7U, 1U, false, false, false, 0.0f),
		&unknownIntent);
	if (!check(unknownController.update(snapshot,
			observation({96.0f, 32.0f, -40.0f}, 7U, 2U, false, true, false, 0.0f),
			&unknownIntent) == astrabot::nav::JumpDropResult::InvalidObservation &&
			!unknownController.isActive(),
			"unknown drop landing damage fails closed"))
	{
		return false;
	}

	astrabot::nav::JumpDropEnvelope observedDrop =
		envelope(astrabot::nav::JumpDropKind::Drop, -40.0f);
	astrabot::nav::JumpDropController controller(config());
	controller.start(corridorFor(snapshot), observedDrop, 7U);
	astrabot::nav::JumpDropIntent intent = {};
	controller.update(snapshot,
		observation({32.0f, 32.0f, 0.0f}, 7U, 1U, false, false, false, 0.0f),
		&intent);
	const astrabot::nav::JumpDropObservation unsafeLanding = observation(
		{96.0f, 32.0f, -40.0f}, 7U, 2U, false, true, true, 26.0f);
	return check(controller.update(snapshot, unsafeLanding, &intent) ==
			astrabot::nav::JumpDropResult::Unsafe &&
			!controller.isActive(),
		"landing damage beyond the policy is rejected");
}

bool testMissingFeedbackTimesOut()
{
	astrabot::nav::NavDocument document = routeDocument(32.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::JumpDropConfig shortConfig = config();
	shortConfig.maximumTraversalFrames = 2U;
	astrabot::nav::JumpDropController controller(shortConfig);
	controller.start(
		corridorFor(snapshot),
		envelope(astrabot::nav::JumpDropKind::Jump, 32.0f),
		7U);
	astrabot::nav::JumpDropIntent intent = {};
	controller.update(snapshot,
		observation({32.0f, 32.0f, 0.0f}, 7U, 10U, false, false, false, 0.0f),
		&intent);
	controller.update(snapshot,
		observation({96.0f, 32.0f, 32.0f}, 7U, 11U, false, false, false, 0.0f),
		&intent);
	if (!check(controller.isActive(),
			"missing landing feedback keeps traversal pending"))
	{
		return false;
	}

	return check(controller.update(snapshot,
		observation({96.0f, 32.0f, 32.0f}, 7U, 13U, false, false, false, 0.0f),
		&intent) == astrabot::nav::JumpDropResult::TimedOut &&
		!controller.isActive(),
		"missing landing feedback times out safely");
}

bool testLandingAreaAndGenerationInvalidation()
{
	astrabot::nav::NavDocument document = routeDocument(32.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::JumpDropController wrongAreaController(config());
	wrongAreaController.start(
		corridorFor(snapshot),
		envelope(astrabot::nav::JumpDropKind::Jump, 32.0f),
		7U);
	astrabot::nav::JumpDropIntent intent = {};
	wrongAreaController.update(snapshot,
		observation({32.0f, 32.0f, 0.0f}, 7U, 1U, false, false, false, 0.0f),
		&intent);
	if (!check(wrongAreaController.update(snapshot,
			observation({32.0f, 32.0f, 0.0f}, 7U, 2U, false, true, false, 0.0f),
			&intent) == astrabot::nav::JumpDropResult::InvalidObservation &&
			!wrongAreaController.isActive(),
			"confirmed landing in the launch area is rejected"))
	{
		return false;
	}

	astrabot::nav::JumpDropController staleController(config());
	staleController.start(
		corridorFor(snapshot),
		envelope(astrabot::nav::JumpDropKind::Jump, 32.0f),
		7U);
	astrabot::nav::NavDocument replacement = routeDocument(32.0f);
	const astrabot::nav::NavSnapshot changedSnapshot =
		snapshotFor(&replacement, 2U);
	return check(staleController.update(changedSnapshot,
		observation({32.0f, 32.0f, 0.0f}, 7U, 1U, false, false, false, 0.0f),
		&intent) == astrabot::nav::JumpDropResult::Invalidated &&
		!staleController.isActive(),
		"changed map generation invalidates traversal");
}

bool testInvalidObservationAndActorGeneration()
{
	astrabot::nav::NavDocument document = routeDocument(32.0f);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::JumpDropController controller(config());
	controller.start(
		corridorFor(snapshot),
		envelope(astrabot::nav::JumpDropKind::Jump, 32.0f),
		7U);
	astrabot::nav::JumpDropIntent intent = {};
	const astrabot::nav::JumpDropObservation invalid = observation(
		{std::numeric_limits<float>::quiet_NaN(), 32.0f, 0.0f},
		7U,
		1U,
		false,
		false,
		false,
		0.0f);
	if (!check(controller.update(snapshot, invalid, &intent) ==
			astrabot::nav::JumpDropResult::InvalidObservation &&
			!controller.isActive(),
			"non-finite physics observation fails closed"))
	{
		return false;
	}

	astrabot::nav::JumpDropController actorController(config());
	actorController.start(
		corridorFor(snapshot),
		envelope(astrabot::nav::JumpDropKind::Jump, 32.0f),
		7U);
	return check(actorController.update(snapshot,
		observation({32.0f, 32.0f, 0.0f}, 8U, 1U, false, false, false, 0.0f),
		&intent) == astrabot::nav::JumpDropResult::Invalidated &&
		!actorController.isActive(),
		"changed actor generation invalidates traversal");
}
}

int main()
{
	if (!testValidLaunchAndLandingFeedback() ||
			!testHeightAndHorizontalReachBounds() ||
			!testDropDamageRiskAndUnknownLanding() ||
			!testMissingFeedbackTimesOut() ||
			!testLandingAreaAndGenerationInvalidation() ||
			!testInvalidObservationAndActorGeneration())
	{
		return 1;
	}

	return 0;
}
