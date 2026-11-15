#include "astrabot/nav/nav_query.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

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

astrabot::nav::NavArea area(astrabot::nav::AreaId id, float lowX, float highX, float floorZ)
{
	astrabot::nav::NavArea result = {};
	result.id = id;
	result.extent.lo = {lowX, 0.0f, floorZ};
	result.extent.hi = {highX, 64.0f, floorZ};
	result.northEastZ = floorZ;
	result.southWestZ = floorZ;
	return result;
}

astrabot::nav::NavSnapshot snapshotFor(astrabot::nav::NavDocument *document, std::uint32_t mapGeneration)
{
	astrabot::nav::NavSnapshotPublisher publisher;
	publisher.publish(document, mapGeneration);
	return publisher.snapshot();
}

bool testReferenceExtentCenters()
{
	using namespace astrabot::nav;
	NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto first = area(1, 0, 64, 0);
	auto saddle = area(2, 64, 128, 0);
	saddle.northEastZ = saddle.southWestZ = 200;
	auto detour = area(3, 64, 128, 0);
	detour.extent.lo.y = 64;
	detour.extent.hi.y = 128;
	auto goal = area(4, 128, 192, 0);
	first.connections[1] = {2, 3};
	saddle.connections[3] = {1};
	saddle.connections[1] = {4};
	detour.connections[0] = {1};
	detour.connections[1] = {4};
	goal.connections[3] = {2, 3};
	for (const auto &item : {first, saddle, detour, goal})
		document.addArea(item);
	const auto snapshot = snapshotFor(&document, 1);
	NavQuery query(snapshot);
	NavCorridor route{};
	NavPathCostContext cost{};
	query.setPathCostContext(cost);
	bool ok = check(query.buildCorridor(1, 4, NavRouteType::Safest, &route) == NavQueryResult::Found &&
			route.areas == std::vector<AreaId>{1, 3, 4},
		"default route retains surface-center geometry on a saddle");
	cost.useExtentCenters = true;
	query.setPathCostContext(cost);
	ok = check(query.buildCorridor(1, 4, NavRouteType::Safest, &route) == NavQueryResult::Found &&
				 route.areas == std::vector<AreaId>{1, 2, 4} && route.cost == 128,
			 "native route uses extent midpoint rather than saddle floor height") &&
		ok;
	cost.areas = {{2, .1f, 0}};
	query.setPathCostContext(cost);
	ok = check(query.buildCorridor(1, 4, NavRouteType::Safest, &route) == NavQueryResult::Found &&
				 route.areas == std::vector<AreaId>{1, 3, 4},
			 "native center geometry retains Safest danger weighting") &&
		ok;

	NavDocument overlap;
	overlap.setSourceIdentity({5U, 100U, 200U});
	auto flat = area(10, 0, 64, 0), raisedFloor = area(11, 0, 64, 0);
	raisedFloor.northEastZ = raisedFloor.southWestZ = 200;
	flat.connections[1] = {11};
	raisedFloor.connections[3] = {10};
	overlap.addArea(flat);
	overlap.addArea(raisedFloor);
	NavQuery overlapQuery(snapshotFor(&overlap, 1));
	cost.areas.clear();
	overlapQuery.setPathCostContext(cost);
	ok = check(overlapQuery.buildCorridor(10, 11, NavRouteType::Fastest, &route) == NavQueryResult::Found &&
				 route.cost == 0 && route.isValid(),
			 "distinct native areas at the same center retain a valid zero-cost edge") &&
		ok;

	NavDocument drop;
	drop.setSourceIdentity({5U, 100U, 200U});
	auto highSurface = area(20, 0, 64, 0), lowSurface = area(21, 64, 128, 0);
	highSurface.northEastZ = highSurface.southWestZ = 1000;
	highSurface.connections[1] = {21};
	drop.addArea(highSurface);
	drop.addArea(lowSurface);
	NavQuery dropQuery(snapshotFor(&drop, 1));
	cost.health = 5;
	dropQuery.setPathCostContext(cost);
	ok = check(dropQuery.buildCorridor(20, 21, NavRouteType::Safest, &route) == NavQueryResult::NoRoute,
			 "native traversal centers do not erase the lethal drop between interpolated floors") &&
		ok;
	return ok;
}

bool testSpatialTieBreaking()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea highArea = area(20U, 0.0f, 64.0f, 10.0f);
	astrabot::nav::NavArea lowArea = area(10U, 32.0f, 96.0f, 0.0f);
	document.addArea(highArea);
	document.addArea(lowArea);

	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavAreaMatch match = {};
	if (!check(query.findContaining({40.0f, 32.0f, 0.0f}, 0.0f, &match) == astrabot::nav::NavQueryResult::Found &&
				match.area == 10U,
			"containing area prefers closest floor"))
	{
		return false;
	}

	if (!check(query.findContaining({40.0f, 32.0f, 5.0f}, 10.0f, &match) == astrabot::nav::NavQueryResult::Found &&
				match.area == 10U,
			"equal floor distance prefers smallest area ID"))
	{
		return false;
	}

	if (!check(query.findNearest({100.0f, 32.0f, 0.0f}, 8.0f, &match) == astrabot::nav::NavQueryResult::Found &&
				match.area == 10U && std::fabs(match.distanceSquared - 16.0f) < 0.001f,
			"nearest area reports rectangle distance"))
	{
		return false;
	}

	return true;
}

bool testDirectedLinksAndCorridor()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f, 0.0f);
	astrabot::nav::NavArea third = area(3U, 128.0f, 192.0f, 0.0f);
	astrabot::nav::NavArea fourth = area(4U, 192.0f, 256.0f, 0.0f);
	first.connections[2U].push_back(4U);
	first.connections[0U].push_back(2U);
	second.connections[1U].push_back(3U);
	document.addArea(first);
	document.addArea(second);
	document.addArea(third);
	document.addArea(fourth);

	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	std::vector<astrabot::nav::NavDirectedLink> links;
	if (!check(query.outgoingLinks(1U, &links) == astrabot::nav::NavQueryResult::Found && links.size() == 2U &&
				links[0].fromArea == 1U && links[0].toArea == 2U && links[0].direction == 0U && links[1].toArea == 4U &&
				links[1].direction == 2U,
			"directed links are deterministic and preserve direction"))
	{
		return false;
	}

	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 3U, &corridor) == astrabot::nav::NavQueryResult::Found &&
				corridor.navRevision == snapshot.revision() && corridor.mapGeneration == snapshot.mapGeneration() &&
				corridor.areas == std::vector<astrabot::nav::AreaId>({1U, 2U, 3U}),
			"directed corridor follows stored edges"))
	{
		return false;
	}

	if (!check(
			query.buildCorridor(3U, 1U, &corridor) == astrabot::nav::NavQueryResult::NoRoute && corridor.areas.empty(),
			"missing reverse edge does not get inferred"))
	{
		return false;
	}

	return true;
}

bool testApproachTraversalMetadata()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f, 0.0f);
	first.connections[0U].push_back(2U);
	first.approaches.push_back({1U, 0U, 2U, 0U, 4U});
	document.addArea(first);
	document.addArea(second);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	std::vector<astrabot::nav::NavDirectedLink> links;
	if (!check(query.outgoingLinks(1U, &links) == astrabot::nav::NavQueryResult::Found && links.size() == 1U &&
				links[0].how == 4U,
			"directed link preserves approach traversal metadata"))
	{
		return false;
	}
	return true;
}

bool testBoundedSearch()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f, 0.0f);
	astrabot::nav::NavArea third = area(3U, 128.0f, 192.0f, 0.0f);
	first.connections[0U].push_back(2U);
	second.connections[0U].push_back(3U);
	document.addArea(first);
	document.addArea(second);
	document.addArea(third);

	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQueryLimits limits = {3U, 2U};
	const astrabot::nav::NavQuery query(snapshot, limits);
	astrabot::nav::NavCorridor corridor = {};
	astrabot::nav::NavSearchStats stats = {};
	return check(query.buildCorridor(1U, 3U, &corridor, &stats) == astrabot::nav::NavQueryResult::ResourceLimit,
			   "corridor search obeys explicit search queue bounds") &&
		check(stats.enqueueCount == 2U && stats.expandedUniqueAreas == 2U,
			"explicit search queue bound stops before the third record");
}

bool testCorridorCapacityDoesNotBoundSearchRecords()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f, 0.0f);
	astrabot::nav::NavArea third = area(3U, 128.0f, 192.0f, 0.0f);
	first.connections[0U].push_back(2U);
	second.connections[0U].push_back(3U);
	document.addArea(first);
	document.addArea(second);
	document.addArea(third);

	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQueryLimits limits = {2U, 4U};
	const astrabot::nav::NavQuery query(snapshot, limits);
	astrabot::nav::NavCorridor corridor = {};
	astrabot::nav::NavSearchStats stats = {};
	return check(query.buildCorridor(1U, 3U, &corridor, &stats) == astrabot::nav::NavQueryResult::ResourceLimit,
			   "final corridor capacity still rejects an oversized path") &&
		check(stats.enqueueCount == 3U && stats.expandedUniqueAreas == 3U,
			"corridor capacity does not stop search record expansion");
}

bool testAStarPrefersLowerCostPath()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 32.0f, 0.0f);
	astrabot::nav::NavArea longBranch = area(2U, 1000.0f, 1032.0f, 0.0f);
	astrabot::nav::NavArea shortBranch = area(3U, 64.0f, 96.0f, 0.0f);
	astrabot::nav::NavArea goal = area(4U, 128.0f, 160.0f, 0.0f);
	start.connections[0U].push_back(2U);
	start.connections[0U].push_back(3U);
	longBranch.connections[0U].push_back(4U);
	shortBranch.connections[0U].push_back(4U);
	document.addArea(start);
	document.addArea(longBranch);
	document.addArea(shortBranch);
	document.addArea(goal);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 4U, &corridor) == astrabot::nav::NavQueryResult::Found,
			"A* test builds a goal corridor"))
	{
		return false;
	}
	return check(corridor.areas == std::vector<astrabot::nav::AreaId>({1U, 3U, 4U}),
		"A* chooses the lower-cost geometric route over connection order");
}

bool testPathFollowerUsesAreaPortalInsteadOfCenter()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 256.0f, 0.0f);
	first.connections[0U].push_back(2U);
	document.addArea(first);
	document.addArea(second);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 2U, &corridor) == astrabot::nav::NavQueryResult::Found,
			"portal test builds a corridor"))
	{
		return false;
	}
	astrabot::nav::NavPathFollower follower;
	if (!check(
			follower.start(corridor) == astrabot::nav::NavFollowerResult::Started, "portal test starts the follower"))
	{
		return false;
	}
	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	if (!check(follower.update(snapshot, {32.0f, 32.0f, 0.0f}, 1.0f, 1.0f, &target, &targetArea) ==
					astrabot::nav::NavFollowerResult::Advanced &&
				targetArea == 2U && target.x > 64.0f && target.x < 128.0f,
			"follower aims just inside the next area portal"))
	{
		return false;
	}
	return check(follower.update(snapshot, {80.0f, 32.0f, 0.0f}, 1.0f, 1.0f, &target, &targetArea) ==
			astrabot::nav::NavFollowerResult::Reached,
		"follower treats arrival inside the destination area as progress");
}

bool testPathFollowerUsesDirectedPortal()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f, 0.0f);
	first.connections[1U].push_back(2U);
	document.addArea(first);
	document.addArea(second);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 2U, &corridor) == astrabot::nav::NavQueryResult::Found &&
				corridor.links.size() == 1U && corridor.links[0].direction == 1U,
			"corridor retains the directed first link"))
	{
		return false;
	}
	astrabot::nav::NavPathFollower follower;
	follower.start(corridor);
	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	return check(follower.update(snapshot, {32.0f, 32.0f, 0.0f}, 1.0f, 1.0f, &target, &targetArea) ==
				astrabot::nav::NavFollowerResult::Advanced &&
			targetArea == 2U && target.x > 64.0f,
		"directed portal target steps into the next area");
}

bool testDirectedPortalStaysInsideThinDestination()
{
	using namespace astrabot::nav;
	const NavVector destinationLow[] = {
		{0.0f, -8.0f, 0.0f}, {64.0f, 0.0f, 0.0f}, {0.0f, 64.0f, 0.0f}, {-8.0f, 0.0f, 0.0f}};
	const NavVector destinationHigh[] = {
		{64.0f, 0.0f, 0.0f}, {72.0f, 64.0f, 0.0f}, {64.0f, 72.0f, 0.0f}, {0.0f, 64.0f, 0.0f}};
	for (std::uint8_t direction = 0U; direction < 4U; ++direction)
	{
		NavDocument document;
		document.setSourceIdentity({5U, 100U, 200U});
		NavArea source = area(1U, 0.0f, 64.0f, 0.0f);
		NavArea destination = area(2U, 0.0f, 64.0f, 0.0f);
		destination.extent.lo = destinationLow[direction];
		destination.extent.hi = destinationHigh[direction];
		source.connections[direction].push_back(2U);
		document.addArea(source);
		document.addArea(destination);
		const NavSnapshot snapshot = snapshotFor(&document, 1U);
		NavCorridor corridor = {};
		NavQuery(snapshot).buildCorridor(1U, 2U, &corridor);
		NavPathFollower follower;
		follower.start(corridor);
		NavVector target = {};
		AreaId targetArea = 0U;
		const auto result = follower.update(snapshot, {32.0f, 32.0f, 0.0f}, 1.0f, 1.0f, &target, &targetArea);
		if (!check(result == NavFollowerResult::Advanced && targetArea == 2U && target.x > destination.extent.lo.x &&
					target.x < destination.extent.hi.x && target.y > destination.extent.lo.y &&
					target.y < destination.extent.hi.y,
				"directed portal enters an eight-unit wall top without steering past it"))
		{
			return false;
		}
	}
	return true;
}

bool testPathFollowerProgressAndStaleRoute()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea second = area(2U, 64.0f, 128.0f, 0.0f);
	astrabot::nav::NavArea third = area(3U, 128.0f, 192.0f, 0.0f);
	first.connections[0U].push_back(2U);
	second.connections[0U].push_back(3U);
	document.addArea(first);
	document.addArea(second);
	document.addArea(third);

	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	query.buildCorridor(1U, 3U, &corridor);

	astrabot::nav::NavPathFollower follower;
	if (!check(follower.start(corridor) == astrabot::nav::NavFollowerResult::Started,
			"path follower accepts a stamped corridor"))
	{
		return false;
	}

	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	if (!check(follower.update(snapshot, {32.0f, 32.0f, 0.0f}, 1.0f, 1.0f, &target, &targetArea) ==
					astrabot::nav::NavFollowerResult::Advanced &&
				targetArea == 2U && target.x == 80.0f,
			"path follower advances only after position feedback"))
	{
		return false;
	}

	if (!check(follower.update(snapshot, {96.0f, 32.0f, 0.0f}, 1.0f, 1.0f, &target, &targetArea) ==
					astrabot::nav::NavFollowerResult::Advanced &&
				targetArea == 3U,
			"path follower selects the next target"))
	{
		return false;
	}

	if (!check(follower.update(snapshot, {160.0f, 32.0f, 0.0f}, 1.0f, 1.0f, &target, &targetArea) ==
				astrabot::nav::NavFollowerResult::Reached,
			"path follower reports reached only with final position feedback"))
	{
		return false;
	}

	astrabot::nav::NavDocument replacement;
	replacement.setSourceIdentity({5U, 100U, 201U});
	replacement.addArea(area(1U, 0.0f, 64.0f, 0.0f));
	const astrabot::nav::NavSnapshot changedSnapshot = snapshotFor(&replacement, 2U);
	astrabot::nav::NavPathFollower staleFollower;
	if (!check(staleFollower.start(corridor) == astrabot::nav::NavFollowerResult::Started,
			"stale route test starts an active follower"))
	{
		return false;
	}
	return check(staleFollower.update(changedSnapshot, {160.0f, 32.0f, 0.0f}, 1.0f, 1.0f, &target, &targetArea) ==
			astrabot::nav::NavFollowerResult::StaleSnapshot,
		"stale snapshot invalidates the follower");
}

bool testInterpolatedSurfaceHeight()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea sloped = area(7U, 0.0f, 100.0f, 0.0f);
	sloped.extent.hi.y = 100.0f;
	sloped.extent.hi.z = 60.0f;
	sloped.northEastZ = 20.0f;
	sloped.southWestZ = 40.0f;
	document.addArea(sloped);
	const astrabot::nav::NavQuery query(snapshotFor(&document, 1U));
	astrabot::nav::NavAreaMatch match = {};
	if (!check(query.findNearest({25.0f, 25.0f, 100.0f}, 200.0f, &match) == astrabot::nav::NavQueryResult::Found &&
				std::fabs(match.closestPoint.z - 15.0f) < 0.01f,
			"nearest point uses interpolated surface Z"))
	{
		return false;
	}
	return check(query.findNearest({100.0f, 0.0f, 100.0f}, 200.0f, &match) == astrabot::nav::NavQueryResult::Found &&
			std::fabs(match.closestPoint.z - 20.0f) < 0.01f,
		"surface interpolation reaches the NE corner");
}

bool testPortalSteeringUsesInterpolatedSurfaceHeight()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea from = area(1U, 0.0f, 100.0f, 0.0f);
	from.extent.hi.y = 100.0f;
	astrabot::nav::NavArea to = area(2U, 100.0f, 200.0f, 0.0f);
	to.extent.hi.y = 100.0f;
	to.extent.hi.z = 60.0f;
	to.northEastZ = 20.0f;
	to.southWestZ = 0.0f;
	from.connections[0U].push_back(2U);
	document.addArea(from);
	document.addArea(to);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 2U, &corridor) == astrabot::nav::NavQueryResult::Found,
			"portal height fixture builds a corridor"))
	{
		return false;
	}
	astrabot::nav::NavPathFollower follower;
	follower.start(corridor);
	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	if (!check(follower.update(snapshot, {50.0f, 50.0f, 0.0f}, 1.0f, 8.0f, &target, &targetArea) ==
					astrabot::nav::NavFollowerResult::Advanced &&
				targetArea == 2U,
			"portal height fixture advances to destination"))
	{
		return false;
	}
	return check(std::fabs(target.z - astrabot::nav::surfaceZAt(*document.findArea(2U), target.x, target.y)) < 0.01f,
		"portal target Z follows the destination surface");
}

bool testStationaryStairFollowerDoesNotSkipSegment()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 100.0f, 0.0f);
	astrabot::nav::NavArea second = area(2U, 100.0f, 140.0f, 16.0f);
	astrabot::nav::NavArea third = area(3U, 140.0f, 220.0f, 32.0f);
	first.connections[1U].push_back(2U);
	second.connections[1U].push_back(3U);
	document.addArea(first);
	document.addArea(second);
	document.addArea(third);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 3U, &corridor) == astrabot::nav::NavQueryResult::Found,
			"stair fixture builds a corridor"))
	{
		return false;
	}
	astrabot::nav::NavPathFollower follower;
	follower.start(corridor);
	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	if (!check(follower.update(snapshot, {90.0f, 32.0f, 0.0f}, 20.0f, 1.0f, &target, &targetArea) ==
					astrabot::nav::NavFollowerResult::Advanced &&
				targetArea == 2U && follower.currentIndex() == 1U,
			"stair fixture enters the first segment"))
	{
		return false;
	}
	if (!check(follower.update(snapshot, {90.0f, 32.0f, 0.0f}, 20.0f, 1.0f, &target, &targetArea) ==
					astrabot::nav::NavFollowerResult::TargetReady &&
				targetArea == 2U && follower.currentIndex() == 1U,
			"stationary stair actor cannot skip the second segment"))
	{
		return false;
	}
	return check(follower.update(snapshot, {120.0f, 32.0f, 16.0f}, 20.0f, 1.0f, &target, &targetArea) ==
				astrabot::nav::NavFollowerResult::Advanced &&
			targetArea == 3U,
		"entered stair step allows the next segment");
}

bool testNearest3DUsesSurfaceHeight()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea low = area(10U, 0.0f, 64.0f, 0.0f);
	low.extent.hi.y = 64.0f;
	astrabot::nav::NavArea high = area(20U, 0.0f, 64.0f, 100.0f);
	high.extent.hi.y = 64.0f;
	document.addArea(low);
	document.addArea(high);
	const astrabot::nav::NavQuery query(snapshotFor(&document, 1U));
	astrabot::nav::NavAreaMatch match = {};
	return check(query.findNearest3D({32.0f, 32.0f, 100.0f}, 16.0f, &match) == astrabot::nav::NavQueryResult::Found &&
			match.area == 20U,
		"3D nearest selects the standing surface");
}

bool testStationaryFollowerDoesNotSkipPortalSegment()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea first = area(1U, 0.0f, 100.0f, 0.0f);
	first.extent.hi.y = 100.0f;
	astrabot::nav::NavArea second = area(2U, 100.0f, 140.0f, 0.0f);
	second.extent.hi.y = 200.0f;
	astrabot::nav::NavArea third = area(3U, 140.0f, 240.0f, 0.0f);
	third.extent.lo.y = 100.0f;
	third.extent.hi.y = 200.0f;
	first.connections[0U].push_back(2U);
	second.connections[0U].push_back(3U);
	document.addArea(first);
	document.addArea(second);
	document.addArea(third);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 3U, &corridor) == astrabot::nav::NavQueryResult::Found,
			"stationary portal fixture builds corridor"))
	{
		return false;
	}
	astrabot::nav::NavPathFollower follower;
	follower.start(corridor);
	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	if (!check(follower.update(snapshot, {90.0f, 20.0f, 0.0f}, 20.0f, 1.0f, &target, &targetArea) ==
					astrabot::nav::NavFollowerResult::Advanced &&
				targetArea == 2U,
			"stationary portal fixture enters the intermediate segment"))
	{
		return false;
	}
	return check(follower.update(snapshot, {90.0f, 20.0f, 0.0f}, 20.0f, 1.0f, &target, &targetArea) ==
				astrabot::nav::NavFollowerResult::TargetReady &&
			targetArea == 2U && follower.currentIndex() == 1U,
		"stationary actor cannot skip an unentered corridor segment");
}

bool testInvalidInputs()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	document.addArea(area(1U, 0.0f, 64.0f, 0.0f));
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavAreaMatch match = {};
	const float notANumber = std::numeric_limits<float>::quiet_NaN();
	if (!check(query.findContaining({notANumber, 0.0f, 0.0f}, 0.0f, &match) ==
				astrabot::nav::NavQueryResult::InvalidPosition,
			"non-finite containment position is rejected"))
	{
		return false;
	}
	if (!check(query.findNearest({0.0f, 0.0f, 0.0f}, -1.0f, &match) == astrabot::nav::NavQueryResult::InvalidTolerance,
			"negative nearest distance is rejected"))
	{
		return false;
	}
	std::vector<astrabot::nav::NavDirectedLink> links;
	if (!check(query.outgoingLinks(99U, &links) == astrabot::nav::NavQueryResult::AreaNotFound,
			"unknown link source is rejected"))
	{
		return false;
	}
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(99U, 1U, &corridor) == astrabot::nav::NavQueryResult::AreaNotFound,
			"unknown corridor endpoint is rejected"))
	{
		return false;
	}
	const astrabot::nav::NavQueryLimits invalidLimits = {astrabot::nav::NavLimits::kMaximumAreas + 1U, 1U};
	const astrabot::nav::NavQuery boundedQuery(snapshot, invalidLimits);
	if (!check(boundedQuery.buildCorridor(1U, 1U, &corridor) == astrabot::nav::NavQueryResult::ResourceLimit,
			"limits above the model bound are rejected"))
	{
		return false;
	}

	astrabot::nav::NavPathFollower follower;
	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	if (!check(follower.update(snapshot, {0.0f, 0.0f, 0.0f}, 1.0f, 1.0f, &target, &targetArea) ==
					astrabot::nav::NavFollowerResult::Inactive &&
				!follower.isActive(),
			"inactive follower rejects progress"))
	{
		return false;
	}
	if (!check(follower.start(corridor) == astrabot::nav::NavFollowerResult::InvalidCorridor,
			"empty corridor is rejected"))
	{
		return false;
	}
	return check(
		query.findContaining({0.0f, 0.0f, 0.0f}, 0.0f, nullptr) == astrabot::nav::NavQueryResult::InvalidArgument,
		"null query output is rejected");
}

}

bool testReferenceStableEqualCostTieBreak()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea firstDiscovered = area(20U, 64.0f, 128.0f, 0.0f);
	astrabot::nav::NavArea secondDiscovered = area(10U, 64.0f, 128.0f, 0.0f);
	astrabot::nav::NavArea goal = area(4U, 128.0f, 192.0f, 0.0f);
	start.connections[0U].push_back(20U);
	start.connections[0U].push_back(10U);
	firstDiscovered.connections[0U].push_back(4U);
	secondDiscovered.connections[0U].push_back(4U);
	document.addArea(start);
	document.addArea(firstDiscovered);
	document.addArea(secondDiscovered);
	document.addArea(goal);

	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 4U, &corridor) == astrabot::nav::NavQueryResult::Found,
			"equal-cost reference fixture builds"))
	{
		return false;
	}
	return check(corridor.areas == std::vector<astrabot::nav::AreaId>({1U, 20U, 4U}),
		"equal-cost route keeps CSBot discovery order instead of area ID order");
}

bool testConsecutiveJumpAreasAreNotWalkable()
{
	using namespace astrabot::nav;
	NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto first = area(1U, 0.0f, 64.0f, 0.0f);
	auto second = area(2U, 64.0f, 128.0f, 0.0f);
	first.attributes = NavArea::kJump;
	second.attributes = NavArea::kJump;
	first.connections[0U].push_back(2U);
	document.addArea(first);
	document.addArea(second);
	NavQuery query(snapshotFor(&document, 1U));
	NavCorridor corridor = {};
	return check(query.buildCorridor(1U, 2U, &corridor) == NavQueryResult::NoRoute,
		"jump-to-jump areas cannot be treated as a walking edge");
}

bool testReferenceDangerAndFriendDensityCost()
{
	using namespace astrabot::nav;
	NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto first = area(1U, 0.0f, 64.0f, 0.0f);
	auto second = area(2U, 64.0f, 128.0f, 0.0f);
	first.connections[0U].push_back(2U);
	second.connections[2U].push_back(1U);
	document.addArea(first);
	document.addArea(second);
	NavQuery query(snapshotFor(&document, 1U));
	NavPathCostContext context;
	context.aggression = 1.0f;
	context.areas = {{1U, 0.5f, 0U}, {2U, 0.25f, 2U}};
	query.setPathCostContext(context);
	NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 2U, NavRouteType::Safest, &corridor) == NavQueryResult::Found,
			"observed danger corridor builds"))
		return false;
	// Start danger + travel + per-length danger + area crowding.
	const float expected = 2.5f + 64.0f + 80.0f + 100000.0f / 64.0f;
	if (!check(std::fabs(corridor.cost - expected) < 0.01f,
			"safest cost uses aggression, start danger and teammate density"))
		return false;
	context.attacking = true;
	query.setPathCostContext(context);
	return check(query.buildCorridor(1U, 2U, NavRouteType::Fastest, &corridor) == NavQueryResult::Found &&
			std::fabs(corridor.cost - 64.0f) < 0.01f,
		"attacking fastest route omits crowding and danger");
}

bool testReferenceCrouchCost()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea crouch = area(2U, 64.0f, 96.0f, 0.0f);
	astrabot::nav::NavArea normal = area(3U, 64.0f, 160.0f, 0.0f);
	astrabot::nav::NavArea goal = area(4U, 160.0f, 192.0f, 0.0f);
	crouch.attributes = astrabot::nav::NavArea::kCrouch;
	start.connections[0U].push_back(2U);
	start.connections[0U].push_back(3U);
	crouch.connections[0U].push_back(4U);
	normal.connections[0U].push_back(4U);
	document.addArea(start);
	document.addArea(crouch);
	document.addArea(normal);
	document.addArea(goal);

	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 4U, astrabot::nav::NavRouteType::Fastest, &corridor) ==
				astrabot::nav::NavQueryResult::Found,
			"crouch-cost fixture builds"))
	{
		return false;
	}
	return check(corridor.areas == std::vector<astrabot::nav::AreaId>({1U, 3U, 4U}),
		"fastest route applies the CSBot crouch traversal penalty");
}

bool testAlternativeCorridorAvoidsFailedFirstLink(bool danger = false)
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea failedBranch = area(2U, 64.0f, 128.0f, 0.0f);
	astrabot::nav::NavArea alternateBranch = area(3U, 1000.0f, 1064.0f, 0.0f);
	astrabot::nav::NavArea goal = area(4U, 128.0f, 192.0f, 0.0f);
	start.connections[0U].push_back(2U);
	start.connections[0U].push_back(3U);
	failedBranch.connections[0U].push_back(4U);
	alternateBranch.connections[0U].push_back(4U);
	document.addArea(start);
	document.addArea(failedBranch);
	document.addArea(alternateBranch);
	document.addArea(goal);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	astrabot::nav::NavQuery query(snapshot);
	if (danger)
	{
		astrabot::nav::NavPathCostContext context;
		context.aggression = 1.0f;
		context.areas = {{3U, 0.5f, 0U}};
		query.setPathCostContext(context);
	}
	astrabot::nav::NavCorridor primary = {};
	const auto route = danger ? astrabot::nav::NavRouteType::Safest : astrabot::nav::NavRouteType::Fastest;
	if (!check(query.buildCorridor(1U, 4U, route, &primary) == astrabot::nav::NavQueryResult::Found &&
				primary.areas == std::vector<astrabot::nav::AreaId>({1U, 2U, 4U}),
			"primary objective route uses the shortest first link"))
	{
		return false;
	}
	astrabot::nav::NavCorridor alternative = {};
	if (!check(query.buildAlternativeCorridor(primary, primary.links.front(), &alternative) ==
				astrabot::nav::NavQueryResult::Found,
			"objective route can bypass a failed first link"))
	{
		return false;
	}
	return check(alternative.isValid() && alternative.areas == std::vector<astrabot::nav::AreaId>({1U, 3U, 4U}) &&
			alternative.links.size() == 2U && alternative.links.front().fromArea == 1U &&
			alternative.links.front().toArea == 3U && alternative.routeType == primary.routeType &&
			std::fabs(alternative.cost - (danger ? 4372.0f : 1872.0f)) < 0.01f,
		"alternative objective route preserves a valid corridor and route type");
}

bool testAlternativeSearchCanBypassAFormerShortestReturn()
{
	using namespace astrabot::nav;
	NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	auto start = area(1U, 0.0f, 64.0f, 0.0f), failed = area(2U, 64.0f, 128.0f, 0.0f);
	auto alternate = area(3U, 1000.0f, 1064.0f, 0.0f), goal = area(4U, 128.0f, 192.0f, 0.0f);
	auto detour = area(5U, 3000.0f, 3064.0f, 0.0f);
	start.connections[0U] = {2U, 3U};
	failed.connections[0U] = {4U};
	alternate.connections[0U] = {1U, 5U};
	detour.connections[0U] = {4U};
	document.addArea(start);
	document.addArea(failed);
	document.addArea(alternate);
	document.addArea(goal);
	document.addArea(detour);
	const auto snapshot = snapshotFor(&document, 1U);
	NavQuery query(snapshot);
	NavCorridor primary{}, bypass{};
	NavSearchStats stats{};
	if (!check(query.buildCorridor(1U, 4U, &primary) == NavQueryResult::Found,
			"returning-suffix fixture has a primary route"))
		return false;
	if (!check(
			query.buildAlternativeCorridor(primary, primary.links.front(), &bypass, &stats) == NavQueryResult::Found &&
				bypass.areas == std::vector<AreaId>{1U, 3U, 5U, 4U} && std::fabs(bypass.cost - 5872.0f) < 0.01f,
			"alternative A* finds the longer valid detour when the unrestricted suffix returns via the failed edge"))
		return false;
	if (!check(stats.searchCalls == 1U && stats.firstStartArea == 1U && stats.firstGoalArea == 4U,
			"bypass uses a complete start-to-goal search instead of independent truncated searches"))
		return false;
	NavCorridor ordinary{};
	if (!check(query.buildCorridor(1U, 4U, &ordinary) == NavQueryResult::Found && ordinary.areas == primary.areas,
			"temporary edge exclusion does not alter later ordinary searches"))
		return false;
	NavQuery limited(snapshot, {256U, 2U});
	return check(
		limited.buildAlternativeCorridor(primary, primary.links.front(), &bypass) == NavQueryResult::ResourceLimit,
		"a bounded bypass reports queue exhaustion distinctly from a proven missing route");
}

bool testAStarSearchStats()
{
	astrabot::nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	astrabot::nav::NavArea start = area(1U, 0.0f, 64.0f, 0.0f);
	astrabot::nav::NavArea middle = area(2U, 64.0f, 128.0f, 0.0f);
	astrabot::nav::NavArea goal = area(3U, 128.0f, 192.0f, 0.0f);
	start.connections[0U].push_back(2U);
	middle.connections[0U].push_back(3U);
	document.addArea(start);
	document.addArea(middle);
	document.addArea(goal);
	const astrabot::nav::NavSnapshot snapshot = snapshotFor(&document, 1U);
	const astrabot::nav::NavQuery query(snapshot);
	astrabot::nav::NavCorridor corridor = {};
	astrabot::nav::NavSearchStats stats = {};
	if (!check(query.buildCorridor(1U, 3U, &corridor, &stats) == astrabot::nav::NavQueryResult::Found,
			"A* stats fixture finds the route"))
	{
		return false;
	}
	return check(stats.expandedUniqueAreas >= 2U && stats.enqueueCount >= 3U && stats.reopenCount == 0U &&
			stats.staleQueueEntries == 0U && stats.equalCostReplacements == 0U,
		"A* stats expose unique expansion and queue behavior");
}

int main()
{
	{
		using namespace astrabot::nav;
		NavDocument d;
		d.setSourceIdentity({5U, 100U, 200U});
		auto a = area(1U, 0, 64, 0), b = area(2U, 64, 256, 0);
		a.connections[1U] = {2U};
		d.addArea(a);
		d.addArea(b);
		auto snapshot = snapshotFor(&d, 1U);
		NavQuery query(snapshot);
		NavCorridor c{};
		query.buildCorridor(1U, 2U, &c);
		c.hasEndpoint = true;
		c.endpoint = {180, 32, 0};
		NavPathFollower follower;
		follower.start(c);
		NavVector target{};
		AreaId id = 0;
		follower.update(snapshot, {32, 32, 0}, 20, 64, &target, &id);
		if (!check(follower.update(snapshot, {80, 32, 0}, 20, 64, &target, &id) == NavFollowerResult::TargetReady &&
					target.x == 180,
				"explicit endpoint keeps moving inside a large destination area"))
			return 1;
		if (!check(follower.update(snapshot, {180, 32, 25}, 20, 64, &target, &id) == NavFollowerResult::TargetReady,
				"CSBot endpoint distance includes vertical separation despite area tolerance64"))
			return 1;
		if (!check(follower.update(snapshot, {160, 32, 0}, 20, 64, &target, &id) == NavFollowerResult::TargetReady &&
					follower.update(snapshot, {160.1f, 32, 0}, 20, 64, &target, &id) == NavFollowerResult::Reached,
				"CSBot endpoint20 boundary is strict"))
			return 1;
		c.endpoint.x = std::numeric_limits<float>::quiet_NaN();
		if (!check(follower.start(c) == NavFollowerResult::InvalidCorridor,
				"invalid explicit endpoint cannot reuse old state"))
			return 1;
	}
	if (!testSpatialTieBreaking() || !testDirectedLinksAndCorridor() || !testApproachTraversalMetadata() ||
		!testPathFollowerUsesAreaPortalInsteadOfCenter() || !testPathFollowerUsesDirectedPortal() ||
		!testDirectedPortalStaysInsideThinDestination() || !testAStarPrefersLowerCostPath() || !testBoundedSearch() ||
		!testCorridorCapacityDoesNotBoundSearchRecords() || !testPathFollowerProgressAndStaleRoute() ||
		!testStationaryFollowerDoesNotSkipPortalSegment() || !testInterpolatedSurfaceHeight() ||
		!testPortalSteeringUsesInterpolatedSurfaceHeight() || !testStationaryStairFollowerDoesNotSkipSegment() ||
		!testNearest3DUsesSurfaceHeight() || !testInvalidInputs() || !testAlternativeCorridorAvoidsFailedFirstLink() ||
		!testAlternativeCorridorAvoidsFailedFirstLink(true) || !testAlternativeSearchCanBypassAFormerShortestReturn() ||
		!testAStarSearchStats() || !testReferenceStableEqualCostTieBreak() || !testReferenceCrouchCost() ||
		!testConsecutiveJumpAreasAreNotWalkable() || !testReferenceDangerAndFriendDensityCost() ||
		!testReferenceExtentCenters())
	{
		return 1;
	}

	return 0;
}
