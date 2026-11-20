#include "astrabot/nav/native_ladder.hpp"
#include "astrabot/runtime/nav_roam_controller.hpp"
#include <cstdio>
#include <cmath>
#include <functional>
using namespace astrabot;
namespace
{
unsigned failures = 0;
bool check(bool pass, const char *message)
{
	if (!pass)
	{
		++failures;
		std::fprintf(stderr, "FAIL: %s\n", message);
	}
	return pass;
}
nav::NavSnapshot makeSnapshot()
{
	nav::NavDocument document;
	document.setSourceIdentity({5, 100, 200});
	for (unsigned id = 1; id <= 3; ++id)
	{
		nav::NavArea area{};
		area.id = id;
		area.extent = {{-64, -128, id == 1 ? 0.0f : 200.0f}, {64, 64, id == 1 ? 0.0f : 200.0f}};
		area.northEastZ = area.southWestZ = area.extent.lo.z;
		if (id == 1)
			area.extent.hi.y = 128;
		if (id == 3)
			area.extent = {{64, -128, 200}, {128, 64, 200}};
		if (id == 3)
			area.attributes = nav::NavArea::kCrouch;
		document.addArea(area);
	}
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1);
	return publisher.snapshot();
}
struct Probe : nav::INativeLadderProbe
{
	bool valid = true;
	float fraction = 1;
	unsigned calls = 0;
	std::function<void()> callback;
	nav::NavVector from{}, to{};
	bool current() const override
	{
		return valid;
	}
	bool trace(const nav::NavLadder &, const nav::NavVector &a, const nav::NavVector &b, float *output) override
	{
		++calls;
		from = a;
		to = b;
		*output = fraction;
		if (callback)
			callback();
		return valid;
	}
};
nav::NavLadder ladder()
{
	nav::NavLadder result;
	result.bottom = {0, 0, 0};
	result.top = {0, 0, 200};
	result.length = 200;
	result.direction = 2;
	result.bottomArea = 1;
	result.topAreas = {{2, 3, 0, 0}};
	return result;
}
nav::NativeLadderObservation observation()
{
	nav::NativeLadderObservation result;
	result.actor = {1, 7};
	result.frame = {1, 1, 1};
	result.now = 1;
	result.origin = {0, 100, 36};
	result.feetZ = 0;
	result.yaw = -90;
	result.lastKnownArea = 1;
	return result;
}
void advance(nav::NativeLadderObservation &in, float seconds = .1f)
{
	++in.frame.tick;
	in.now += seconds;
}
void testAscending()
{
	const auto snapshot = makeSnapshot();
	Probe probe;
	nav::NativeLadderController controller;
	auto in = observation();
	check(controller.start(snapshot, {1, 2, 2, 4}, ladder(), in, probe) &&
			controller.phase() == nav::NativeLadderPhase::ApproachUp,
		"ascending route starts native approach without a ground portal");
	nav::NativeLadderIntent intent{};
	advance(in);
	check(controller.update(snapshot, in, probe, &intent) == nav::NativeLadderResult::Active && intent.goal.y == 32 &&
			intent.forward == 1 && !intent.walking,
		"ascending mount point is 32 units in ladder facing direction");
	in.origin = {0, 20, 36};
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::FaceUp && intent.walking && !intent.crouching,
		"ascending precise approach requires signed plane, lateral tolerance and close distance");
	auto look = controller.look(snapshot, in);
	check(look.active && look.pitchOverride && look.pitch == -60 && std::fabs(look.yaw + 90) < .01f,
		"face-up look points at ladder with native negative60 pitch");
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::MountUp && intent.forward == 0,
		"facing within5 degrees changes state without mounting movement in that branch");
	in.onLadder = true;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::Ascend && intent.forward == 1 && probe.calls == 1 &&
			probe.from.z == 36 && probe.to.z == 200,
		"native mount confirms public contact and traces endpoint from entity origin height");
	in.origin.x = 7;
	look = controller.look(snapshot, in);
	check(look.active && std::fabs(look.yaw - (-97)) < .01f,
		"per-command StayOnLadderLine uses current facing quadrant and public lateral error");
	in.origin = {0, 0, 236};
	in.feetZ = 200;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::DismountUp && intent.forward == 1 && !intent.walking,
		"ascending reaches endpoint using feet and emits the final forward climb");
	advance(in, .39f);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::DismountUp && intent.forward == 1,
		"dismount persists before400 milliseconds");
	advance(in, .02f);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::MoveToDestination,
		"dismount changes to destination after400 milliseconds");
	advance(in);
	check(controller.update(snapshot, in, probe, &intent) == nav::NativeLadderResult::Complete && !controller.active(),
		"actual target area containment completes ladder and permits stored path suffix");
}
void testDescendingAndFailures()
{
	const auto snapshot = makeSnapshot();
	Probe probe;
	nav::NativeLadderController controller;
	auto in = observation();
	in.origin = {0, -100, 236};
	in.feetZ = 200;
	in.yaw = 90;
	in.lastKnownArea = 2;
	check(controller.start(snapshot, {2, 1, 2, 5}, ladder(), in, probe) && probe.calls == 1,
		"descending mount probes face-out clearance once");
	nav::NativeLadderIntent intent{};
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(intent.goal.y == -32 && intent.walking,
		"descending face-out approaches opposite mount point and walks inside150 units");
	in.origin.y = -20;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(
		controller.phase() == nav::NativeLadderPhase::FaceDown, "descending approach uses opposite signed mount plane");
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::MountDown, "descending face respects public yaw alignment");
	in.onLadder = true;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::Descend && intent.forward == 1 && probe.calls == 2,
		"descending contact traces actual bottom endpoint");
	check(controller.look(snapshot, in).pitch == 60, "descending view uses positive60 pitch");
	in.feetZ = 36;
	in.origin.z = 72;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::MoveToDestination && intent.forward == 1,
		"descending completes at endpoint plus half human height");
	const auto destinationLook = controller.look(snapshot, in);
	check(!destinationLook.active && destinationLook.yaw == 90.0f && destinationLook.pitch == 0.0f,
		"descending destination exposes the ordinary path look while movement still uses ladder axes");
	probe = {};
	probe.fraction = .5f;
	controller.reset();
	in = observation();
	in.origin = {0, 20, 236};
	in.feetZ = 200;
	check(controller.start(snapshot, {2, 1, 2, 5}, ladder(), in, probe),
		"blocked face-out trace still starts face-in descending");
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(intent.goal.y == 16, "blocked descending mount uses native face-in16 offset");
	controller.reset();
	probe = {};
	in = observation();
	controller.start(snapshot, {1, 2, 2, 4}, ladder(), in, probe);
	advance(in, 10.0f);
	check(controller.update(snapshot, in, probe, &intent) == nav::NativeLadderResult::Active,
		"ladder timeout is strict greater than10 seconds");
	advance(in, .01f);
	check(controller.update(snapshot, in, probe, &intent) == nav::NativeLadderResult::Failed && intent.mustJump &&
			intent.wiggle,
		"timeout jumps and requests native Wiggle before path destruction");
	controller.reset();
	in = observation();
	controller.start(snapshot, {1, 2, 2, 4}, ladder(), in, probe);
	++in.frame.roundGeneration;
	advance(in);
	check(controller.update(snapshot, in, probe, &intent) == nav::NativeLadderResult::Stale && !controller.active(),
		"round identity invalidates native ladder execution");
	controller.reset();
	in = observation();
	probe = {};
	probe.callback = [&]
	{
		controller.reset();
	};
	check(!controller.start(snapshot, {2, 1, 2, 5}, ladder(), in, probe) && !controller.active(),
		"trace callback reset cannot publish stale native ladder state");
}
void testContactBoundsAndDismount()
{
	const auto snapshot = makeSnapshot();
	Probe probe;
	nav::NativeLadderController controller;
	auto in = observation();
	nav::NativeLadderIntent intent{};
	controller.start(snapshot, {1, 3, 2, 4}, ladder(), in, probe);
	in.onLadder = true;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	in.origin = {0, 0, 236};
	in.feetZ = 200;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::DismountUp && intent.side == -1 && intent.forward == 0 &&
			intent.crouching,
		"left top area uses native left strafe dismount and its crouch attribute");
	controller.reset();
	in = observation();
	probe = {};
	probe.fraction = .5f;
	controller.start(snapshot, {1, 2, 2, 4}, ladder(), in, probe);
	in.onLadder = true;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	in.origin.z = 153;
	in.feetZ = 117;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(
		controller.phase() == nav::NativeLadderPhase::Ascend, "fraction-clipped endpoint remains above117 feet units");
	in.origin.z = 154;
	in.feetZ = 118;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::DismountUp,
		"endpoint fraction interpolates from entity origin to top instead of floor height");
	controller.reset();
	in = observation();
	probe = {};
	controller.start(snapshot, {1, 2, 2, 4}, ladder(), in, probe);
	in.onLadder = true;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	in.onLadder = false;
	advance(in);
	check(controller.update(snapshot, in, probe, &intent) == nav::NativeLadderResult::Failed && !intent.mustJump &&
			!intent.wiggle,
		"losing contact while ascending destroys path without synthetic timeout jump or Wiggle");
	controller.reset();
	in = observation();
	in.origin = {10, 20, 36};
	controller.start(snapshot, {1, 2, 2, 4}, ladder(), in, probe);
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::ApproachUp, "lateral error exactly10 stays in native approach");
	in.origin.x = 9.99f;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::FaceUp, "lateral error strictly below10 enters face phase");
	in.yaw = std::atan2(-in.origin.y, -in.origin.x) * 57.29577951308232f;
	in.stuck = true;
	advance(in);
	controller.update(snapshot, in, probe, &intent);
	check(controller.phase() == nav::NativeLadderPhase::MountUp, "face phase does not consume stale stuck flag");
	in.origin.x = 201;
	advance(in);
	check(controller.update(snapshot, in, probe, &intent) == nav::NativeLadderResult::Failed && intent.mustJump &&
			!intent.wiggle,
		"missed mount beyond200 units jumps and destroys path without timeout Wiggle");
	controller.reset();
	in = observation();
	probe = {};
	controller.start(snapshot, {1, 2, 2, 4}, ladder(), in, probe);
	probe.callback = [&]
	{
		controller.reset();
	};
	in.onLadder = true;
	advance(in);
	check(
		controller.update(snapshot, in, probe, &intent) == nav::NativeLadderResult::Unavailable && !controller.active(),
		"endpoint trace callback reset cannot revive an old ascending controller");
}
class MinimumRandom : public compat::ICompatibilityRandomSource
{
public:
	compat::RandomFloatResult nextFloat(const compat::RandomRequest &request) override
	{
		return {compat::RandomStatus::Ok, request.floatLower};
	}
	compat::RandomLongResult nextLong(const compat::RandomRequest &request) override
	{
		return {compat::RandomStatus::Ok, request.longLower};
	}
};
void testNativeRecovery()
{
	const compat::RandomActor actor{1, 7};
	compat::ScriptedRandomSource random(
		{compat::RandomTapeEntry::longEntry("CSBOT-WIGGLE-DIRECTION", actor, {}, 0, 3, 3),
			compat::RandomTapeEntry::floatEntry("CSBOT-WIGGLE-DURATION", actor, {}, .5f, 1.5f, 1)});
	nav::NativeLadderRecovery recovery;
	auto in = observation();
	in.now = 10;
	nav::NativeLadderIntent out{};
	out.mustJump = true;
	out.wiggle = true;
	const auto current = []()
	{
		return true;
	};
	check(recovery.apply(in, false, 0, &random, {}, current, &out) && out.mustJump && out.side == -1 &&
			random.verifyComplete(),
		"timeout jumps off public ladder then applies native left Wiggle without a second jump RNG");
	in.now = 10.5f;
	out = {};
	out.mustJump = true;
	check(recovery.apply(in, true, 0, &random, {}, current, &out) && !out.mustJump,
		"native IsJumping prevents another MUST_JUMP within one second even after landing");
	in.now = 11.1f;
	out = {};
	out.mustJump = true;
	check(recovery.apply(in, false, 0, &random, {}, current, &out) && !out.mustJump,
		"airborne jump state persists between one and three seconds");
	in.now = 13.1f;
	out = {};
	out.mustJump = true;
	check(recovery.apply(in, false, 0, &random, {}, current, &out) && out.mustJump,
		"native jump state expires strictly after three seconds while detached");
	in.now = 20;
	in.crouching = true;
	out = {};
	out.mustJump = true;
	out.wiggle = true;
	check(recovery.apply(in, true, 0, &random, {}, current, &out) && !out.mustJump && random.verifyComplete(),
		"crouching rejects jump and Wiggle without extra random calls");
	in.crouching = false;
	out = {};
	out.mustJump = true;
	check(recovery.apply(in, true, nav::NavArea::kCrouch, &random, {}, current, &out) && !out.mustJump,
		"native crouch NAV without jump attribute prevents ladder escape jump");
}

void testNativeRouteIntegration(bool preEntryStuck = false, bool redirectNoise = false)
{
	const auto snapshot = makeSnapshot();
	Probe probe;
	MinimumRandom random;
	auto climb = ladder();
	climb.entityIndex = 6;
	climb.entityGeneration = 100;
	nav::NavLadderTopology topology{snapshot.revision(), 1, {climb}};
	runtime::NavRoamController controller(compat::RuntimeMode::Compatibility);
	controller.setRandomSource(&random);
	runtime::NavRoamObservation in{};
	in.actor = {1, 7};
	in.frame = {1, 1, 1};
	in.team = 1;
	in.hasObjectiveTarget = true;
	in.objectiveTarget = {32, -100, 200};
	in.objectiveRouteType = nav::NavRouteType::Safest;
	in.collectPathStats = true;
	in.nativeLadders = &topology;
	in.nativeLadderProbe = &probe;
	in.friendRayOriginAvailable = true;
	in.friendRayOrigin = {0, 100, 36};
	in.locomotion.position = {0, 100, 0};
	in.locomotion.grounded = true;
	in.locomotion.standingClearance = 72;
	in.locomotion.crouchingClearance = 36;
	in.locomotion.clearanceAvailable = true;
	in.locomotion.timingAvailable = true;
	in.locomotion.observedAt = 1;
	in.locomotion.viewYawAvailable = true;
	in.locomotion.viewYaw = -90;
	nav::NavQuery preflight(snapshot);
	nav::NavAreaMatch startArea{};
	if (!check(preflight.findContaining(in.locomotion.position, 1, &startArea) == nav::NavQueryResult::Found &&
				startArea.area == 1,
			"native route fixture starts physically inside the bottom NAV area"))
		return;
	if (preEntryStuck)
	{
		for (unsigned tick = 1; tick <= 7; ++tick)
		{
			in.frame.tick = tick;
			in.locomotion.observedAt = 1 + static_cast<float>(tick) * .1f;
			controller.observeMotion(in.actor, in.frame, in.locomotion);
		}
	}
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	if (redirectNoise)
	{
		in.noiseRoute = runtime::NoiseRouteUpdate::Enter;
		in.noiseGoalArea = 2U;
		if (!check(controller.update(snapshot, in, &intent, &decision) == runtime::NavRoamResult::MovementSuppressed &&
				controller.hasNativeLadderPath() && !controller.usingNativeLadder(),
				"investigation entry exposes the pending ladder route before movement starts"))
			return;
		const auto path = decision.pathSequence;
		in.noiseRoute = runtime::NoiseRouteUpdate::Redirect;
		in.noiseGoalArea = 1U;
		in.objectiveTarget = {0.0f, 75.0f, 0.0f};
		++in.frame.tick;
		in.locomotion.observedAt += .1f;
		check(controller.update(snapshot, in, &intent, &decision) == runtime::NavRoamResult::IntentReady &&
				controller.usingNativeLadder() && decision.pathSequence == path,
			"a throttled same-area redirect begins the retained ladder route");
		return;
	}
	auto result = controller.update(snapshot, in, &intent, &decision);
	if (preEntryStuck)
	{
		check(result == runtime::NavRoamResult::IntentReady && intent.nativeLadderControl && intent.nativeMustJump &&
				!controller.hasPath() && decision.recomputeReason == runtime::NavRecomputeReason::PathInvalidated,
			"pre-entry public motion stuck state reaches first ladder check before successful-update reset");
		return;
	}
	check(result == runtime::NavRoamResult::IntentReady && intent.nativeLadderControl && decision.linkHow == 4 &&
			intent.nativeForward == 1,
		"actual NavRoam selects ladder-only route and emits native approach instead of a fake floor portal");
	if (!intent.nativeLadderControl)
	{
		std::fprintf(stderr, "native route result=%u stage=%u failure=%u current=%u path=%u\n",
			static_cast<unsigned>(result), static_cast<unsigned>(decision.stage),
			static_cast<unsigned>(decision.failureReason), decision.currentArea,
			static_cast<unsigned>(decision.pathResult));
		return;
	}
	unsigned searches = decision.pathSearchStats.searchCalls;
	const auto sequence = decision.pathSequence;
	const auto step = [&](float seconds = .1f)
	{
		++in.frame.tick;
		in.locomotion.observedAt += seconds;
		result = controller.update(snapshot, in, &intent, &decision);
		searches += decision.pathSearchStats.searchCalls;
	};
	nav::NativeLadderObservation lookIn = observation();
	lookIn.frame = in.frame;
	lookIn.now = in.locomotion.observedAt;
	nav::NativeLadderLook look{};
	nav::NavLadder source{};
	check(controller.nativeLadderLook(snapshot, lookIn, &look, &source) && look.pitch == -60 &&
			source.entityGeneration == 100,
		"native path exposes actor-qualified per-command ladder view and exact source identity");
	in.locomotion.position.y = 20;
	in.friendRayOrigin.y = 20;
	step();
	step();
	in.locomotion.onLadder = true;
	in.ladderContact = true;
	in.locomotion.grounded = false;
	step();
	in.locomotion.position.z = 100;
	in.friendRayOrigin.z = 136;
	step();
	check(result == runtime::NavRoamResult::IntentReady && intent.nativeLadderControl && intent.nativeForward == 1,
		"native climb continues between disconnected floors without offmesh or ground recovery");
	in.locomotion.position = {0, 0, 200};
	in.friendRayOrigin = {0, 0, 236};
	step();
	step(.41f);
	in.locomotion.onLadder = false;
	in.ladderContact = false;
	in.locomotion.grounded = true;
	in.locomotion.position.y = -16;
	in.friendRayOrigin.y = -16;
	step();
	check(result == runtime::NavRoamResult::IntentReady && !intent.nativeLadderControl && intent.speed > 0 &&
			searches == 1 && decision.pathSequence == sequence,
		"completed ladder resumes stored floor suffix toward copied endpoint with no repeated AStar or new path identity");
	in.locomotion.position = {32, -100, 200};
	in.friendRayOrigin = {32, -100, 236};
	step();
	check(result == runtime::NavRoamResult::TargetReached,
		"mixed native path completes at its copied endpoint after ladder dismount");
	controller.reset();
	in.frame.tick++;
	in.locomotion.position = {0, 100, 0};
	in.friendRayOrigin = {0, 100, 36};
	in.locomotion.observedAt += 1;
	controller.update(snapshot, in, &intent, &decision);
	controller.destroyPath();
	lookIn.frame = in.frame;
	check(!controller.hasPath() && !controller.nativeLadderLook(snapshot, lookIn, &look),
		"DestroyPath clears native traversal and cached ladder look");
}
void testFloorPrefixAndSourceReplacement()
{
	nav::NavDocument document;
	document.setSourceIdentity({5, 100, 200});
	for (unsigned id = 1; id <= 4; ++id)
	{
		nav::NavArea area{};
		area.id = id;
		const float z = id <= 2 ? 0.0f : 200.0f;
		area.extent = {{id == 1			 ? -128.0f
							   : id == 4 ? 64.0f
										 : -64.0f,
						   id <= 2 ? 0.0f : -128.0f, z},
			{id == 1		  ? -64.0f
					: id == 4 ? 256.0f
							  : 64.0f,
				64, z}};
		area.northEastZ = area.southWestZ = z;
		if (id == 1)
			area.connections[1].push_back(2);
		if (id == 2)
			area.connections[3].push_back(1);
		if (id == 3)
			area.connections[1].push_back(4);
		if (id == 4)
			area.connections[3].push_back(3);
		document.addArea(area);
	}
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 1);
	const auto snapshot = publisher.snapshot();
	auto climb = ladder();
	climb.bottomArea = 2;
	climb.topAreas = {{3, 0, 0, 0}};
	climb.entityIndex = 6;
	climb.entityGeneration = 100;
	nav::NavLadderTopology topology{snapshot.revision(), 1, {climb}};
	Probe probe;
	MinimumRandom random;
	runtime::NavRoamController controller(compat::RuntimeMode::Compatibility);
	controller.setRandomSource(&random);
	runtime::NavRoamObservation in{};
	in.actor = {1, 7};
	in.frame = {1, 1, 1};
	in.team = 1;
	in.hasObjectiveTarget = true;
	in.objectiveTarget = {180, -64, 200};
	in.objectiveRouteType = nav::NavRouteType::Safest;
	in.collectPathStats = true;
	in.nativeLadders = &topology;
	in.nativeLadderProbe = &probe;
	in.friendRayOriginAvailable = true;
	in.friendRayOrigin = {-100, 32, 36};
	in.locomotion.position = {-100, 32, 0};
	in.locomotion.grounded = true;
	in.locomotion.standingClearance = 72;
	in.locomotion.crouchingClearance = 36;
	in.locomotion.clearanceAvailable = true;
	in.locomotion.timingAvailable = true;
	in.locomotion.observedAt = 1;
	in.locomotion.viewYawAvailable = true;
	in.locomotion.viewYaw = -90;
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	auto result = controller.update(snapshot, in, &intent, &decision);
	check(result == runtime::NavRoamResult::IntentReady && !intent.nativeLadderControl && intent.direction.x > 0 &&
			decision.selectedPath == std::vector<nav::AreaId>({1, 2, 3, 4}),
		"mixed native route follows its actual floor prefix before ladder approach");
	if (!controller.hasPath())
		return;
	in.locomotion.position = {0, 20, 0};
	in.friendRayOrigin = {0, 20, 36};
	++in.frame.tick;
	in.locomotion.observedAt += .1f;
	result = controller.update(snapshot, in, &intent, &decision);
	check(result == runtime::NavRoamResult::IntentReady && intent.nativeLadderControl && decision.linkFromArea == 2 &&
			decision.linkToArea == 3,
		"entering ladder source area switches from floor prefix to copied native step");
	topology.ladders[0].entityGeneration = 101;
	++in.frame.tick;
	in.locomotion.observedAt += .1f;
	controller.update(snapshot, in, &intent, &decision);
	check(!intent.nativeLadderControl && !controller.hasPath(),
		"reused public ladder entity cannot continue a path captured for its old serial");
	controller.reset();
	probe = {};
	topology.ladders[0].entityGeneration = 100;
	in.frame.tick++;
	in.locomotion.observedAt += 1;
	in.locomotion.position = {-100, 32, 0};
	in.friendRayOrigin = {-100, 32, 36};
	controller.update(snapshot, in, &intent, &decision);
	probe.callback = [&]
	{
		controller.destroyPath();
	};
	in.locomotion.onLadder = true;
	in.ladderContact = true;
	in.locomotion.grounded = false;
	in.locomotion.position = {0, 20, 0};
	in.friendRayOrigin = {0, 20, 36};
	in.frame.tick++;
	in.locomotion.observedAt += .1f;
	controller.update(snapshot, in, &intent, &decision);
	check(!intent.nativeLadderControl && !controller.hasPath(),
		"endpoint trace reentry cannot restore the route that its callback destroyed");
}
void testFreshOwnerRouteSurvivesOldTrace()
{
	const auto snapshot = makeSnapshot();
	auto climb = ladder();
	climb.entityIndex = 6;
	climb.entityGeneration = 100;
	nav::NavLadderTopology topology{snapshot.revision(), 1, {climb}};
	Probe oldProbe, newProbe;
	MinimumRandom random;
	runtime::NavAreaVisitHistory history;
	runtime::NavRoamController controller(compat::RuntimeMode::Compatibility);
	controller.setRandomSource(&random);
	controller.setAreaVisitHistory(&history);
	runtime::NavRoamObservation old{};
	old.actor = {1, 7};
	old.frame = {1, 1, 1};
	old.team = 1;
	old.hasObjectiveTarget = true;
	old.objectiveTarget = {32, 32, 0};
	old.nativeLadders = &topology;
	old.nativeLadderProbe = &oldProbe;
	old.friendRayOriginAvailable = true;
	old.friendRayOrigin = {0, -100, 236};
	old.locomotion.position = {0, -100, 200};
	old.locomotion.grounded = true;
	old.locomotion.standingClearance = 72;
	old.locomotion.crouchingClearance = 36;
	old.locomotion.clearanceAvailable = true;
	old.locomotion.timingAvailable = true;
	old.locomotion.observedAt = 1;
	old.locomotion.viewYawAvailable = true;
	old.locomotion.viewYaw = 90;
	auto fresh = old;
	fresh.actor = {2, 9};
	fresh.nativeLadderProbe = &newProbe;
	fresh.objectiveTarget = {32, -100, 200};
	fresh.friendRayOrigin = {0, 100, 36};
	fresh.locomotion.position = {0, 100, 0};
	fresh.locomotion.viewYaw = -90;
	bool replaced = false;
	oldProbe.callback = [&]
	{
		oldProbe.valid = false;
		controller = runtime::NavRoamController(compat::RuntimeMode::Compatibility);
		controller.setRandomSource(&random);
		controller.setAreaVisitHistory(&history);
		nav::LocomotionIntent intent{};
		runtime::NavRoamDecision decision{};
		controller.update(snapshot, fresh, &intent, &decision);
		replaced = controller.hasPath() && intent.nativeLadderControl;
	};
	nav::LocomotionIntent intent{};
	runtime::NavRoamDecision decision{};
	controller.update(snapshot, old, &intent, &decision);
	nav::NativeLadderObservation lookIn = observation();
	lookIn.actor = {2, 9};
	nav::NativeLadderLook look{};
	check(replaced && controller.hasPath() && controller.nativeLadderLook(snapshot, lookIn, &look) &&
			!intent.nativeLadderControl,
		"old descending trace cannot reset the fresh owner route even when local revision counters repeat");
}
}
int main()
{
	testNativeRecovery();
	testFreshOwnerRouteSurvivesOldTrace();
	testFloorPrefixAndSourceReplacement();
	testNativeRouteIntegration();
	testNativeRouteIntegration(true);
	testNativeRouteIntegration(false, true);
	testContactBoundsAndDismount();
	testAscending();
	testDescendingAndFailures();
	return failures ? 1 : 0;
}
