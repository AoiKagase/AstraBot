// SPDX-License-Identifier: MPL-2.0
#include "nav/local/local_door.hpp"
#include "route_fixture.hpp"

#include <cassert>

using namespace astrabot;
using namespace astrabot::nav;

namespace
{
local::Binding binding()
{
	return {{1}, {2, {3}}, {4}, 7, 0};
}

runtime::MovementSnapshot actor()
{
	runtime::MovementSnapshot s;
	const auto b = binding();
	s.agent = b.agent;
	s.actor = b.actor;
	s.map = b.map;
	s.tick = {1};
	s.elapsedUs = 40'000;
	s.kind = runtime::ActorKind::ManagedBot;
	s.connected = s.alive = s.joined = s.grounded = true;
	s.position = model::NavVector3{50, 50, 36};
	s.hull = runtime::HullDimensions{{-16, -16, -36}, {16, 16, 36}};
	return s;
}

struct DoorWorld final : runtime::IWorldQueries
{
	runtime::DoorObservation door{42, false, true, model::NavVector3{0, 90, 0}, false};
	runtime::QueryError error{runtime::QueryError::None};
	unsigned calls{};

	runtime::WorldQueryResult query(const runtime::QueryRequest& q) override
	{
		++calls;
		runtime::WorldQueryResult r;
		r.stamp = q.stamp;
		r.kind = q.kind;
		r.error = error;
		if (q.kind == runtime::QueryKind::Door && error == runtime::QueryError::None)
			r.door = door;
		if (q.kind == runtime::QueryKind::Door && door.canTouch)
			r.hull = runtime::HullObservation{0.0025F, {50.1F, 50, 36}, {-1, 0, 0}, false, false};
		return r;
	}
};

std::shared_ptr<const query::NavSpatialIndex> index()
{
	route_test::Area area{1, {{0, 0, 0}, {100, 100, 0}, 0, 0}};
	const auto built = query::NavSpatialIndex::build(route_test::snapshot({area}), {2, 3, 1'000'000});
	assert(built);
	return *built.value;
}
} // namespace

int main()
{
	const auto spatial = index();
	const local::GroundProbeLimits limits{9, 4, 48, 16, 18, 18, 64, 4, 2, 0.7};
	auto s = actor();
	DoorWorld world;
	local::LocalDoor localDoor;
	std::uint32_t used = 0;

	const auto press = localDoor.update(s, binding(), {90, 50, 36}, *spatial, s.map, limits, world, 0, used, 4);
	assert(press && press->disposition == local::MotionDisposition::Hold);
	assert(press->intent.use == local::ActionRequest::Press && press->intent.view);
	assert(used == 1 && world.calls == 1);

	++s.tick.value;
	const auto waiting = localDoor.update(s, binding(), {90, 50, 36}, *spatial, s.map, limits, world, 1'000, used, 4);
	assert(waiting && waiting->intent.use == local::ActionRequest::None);

	++s.tick.value;
	world.door.open = true;
	const auto clear = localDoor.update(s, binding(), {90, 50, 36}, *spatial, s.map, limits, world, 2'000, used, 4);
	assert(clear && clear->doorState == local::DoorWaitState::Clear);

	localDoor.reset();
	world.door.open = false;
	used = 0;
	s.tick = {10};
	assert(localDoor.update(s, binding(), {90, 50, 36}, *spatial, s.map, limits, world, 10, used, 2)->intent.use ==
		   local::ActionRequest::Press);
	++s.tick.value;
	const auto timedOut = localDoor.update(s, binding(), {90, 50, 36}, *spatial, s.map, limits, world,
										   10 + local::LocalDoor::timeoutUs, used, 2);
	assert(timedOut && timedOut->reason == local::WalkReason::DoorBlocked &&
		   timedOut->doorReason == local::DoorWaitReason::TimedOut);

	localDoor.reset();
	world.error = runtime::QueryError::Unavailable;
	used = 0;
	s.tick = {20};
	const auto unknown = localDoor.update(s, binding(), {90, 50, 36}, *spatial, s.map, limits, world, 0, used, 1);
	assert(unknown && unknown->disposition == local::MotionDisposition::Hold &&
		   unknown->state == local::WalkState::Running && unknown->probeReason == local::ProbeReason::QueryUnavailable);

	localDoor.reset();
	world.error = runtime::QueryError::None;
	world.door = {42, false, false, std::nullopt, true};
	used = 0;
	s.tick = {30};
	const auto unsupportedContact =
		localDoor.update(s, binding(), {90, 50, 36}, *spatial, s.map, limits, world, 0, used, 1);
	assert(unsupportedContact && !unsupportedContact->contact &&
		   unsupportedContact->probeReason == local::ProbeReason::BudgetExceeded);
}
