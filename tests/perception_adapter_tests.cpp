#include "astrabot/perception/perception.hpp"
#include "observation_adapter.hpp"

#include <cmath>
#include <cstdio>

namespace
{
using namespace astrabot;

int gTraceMode = 0;
int gTraceCount = 0;

void traceLine(const float *start, const float *end, int noMonsters,
	edict_t *skip, TraceResult *result)
{
	(void)start;
	(void)noMonsters;
	(void)skip;
	++gTraceCount;
	*result = {};
	if (gTraceMode == 1)
		result->flFraction = 0.5f;
	else if (gTraceMode == 2 && end[2] <= 20.0f)
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

world::ActorObservation actor(world::ActorKey key, world::TeamRelation relation,
	world::WorldPosition position, bool visible, std::uint8_t parts)
{
	world::ActorObservation value = {};
	value.actor = key;
	value.state = visible ? world::ObservationState::ObservedPresent
		: world::ObservationState::ObservedAbsent;
	value.relation = relation;
	value.position = visible ? position : world::WorldPosition{};
	value.confidence = visible ? world::ContactConfidence{1.0f, 0U}
		: world::ContactConfidence{0.0f, 0U};
	value.visible = visible;
	value.fovPassed = visible;
	value.losPassed = visible;
	value.visibleParts = visible ? parts : world::VisibleNone;
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
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(10U),
			&visible) == metamod::ObservationAdapterResult::Accepted &&
			visible.visible && visible.fovPassed && visible.losPassed &&
			(visible.visibleParts & world::VisibleChest) != 0U && gTraceCount == 5,
			"adapter feeds five ordered CSBot body probes into perception"))
		return false;

	target.v.origin[0] = 0.0f;
	target.v.origin[1] = 128.0f;
	gTraceCount = 0;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(11U),
			&visible) == metamod::ObservationAdapterResult::Accepted &&
			!visible.visible && !visible.fovPassed && gTraceCount == 0,
			"outside-FOV enemy is rejected before trace"))
		return false;

	const float angle = 50.0f * 3.14159265358979323846f / 180.0f;
	observer.v.fov = 15.0f;
	target.v.origin[0] = 128.0f * std::cos(angle);
	target.v.origin[1] = 128.0f * std::sin(angle);
	gTraceMode = 0;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(111U),
			&visible) == metamod::ObservationAdapterResult::Accepted &&
			visible.visible,
			"scoped public fov does not replace the reference private view-cone threshold"))
		return false;

	observer.v.fov = 90.0f;
	target.v.origin[0] = 128.0f;
	target.v.origin[1] = 0.0f;
	gTraceMode = 2;
	if (!check(adapter.collectVisibility(&observer, &target, {2U, 1U}, frame(12U),
			&visible) == metamod::ObservationAdapterResult::Accepted &&
			visible.visible && visible.visibleParts == world::VisibleHead,
			"partial visibility keeps the reference body-region mask"))
		return false;

	perception::PerceptionAssembler assembler;
	world::WorldSnapshot snapshot;
	perception::PerceptionInput input({frame(12U), {1U, 1U}, 0U});
	input.addActor(actor({1U, 1U}, world::TeamRelation::Friendly,
		{0.0f, 0.0f, 0.0f}, true, static_cast<std::uint8_t>(
			world::VisibleChest | world::VisibleHead | world::VisibleFeet |
			world::VisibleLeftSide | world::VisibleRightSide)));
	world::ActorObservation targetObservation = actor(
		{2U, 1U}, world::TeamRelation::Hostile,
		{128.0f, 0.0f, 0.0f}, true, visible.visibleParts);
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

int main()
{
	return testAdapterPerceptionProductionFixture() ? 0 : 1;
}
