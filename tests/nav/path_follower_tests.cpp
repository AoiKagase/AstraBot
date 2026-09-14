// SPDX-License-Identifier: MPL-2.0
#include "nav/local/path_follower.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <limits>

using namespace astrabot::nav;
namespace
{
route_test::Area area(std::uint32_t id, float x, float width = 100)
{
	return {id, {{x, 0, 0}, {x + width, 100, 0}, 0, 0}, {}};
}
std::shared_ptr<const corridor::Corridor> path(std::vector<route_test::Area> areas, std::uint32_t goal)
{
	const auto graph = query::NavGraph::build(route_test::snapshot(areas), {100, 100, 1000000});
	assert(graph);
	const auto route = query::NavRouteSearch::search(**graph.value, {{1}, {goal}, {100, 1000000}, false});
	assert(route);
	const auto result = corridor::Corridor::build(**graph.value, *route.value, {16, 16}, {100, 1000000, 1000},
												  corridor::PortalPolicy::AllowMicroTransit);
	assert(result);
	return result.value;
}
void closeBoundariesAndMissingSupport()
{
	auto a = area(1, 0), b = area(2, 100, 4), c = area(3, 104, 4), d = area(4, 108);
	a.targets[1] = {2};
	b.targets[1] = {3};
	c.targets[1] = {4};
	local::PathFollower follower(path({a, b, c, d}, 4), {180, 50, 0});
	auto r = follower.update({90, 50, 0}, {1}, true, 80);
	assert(r.target && r.target->x > 108 && r.step == 0);
	r = follower.update({109, 50, 0}, {4}, false, 80);
	assert(r.status == local::FollowStatus::MissingSupport && follower.step() == 0);
	r = follower.update({109, 50, 0}, {4}, true, 80);
	assert(r.step == 3 && r.changed && r.target && r.target->x > 109 && !r.arrived);
	r = follower.update({180, 50, 0}, {4}, true, 80);
	assert(r.arrived && r.status == local::FollowStatus::Arrived);
}
void driftDoesNotRewind()
{
	auto a = area(1, 0), b = area(2, 100), c = area(3, 200);
	a.targets[1] = {2};
	b.targets[1] = {3};
	local::PathFollower follower(path({a, b, c}, 3), {280, 50, 0});
	assert(follower.update({101, 50, 0}, {2}, true, 60).step == 1);
	const auto drift = follower.update({99, 50, 0}, {1}, true, 60);
	assert(drift.step == 1 && drift.target && drift.target->x > 99);
	const auto invalid = follower.update({150, 50, 0}, {3}, true, 60);
	assert(invalid.status == local::FollowStatus::OutsideCorridor && follower.step() == 1);
}
void specialIsBarrier()
{
	auto a = area(1, 0), b = area(2, 100), c = area(3, 200), d = area(4, 300);
	a.targets[1] = {2};
	b.targets[1] = {3};
	c.targets[1] = {4};
	c.attributes = 2;
	local::PathFollower follower(path({a, b, c, d}, 4), {380, 50, 0});
	const auto first = follower.update({50, 50, 0}, {1}, true, 500);
	assert(first.target && first.target->x <= 200);
	auto r = follower.update({150, 50, 0}, {2}, true, 500);
	assert(r.step == 1 && r.specialTransition && r.target && r.target->x == 200);
	r = follower.update({350, 50, 0}, {4}, true, 500);
	assert(r.step == 1 && r.status == local::FollowStatus::OutsideCorridor);
	assert(!follower.completeSpecial(1, {3}, false));
	assert(follower.completeSpecial(1, {3}, true));
	assert(follower.step() == 2);
}
void sameAreaAndInvalidPosition()
{
	local::PathFollower follower(path({area(1, 0)}, 1), {80, 50, 0});
	assert(follower.update({50, 50, 0}, {1}, true, 15).target->x == 65);
	assert(follower.update({80, 50, 0}, {1}, true, 15).arrived);
	const auto bad = follower.update({std::numeric_limits<double>::quiet_NaN(), 50, 0}, {1}, true, 15);
	assert(bad.status == local::FollowStatus::Invalid);
}
void bendDoesNotCutCorner()
{
	auto a = area(1, 0), b = area(2, 100), c = area(3, 100);
	c.extent.northWest.y = 100;
	c.extent.southEast.y = 200;
	a.targets[1] = {2};
	b.targets[2] = {3};
	local::PathFollower follower(path({a, b, c}, 3), {150, 180, 0});
	const auto r = follower.update({20, 90, 0}, {1}, true, 300);
	assert(r.target && r.target->x >= 100 && r.target->y < 100);
	// Direct goal ray would cross x=100 beyond the first portal's y=84 end.
	const double crossingY = 90 + (r.target->y - 90) * (100 - 20) / (r.target->x - 20);
	assert(crossingY >= 16 && crossingY <= 84);
}
void supportedBoundaryRecoveryKeepsMeasuredPosition()
{
	auto a = area(1, 0), b = area(2, 100);
	a.targets[1] = {2};
	local::PathFollower follower(path({a, b}, 2), {180, 50, 0});
	const query::NavQueryPoint measured{-10, 50, 0};
	const auto recovered = follower.update(measured, {1}, true, 50);
	assert(recovered.status == local::FollowStatus::Moving && recovered.target && recovered.boundaryRecovery);
	assert(recovered.target->x == 0 && recovered.target->y == 50);
	assert(!recovered.changed && !recovered.arrived && follower.step() == 0);
	assert(measured.x == -10); // Guidance does not replace the observed position.
	const auto far = follower.update({-17, 50, 0}, {1}, true, 50);
	assert(far.status == local::FollowStatus::OutsideCorridor && !far.target);
	const auto unsupported = follower.update(measured, {1}, false, 50);
	assert(unsupported.status == local::FollowStatus::MissingSupport && !unsupported.target);
	const auto wrong = follower.update(measured, {2}, true, 50);
	assert(wrong.status == local::FollowStatus::OutsideCorridor && !wrong.target);
	const auto high = follower.update({-2, 50, 21}, {1}, true, 50);
	assert(high.status == local::FollowStatus::OutsideCorridor && !high.target);
	const auto shortAim = follower.update(measured, {1}, true, 4);
	assert(shortAim.target && shortAim.target->x == -6 && shortAim.boundaryRecovery);
	// A supported successor fallback is guidance only until actual containment.
	const auto successor = follower.update({110, -2, 0}, {2}, true, 50);
	assert(successor.target && successor.target->y == 0 && successor.boundaryRecovery);
	assert(!successor.changed && follower.step() == 0);
	const auto inside = follower.update({110, 0, 0}, {2}, true, 50);
	assert(inside.changed && follower.step() == 1 && !inside.boundaryRecovery);
}
void boundedGoalRecoveryNeverArrivesOutside()
{
	local::PathFollower follower(path({area(1, 0)}, 1), {0, 50, 0});
	const auto r = follower.update({-2, 50, 0}, {1}, true, 50, 12);
	assert(r.target && r.target->x == 0 && !r.arrived && !r.changed && r.boundaryRecovery);
	assert(follower.update({0, 50, 0}, {1}, true, 50, 12).arrived);
}
void boundaryRecoveryDoesNotSkipSpecial()
{
	auto a = area(1, 0), b = area(2, 100), c = area(3, 200);
	a.targets[1] = {2};
	b.targets[1] = {3};
	b.attributes = 2;
	local::PathFollower follower(path({a, b, c}, 3), {280, 50, 0});
	const auto r = follower.update({205, -2, 0}, {3}, true, 50);
	assert(r.status == local::FollowStatus::OutsideCorridor && follower.step() == 0);
}
void forwardProjectionSurvivesDriftAndLookAheadChanges()
{
	auto a = area(1, 0), b = area(2, 100), c = area(3, 200);
	a.targets[1] = {2};
	b.targets[1] = {3};
	local::PathFollower follower(path({a, b, c}, 3), {280, 50, 0});
	const auto initial = follower.update({40, 50, 0}, {1}, true, 250);
	assert(initial.target && initial.routeForward.x == 1 && initial.targetProjection > 0);
	const auto ahead = follower.update({80, 50, 0}, {1}, true, 250);
	const auto drift = follower.update({70, 50, 0}, {1}, true, 25);
	assert(drift.target && drift.targetProjection > 0 && drift.retainedTarget);
	assert(drift.routeProgress == ahead.routeProgress && drift.routeProgress >= initial.routeProgress);
	assert(drift.routeProjection < ahead.routeProjection); // Raw drift stays observable.
	const auto crossed = follower.update({110, 50, 0}, {2}, true, 40);
	assert(crossed.changed && crossed.step == 1 && crossed.routeProgress >= drift.routeProgress);
}
void crossedPortalNeverPullsBackToSource()
{
	auto a = area(1, 0), b = area(2, 100);
	a.targets[1] = {2};
	local::PathFollower follower(path({a, b}, 2), {180, 50, 0});
	assert(follower.update({90, 50, 0}, {1}, true, 80).target);
	// Stale area identity while world-supported beyond the source portal must
	// not generate the old backwards x=100 boundary recovery command.
	const auto stale = follower.update({110, 50, 0}, {1}, true, 80);
	assert(!stale.target && stale.status == local::FollowStatus::OutsideCorridor && stale.step == 0);
	const auto measured = follower.update({110, 50, 0}, {2}, true, 80);
	assert(measured.target && measured.target->x > 110 && measured.step == 1);
}
void pointPortalUsesCardinalOrientation()
{
	const float targetX[]{100, 100, 100, -100};
	const float targetY[]{-100, 100, 100, 100};
	const query::NavQueryPoint starts[]{{100, 10, 0}, {90, 100, 0}, {100, 90, 0}, {10, 100, 0}};
	const query::NavQueryPoint goals[]{{100, -10, 0}, {110, 100, 0}, {100, 110, 0}, {-10, 100, 0}};
	for (std::uint8_t direction = 0; direction < 4; ++direction)
	{
		auto a = area(1, 0), b = area(2, targetX[direction]);
		b.extent.northWest.y = targetY[direction];
		b.extent.southEast.y = targetY[direction] + 100;
		a.targets[direction] = {2};
		const auto corridor = path({a, b}, 2);
		const auto& portal = corridor->transitions().front();
		assert(portal.sourceLow.x == portal.sourceHigh.x && portal.sourceLow.y == portal.sourceHigh.y);
		local::PathFollower follower(corridor, goals[direction]);
		const auto result = follower.update(starts[direction], {1}, true, 50);
		assert(result.status == local::FollowStatus::Moving && result.target && result.targetProjection > 0);
		assert(follower.step() == 0); // Guidance alone never establishes crossing.
		assert(follower.update(goals[direction], {2}, true, 50).arrived);
	}
}
void specialBackDriftRequiresForwardPortalCrossing()
{
	auto a = area(1, 0), b = area(2, 100), c = area(3, 100);
	c.extent.northWest.y = 100;
	c.extent.southEast.y = 200;
	c.attributes = 2;
	a.targets[1] = {2};
	b.targets[2] = {3};
	local::PathFollower follower(path({a, b, c}, 3), {150, 180, 0});
	assert(follower.update({150, 50, 0}, {2}, true, 80).specialTransition);
	const auto blocked = follower.update({99, 90, 0}, {1}, true, 80);
	assert(blocked.status == local::FollowStatus::OutsideCorridor && !blocked.target);
	assert(blocked.step == 1 && !blocked.changed && !blocked.specialTransition);
	const auto reachable = follower.update({99, 50, 0}, {1}, true, 80);
	assert(reachable.target && reachable.target->x > 99 && reachable.target->y >= 50);
	assert(reachable.targetProjection > 0 && !reachable.specialTransition && reachable.step == 1);
}
void nearbyUnrelatedAreaCannotSupplyRecoveryTarget()
{
	auto a = area(1, 0), b = area(2, 100);
	a.targets[1] = {2};
	local::PathFollower follower(path({a, b}, 2), {180, 50, 0});
	const auto wrong = follower.update({140, 50, 0}, {999}, true, 80);
	assert(wrong.status == local::FollowStatus::OutsideCorridor && !wrong.target && wrong.step == 0);
}
void denseRouteSupportsThreeHundredUnitLookAhead()
{
	std::vector<route_test::Area> areas;
	areas.reserve(25);
	for (std::uint32_t id = 1; id <= 25; ++id)
	{
		areas.push_back(area(id, static_cast<float>((id - 1) * 10), 10));
		if (id > 1)
			areas[id - 2].targets[1] = {id};
	}
	local::PathFollower follower(path(std::move(areas), 25), {340, 50, 0});
	const auto result = follower.update({5, 50, 0}, {1}, true, 300);
	assert(result.status == local::FollowStatus::Moving && result.target);
	assert(result.target->x >= 250 && result.target->x < 340);
	assert(result.targetProjection > 0);
}
} // namespace
int main()
{
	specialBackDriftRequiresForwardPortalCrossing();
	nearbyUnrelatedAreaCannotSupplyRecoveryTarget();
	denseRouteSupportsThreeHundredUnitLookAhead();
	pointPortalUsesCardinalOrientation();
	closeBoundariesAndMissingSupport();
	driftDoesNotRewind();
	specialIsBarrier();
	sameAreaAndInvalidPosition();
	bendDoesNotCutCorner();
	supportedBoundaryRecoveryKeepsMeasuredPosition();
	boundedGoalRecoveryNeverArrivesOutside();
	boundaryRecoveryDoesNotSkipSpecial();
	forwardProjectionSurvivesDriftAndLookAheadChanges();
	crossedPortalNeverPullsBackToSource();
}
