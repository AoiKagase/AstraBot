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
	if (!check(query.findContaining(
			{40.0f, 32.0f, 0.0f},
			0.0f,
			&match) == astrabot::nav::NavQueryResult::Found &&
			match.area == 10U,
			"containing area prefers closest floor"))
	{
		return false;
	}

	if (!check(query.findContaining(
			{40.0f, 32.0f, 5.0f},
			10.0f,
			&match) == astrabot::nav::NavQueryResult::Found &&
			match.area == 10U,
			"equal floor distance prefers smallest area ID"))
	{
		return false;
	}

	if (!check(query.findNearest(
			{100.0f, 32.0f, 0.0f},
			8.0f,
			&match) == astrabot::nav::NavQueryResult::Found &&
			match.area == 10U &&
			std::fabs(match.distanceSquared - 16.0f) < 0.001f,
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
	if (!check(query.outgoingLinks(1U, &links) ==
			astrabot::nav::NavQueryResult::Found &&
			links.size() == 2U &&
			links[0].fromArea == 1U &&
			links[0].toArea == 2U &&
			links[0].direction == 0U &&
			links[1].toArea == 4U &&
			links[1].direction == 2U,
			"directed links are deterministic and preserve direction"))
	{
		return false;
	}

	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(1U, 3U, &corridor) ==
			astrabot::nav::NavQueryResult::Found &&
			corridor.navRevision == snapshot.revision() &&
			corridor.mapGeneration == snapshot.mapGeneration() &&
			corridor.areas == std::vector<astrabot::nav::AreaId>({1U, 2U, 3U}),
			"directed corridor follows stored edges"))
	{
		return false;
	}

	if (!check(query.buildCorridor(3U, 1U, &corridor) ==
			astrabot::nav::NavQueryResult::NoRoute &&
			corridor.areas.empty(),
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
	if (!check(query.outgoingLinks(1U, &links) == astrabot::nav::NavQueryResult::Found &&
				  links.size() == 1U && links[0].how == 4U,
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
	return check(query.buildCorridor(1U, 3U, &corridor, &stats) ==
			astrabot::nav::NavQueryResult::ResourceLimit,
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
	return check(query.buildCorridor(1U, 3U, &corridor, &stats) ==
			astrabot::nav::NavQueryResult::ResourceLimit,
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
	if (!check(query.buildCorridor(1U, 4U, &corridor) ==
				astrabot::nav::NavQueryResult::Found,
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
	if (!check(query.buildCorridor(1U, 2U, &corridor) ==
				astrabot::nav::NavQueryResult::Found,
			"portal test builds a corridor"))
	{
		return false;
	}
	astrabot::nav::NavPathFollower follower;
	if (!check(follower.start(corridor) == astrabot::nav::NavFollowerResult::Started,
			"portal test starts the follower"))
	{
		return false;
	}
	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	if (!check(follower.update(
				snapshot, {32.0f, 32.0f, 0.0f}, 1.0f, 1.0f,
				&target, &targetArea) == astrabot::nav::NavFollowerResult::Advanced &&
				targetArea == 2U && target.x > 64.0f && target.x < 128.0f,
			"follower aims just inside the next area portal"))
	{
		return false;
	}
	return check(follower.update(
				snapshot, {80.0f, 32.0f, 0.0f}, 1.0f, 1.0f,
				&target, &targetArea) == astrabot::nav::NavFollowerResult::Reached,
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
	if (!check(query.buildCorridor(1U, 2U, &corridor) ==
			astrabot::nav::NavQueryResult::Found && corridor.links.size() == 1U &&
			corridor.links[0].direction == 1U,
			"corridor retains the directed first link"))
	{
		return false;
	}
	astrabot::nav::NavPathFollower follower;
	follower.start(corridor);
	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	return check(follower.update(
			snapshot, {32.0f, 32.0f, 0.0f}, 1.0f, 1.0f,
			&target, &targetArea) == astrabot::nav::NavFollowerResult::Advanced &&
			targetArea == 2U && target.x > 64.0f,
			"directed portal target steps into the next area");
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
	if (!check(follower.start(corridor) ==
			astrabot::nav::NavFollowerResult::Started,
			"path follower accepts a stamped corridor"))
	{
		return false;
	}

	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	if (!check(follower.update(
			snapshot,
			{32.0f, 32.0f, 0.0f},
			1.0f,
			1.0f,
			&target,
			&targetArea) == astrabot::nav::NavFollowerResult::Advanced &&
			targetArea == 2U &&
			target.x == 80.0f,
			"path follower advances only after position feedback"))
	{
		return false;
	}

	if (!check(follower.update(
			snapshot,
			{96.0f, 32.0f, 0.0f},
			1.0f,
			1.0f,
			&target,
			&targetArea) == astrabot::nav::NavFollowerResult::Advanced &&
			targetArea == 3U,
			"path follower selects the next target"))
	{
		return false;
	}

	if (!check(follower.update(
			snapshot,
			{160.0f, 32.0f, 0.0f},
			1.0f,
			1.0f,
			&target,
			&targetArea) == astrabot::nav::NavFollowerResult::Reached,
			"path follower reports reached only with final position feedback"))
	{
		return false;
	}

	astrabot::nav::NavDocument replacement;
	replacement.setSourceIdentity({5U, 100U, 201U});
	replacement.addArea(area(1U, 0.0f, 64.0f, 0.0f));
	const astrabot::nav::NavSnapshot changedSnapshot =
		snapshotFor(&replacement, 2U);
	astrabot::nav::NavPathFollower staleFollower;
	if (!check(staleFollower.start(corridor) ==
			astrabot::nav::NavFollowerResult::Started,
			"stale route test starts an active follower"))
	{
		return false;
	}
	return check(staleFollower.update(
			changedSnapshot,
			{160.0f, 32.0f, 0.0f},
			1.0f,
			1.0f,
			&target,
			&targetArea) == astrabot::nav::NavFollowerResult::StaleSnapshot,
		"stale snapshot invalidates the follower");
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
	if (!check(query.findContaining(
			{notANumber, 0.0f, 0.0f},
			0.0f,
			&match) == astrabot::nav::NavQueryResult::InvalidPosition,
			"non-finite containment position is rejected"))
	{
		return false;
	}
	if (!check(query.findNearest(
			{0.0f, 0.0f, 0.0f},
			-1.0f,
			&match) == astrabot::nav::NavQueryResult::InvalidTolerance,
			"negative nearest distance is rejected"))
	{
		return false;
	}
	std::vector<astrabot::nav::NavDirectedLink> links;
	if (!check(query.outgoingLinks(99U, &links) ==
			astrabot::nav::NavQueryResult::AreaNotFound,
			"unknown link source is rejected"))
	{
		return false;
	}
	astrabot::nav::NavCorridor corridor = {};
	if (!check(query.buildCorridor(99U, 1U, &corridor) ==
			astrabot::nav::NavQueryResult::AreaNotFound,
			"unknown corridor endpoint is rejected"))
	{
		return false;
	}
	const astrabot::nav::NavQueryLimits invalidLimits = {
		astrabot::nav::NavLimits::kMaximumAreas + 1U,
		1U
	};
	const astrabot::nav::NavQuery boundedQuery(snapshot, invalidLimits);
	if (!check(boundedQuery.buildCorridor(1U, 1U, &corridor) ==
			astrabot::nav::NavQueryResult::ResourceLimit,
			"limits above the model bound are rejected"))
	{
		return false;
	}

	astrabot::nav::NavPathFollower follower;
	astrabot::nav::NavVector target = {};
	astrabot::nav::AreaId targetArea = 0U;
	if (!check(follower.update(
			snapshot,
			{0.0f, 0.0f, 0.0f},
			1.0f,
			1.0f,
			&target,
			&targetArea) == astrabot::nav::NavFollowerResult::Inactive &&
			!follower.isActive(),
			"inactive follower rejects progress"))
	{
		return false;
	}
	if (!check(follower.start(corridor) ==
			astrabot::nav::NavFollowerResult::InvalidCorridor,
			"empty corridor is rejected"))
	{
		return false;
	}
	return check(query.findContaining(
			{0.0f, 0.0f, 0.0f},
			0.0f,
			nullptr) == astrabot::nav::NavQueryResult::InvalidArgument,
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
	if (!check(query.buildCorridor(1U, 4U, &corridor) ==
				astrabot::nav::NavQueryResult::Found,
			"equal-cost reference fixture builds"))
	{
		return false;
	}
	return check(corridor.areas ==
				std::vector<astrabot::nav::AreaId>({1U, 20U, 4U}),
			"equal-cost route keeps CSBot discovery order instead of area ID order");
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
	if (!check(query.buildCorridor(
				1U, 4U, astrabot::nav::NavRouteType::Fastest, &corridor) ==
				astrabot::nav::NavQueryResult::Found,
			"crouch-cost fixture builds"))
	{
		return false;
	}
	return check(corridor.areas ==
				std::vector<astrabot::nav::AreaId>({1U, 3U, 4U}),
			"fastest route applies the CSBot crouch traversal penalty");
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
	if (!check(query.buildCorridor(1U, 3U, &corridor, &stats) ==
			astrabot::nav::NavQueryResult::Found,
		"A* stats fixture finds the route"))
	{
		return false;
	}
	return check(stats.expandedUniqueAreas >= 2U && stats.enqueueCount >= 3U &&
		stats.reopenCount == 0U && stats.staleQueueEntries == 0U &&
		stats.equalCostReplacements == 0U,
		"A* stats expose unique expansion and queue behavior");
}

int main()
{
	if (!testSpatialTieBreaking() ||
			 !testDirectedLinksAndCorridor() ||
		!testApproachTraversalMetadata() ||
		!testPathFollowerUsesAreaPortalInsteadOfCenter() ||
		!testPathFollowerUsesDirectedPortal() ||
		!testAStarPrefersLowerCostPath() ||
		!testBoundedSearch() ||
		!testCorridorCapacityDoesNotBoundSearchRecords() ||
			!testPathFollowerProgressAndStaleRoute() ||
			!testInvalidInputs() ||
			!testAStarSearchStats() ||
			!testReferenceStableEqualCostTieBreak() ||
			!testReferenceCrouchCost())
	{
		return 1;
	}

	return 0;
}
