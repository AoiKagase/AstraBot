#include "astrabot/nav/nav_query.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <new>

namespace astrabot
{
namespace nav
{
namespace
{
bool isBetterMatch(
	float candidateDistance,
	AreaId candidateArea,
	float bestDistance,
	AreaId bestArea)
{
	return candidateDistance < bestDistance ||
		(candidateDistance == bestDistance && candidateArea < bestArea);
}

bool isValidLimits(const NavQueryLimits &limits)
{
	return limits.maximumCorridorAreas != 0U &&
		limits.maximumSearchQueue != 0U &&
		limits.maximumCorridorAreas <= NavLimits::kMaximumAreas &&
		limits.maximumSearchQueue <= NavLimits::kMaximumAreas;
}

bool isFinitePosition(const NavVector &position)
{
	return std::isfinite(position.x) && std::isfinite(position.y) &&
		std::isfinite(position.z);
}

bool isFiniteTolerance(float tolerance)
{
	return std::isfinite(tolerance) && tolerance >= 0.0f;
}

float floorDistance(const NavArea &area, float z)
{
	const float low = std::min(area.northEastZ, area.southWestZ);
	const float high = std::max(area.northEastZ, area.southWestZ);
	if (z < low)
	{
		return low - z;
	}
	if (z > high)
	{
		return z - high;
	}
	return 0.0f;
}

NavVector centerOf(const NavArea &area)
{
	return {
		area.extent.lo.x * 0.5f + area.extent.hi.x * 0.5f,
		area.extent.lo.y * 0.5f + area.extent.hi.y * 0.5f,
		area.northEastZ * 0.5f + area.southWestZ * 0.5f
	};
}

float rectangleDistanceSquared(
	const NavArea &area,
	const NavVector &position)
{
	float deltaX = 0.0f;
	float deltaY = 0.0f;
	if (position.x < area.extent.lo.x)
	{
		deltaX = area.extent.lo.x - position.x;
	}
	else if (position.x > area.extent.hi.x)
	{
		deltaX = position.x - area.extent.hi.x;
	}
	if (position.y < area.extent.lo.y)
	{
		deltaY = area.extent.lo.y - position.y;
	}
	else if (position.y > area.extent.hi.y)
	{
		deltaY = position.y - area.extent.hi.y;
	}
	return deltaX * deltaX + deltaY * deltaY;
}

NavVector closestPointOnArea(
	const NavArea &area,
	const NavVector &position)
{
	NavVector point = position;
	point.x = (std::max)(area.extent.lo.x, (std::min)(point.x, area.extent.hi.x));
	point.y = (std::max)(area.extent.lo.y, (std::min)(point.y, area.extent.hi.y));
	point.z = area.northEastZ * 0.5f + area.southWestZ * 0.5f;
	return point;
}

bool isWithinTarget(
		const NavArea &area,
		const NavVector &position,
	float horizontalTolerance,
	float verticalTolerance)
{
	return rectangleDistanceSquared(area, position) <=
			horizontalTolerance * horizontalTolerance &&
			floorDistance(area, position.z) <= verticalTolerance;
}

NavVector portalSteeringPoint(
	const NavArea &area,
	const NavVector &position)
{
	constexpr float kPortalInset = 16.0f;
	const NavVector closest = closestPointOnArea(area, position);
	const NavVector center = centerOf(area);
	const float deltaX = center.x - closest.x;
	const float deltaY = center.y - closest.y;
	const float distance = std::hypot(deltaX, deltaY);
	if (!std::isfinite(distance) || distance <= kPortalInset)
	{
		return center;
	}
	return {
		closest.x + deltaX / distance * kPortalInset,
		closest.y + deltaY / distance * kPortalInset,
		closest.z};
}

NavVector portalSteeringPoint(
	const NavArea &from,
	const NavArea &to,
	const NavVector &position)
{
	const float overlapLoX = (std::max)(from.extent.lo.x, to.extent.lo.x);
	const float overlapHiX = (std::min)(from.extent.hi.x, to.extent.hi.x);
	const float overlapLoY = (std::max)(from.extent.lo.y, to.extent.lo.y);
	const float overlapHiY = (std::min)(from.extent.hi.y, to.extent.hi.y);
	if (overlapLoX > overlapHiX || overlapLoY > overlapHiY)
	{
		return portalSteeringPoint(to, position);
	}
	const NavVector portal = {
		(overlapLoX + overlapHiX) * 0.5f,
		(overlapLoY + overlapHiY) * 0.5f,
		to.northEastZ * 0.5f + to.southWestZ * 0.5f};
	const NavVector center = centerOf(to);
	const float deltaX = center.x - portal.x;
	const float deltaY = center.y - portal.y;
	const float distance = std::hypot(deltaX, deltaY);
	if (!std::isfinite(distance) || distance <= 16.0f)
	{
		return center;
	}
	return {
		portal.x + deltaX / distance * 16.0f,
		portal.y + deltaY / distance * 16.0f,
		portal.z};
}

NavVector portalSteeringPoint(
	const NavArea &from,
	const NavArea &to,
	const NavVector &position,
	std::uint8_t direction)
{
	if (direction >= NavArea::kDirectionCount)
	{
		return portalSteeringPoint(to, position);
	}
	const float overlapLoX = (std::max)(from.extent.lo.x, to.extent.lo.x);
	const float overlapHiX = (std::min)(from.extent.hi.x, to.extent.hi.x);
	const float overlapLoY = (std::max)(from.extent.lo.y, to.extent.lo.y);
	const float overlapHiY = (std::min)(from.extent.hi.y, to.extent.hi.y);
	if (overlapLoX > overlapHiX || overlapLoY > overlapHiY)
	{
		return portalSteeringPoint(to, position);
	}
	const bool directionMatchesGeometry =
		(direction == 0U && to.extent.hi.y <= from.extent.lo.y + 1.0f) ||
		(direction == 1U && to.extent.lo.x >= from.extent.hi.x - 1.0f) ||
		(direction == 2U && to.extent.lo.y >= from.extent.hi.y - 1.0f) ||
		(direction == 3U && to.extent.hi.x <= from.extent.lo.x + 1.0f);
	if (!directionMatchesGeometry)
	{
		return portalSteeringPoint(from, to, position);
	}
	constexpr float kPortalMargin = 16.0f;
	const auto clampWithMargin = [kPortalMargin](float value, float lo, float hi) {
		if (hi - lo <= 2.0f * kPortalMargin)
		{
			return (lo + hi) * 0.5f;
		}
		return (std::max)(lo + kPortalMargin,
			(std::min)(hi - kPortalMargin, value));
	};
	NavVector result = {
		clampWithMargin(position.x, overlapLoX, overlapHiX),
		clampWithMargin(position.y, overlapLoY, overlapHiY),
		to.northEastZ * 0.5f + to.southWestZ * 0.5f};
	switch (direction)
	{
	case 0U:
		result.y = from.extent.lo.y - kPortalMargin;
		break;
	case 1U:
		result.x = from.extent.hi.x + kPortalMargin;
		break;
	case 2U:
		result.y = from.extent.hi.y + kPortalMargin;
		break;
	case 3U:
		result.x = from.extent.lo.x - kPortalMargin;
		break;
	default:
		break;
	}
	return result;
}

std::uint8_t howForLink(
	const NavDocument *document,
	AreaId fromArea,
	AreaId toArea)
{
	if (document == nullptr)
	{
		return 0U;
	}
	const NavArea *from = document->findArea(fromArea);
	if (from != nullptr)
	{
		for (const NavApproach &approach : from->approaches)
		{
			if (approach.here == fromArea && approach.next == toArea)
			{
				return approach.hereToNextHow;
			}
		}
	}
	const NavArea *to = document->findArea(toArea);
	if (to != nullptr)
	{
		for (const NavApproach &approach : to->approaches)
		{
			if (approach.here == toArea && approach.previous == fromArea)
			{
				return approach.previousToHereHow;
			}
		}
	}
	return 0U;
}

NavDirectedLink directedLinkFor(
	const NavDocument *document,
	AreaId fromArea,
	AreaId toArea)
{
	if (document == nullptr)
	{
		return {fromArea, toArea, 0U, 0U};
	}
	const NavArea *from = document->findArea(fromArea);
	if (from == nullptr)
	{
		return {fromArea, toArea, 0U, 0U};
	}
	for (std::size_t direction = 0U;
			direction < NavArea::kDirectionCount; ++direction)
	{
		for (const AreaId target : from->connections[direction])
		{
			if (target == toArea)
			{
				return {
					fromArea,
					toArea,
					static_cast<std::uint8_t>(direction),
					howForLink(document, fromArea, toArea)};
			}
		}
	}
	return {fromArea, toArea, 0U, howForLink(document, fromArea, toArea)};
}
}

bool NavCorridor::isValid() const
{
	if (navRevision == 0U || mapGeneration == 0U || !std::isfinite(cost) ||
			areas.empty())
	{
		return false;
	}
	if (!links.empty() && links.size() + 1U != areas.size())
	{
		return false;
	}

	for (std::size_t index = 0U; index < areas.size(); ++index)
	{
		if (areas[index] == 0U)
		{
			return false;
		}
		for (std::size_t previous = 0U; previous < index; ++previous)
		{
			if (areas[previous] == areas[index])
			{
				return false;
			}
		}
	}
	return true;
}

NavQuery::NavQuery(const NavSnapshot &snapshot) :
	snapshot_(snapshot),
	limits_{
		NavQueryLimits::kDefaultMaximumCorridorAreas,
		NavQueryLimits::kDefaultMaximumSearchQueue
	}
{
}

NavQuery::NavQuery(
	const NavSnapshot &snapshot,
	const NavQueryLimits &limits) :
	snapshot_(snapshot),
	limits_(limits)
{
}

NavQueryResult NavQuery::findContaining(
	const NavVector &position,
	float floorTolerance,
	NavAreaMatch *match) const
{
	if (match == nullptr)
	{
		return NavQueryResult::InvalidArgument;
	}
	if (document() == nullptr)
	{
		return NavQueryResult::EmptySnapshot;
	}
	if (!isFinitePosition(position))
	{
		return NavQueryResult::InvalidPosition;
	}
	if (!isFiniteTolerance(floorTolerance))
	{
		return NavQueryResult::InvalidTolerance;
	}

	bool found = false;
	AreaId bestArea = 0U;
	float bestVerticalDistance = std::numeric_limits<float>::max();
	for (const NavArea &area : document()->areas())
	{
		if (position.x < area.extent.lo.x ||
				position.x > area.extent.hi.x ||
				position.y < area.extent.lo.y ||
				position.y > area.extent.hi.y)
		{
			continue;
		}

		const float candidateDistance = floorDistance(area, position.z);
		if (candidateDistance > floorTolerance ||
				(found && !isBetterMatch(
					candidateDistance,
					area.id,
					bestVerticalDistance,
					bestArea)))
		{
			continue;
		}
		found = true;
		bestArea = area.id;
		bestVerticalDistance = candidateDistance;
	}

	if (!found)
	{
		return NavQueryResult::NoAreaContaining;
	}

	const NavArea *area = document()->findArea(bestArea);
	match->area = bestArea;
	match->distanceSquared = rectangleDistanceSquared(*area, position);
	match->closestPoint = closestPointOnArea(*area, position);
	return NavQueryResult::Found;
}

NavQueryResult NavQuery::findNearest(
	const NavVector &position,
	float maximumDistance,
	NavAreaMatch *match) const
{
	if (match == nullptr)
	{
		return NavQueryResult::InvalidArgument;
	}
	if (document() == nullptr)
	{
		return NavQueryResult::EmptySnapshot;
	}
	if (!isFinitePosition(position))
	{
		return NavQueryResult::InvalidPosition;
	}
	if (!isFiniteTolerance(maximumDistance) || maximumDistance < 0.0f)
	{
		return NavQueryResult::InvalidTolerance;
	}

	const float maximumDistanceSquared = maximumDistance * maximumDistance;
	bool found = false;
	AreaId bestArea = 0U;
	float bestDistance = std::numeric_limits<float>::max();
	for (const NavArea &area : document()->areas())
	{
		const float candidateDistance =
			rectangleDistanceSquared(area, position);
		if (candidateDistance > maximumDistanceSquared ||
				(found && !isBetterMatch(
					candidateDistance,
					area.id,
					bestDistance,
					bestArea)))
		{
			continue;
		}
		found = true;
		bestArea = area.id;
		bestDistance = candidateDistance;
	}

	if (!found)
	{
		return NavQueryResult::NoAreaContaining;
	}

	match->area = bestArea;
	match->distanceSquared = bestDistance;
	match->closestPoint = closestPointOnArea(*document()->findArea(bestArea), position);
	return NavQueryResult::Found;
}

NavQueryResult NavQuery::outgoingLinks(
	AreaId fromArea,
	std::vector<NavDirectedLink> *links) const
{
	if (links == nullptr)
	{
		return NavQueryResult::InvalidArgument;
	}
	links->clear();
	const NavDocument *currentDocument = document();
	if (currentDocument == nullptr)
	{
		return NavQueryResult::EmptySnapshot;
	}

	const NavArea *area = currentDocument->findArea(fromArea);
	if (area == nullptr)
	{
		return NavQueryResult::AreaNotFound;
	}

	std::vector<NavDirectedLink> candidate;
	try
	{
		candidate.reserve(
			NavArea::kDirectionCount *
			NavLimits::kMaximumConnectionsPerDirection);
		for (std::size_t direction = 0U;
				direction < NavArea::kDirectionCount;
				++direction)
		{
			for (const AreaId target : area->connections[direction])
			{
			candidate.push_back({
					fromArea,
					target,
					static_cast<std::uint8_t>(direction),
					howForLink(currentDocument, fromArea, target)
				});
			}
		}
	}
	catch (const std::bad_alloc &)
	{
		return NavQueryResult::ResourceLimit;
	}

	*links = candidate;
	return NavQueryResult::Found;
}

namespace
{
struct AStarRecord
{
	AreaId area;
	AreaId parent;
	float costSoFar;
	float totalCost;
	bool closed;
	bool expandedEver;
};

struct AStarStatsTimer
{
	NavSearchStats *stats;
	std::chrono::steady_clock::time_point start;

	explicit AStarStatsTimer(NavSearchStats *value)
		: stats(value), start(value != nullptr ? std::chrono::steady_clock::now() :
			std::chrono::steady_clock::time_point())
	{
		if (stats != nullptr)
		{
			static std::uint64_t nextSearchId = 0U;
			stats->firstSearchId = ++nextSearchId;
			stats->lastSearchId = stats->firstSearchId;
		}
	}

	~AStarStatsTimer()
	{
		if (stats == nullptr)
		{
			return;
		}
		++stats->searchCalls;
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - start).count();
		const std::uint64_t usec = elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0U;
		stats->totalUsec += usec;
		if (usec > stats->maxUsec)
		{
			stats->maxUsec = usec;
		}
	}
};

NavQueryResult buildAStarCorridor(
	const NavSnapshot &snapshot,
	const NavQueryLimits &limits,
	AreaId start,
	AreaId goal,
	NavRouteType routeType,
	NavCorridor *corridor,
	NavSearchStats *stats)
{
	if (stats != nullptr)
	{
		*stats = {};
	}
	AStarStatsTimer statsTimer(stats);
	if (corridor == nullptr)
	{
		return NavQueryResult::InvalidArgument;
	}
	*corridor = {};
	const NavDocument *currentDocument = snapshot.document();
	if (currentDocument == nullptr)
	{
		return NavQueryResult::EmptySnapshot;
	}
	if (start == 0U || goal == 0U ||
			currentDocument->findArea(start) == nullptr ||
			currentDocument->findArea(goal) == nullptr)
	{
		return NavQueryResult::AreaNotFound;
	}
	if (!isValidLimits(limits))
	{
		return NavQueryResult::ResourceLimit;
	}

	const auto findRecord = [](const std::vector<AStarRecord> &records, AreaId area) {
		for (std::size_t index = 0U; index < records.size(); ++index)
		{
			if (records[index].area == area)
			{
				return index;
			}
		}
		return records.size();
	};
	const auto contains = [](const std::vector<AreaId> &areas, AreaId area) {
		return std::find(areas.begin(), areas.end(), area) != areas.end();
	};
	const auto distanceBetween = [](const NavArea &left, const NavArea &right) {
		const NavVector leftCenter = centerOf(left);
		const NavVector rightCenter = centerOf(right);
		const float dx = rightCenter.x - leftCenter.x;
		const float dy = rightCenter.y - leftCenter.y;
		const float dz = rightCenter.z - leftCenter.z;
		const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		return std::isfinite(distance) && distance > 1.0f ? distance : 1.0f;
	};
	const auto traversalCost = [routeType, &distanceBetween](
			const NavArea &from, const NavArea &to)
	{
		const float distance = distanceBetween(from, to);
		const float crouchPenalty =
				routeType == NavRouteType::Fastest ? 20.0f : 5.0f;
		const float jumpPenalty = 1.0f;
		float cost = distance;
		if ((to.attributes & NavArea::kCrouch) != 0U)
		{
			cost += crouchPenalty * distance;
		}
		if ((to.attributes & NavArea::kJump) != 0U)
		{
			cost += jumpPenalty * distance;
		}
		return cost;
	};

	try
	{
		std::vector<AStarRecord> records;
		std::vector<AreaId> open;
		records.reserve(limits.maximumSearchQueue);
		open.reserve(limits.maximumSearchQueue);
		const NavArea *startArea = currentDocument->findArea(start);
		const NavArea *goalArea = currentDocument->findArea(goal);
		const float initialHeuristic = distanceBetween(*startArea, *goalArea);
		records.push_back({start, 0U, 0.0f, initialHeuristic, false, false});
		open.push_back(start);
		if (stats != nullptr)
		{
			++stats->enqueueCount;
		}

		while (!open.empty())
		{
			std::size_t bestOpenIndex = 0U;
			std::size_t bestRecordIndex = findRecord(records, open[0]);
			for (std::size_t index = 1U; index < open.size(); ++index)
			{
				const std::size_t candidateRecordIndex = findRecord(records, open[index]);
		if (candidateRecordIndex == records.size())
		{
			if (stats != nullptr)
			{
				++stats->staleQueueEntries;
			}
			continue;
				}
				const AStarRecord &candidate = records[candidateRecordIndex];
				const AStarRecord &best = records[bestRecordIndex];
			if (candidate.totalCost < best.totalCost)
				{
					bestOpenIndex = index;
					bestRecordIndex = candidateRecordIndex;
				}
			}

			const AreaId current = open[bestOpenIndex];
			open.erase(open.begin() + static_cast<std::ptrdiff_t>(bestOpenIndex));
			const std::size_t currentRecordIndex = findRecord(records, current);
		if (currentRecordIndex == records.size())
		{
			if (stats != nullptr)
			{
				++stats->staleQueueEntries;
			}
			return NavQueryResult::NoRoute;
		}
		if (stats != nullptr && !records[currentRecordIndex].expandedEver)
		{
			++stats->expandedUniqueAreas;
			records[currentRecordIndex].expandedEver = true;
		}
		records[currentRecordIndex].closed = true;
			if (current == goal)
			{
				break;
			}

			const NavArea *currentArea = currentDocument->findArea(current);
			if (currentArea == nullptr)
			{
				return NavQueryResult::AreaNotFound;
			}
			for (std::size_t direction = 0U;
					direction < NavArea::kDirectionCount; ++direction)
			{
				for (const AreaId target : currentArea->connections[direction])
				{
					const NavArea *targetArea = currentDocument->findArea(target);
					if (targetArea == nullptr)
					{
						return NavQueryResult::AreaNotFound;
					}
			const float tentativeCost = records[currentRecordIndex].costSoFar +
					traversalCost(*currentArea, *targetArea);
					std::size_t targetRecordIndex = findRecord(records, target);
					if (targetRecordIndex == records.size())
					{
						if (records.size() >= limits.maximumSearchQueue)
						{
							return NavQueryResult::ResourceLimit;
						}
						const float heuristic = distanceBetween(*targetArea, *goalArea);
			records.push_back({
				target, current, tentativeCost,
				tentativeCost + heuristic, false, false});
			open.push_back(target);
			if (stats != nullptr)
			{
				++stats->enqueueCount;
			}
			continue;
					}

					AStarRecord &targetRecord = records[targetRecordIndex];
					if (tentativeCost >= targetRecord.costSoFar)
					{
						continue;
					}
					targetRecord.parent = current;
					targetRecord.costSoFar = tentativeCost;
					targetRecord.totalCost = tentativeCost +
						distanceBetween(*targetArea, *goalArea);
		if (targetRecord.closed)
		{
			targetRecord.closed = false;
			if (stats != nullptr)
			{
				++stats->reopenCount;
			}
		}
		if (!contains(open, target))
		{
			open.push_back(target);
			if (stats != nullptr)
			{
				++stats->enqueueCount;
			}
		}
				}
			}
		}

		const std::size_t goalRecordIndex = findRecord(records, goal);
		if (goalRecordIndex == records.size())
		{
			return NavQueryResult::NoRoute;
		}
		std::vector<AreaId> reversed;
		reversed.reserve(records.size());
		AreaId current = goal;
		while (current != 0U)
		{
			reversed.push_back(current);
			const std::size_t recordIndex = findRecord(records, current);
			if (recordIndex == records.size())
			{
				return NavQueryResult::NoRoute;
			}
			current = records[recordIndex].parent;
		}
		if (reversed.size() > limits.maximumCorridorAreas)
		{
			return NavQueryResult::ResourceLimit;
		}
		std::reverse(reversed.begin(), reversed.end());
	corridor->navRevision = snapshot.revision();
	corridor->mapGeneration = snapshot.mapGeneration();
	corridor->routeType = routeType;
	corridor->cost = records[goalRecordIndex].costSoFar;
	corridor->areas = reversed;
		corridor->links.clear();
		corridor->links.reserve(reversed.size() > 0U ? reversed.size() - 1U : 0U);
		for (std::size_t index = 1U; index < reversed.size(); ++index)
		{
			corridor->links.push_back(directedLinkFor(
				currentDocument, reversed[index - 1U], reversed[index]));
		}
	}
	catch (const std::bad_alloc &)
	{
		*corridor = {};
		return NavQueryResult::ResourceLimit;
	}
	return NavQueryResult::Found;
}
}

NavQueryResult NavQuery::buildCorridor(
	AreaId start,
	AreaId goal,
	NavCorridor *corridor) const
{
	return buildCorridor(start, goal, NavRouteType::Fastest, corridor);
}

NavQueryResult NavQuery::buildCorridor(
	AreaId start, AreaId goal, NavCorridor *corridor, NavSearchStats *stats) const
{
	return buildCorridor(start, goal, NavRouteType::Fastest, corridor, stats);
}

NavQueryResult NavQuery::buildCorridor(
	AreaId start,
	AreaId goal,
	NavRouteType routeType,
	NavCorridor *corridor) const
{
	return buildCorridor(start, goal, routeType, corridor, nullptr);
}

NavQueryResult NavQuery::buildCorridor(
	AreaId start,
	AreaId goal,
	NavRouteType routeType,
	NavCorridor *corridor,
	NavSearchStats *stats) const
{
	return buildAStarCorridor(
		snapshot_, limits_, start, goal, routeType, corridor, stats);
}

const NavDocument *NavQuery::document() const
{
	return snapshot_.document();
}

std::size_t NavQuery::findDiscoveredArea(
	const std::vector<AreaId> &areas,
	AreaId id)
{
	for (std::size_t index = 0U; index < areas.size(); ++index)
	{
		if (areas[index] == id)
		{
			return index;
		}
	}
	return areas.size();
}

NavPathFollower::NavPathFollower() :
	corridor_(),
	links_(),
	navRevision_(0U),
	mapGeneration_(0U),
	currentIndex_(0U),
	active_(false)
{
}

NavFollowerResult NavPathFollower::start(const NavCorridor &corridor)
{
	if (!corridor.isValid() ||
			corridor.areas.size() > NavLimits::kMaximumAreas)
	{
		return NavFollowerResult::InvalidCorridor;
	}
	try
	{
		corridor_ = corridor.areas;
		links_ = corridor.links;
	}
	catch (const std::bad_alloc &)
	{
		return NavFollowerResult::ResourceLimit;
	}
	navRevision_ = corridor.navRevision;
	mapGeneration_ = corridor.mapGeneration;
	currentIndex_ = 0U;
	active_ = true;
	return NavFollowerResult::Started;
}

NavFollowerResult NavPathFollower::update(
	const NavSnapshot &snapshot,
	const NavVector &position,
	float horizontalTolerance,
	float verticalTolerance,
	NavVector *target,
	AreaId *targetArea)
{
	if (target == nullptr || targetArea == nullptr)
	{
		return NavFollowerResult::InvalidArgument;
	}
	if (!active_)
	{
		return NavFollowerResult::Inactive;
	}
	if (!snapshot.isValid())
	{
		return NavFollowerResult::InvalidSnapshot;
	}
	if (snapshot.revision() != navRevision_ ||
			snapshot.mapGeneration() != mapGeneration_)
	{
		return NavFollowerResult::StaleSnapshot;
	}
	if (!isFinitePosition(position))
	{
		return NavFollowerResult::InvalidPosition;
	}
	if (!isFiniteTolerance(horizontalTolerance) ||
			!isFiniteTolerance(verticalTolerance))
	{
		return NavFollowerResult::InvalidTolerance;
	}

	const NavDocument *currentDocument = snapshot.document();
	if (currentDocument == nullptr || currentIndex_ >= corridor_.size())
	{
		return NavFollowerResult::InvalidCorridor;
	}
	const NavArea *area = currentDocument->findArea(corridor_[currentIndex_]);
	if (area == nullptr)
	{
		return NavFollowerResult::InvalidCorridor;
	}

	NavVector currentTarget = portalSteeringPoint(*area, position);
	if (currentIndex_ > 0U)
	{
		const NavArea *previousArea =
				currentDocument->findArea(corridor_[currentIndex_ - 1U]);
		if (previousArea != nullptr)
		{
			const std::uint8_t direction = currentIndex_ - 1U < links_.size()
				? links_[currentIndex_ - 1U].direction
				: NavArea::kDirectionCount;
			currentTarget = portalSteeringPoint(
				*previousArea, *area, position, direction);
		}
	}
	*target = currentTarget;
	*targetArea = area->id;
	if (!isWithinTarget(
			*area,
			position,
			horizontalTolerance,
			verticalTolerance))
	{
		return NavFollowerResult::TargetReady;
	}
	if (currentIndex_ + 1U < corridor_.size())
	{
		++currentIndex_;
		const NavArea *nextArea =
			currentDocument->findArea(corridor_[currentIndex_]);
		if (nextArea == nullptr)
		{
			return NavFollowerResult::InvalidCorridor;
		}
		const NavArea *previousArea =
				currentDocument->findArea(corridor_[currentIndex_ - 1U]);
		const std::uint8_t direction = currentIndex_ - 1U < links_.size()
				? links_[currentIndex_ - 1U].direction
				: NavArea::kDirectionCount;
		*target = previousArea != nullptr
				? portalSteeringPoint(*previousArea, *nextArea, position, direction)
				: portalSteeringPoint(*nextArea, position);
		*targetArea = nextArea->id;
		return NavFollowerResult::Advanced;
	}

	active_ = false;
	return NavFollowerResult::Reached;
}

bool NavPathFollower::isActive() const
{
	return active_;
}

std::size_t NavPathFollower::currentIndex() const
{
	return currentIndex_;
}

}
}
