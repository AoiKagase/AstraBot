#include "astrabot/perception/perception.hpp"
#include "bridge/observation_adapter.hpp"

#include <cmath>
#include <cstdio>

namespace
{
using namespace astrabot;

int gTraceMode = 0;
int gTraceCount = 0;
float gTraceEnd[5][3] = {};
float gEngineForwardX = 0.5f;
float gEngineModelPitch = 0.0f;

void angleVectors(const float *angles, float *forward, float *, float *)
{
	gEngineModelPitch = angles[0];
	forward[0] = gEngineForwardX;
	forward[1] = 0.0f;
	forward[2] = 0.0f;
}

void traceLine(const float *start, const float *end, int noMonsters, edict_t *skip, TraceResult *result)
{
	(void)start;
	(void)skip;
	if (gTraceCount < 5)
		for (int axis = 0; axis < 3; ++axis)
			gTraceEnd[gTraceCount][axis] = end[axis];
	++gTraceCount;
	*result = {};
	if (gTraceMode == 1)
		result->flFraction = 0.5f;
	else if (gTraceMode == 2 && end[2] <= 20.0f)
		result->flFraction = 0.5f;
	else if (gTraceMode == 3 && noMonsters != 0x101)
		result->flFraction = 0.5f;
	else
		result->flFraction = 1.0f;
}

bool check(bool condition, const char *message)
{
	if (!condition)
		std::fprintf(stderr, "FAIL: %s\n", message);
	return condition;
}

world::FrameIdentity frame(std::uint32_t tick)
{
	return {3U, 7U, tick};
}

world::ActorObservation actor(
	world::ActorKey key, world::TeamRelation relation, world::WorldPosition position, bool visible, std::uint8_t parts)
{
	world::ActorObservation value = {};
	value.actor = key;
	value.state = visible ? world::ObservationState::ObservedPresent : world::ObservationState::ObservedAbsent;
	value.relation = relation;
	value.position = visible ? position : world::WorldPosition{};
	value.confidence = visible ? world::ContactConfidence{1.0f, 0U} : world::ContactConfidence{0.0f, 0U};
	value.visible = visible;
	value.fovPassed = visible;
	value.losPassed = visible;
	value.visibleParts = visible ? parts : static_cast<std::uint8_t>(world::VisibleNone);
	return value;
}
}

bool testAdapterPerceptionProductionFixture()
{
	using namespace astrabot;
	enginefuncs_t engine = {};
	globalvars_t globals = {};
	engine.pfnTraceLine = traceLine;
	metamod::ObservationAdapter adapter;
	adapter.configure(&engine, &globals);
	edict_t observer = {};
	edict_t target = {};
	observer.free = 0;
	target.free = 0;
	observer.v.origin[0] = 0.0f;
	observer.v.origin[1] = 0.0f;
	observer.v.origin[2] = 0.0f;
	observer.v.view_ofs[2] = 16.0f;
	observer.v.angles[1] = 0.0f;
	target.v.origin[0] = 128.0f;
	target.v.origin[1] = 0.0f;
	target.v.origin[2] = 0.0f;

	perception::VisionObservation visible = {};
	gTraceMode = 0;
	gTraceCount = 0;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(10U), &visible) ==
					metamod::ObservationAdapterResult::Accepted &&
				visible.visible && visible.fovPassed && visible.losPassed &&
				(visible.visibleParts & world::VisibleChest) != 0U && gTraceCount == 5,
			"adapter feeds five ordered CSBot body probes into perception"))
		return false;

	const auto clearParts = visible.visibleParts;
	constexpr auto ignoreGlass = metamod::PlayerVisionGlass::IgnoreTransparent;
	gTraceMode = 3;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(10U), &visible) ==
					metamod::ObservationAdapterResult::Accepted && !visible.visible,
			"default player vision is blocked by transparent glass") ||
		!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(10U), &visible, true, true, ignoreGlass) ==
					metamod::ObservationAdapterResult::Accepted && visible.visible &&
				visible.visibleParts == clearParts && visible.visibleInFovParts == clearParts,
			"CSBot player vision sees the same body parts through transparent glass as in clear sight"))
		return false;
	gTraceMode = 1;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(10U), &visible, true, true, ignoreGlass) ==
					metamod::ObservationAdapterResult::Accepted && !visible.visible,
			"CSBot player vision still respects opaque cover"))
		return false;

	gTraceMode = 0;
	target.v.origin[0] = 0.0f;
	target.v.origin[1] = 128.0f;
	gTraceCount = 0;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(11U), &visible) ==
					metamod::ObservationAdapterResult::Accepted &&
				!visible.visible && !visible.fovPassed && gTraceCount == 0,
			"outside-FOV enemy is rejected before trace"))
		return false;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(11U), &visible, false, true) ==
					metamod::ObservationAdapterResult::Accepted &&
				visible.visible && visible.visibleParts != world::VisibleNone &&
				visible.visibleInFovParts == world::VisibleNone && gTraceCount == 5,
			"tracking keeps outside-FOV line of sight without adding any FOV-qualified traces"))
		return false;

	target.v.origin[0] = 180.0f;
	target.v.origin[1] = 0.0f;
	observer.v.angles[1] = 61.5f;
	gTraceCount = 0;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(11U), &visible, false, true) ==
					metamod::ObservationAdapterResult::Accepted &&
				visible.visibleInFovParts == world::VisibleLeftSide &&
				(visible.visibleParts & world::VisibleChest) != 0U && gTraceCount == 5,
			"the tracked target's side is the only visible part inside the FOV at the edge"))
		return false;

	constexpr auto modelCone = metamod::PlayerVisionCone::ModelPitchAndYaw;
	const auto collectWithModelCone = [&](bool checkFov, bool reportFovParts,
									  metamod::PlayerVisionGlass glass = metamod::PlayerVisionGlass::Occludes) {
		gTraceCount = 0;
		return adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(11U), &visible,
			checkFov, reportFovParts, glass, modelCone) ==
			metamod::ObservationAdapterResult::Accepted;
	};
	observer.v.v_angle[0] = 70.0f;
	observer.v.v_angle[1] = 180.0f;
	if (!check(collectWithModelCone(true, true) && visible.visibleParts == world::VisibleLeftSide &&
			gTraceCount == 1, "the reference cone uses the model angles independently of command angles"))
		return false;
	observer.v.angles[0] = 30.0f;
	if (!check(collectWithModelCone(true, true) && !visible.visible && !visible.fovPassed && gTraceCount == 0,
			"model pitch narrows the horizontal cone before issuing any LOS traces") ||
		!check(collectWithModelCone(false, true) && visible.visible && visible.visibleParts == clearParts &&
			visible.visibleInFovParts == world::VisibleNone && gTraceCount == 5,
			"tracked LOS retains its five probes while reporting the pitched candidate mask"))
		return false;
	gTraceCount = 0;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(11U), &visible) ==
			metamod::ObservationAdapterResult::Accepted && visible.visibleParts == world::VisibleLeftSide &&
			gTraceCount == 1, "yaw-only perception ignores model pitch"))
		return false;

	observer.v.angles[0] = 0.0f;
	observer.v.angles[1] = 0.0f;
	target.v.origin[0] = 0.0f;
	target.v.origin[2] = 64.0f;
	if (!check(collectWithModelCone(true, true) && !visible.visible && gTraceCount == 0,
			"at yaw zero neither coincident side passes the strict reference FOV test"))
		return false;
	observer.v.angles[1] = 90.0f;
	if (!check(collectWithModelCone(true, true) && visible.visibleParts == world::VisibleLeftSide &&
			visible.visibleInFovParts == world::VisibleLeftSide && gTraceCount == 1 &&
			gTraceEnd[0][0] == 0.0f && gTraceEnd[0][1] == 13.0f && gTraceEnd[0][2] == 64.0f,
			"coincident origins use the reference left probe at +13 Y"))
		return false;
	observer.v.angles[1] = 270.0f;
	if (!check(collectWithModelCone(true, true) && visible.visibleParts == world::VisibleRightSide &&
			visible.visibleInFovParts == world::VisibleRightSide && gTraceCount == 1 &&
			gTraceEnd[0][0] == 0.0f && gTraceEnd[0][1] == -13.0f && gTraceEnd[0][2] == 64.0f,
			"reversing yaw admits only the coincident right probe at -13 Y"))
		return false;
	gTraceMode = 3;
	if (!check(collectWithModelCone(false, true, ignoreGlass) && visible.visibleParts == clearParts &&
			visible.visibleInFovParts == world::VisibleRightSide && gTraceCount == 5 &&
			gTraceEnd[3][1] == 13.0f && gTraceEnd[4][1] == -13.0f,
			"retained LOS traces all five coincident points through glass and reports the independent FOV mask") ||
		!check(collectWithModelCone(false, false, ignoreGlass) && visible.visibleParts == clearParts && gTraceCount == 5,
			"unrestricted reference LOS traces both coincident sides without a cone"))
		return false;
	gTraceMode = 0;
	observer.v.angles[0] = 70.0f;
	if (!check(collectWithModelCone(true, true) && !visible.visible && gTraceCount == 0,
			"model pitch can still exclude the coincident side"))
		return false;
	gTraceCount = 0;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(11U), &visible, false, true) ==
			metamod::ObservationAdapterResult::Accepted &&
			visible.visibleParts == (world::VisibleChest | world::VisibleHead | world::VisibleFeet) && gTraceCount == 3,
			"yaw-only vision retains its three center probes at coincident XY"))
		return false;
	target.v.origin[2] = 0.0f;
	observer.v.angles[1] = 0.0f;

	engine.pfnAngleVectors = angleVectors;
	observer.v.angles[0] = 60.0f;
	target.v.origin[0] = 128.0f;
	gEngineForwardX = 0.5f;
	if (!check(collectWithModelCone(true, true) && gEngineModelPitch == 60.0f && !visible.visible &&
			gTraceCount == 0, "the engine model forward vector meets a strict 0.5 cutoff"))
		return false;
	gEngineForwardX = std::nextafter(0.5f, 1.0f);
	if (!check(collectWithModelCone(true, true) && visible.visible && gTraceCount == 3,
			"the smallest engine forward component above 0.5 admits the three center probes"))
		return false;
	engine.pfnAngleVectors = nullptr;
	observer.v.angles[0] = 0.0f;

	const float angle = 50.0f * 3.14159265358979323846f / 180.0f;
	observer.v.fov = 15.0f;
	target.v.origin[0] = 128.0f * std::cos(angle);
	target.v.origin[1] = 128.0f * std::sin(angle);
	gTraceMode = 0;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(111U), &visible) ==
					metamod::ObservationAdapterResult::Accepted &&
				visible.visible,
			"scoped public fov does not replace the reference private view-cone threshold"))
		return false;

	observer.v.fov = 90.0f;
	target.v.origin[0] = 128.0f;
	target.v.origin[1] = 0.0f;
	gTraceMode = 2;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(12U), &visible) ==
					metamod::ObservationAdapterResult::Accepted &&
				visible.visible && visible.visibleParts == world::VisibleHead,
			"partial visibility keeps the reference body-region mask"))
		return false;

	perception::PerceptionAssembler assembler;
	world::WorldSnapshot snapshot;
	perception::PerceptionInput input({frame(12U), {1U, 1U}, 0U});
	input.addActor(actor({1U, 1U}, world::TeamRelation::Friendly, {0.0f, 0.0f, 0.0f}, true,
		static_cast<std::uint8_t>(world::VisibleChest | world::VisibleHead | world::VisibleFeet |
			world::VisibleLeftSide | world::VisibleRightSide)));
	world::ActorObservation targetObservation =
		actor({2U, 1U}, world::TeamRelation::Hostile, {128.0f, 0.0f, 0.0f}, true, visible.visibleParts);
	input.addActor(targetObservation);
	if (!check(assembler.publish(input, &snapshot) == perception::PerceptionResult::Published,
			"adapter observation publishes into Core belief"))
		return false;
	const world::ActorObservation *published = nullptr;
	return check(snapshot.findActor({2U, 1U}, &published) == world::ContactLookupResult::Found &&
			published != nullptr && published->memory.isUsable() &&
			published->memory.knowledge == world::KnowledgeState::Observed,
		"production fixture ends at a compatibility belief");
}

bool testAdapterProfilerCounters()
{
	using namespace astrabot;
	enginefuncs_t engine = {};
	globalvars_t globals = {};
	engine.pfnTraceLine = traceLine;
	metamod::RuntimeProfiler profiler;
	profiler.setEnabled(true, 0.0);
	metamod::ObservationAdapter adapter;
	adapter.configure(&engine, &globals);
	adapter.setProfiler(&profiler);
	edict_t observer = {};
	edict_t target = {};
	observer.free = 0;
	target.free = 0;
	observer.v.view_ofs[2] = 16.0f;
	target.v.origin[0] = 128.0f;
	perception::VisionObservation visible = {};
	gTraceMode = 0;
	gTraceCount = 0;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(20U), &visible) ==
					metamod::ObservationAdapterResult::Accepted &&
				visible.visible && gTraceCount == 5,
			"profiler fixture performs five visibility traces"))
	{
		return false;
	}
	metamod::RuntimeProfilerReport report = {};
	if (!check(profiler.consumeReport(1.0, &report), "profiler fixture emits one-second report"))
	{
		return false;
	}
	const auto &vision = report.stages[static_cast<std::size_t>(metamod::RuntimeProfilerStage::Vision)];
	return check(vision.calls == 1U && report.traceLineCalls == 5U && report.visibilityCandidates == 1U &&
			report.fovChecks == 5U && report.losChecks == 5U && report.bodyProbeCalls == 5U,
		"profiler separates vision candidate, FOV, LOS, TraceLine, and body probes");
}

bool testAdapterDiagnosticToggles()
{
	using namespace astrabot;
	enginefuncs_t engine = {};
	globalvars_t globals = {};
	engine.pfnTraceLine = traceLine;
	metamod::RuntimeProfiler profiler;
	profiler.setEnabled(true, 1.0);
	metamod::ObservationAdapter adapter;
	adapter.configure(&engine, &globals);
	adapter.setProfiler(&profiler);
	edict_t observer = {};
	edict_t target = {};
	observer.free = 0;
	target.free = 0;
	target.v.origin[0] = 128.0f;
	perception::VisionObservation visible = {};
	gTraceMode = 0;
	gTraceCount = 0;
	adapter.setPerformanceToggles(true, false);
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(30U), &visible) ==
					metamod::ObservationAdapterResult::Accepted &&
				gTraceCount == 0,
			"vision diagnostic toggle skips all probes"))
	{
		return false;
	}
	metamod::RuntimeProfilerReport report = {};
	if (!check(profiler.consumeReport(2.0, &report) && report.visibilityCandidates == 1U && report.fovChecks == 0U &&
				report.traceLineCalls == 0U,
			"vision diagnostic report records skipped probes"))
	{
		return false;
	}
	profiler.setEnabled(true, 3.0);
	adapter.setPerformanceToggles(false, true);
	gTraceCount = 0;
	visible = {};
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(31U), &visible) ==
					metamod::ObservationAdapterResult::Accepted &&
				gTraceCount == 0,
			"trace diagnostic toggle skips engine traces"))
	{
		return false;
	}
	return check(profiler.consumeReport(4.0, &report) && report.visibilityCandidates == 1U && report.fovChecks == 5U &&
			report.traceLineCalls == 0U && report.bodyProbeCalls == 5U,
		"trace diagnostic report keeps FOV/body counters separate");
}

int main()
{
	return testAdapterPerceptionProductionFixture() && testAdapterProfilerCounters() && testAdapterDiagnosticToggles()
		? 0
		: 1;
}
