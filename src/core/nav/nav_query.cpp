#include "astrabot/nav/nav_query.hpp"

#include <algorithm>
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

bool isWithinTarget(
	const NavArea &area,
	const NavVector &position,
	float horizontalTolerance,
	float verticalTolerance)
{
	const NavVector target = centerOf(area);
	const float deltaX = position.x - target.x;
	const float deltaY = position.y - target.y;
	const float horizontalDistanceSquared =
		deltaX * deltaX + deltaY * deltaY;
	return horizontalDistanceSquared <=
			horizontalTolerance * horizontalTolerance &&
		floorDistance(area, position.z) <= verticalTolerance;
}
}

bool NavCorridor::isValid() const
{
	if (navRevision == 0U || mapGeneration == 0U || areas.empty())
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
					static_cast<std::uint8_t>(direction)
				});
			}
		}
	}
	catch (const std::bad_alloc &)
	{
		return NavQueryResult::ResourceLimit;
	}

	std::sort(candidate.begin(), candidate.end(),
		[](const NavDirectedLink &left, const NavDirectedLink &right)
		{
			if (left.toArea != right.toArea)
			{
				return left.toArea < right.toArea;
			}
			return left.direction < right.direction;
		});
	*links = candidate;
	return NavQueryResult::Found;
}

NavQueryResult NavQuery::buildCorridor(
	AreaId start,
	AreaId goal,
	NavCorridor *corridor) const
{
	if (corridor == nullptr)
	{
		return NavQueryResult::InvalidArgument;
	}
	*corridor = {};
	const NavDocument *currentDocument = document();
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
	if (!isValidLimits(limits_))
	{
		return NavQueryResult::ResourceLimit;
	}

	std::vector<AreaId> discovered;
	std::vector<AreaId> predecessor;
	try
	{
		discovered.reserve(limits_.maximumSearchQueue);
		predecessor.reserve(limits_.maximumSearchQueue);
		discovered.push_back(start);
		predecessor.push_back(0U);

		std::size_t queueIndex = 0U;
		while (queueIndex < discovered.size())
		{
			const AreaId current = discovered[queueIndex];
			++queueIndex;
			if (current == goal)
			{
				break;
			}

			const NavArea *area = currentDocument->findArea(current);
			if (area == nullptr)
			{
				return NavQueryResult::AreaNotFound;
			}
			for (std::size_t direction = 0U;
					direction < NavArea::kDirectionCount;
					++direction)
			{
				for (const AreaId target : area->connections[direction])
				{
					if (findDiscoveredArea(discovered, target) !=
							discovered.size())
					{
						continue;
					}
					if (discovered.size() >= limits_.maximumCorridorAreas ||
							discovered.size() >= limits_.maximumSearchQueue)
					{
						return NavQueryResult::ResourceLimit;
					}
					discovered.push_back(target);
					predecessor.push_back(current);
				}
			}
		}

		if (findDiscoveredArea(discovered, goal) == discovered.size())
		{
			return NavQueryResult::NoRoute;
		}

		std::vector<AreaId> reversed;
		reversed.reserve(discovered.size());
		AreaId current = goal;
		while (current != 0U)
		{
			reversed.push_back(current);
			const std::size_t index = findDiscoveredArea(discovered, current);
			if (index == discovered.size())
			{
				return NavQueryResult::NoRoute;
			}
			current = predecessor[index];
		}
		if (reversed.size() > limits_.maximumCorridorAreas)
		{
			return NavQueryResult::ResourceLimit;
		}
		std::reverse(reversed.begin(), reversed.end());

		corridor->navRevision = snapshot_.revision();
		corridor->mapGeneration = snapshot_.mapGeneration();
		corridor->areas = reversed;
	}
	catch (const std::bad_alloc &)
	{
		*corridor = {};
		return NavQueryResult::ResourceLimit;
	}

	return NavQueryResult::Found;
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

	NavVector currentTarget = centerOf(*area);
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
		*target = centerOf(*nextArea);
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
