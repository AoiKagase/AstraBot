#include "astrabot/nav/nav_query.hpp"
#include "native_path_geometry.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <new>
#include <utility>
#include <unordered_map>

namespace astrabot
{
namespace nav
{
namespace
{
constexpr auto kNorthDirection = 0U;
constexpr auto kEastDirection = 1U;
constexpr auto kSouthDirection = 2U;
constexpr auto kWestDirection = 3U;
constexpr float kDirectionGeometryTolerance = 1.0f;
constexpr float kMinimumTraversalDistance = 1.0f;
constexpr float kDangerCostScale = 100.0f;
constexpr float kImpassableTraversalCost = -1.0f;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_NAV_QUERY_AGGRESSION_DANGER_REDUCTION 0.95f
constexpr float kFastestCrouchPenalty = 20.0f;
constexpr float kSafestCrouchPenalty = 5.0f;
#define ASTRABOT_NAV_QUERY_ESTIMATED_FALL_DAMAGE_PER_UNIT 0.2178f
constexpr float kEstimatedFallDamageOffset = 26.0f;
constexpr float kFallDamageHealthReserve = 10.0f;
constexpr float kAggressionFallDamageAllowance = 15.0f;
constexpr float kBaseFallDamageAllowance = 10.0f;
constexpr float kFallDamageCostScale = 100.0f;
constexpr float kHostageCrouchPenaltyMultiplier = 3.0f;
constexpr float kMinimumCrowdPenaltyAreaSize = 1.0f;
constexpr float kCrowdPenaltyScale = 50000.0f;
constexpr auto kMinimumCorridorAreaCount = 2U;

bool isBetterMatch(float candidateDistance, AreaId candidateArea, float bestDistance, AreaId bestArea)
{
	return candidateDistance < bestDistance || (candidateDistance == bestDistance && candidateArea < bestArea);
}

bool isValidLimits(const NavQueryLimits &limits)
{
	return limits.maximumCorridorAreas != 0U && limits.maximumSearchQueue != 0U &&
		limits.maximumCorridorAreas <= NavLimits::kMaximumAreas &&
		limits.maximumSearchQueue <= NavLimits::kMaximumAreas;
}

bool isFinitePosition(const NavVector &position)
{
	return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

bool isFiniteTolerance(float tolerance)
{
	return std::isfinite(tolerance) && tolerance >= 0.0f;
}

float surfaceZAtInternal(const NavArea &area, float x, float y);

float surfaceDistance(const NavArea &area, const NavVector &position)
{
	return std::fabs(surfaceZAtInternal(area, position.x, position.y) - position.z);
}

float surfaceZAtInternal(const NavArea &area, float x, float y)
{
	const float dx = area.extent.hi.x - area.extent.lo.x;
	const float dy = area.extent.hi.y - area.extent.lo.y;
	if (dx == 0.0f || dy == 0.0f)
	{
		return area.extent.lo.z;
	}
	float u = (x - area.extent.lo.x) / dx;
	float v = (y - area.extent.lo.y) / dy;
	u = (std::max)(0.0f, (std::min)(1.0f, u));
	v = (std::max)(0.0f, (std::min)(1.0f, v));
	const float northZ = area.extent.lo.z + u * (area.northEastZ - area.extent.lo.z);
	const float southZ = area.southWestZ + u * (area.extent.hi.z - area.southWestZ);
	return northZ + v * (southZ - northZ);
}

NavVector centerOf(const NavArea &area)
{
	return {area.extent.lo.x * 0.5f + area.extent.hi.x * 0.5f, area.extent.lo.y * 0.5f + area.extent.hi.y * 0.5f,
		surfaceZAtInternal(
			area, (area.extent.lo.x + area.extent.hi.x) * 0.5f, (area.extent.lo.y + area.extent.hi.y) * 0.5f)};
}

float rectangleDistanceSquared(const NavArea &area, const NavVector &position)
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

NavVector closestPointOnArea(const NavArea &area, const NavVector &position)
{
	NavVector point = position;
	point.x = (std::max)(area.extent.lo.x, (std::min)(point.x, area.extent.hi.x));
	point.y = (std::max)(area.extent.lo.y, (std::min)(point.y, area.extent.hi.y));
	point.z = surfaceZAtInternal(area, point.x, point.y);
	return point;
}

bool isWithinTarget(const NavArea &area, const NavVector &position, float horizontalTolerance, float verticalTolerance)
{
	return rectangleDistanceSquared(area, position) <= horizontalTolerance * horizontalTolerance &&
		surfaceDistance(area, position) <= verticalTolerance;
}

bool isInsideArea(const NavArea &area, const NavVector &position, float verticalTolerance)
{
	return rectangleDistanceSquared(area, position) == 0.0f && surfaceDistance(area, position) <= verticalTolerance;
}

NavVector portalSteeringPoint(const NavArea &area, const NavVector &position)
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
	return {closest.x + deltaX / distance * kPortalInset, closest.y + deltaY / distance * kPortalInset, closest.z};
}

NavVector portalSteeringPoint(const NavArea &from, const NavArea &to, const NavVector &position)
{
	constexpr float kDestinationPortalInset = 16.0f;
	const float overlapLoX = (std::max)(from.extent.lo.x, to.extent.lo.x);
	const float overlapHiX = (std::min)(from.extent.hi.x, to.extent.hi.x);
	const float overlapLoY = (std::max)(from.extent.lo.y, to.extent.lo.y);
	const float overlapHiY = (std::min)(from.extent.hi.y, to.extent.hi.y);
	if (overlapLoX > overlapHiX || overlapLoY > overlapHiY)
	{
		return portalSteeringPoint(to, position);
	}
	NavVector portal = {(overlapLoX + overlapHiX) * 0.5f, (overlapLoY + overlapHiY) * 0.5f, 0.0f};
	const NavVector center = centerOf(to);
	const float deltaX = center.x - portal.x;
	const float deltaY = center.y - portal.y;
	const float distance = std::hypot(deltaX, deltaY);
	if (!std::isfinite(distance) || distance <= kDestinationPortalInset)
	{
		return center;
	}
	const NavVector steering = {portal.x + deltaX / distance * kDestinationPortalInset,
		portal.y + deltaY / distance * kDestinationPortalInset, 0.0f};
	return {steering.x, steering.y, surfaceZAtInternal(to, steering.x, steering.y)};
}

NavVector portalSteeringPoint(const NavArea &from, const NavArea &to, const NavVector &position, std::uint8_t direction)
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
		(direction == kNorthDirection && to.extent.hi.y <= from.extent.lo.y + kDirectionGeometryTolerance) ||
		(direction == kEastDirection && to.extent.lo.x >= from.extent.hi.x - kDirectionGeometryTolerance) ||
		(direction == kSouthDirection && to.extent.lo.y >= from.extent.hi.y - kDirectionGeometryTolerance) ||
		(direction == kWestDirection && to.extent.hi.x <= from.extent.lo.x + kDirectionGeometryTolerance);
	if (!directionMatchesGeometry)
	{
		return portalSteeringPoint(from, to, position);
	}
	constexpr float kPortalMargin = 16.0f;
	const auto clampWithMargin = [kPortalMargin](float value, float lo, float hi)
	{
		if (hi - lo <= 2.0f * kPortalMargin)
		{
			return (lo + hi) * 0.5f;
		}
		return (std::max)(lo + kPortalMargin, (std::min)(hi - kPortalMargin, value));
	};
	NavVector result = {
		clampWithMargin(position.x, overlapLoX, overlapHiX), clampWithMargin(position.y, overlapLoY, overlapHiY), 0.0f};
	// A portal target must enter the next area without crossing its far edge.
	// CSBot's step-in is explicitly smaller than the minimum area size; bound
	// our inset by half the destination depth for thin wall-top NAV polygons.
	const float insetX = (std::min)(kPortalMargin, (to.extent.hi.x - to.extent.lo.x) * 0.5f);
	const float insetY = (std::min)(kPortalMargin, (to.extent.hi.y - to.extent.lo.y) * 0.5f);
	switch (direction)
	{
	case kNorthDirection:
		result.y = to.extent.hi.y - insetY;
		break;
	case kEastDirection:
		result.x = to.extent.lo.x + insetX;
		break;
	case kSouthDirection:
		result.y = to.extent.lo.y + insetY;
		break;
	case kWestDirection:
		result.x = to.extent.hi.x - insetX;
		break;
	default:
		break;
	}
	result.z = surfaceZAtInternal(to, result.x, result.y);
	return result;
}

std::uint8_t howForLink(const NavDocument *document, AreaId fromArea, AreaId toArea)
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

NavDirectedLink directedLinkFor(const NavDocument *document, AreaId fromArea, AreaId toArea)
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
	for (std::size_t direction = 0U; direction < NavArea::kDirectionCount; ++direction)
	{
		for (const AreaId target : from->connections[direction])
		{
			if (target == toArea)
			{
				return {fromArea, toArea, static_cast<std::uint8_t>(direction), howForLink(document, fromArea, toArea)};
			}
		}
	}
	return {fromArea, toArea, 0U, howForLink(document, fromArea, toArea)};
}
}

float surfaceZAt(const NavArea &area, float x, float y)
{
	return surfaceZAtInternal(area, x, y);
}

NavVector extentCenterOf(const NavArea &area)
{
	return {(area.extent.lo.x + area.extent.hi.x) / 2.0f, (area.extent.lo.y + area.extent.hi.y) / 2.0f,
		(area.extent.lo.z + area.extent.hi.z) / 2.0f};
}

bool NavCorridor::isValid() const
{
	if (navRevision == 0U || mapGeneration == 0U || !std::isfinite(cost) || areas.empty() ||
		(hasEndpoint && !isFinitePosition(endpoint)))
	{
		return false;
	}
	if (!links.empty() && links.size() + 1U != areas.size())
	{
		return false;
	}

	for (std::size_t index = 0; index < nativeLadders.size(); ++index)
	{
		const auto &step = nativeLadders[index];
		if (step.linkIndex >= links.size() || (index && nativeLadders[index - 1].linkIndex >= step.linkIndex) ||
			!isFinitePosition(step.ladder.top) || !isFinitePosition(step.ladder.bottom) || step.ladder.direction >= 4 ||
			!std::isfinite(step.ladder.length) || step.ladder.length < 0)
			return false;
		const auto &link = links[step.linkIndex];
		if (link.fromArea != areas[step.linkIndex] || link.toArea != areas[step.linkIndex + 1] ||
			(link.how != 4 && link.how != 5))
			return false;
		if (link.direction != step.ladder.direction)
			return false;
		if (link.how == 4)
		{
			if (step.ladder.dangling || link.fromArea != step.ladder.bottomArea ||
				std::find(step.ladder.topAreas.begin(), step.ladder.topAreas.begin() + 3, link.toArea) ==
					step.ladder.topAreas.begin() + 3)
				return false;
		}
		else if (link.toArea != step.ladder.bottomArea ||
			std::find(step.ladder.topAreas.begin(), step.ladder.topAreas.end(), link.fromArea) ==
				step.ladder.topAreas.end())
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

NavVector portalSteeringPointForLink(
	const NavArea &fromArea, const NavArea &toArea, const NavVector &position, std::uint8_t direction)
{
	return portalSteeringPoint(fromArea, toArea, position, direction);
}

NavQuery::NavQuery(const NavSnapshot &snapshot)
	: snapshot_(snapshot), limits_{
							   NavQueryLimits::kDefaultMaximumCorridorAreas, NavQueryLimits::kDefaultMaximumSearchQueue}

{
}

NavQuery::NavQuery(const NavSnapshot &snapshot, const NavQueryLimits &limits) : snapshot_(snapshot), limits_(limits)
{
}

void NavQuery::setPathCostContext(const NavPathCostContext &context)
{
	costContext_ = context;
	hasCostContext_ = true;
}

NavQueryResult NavQuery::findContaining(const NavVector &position, float floorTolerance, NavAreaMatch *match) const
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
		if (position.x < area.extent.lo.x || position.x > area.extent.hi.x || position.y < area.extent.lo.y ||
			position.y > area.extent.hi.y)
		{
			continue;
		}

		const float candidateDistance = surfaceDistance(area, position);
		if (candidateDistance > floorTolerance ||
			(found && !isBetterMatch(candidateDistance, area.id, bestVerticalDistance, bestArea)))
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

NavQueryResult NavQuery::findNearest(const NavVector &position, float maximumDistance, NavAreaMatch *match) const
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
		const float candidateDistance = rectangleDistanceSquared(area, position);
		if (candidateDistance > maximumDistanceSquared ||
			(found && !isBetterMatch(candidateDistance, area.id, bestDistance, bestArea)))
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

NavQueryResult NavQuery::findNearest3D(const NavVector &position, float maximumDistance, NavAreaMatch *match) const
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
		const float horizontalDistance = rectangleDistanceSquared(area, position);
		const float verticalDistance = surfaceDistance(area, position);
		const float candidateDistance = horizontalDistance + verticalDistance * verticalDistance;
		if (candidateDistance > maximumDistanceSquared ||
			(found && !isBetterMatch(candidateDistance, area.id, bestDistance, bestArea)))
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

	const NavArea *area = document()->findArea(bestArea);
	match->area = bestArea;
	match->distanceSquared = bestDistance;
	match->closestPoint = closestPointOnArea(*area, position);
	return NavQueryResult::Found;
}

NavQueryResult NavQuery::outgoingLinks(AreaId fromArea, std::vector<NavDirectedLink> *links) const
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
		candidate.reserve(NavArea::kDirectionCount * NavLimits::kMaximumConnectionsPerDirection);
		for (std::size_t direction = 0U; direction < NavArea::kDirectionCount; ++direction)
		{
			for (const AreaId target : area->connections[direction])
			{
				candidate.push_back({fromArea, target, static_cast<std::uint8_t>(direction),
					howForLink(currentDocument, fromArea, target)});
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
	NavDirectedLink parentLink{};
	std::int32_t parentLadder = -1;
};

struct AStarStatsTimer
{
	NavSearchStats *stats;
	std::chrono::steady_clock::time_point start;

	AStarStatsTimer(NavSearchStats *value, AreaId startArea, AreaId goalArea, NavRouteType routeType)
		: stats(value),
		  start(value != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point())
	{
		if (stats != nullptr)
		{
			static std::uint64_t nextSearchId = 0U;
			stats->firstSearchId = ++nextSearchId;
			stats->lastSearchId = stats->firstSearchId;
			stats->firstStartArea = startArea;
			stats->firstGoalArea = goalArea;
			stats->lastStartArea = startArea;
			stats->lastGoalArea = goalArea;
			stats->firstRouteType = static_cast<std::uint8_t>(routeType);
			stats->lastRouteType = stats->firstRouteType;
		}
	}

	~AStarStatsTimer()
	{
		if (stats == nullptr)
		{
			return;
		}
		++stats->searchCalls;
		const auto elapsed =
			std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
		const std::uint64_t usec = elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0U;
		stats->totalUsec += usec;
		if (usec > stats->maxUsec)
		{
			stats->maxUsec = usec;
		}
	}
};

NavQueryResult validateNativeTopology(const NavSnapshot &snapshot, const NavLadderTopology &ladders)
{
	if (!snapshot.isValid())
		return NavQueryResult::EmptySnapshot;
	if (ladders.navRevision != snapshot.revision() || ladders.mapGeneration != snapshot.mapGeneration() ||
		ladders.ladders.size() > NavLimits::kMaximumAreas)
		return NavQueryResult::InvalidArgument;
	const auto *doc = snapshot.document();
	for (const auto &ladder : ladders.ladders)
	{
		if (!isFinitePosition(ladder.top) || !isFinitePosition(ladder.bottom) || !std::isfinite(ladder.length) ||
			ladder.length < 0 || ladder.direction >= 4 || (ladder.bottomArea && !doc->findArea(ladder.bottomArea)))
			return NavQueryResult::InvalidArgument;
		for (const auto id : ladder.topAreas)
			if (id && !doc->findArea(id))
				return NavQueryResult::InvalidArgument;
	}
	return NavQueryResult::Found;
}

NavQueryResult buildAStarCorridor(const NavSnapshot &snapshot, const NavQueryLimits &limits, AreaId start, AreaId goal,
	NavRouteType routeType, NavCorridor *corridor, NavSearchStats *stats, const NavPathCostContext *context,
	const NavDirectedLink *excludedLink = nullptr, const NavLadderTopology *ladders = nullptr,
	bool allowDeadRange = false, bool useClosestArea = false, const NavVector *explicitGoal = nullptr)
{
	if (stats != nullptr)
	{
		*stats = {};
	}
	AStarStatsTimer statsTimer(stats, start, goal, routeType);
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
	if (start == 0U || goal == 0U || currentDocument->findArea(start) == nullptr ||
		currentDocument->findArea(goal) == nullptr)
	{
		return NavQueryResult::AreaNotFound;
	}
	if (!isValidLimits(limits))
	{
		return NavQueryResult::ResourceLimit;
	}

	// Index records without changing the ordered open list or its strict-cost
	// tie handling. A linear record scan inside every open-list comparison
	// otherwise multiplies the two searches on large, branching NAV graphs.
	std::unordered_map<AreaId, std::size_t> recordLookup;
	const auto findRecord = [&recordLookup](const std::vector<AStarRecord> &records, AreaId area)
	{
		const auto found = recordLookup.find(area);
		return found == recordLookup.end() ? records.size() : found->second;
	};
	const auto contains = [](const std::vector<AreaId> &areas, AreaId area)
	{
		return std::find(areas.begin(), areas.end(), area) != areas.end();
	};
	const auto distanceBetween = [context](const NavArea &left, const NavArea &right)
	{
		const bool extentCenters = context != nullptr && context->useExtentCenters;
		const NavVector leftCenter = extentCenters ? extentCenterOf(left) : centerOf(left);
		const NavVector rightCenter = extentCenters ? extentCenterOf(right) : centerOf(right);
		const float dx = rightCenter.x - leftCenter.x;
		const float dy = rightCenter.y - leftCenter.y;
		const float dz = rightCenter.z - leftCenter.z;
		const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (extentCenters && std::isfinite(distance))
			return distance;
		return std::isfinite(distance) && distance > kMinimumTraversalDistance ? distance : kMinimumTraversalDistance;
	};
	// Native Help range includes dead recipients; movement must opt out of that exception.
	if (context != nullptr &&
		(!std::isfinite(context->aggression) || context->aggression < 0.0f || context->aggression > 1.0f ||
			!std::isfinite(context->health) || (context->health <= 0.0f && !allowDeadRange)))
	{
		return NavQueryResult::InvalidArgument;
	}
	const auto areaState = [context](AreaId id) -> const NavAreaCostState *
	{
		if (context == nullptr)
			return nullptr;
		for (const auto &state : context->areas)
		{
			if (state.area == id)
				return &state;
		}
		return nullptr;
	};
	if (context != nullptr)
	{
		for (const auto &state : context->areas)
		{
			if (!std::isfinite(state.danger) || state.danger < 0.0f)
				return NavQueryResult::InvalidArgument;
		}
	}
	const float dangerFactor = context == nullptr
		? 0.0f
		: kDangerCostScale * (1.0f - ASTRABOT_NAV_QUERY_AGGRESSION_DANGER_REDUCTION * context->aggression);
	const auto traversalCost = [routeType, &distanceBetween, context, &areaState, dangerFactor, ladders](
								   const NavArea &from, const NavArea &to, float ladderLength)
	{
		if ((from.attributes & NavArea::kJump) != 0U && (to.attributes & NavArea::kJump) != 0U)
			return kImpassableTraversalCost;
		const float distance = ladderLength >= 0 ? ladderLength : distanceBetween(from, to);
		float crouchPenalty = routeType == NavRouteType::Fastest ? kFastestCrouchPenalty : kSafestCrouchPenalty;
		const float jumpPenalty = 1.0f;
		float cost = distance;
		if (context != nullptr && context->zombie)
			return cost;
		if (context != nullptr)
		{
			bool reverseConnected = false;
			for (const auto &connections : to.connections)
				reverseConnected =
					reverseConnected || std::find(connections.begin(), connections.end(), from.id) != connections.end();
			if (ladders && !reverseConnected)
				for (const auto &ladder : ladders->ladders)
					for (const auto top : ladder.topAreas)
						reverseConnected = reverseConnected || (ladder.bottomArea == to.id && top == from.id) ||
							(ladder.bottomArea == from.id && top == to.id);
			if (!reverseConnected)
			{
				const float drop = centerOf(from).z - centerOf(to).z;
				// CSBot's navigation estimate is empirical, distinct from actual damage.
				const float damage = (std::max)(
					0.0f, ASTRABOT_NAV_QUERY_ESTIMATED_FALL_DAMAGE_PER_UNIT * drop - kEstimatedFallDamageOffset);
				if (damage > 0.0f)
				{
					if (damage + kFallDamageHealthReserve >= context->health)
						return kImpassableTraversalCost;
					if (routeType == NavRouteType::Safest ||
						damage > kAggressionFallDamageAllowance * context->aggression + kBaseFallDamageAllowance)
						cost += kFallDamageCostScale * damage * damage;
				}
			}
			if (context->hostageEscortCount != 0U)
				crouchPenalty *= kHostageCrouchPenaltyMultiplier;
		}
		if ((to.attributes & NavArea::kCrouch) != 0U)
		{
			cost += crouchPenalty * distance;
		}
		if ((to.attributes & NavArea::kJump) != 0U)
		{
			cost += jumpPenalty * distance;
		}
		const auto state = areaState(to.id);
		if (state != nullptr)
		{
			if (routeType == NavRouteType::Safest)
				cost += distance * dangerFactor * state->danger;
			const float size = ((to.extent.hi.x - to.extent.lo.x) + (to.extent.hi.y - to.extent.lo.y)) * 0.5f;
			if (!context->attacking && size >= kMinimumCrowdPenaltyAreaSize)
				cost += kCrowdPenaltyScale * static_cast<float>(state->teammates) / size;
		}
		return cost;
	};

	try
	{
		std::vector<AStarRecord> records;
		std::vector<AreaId> open;
		records.reserve(limits.maximumSearchQueue);
		open.reserve(limits.maximumSearchQueue);
		recordLookup.reserve((std::min)(limits.maximumSearchQueue, currentDocument->areaCount()));
		const NavArea *startArea = currentDocument->findArea(start);
		const NavArea *goalArea = currentDocument->findArea(goal);
		const auto goalPosition = explicitGoal ? *explicitGoal : extentCenterOf(*goalArea);
		const auto distanceToGoal = [&goalPosition](const NavArea &area)
		{
			const auto center = extentCenterOf(area);
			const float dx = center.x - goalPosition.x, dy = center.y - goalPosition.y, dz = center.z - goalPosition.z;
			return std::sqrt(dx * dx + dy * dy + dz * dz);
		};
		AreaId closestArea = start;
		float closestDistance = useClosestArea ? distanceToGoal(*startArea) : 0.0f;
		const float initialHeuristic = explicitGoal ? distanceToGoal(*startArea) : distanceBetween(*startArea, *goalArea);
		const auto startState = areaState(start);
		const float initialCost =
			routeType == NavRouteType::Safest && startState != nullptr ? dangerFactor * startState->danger : 0.0f;
		records.push_back({start, 0U, initialCost, initialCost + initialHeuristic, false, false});
		recordLookup.emplace(start, 0U);
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
			const auto visit = [&](AreaId target, float ladderLength, std::uint8_t direction, std::uint8_t how,
								   std::int32_t ladderIndex) -> NavQueryResult
			{
				if (!target || target == current)
					return NavQueryResult::Found;
				if (excludedLink != nullptr && current == excludedLink->fromArea && target == excludedLink->toArea)
					return NavQueryResult::Found;
				const NavArea *targetArea = currentDocument->findArea(target);
				if (targetArea == nullptr)
				{
					return NavQueryResult::AreaNotFound;
				}
				const float edgeCost = traversalCost(*currentArea, *targetArea, ladderLength);
				if (edgeCost < 0.0f || !std::isfinite(edgeCost))
					return NavQueryResult::Found;
				const float tentativeCost = records[currentRecordIndex].costSoFar + edgeCost;
				std::size_t targetRecordIndex = findRecord(records, target);
				if (targetRecordIndex != records.size() && tentativeCost >= records[targetRecordIndex].costSoFar)
					return NavQueryResult::Found;
				const float goalDistance = (useClosestArea || explicitGoal) ? distanceToGoal(*targetArea) : 0.0f;
				if (useClosestArea && goalDistance < closestDistance)
				{
					closestArea = target;
					closestDistance = goalDistance;
				}
				const float heuristic = explicitGoal ? goalDistance : distanceBetween(*targetArea, *goalArea);
				if (targetRecordIndex == records.size())
				{
					if (records.size() >= limits.maximumSearchQueue)
					{
						return NavQueryResult::ResourceLimit;
					}
					records.push_back({target, current, tentativeCost, tentativeCost + heuristic, false, false});
					records.back().parentLink = {current, target, direction, how};
					records.back().parentLadder = ladderIndex;
					recordLookup.emplace(target, records.size() - 1U);
					open.push_back(target);
					if (stats != nullptr)
					{
						++stats->enqueueCount;
					}
					return NavQueryResult::Found;
				}

				AStarRecord &targetRecord = records[targetRecordIndex];
				targetRecord.parent = current;
				targetRecord.parentLink = {current, target, direction, how};
				targetRecord.parentLadder = ladderIndex;
				targetRecord.costSoFar = tentativeCost;
				targetRecord.totalCost = tentativeCost + heuristic;
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
				return NavQueryResult::Found;
			};
			for (std::size_t direction = 0U; direction < NavArea::kDirectionCount; ++direction)
				for (const AreaId target : currentArea->connections[direction])
				{
					const auto result = visit(
						target, -1.0f, static_cast<std::uint8_t>(direction), static_cast<std::uint8_t>(direction), -1);
					if (result != NavQueryResult::Found)
						return result;
				}
			if (ladders)
			{
				// Native adjacency order is floor, up (forward/left/right), then down.
				bool upDirectionsConsumed = false;
				for (const auto &ladder : ladders->ladders)
					if (ladder.bottomArea == current && !ladder.dangling && !upDirectionsConsumed)
					{
						upDirectionsConsumed = true;
						for (unsigned side = 0; side < 3; ++side)
						{
							const auto result = visit(ladder.topAreas[side], ladder.length, ladder.direction, 4,
								static_cast<std::int32_t>(&ladder - ladders->ladders.data()));
							if (result != NavQueryResult::Found)
								return result;
						}
					}
				for (const auto &ladder : ladders->ladders)
					// A top area may be present in more than one native ladder-down list entry.
					for (const auto top : ladder.topAreas)
						if (top == current)
						{
							const auto result = visit(ladder.bottomArea, ladder.length, ladder.direction, 5,
								static_cast<std::int32_t>(&ladder - ladders->ladders.data()));
							if (result != NavQueryResult::Found)
								return result;
						}
			}
		}

		const AreaId endpointArea = useClosestArea && findRecord(records, goal) == records.size() ? closestArea : goal;
		const std::size_t goalRecordIndex = findRecord(records, endpointArea);
		if (goalRecordIndex == records.size())
		{
			return NavQueryResult::NoRoute;
		}
		std::vector<AreaId> reversed;
		reversed.reserve(records.size());
		AreaId current = endpointArea;
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
			if (!ladders)
				corridor->links.push_back(directedLinkFor(currentDocument, reversed[index - 1], reversed[index]));
			else
			{
				const auto &record = records[findRecord(records, reversed[index])];
				corridor->links.push_back(record.parentLink);
				if (record.parentLadder >= 0)
					corridor->nativeLadders.push_back(
						{index - 1, ladders->ladders[static_cast<std::size_t>(record.parentLadder)]});
			}
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

NavQueryResult NavQuery::buildNativeCorridor(AreaId start, AreaId goal, NavRouteType routeType,
	const NavLadderTopology &ladders, NavCorridor *corridor, NavSearchStats *stats) const
{
	if (!corridor)
		return NavQueryResult::InvalidArgument;
	*corridor = {};
	if (stats)
		*stats = {};
	const auto validation = validateNativeTopology(snapshot_, ladders);
	if (validation != NavQueryResult::Found)
		return validation;
	auto context = costContext_;
	context.useExtentCenters = true;
	return buildAStarCorridor(
		snapshot_, limits_, start, goal, routeType, corridor, stats, &context, nullptr, &ladders, false);
}

NavQueryResult NavQuery::buildInvestigationCorridor(AreaId start, AreaId goal, const NavVector &noisePosition,
	const NavLadderTopology *ladders, NavCorridor *corridor, NavSearchStats *stats,
	const NavDirectedLink *excludedFirstLink) const
{
	return buildPositionCorridor(start, goal, noisePosition, NavRouteType::Safest,
		ladders, corridor, stats, excludedFirstLink);
}

NavQueryResult NavQuery::buildPositionCorridor(AreaId start, AreaId goal, const NavVector &position,
	NavRouteType routeType, const NavLadderTopology *ladders, NavCorridor *corridor, NavSearchStats *stats,
	const NavDirectedLink *excludedFirstLink) const
{
	if (stats != nullptr)
		*stats = {};
	if (corridor == nullptr)
		return NavQueryResult::InvalidArgument;
	*corridor = {};
	if (!isFinitePosition(position))
		return NavQueryResult::InvalidPosition;
	const auto *doc = document();
	if (doc == nullptr)
		return NavQueryResult::EmptySnapshot;
	const auto *startArea = doc->findArea(start);
	const auto *goalArea = doc->findArea(goal);
	if (startArea == nullptr || goalArea == nullptr)
		return NavQueryResult::AreaNotFound;
	if (ladders != nullptr)
	{
		const auto validation = validateNativeTopology(snapshot_, *ladders);
		if (validation != NavQueryResult::Found)
			return validation;
	}
	if (start == goal)
	{
		corridor->navRevision = snapshot_.revision();
		corridor->mapGeneration = snapshot_.mapGeneration();
		corridor->routeType = routeType;
		corridor->areas = {start};
	}
	else
	{
		auto context = costContext_;
		if (ladders != nullptr)
			context.useExtentCenters = true;
		const auto *costs = hasCostContext_ || ladders != nullptr ? &context : nullptr;
		auto result = buildAStarCorridor(snapshot_, limits_, start, goal, routeType, corridor, stats,
			costs, excludedFirstLink, ladders, false, true, &position);
		// A straight-line fallback must not steer back into a rejected exit.
		if (result == NavQueryResult::Found && excludedFirstLink != nullptr && corridor->areas.size() == 1U)
		{
			*corridor = {};
			result = NavQueryResult::NoRoute;
		}
		if (stats != nullptr && stats->searchCalls != 0U)
		{
			stats->successCount = result == NavQueryResult::Found ? 1U : 0U;
			stats->failureCount = 1U - stats->successCount;
		}
		if (result != NavQueryResult::Found)
			return result;
	}
	// ComputePath keeps the original sound position on partial paths. Its
	// one-area case reprojects the same XY through BuildTrivialPath.
	const auto &floorArea = corridor->areas.size() == 1U ? *startArea : *goalArea;
	corridor->hasEndpoint = true;
	corridor->endpoint = position;
	corridor->endpoint.z = surfaceZAtInternal(floorArea, position.x, position.y);
	return NavQueryResult::Found;
}

NavQueryResult NavQuery::travelDistance(
	AreaId start, AreaId goal, NavRouteType routeType, const NavLadderTopology &ladders, float *distance) const
{
	if (!distance)
		return NavQueryResult::InvalidArgument;
	*distance = -1.0f;
	const auto validation = validateNativeTopology(snapshot_, ladders);
	if (validation != NavQueryResult::Found)
		return validation;
	const auto *doc = document();
	if (!start || !goal || !doc->findArea(start) || !doc->findArea(goal))
		return NavQueryResult::AreaNotFound;
	if (start == goal)
	{
		*distance = 0;
		return NavQueryResult::Found;
	}
	auto context = costContext_;
	context.useExtentCenters = true;
	NavCorridor corridor;
	const auto result = buildAStarCorridor(
		snapshot_, limits_, start, goal, routeType, &corridor, nullptr, &context, nullptr, &ladders, true);
	if (result != NavQueryResult::Found)
		return result;
	float total = 0;
	// Match native end-to-start accumulation order, which matters to 32-bit floats.
	for (std::size_t index = corridor.areas.size() - 1; index > 0; --index)
	{
		const auto a = extentCenterOf(*doc->findArea(corridor.areas[index]));
		const auto b = extentCenterOf(*doc->findArea(corridor.areas[index - 1]));
		const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
		total += std::sqrt(dx * dx + dy * dy + dz * dz);
	}
	if (!std::isfinite(total))
		return NavQueryResult::InvalidArgument;
	*distance = total;
	return NavQueryResult::Found;
}

NavQueryResult NavQuery::buildCorridor(AreaId start, AreaId goal, NavCorridor *corridor) const
{
	return buildCorridor(start, goal, NavRouteType::Fastest, corridor);
}

NavQueryResult NavQuery::buildCorridor(AreaId start, AreaId goal, NavCorridor *corridor, NavSearchStats *stats) const
{
	return buildCorridor(start, goal, NavRouteType::Fastest, corridor, stats);
}

NavQueryResult NavQuery::buildCorridor(AreaId start, AreaId goal, NavRouteType routeType, NavCorridor *corridor) const
{
	return buildCorridor(start, goal, routeType, corridor, nullptr);
}

NavQueryResult NavQuery::buildCorridor(
	AreaId start, AreaId goal, NavRouteType routeType, NavCorridor *corridor, NavSearchStats *stats) const
{
	return buildAStarCorridor(
		snapshot_, limits_, start, goal, routeType, corridor, stats, hasCostContext_ ? &costContext_ : nullptr);
}

namespace
{
void setAreaPathEndpoint(const NavDocument &document, AreaId goal, NavCorridor &corridor,
	const NavHuntPathProbe *probe, bool hunt)
{
	const auto *doc = &document;
	const auto *destination = doc->findArea(corridor.areas.back());
	auto endpoint = centerOf(*doc->findArea(goal));
	const float requestedHeight = endpoint.z;
	if (!corridor.links.empty())
	{
		auto portal = extentCenterOf(*doc->findArea(corridor.areas.front()));
		const bool recordWaypoints = hunt && corridor.nativeLadders.empty();
		if (recordWaypoints)
		{
			corridor.huntWaypoints.reserve(2U * corridor.links.size() + 2U);
			corridor.huntWaypoints.push_back({portal, 0U});
		}
		auto ladderStep = corridor.nativeLadders.begin();
		for (std::size_t i = 0U; i < corridor.links.size(); ++i)
		{
			const auto &link = corridor.links[i];
			if (ladderStep != corridor.nativeLadders.end() && ladderStep->linkIndex == i)
			{
				const auto &ladder = ladderStep->ladder;
				const bool ascending = link.fromArea == ladder.bottomArea;
				portal = ascending ? ladder.bottom : ladder.top;
				const auto direction = ascending ? ladder.direction : (ladder.direction + 2U) % 4U;
				portal.x += direction == kEastDirection ? 32.0f : direction == kWestDirection ? -32.0f : 0.0f;
				portal.y += direction == kSouthDirection ? 32.0f : direction == kNorthDirection ? -32.0f : 0.0f;
				++ladderStep;
				continue;
			}
			const auto &from = *doc->findArea(link.fromArea);
			const auto &to = *doc->findArea(link.toArea);
			const bool reverse = std::any_of(to.connections.begin(), to.connections.end(),
				[&](const std::vector<AreaId> &neighbors)
				{ return std::find(neighbors.begin(), neighbors.end(), from.id) != neighbors.end(); });
			portal = nativeGroundPortal(document, link, portal, reverse);
			const auto areaIndex = i + 1U;
			if (recordWaypoints)
				corridor.huntWaypoints.push_back({portal, areaIndex});
			if (!reverse)
			{
				portal.x += link.direction == kEastDirection ? 25.0f : link.direction == kWestDirection ? -25.0f : 0.0f;
				portal.y += link.direction == kSouthDirection ? 25.0f : link.direction == kNorthDirection ? -25.0f : 0.0f;
				portal.z = surfaceZAtInternal(to, portal.x, portal.y);
				if (recordWaypoints)
					corridor.huntWaypoints.push_back({portal, areaIndex});
			}
		}
		if (corridor.nativeLadders.empty() || corridor.nativeLadders.back().linkIndex + 1U != corridor.links.size())
		{
			endpoint = extentCenterOf(*destination);
			const auto direction = corridor.links.back().direction;
			if (direction == kNorthDirection || direction == kSouthDirection)
				endpoint.x = portal.x;
			else
				endpoint.y = portal.y;
		}
	}
	corridor.hasEndpoint = true;
	corridor.huntEndpointSteering = hunt;
	corridor.endpoint = {endpoint.x, endpoint.y, surfaceZAtInternal(*destination, endpoint.x, endpoint.y)};
	if (probe && !corridor.links.empty())
	{
		float ground = requestedHeight;
		const bool sampled = probe->groundHeight({endpoint.x, endpoint.y, requestedHeight}, &ground);
		corridor.endpoint.z = sampled && std::isfinite(ground) ? ground : requestedHeight;
	}
	if (!corridor.huntWaypoints.empty())
	{
		corridor.huntWaypoints.push_back({corridor.endpoint, corridor.areas.size() - 1U});
	}
}
}

NavVector nativeGroundPortal(const NavDocument &document, const NavDirectedLink &link,
	const NavVector &previous, bool bidirectional)
{
	const auto &from = *document.findArea(link.fromArea);
	const auto &to = *document.findArea(link.toArea);
	const auto onEdge = [&](unsigned direction) {
		for (const auto id : to.connections[direction])
		{
			const auto &reverse = document.findArea(id)->connections[(direction + 2) % 4];
			if (std::find(reverse.begin(), reverse.end(), to.id) != reverse.end())
				return false;
		}
		return true;
	};
	const auto portalCoordinate = [](float value, float fromLo, float fromHi, float toLo, float toHi,
									  bool lowEdge, bool highEdge) {
		constexpr float margin = 12.5f;
		const float low = (std::min)(fromHi, (std::max)(fromLo, toLo)) + (lowEdge ? margin : 0.0f);
		const float high = (std::max)(fromLo, (std::min)(fromHi, toHi)) - (highEdge ? margin : 0.0f);
		return value < low ? low : (std::min)(value, high);
	};
	auto point = previous;
	const float step = bidirectional ? 5.0f : 30.0f;
	if (link.direction == kNorthDirection || link.direction == kSouthDirection)
	{
		point.x = portalCoordinate(point.x, from.extent.lo.x, from.extent.hi.x, to.extent.lo.x, to.extent.hi.x,
			onEdge(kWestDirection), onEdge(kEastDirection));
		point.y = link.direction == kNorthDirection ? from.extent.lo.y - step : from.extent.hi.y + step;
	}
	else
	{
		point.y = portalCoordinate(point.y, from.extent.lo.y, from.extent.hi.y, to.extent.lo.y, to.extent.hi.y,
			onEdge(kNorthDirection), onEdge(kSouthDirection));
		point.x = link.direction == kWestDirection ? from.extent.lo.x - step : from.extent.hi.x + step;
	}
	point.z = surfaceZAtInternal(from, point.x, point.y);
	return point;
}

NavQueryResult NavQuery::buildHuntCorridor(AreaId start, AreaId goal, NavCorridor *corridor, NavSearchStats *stats,
	const NavHuntPathProbe *probe, const NavLadderTopology *ladders) const
{
	if (ladders)
	{
		const auto validation = validateNativeTopology(snapshot_, *ladders);
		if (validation != NavQueryResult::Found)
			return validation;
	}
	const auto *doc = document();
	const auto *source = doc == nullptr ? nullptr : doc->findArea(start);
	if (corridor != nullptr && start == goal && source != nullptr)
	{
		if (stats != nullptr)
			*stats = {};
		*corridor = {};
		corridor->navRevision = snapshot_.revision();
		corridor->mapGeneration = snapshot_.mapGeneration();
		corridor->routeType = NavRouteType::Safest;
		corridor->areas = {start};
		corridor->hasEndpoint = true;
		corridor->huntEndpointSteering = true;
		corridor->endpoint = centerOf(*source);
		return NavQueryResult::Found;
	}
	const auto result = buildAStarCorridor(snapshot_, limits_, start, goal, NavRouteType::Safest, corridor, stats,
		hasCostContext_ ? &costContext_ : nullptr, nullptr, ladders, false, true);
	// A direct Hunt uses the native portal before its endpoint. Friend waiting
	// and the movement issued on expiry must use the same approach point.
	if (result != NavQueryResult::Found ||
		(corridor->areas.back() == goal && corridor->nativeLadders.empty() && corridor->links.size() > 1U))
		return result;

	try
	{
		setAreaPathEndpoint(*doc, goal, *corridor, probe, true);
	}
	catch (const std::bad_alloc &)
	{
		*corridor = {};
		return NavQueryResult::ResourceLimit;
	}
	return result;
}

NavQueryResult NavQuery::buildFollowCorridor(AreaId start, AreaId goal, const NavLadderTopology *ladders,
	NavCorridor *corridor, NavSearchStats *stats, const NavHuntPathProbe *probe) const
{
	if (ladders)
	{
		const auto validation = validateNativeTopology(snapshot_, *ladders);
		if (validation != NavQueryResult::Found)
			return validation;
	}
	auto context = costContext_;
	context.useExtentCenters = true;
	auto result = buildAStarCorridor(snapshot_, limits_, start, goal, NavRouteType::Fastest, corridor,
		stats, &context, nullptr, ladders, false, true);
	if (result == NavQueryResult::Found)
		setAreaPathEndpoint(*document(), goal, *corridor, probe, false);
	if (stats && stats->searchCalls != 0U)
	{
		stats->successCount = result == NavQueryResult::Found ? 1U : 0U;
		stats->failureCount = 1U - stats->successCount;
	}
	return result;
}

NavQueryResult NavQuery::buildAlternativeCorridor(const NavCorridor &currentCorridor,
	const NavDirectedLink &failedFirstLink, NavCorridor *alternativeCorridor, NavSearchStats *stats, AreaId huntGoal,
	const NavHuntPathProbe *huntProbe, const NavLadderTopology *huntLadders) const
{
	if (alternativeCorridor == nullptr || alternativeCorridor == &currentCorridor || !snapshot_.isValid() ||
		!currentCorridor.isValid() || currentCorridor.areas.size() < kMinimumCorridorAreaCount ||
		currentCorridor.links.size() + 1U != currentCorridor.areas.size() ||
		currentCorridor.navRevision != snapshot_.revision() ||
		currentCorridor.mapGeneration != snapshot_.mapGeneration())
	{
		return NavQueryResult::InvalidArgument;
	}
	const NavDirectedLink &firstLink = currentCorridor.links.front();
	if (firstLink.fromArea != failedFirstLink.fromArea || firstLink.toArea != failedFirstLink.toArea ||
		firstLink.fromArea != currentCorridor.areas.front())
	{
		return NavQueryResult::InvalidArgument;
	}
	if (huntLadders)
	{
		const auto validation = validateNativeTopology(snapshot_, *huntLadders);
		if (validation != NavQueryResult::Found)
			return validation;
	}

	// Search the complete graph with the failed directed edge excluded. Searching
	// independent suffixes can prefer a return through the failed edge and then
	// incorrectly reject every candidate despite a longer valid bypass.
	const auto result = buildAStarCorridor(snapshot_, limits_, currentCorridor.areas.front(),
		currentCorridor.areas.back(), currentCorridor.routeType, alternativeCorridor, stats,
		hasCostContext_ ? &costContext_ : nullptr, &failedFirstLink, huntLadders);
	if (result == NavQueryResult::Found)
	{
		alternativeCorridor->hasEndpoint = currentCorridor.hasEndpoint;
		alternativeCorridor->endpoint = currentCorridor.endpoint;
		if (huntGoal != 0U && document()->findArea(huntGoal) &&
			(huntGoal != alternativeCorridor->areas.back() || !alternativeCorridor->nativeLadders.empty()))
		{
			try
			{
				setAreaPathEndpoint(*document(), huntGoal, *alternativeCorridor, huntProbe, true);
			}
			catch (const std::bad_alloc &)
			{
				*alternativeCorridor = {};
				return NavQueryResult::ResourceLimit;
			}
		}
	}
	return result;
}

const NavDocument *NavQuery::document() const
{
	return snapshot_.document();
}

std::size_t NavQuery::findDiscoveredArea(const std::vector<AreaId> &areas, AreaId id)
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

NavPathFollower::NavPathFollower()
	: corridor_(), links_(), navRevision_(0U), mapGeneration_(0U), currentIndex_(0U), active_(false)
{
}

NavFollowerResult NavPathFollower::start(const NavCorridor &corridor)
{
	if (!corridor.isValid() || !corridor.nativeLadders.empty() || corridor.areas.size() > NavLimits::kMaximumAreas)
	{
		return NavFollowerResult::InvalidCorridor;
	}
	if (!corridor.huntWaypoints.empty() && (!corridor.hasEndpoint || corridor.huntWaypoints.size() < 3U ||
			!isFinitePosition(corridor.huntWaypoints.front().position) ||
			std::any_of(corridor.huntWaypoints.begin() + 1, corridor.huntWaypoints.end(), [&corridor](const auto &waypoint)
			{
				return waypoint.areaIndex == 0U || waypoint.areaIndex >= corridor.areas.size() ||
					!isFinitePosition(waypoint.position);
			})))
		return NavFollowerResult::InvalidCorridor;
	try
	{
		corridor_ = corridor.areas;
		links_ = corridor.links;
		huntWaypoints_ = corridor.huntWaypoints;
	}
	catch (const std::bad_alloc &)
	{
		return NavFollowerResult::ResourceLimit;
	}
	navRevision_ = corridor.navRevision;
	mapGeneration_ = corridor.mapGeneration;
	hasEndpoint_ = corridor.hasEndpoint;
	huntEndpointSteering_ = corridor.huntEndpointSteering;
	huntWalking_ = false;
	endpoint_ = corridor.endpoint;
	currentIndex_ = 0U;
	huntWaypointIndex_ = huntWaypoints_.empty() ? 0U : 1U;
	if (huntWaypointIndex_ != 0U)
		huntTarget_ = huntWaypoints_[huntWaypointIndex_].position;
	active_ = true;
	return NavFollowerResult::Started;
}

namespace
{
NavVector displacement(const NavVector &from, const NavVector &to)
{
	return {to.x - from.x, to.y - from.y, to.z - from.z};
}

float dotProduct(const NavVector &a, const NavVector &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

NavVector pointAlong(const NavVector &from, const NavVector &delta, float fraction)
{
	fraction = (std::max)(0.0f, (std::min)(1.0f, fraction));
	return {from.x + fraction * delta.x, from.y + fraction * delta.y, from.z + fraction * delta.z};
}

NavVector unitVector(const NavVector &v)
{
	const float length = std::sqrt(dotProduct(v, v));
	return length > 0.0f ? NavVector{v.x / length, v.y / length, v.z / length} : NavVector{};
}

float distance2D(const NavVector &from, const NavVector &to)
{
	return std::hypot(to.x - from.x, to.y - from.y);
}
}

NavFollowerResult NavPathFollower::updateHuntPath(const NavDocument &document, const NavVector &feet,
	NavHuntSteeringContext hunt, NavVector &target, AreaId &targetArea)
{
	const std::size_t last = huntWaypoints_.size() - 1U;
	const std::size_t previousWaypoint = huntWaypointIndex_;
	target = huntTarget_;
	const auto traversalArea = [&](std::size_t waypoint)
	{
		const auto areaIndex = huntWaypoints_[waypoint].areaIndex;
		const auto traversal = std::find(corridor_.begin(), corridor_.begin() + areaIndex, hunt.currentArea);
		// Geometric lookahead can skip several tiny areas, but physical
		// clearance and traversal checks still belong to the immediate link.
		return traversal == corridor_.begin() + areaIndex ? corridor_[areaIndex] : *(traversal + 1);
	};
	targetArea = traversalArea(previousWaypoint);
	// CSBot projects onto the nearby path segments before changing its point index.
	const std::size_t firstSegment = previousWaypoint > 3U ? previousWaypoint - 3U : 1U;
	const std::size_t stop = (std::min)(huntWaypoints_.size(), previousWaypoint + 3U);
	float bestDistance = (std::numeric_limits<float>::max)();
	std::size_t closest = 0U;
	NavVector point{};
	for (std::size_t i = firstSegment; i < stop; ++i)
	{
		const auto &from = huntWaypoints_[i - 1U].position;
		const auto delta = displacement(from, huntWaypoints_[i].position);
		const float lengthSquared = dotProduct(delta, delta);
		const auto projected = pointAlong(from, delta,
			lengthSquared > 0.0f ? dotProduct(displacement(from, feet), delta) / lengthSquared : 0.0f);
		const auto offset = displacement(feet, projected);
		const float distance = dotProduct(offset, offset);
		if (distance < bestDistance && (hunt.probe == nullptr || hunt.probe->clear(feet, projected, true)))
		{
			bestDistance = distance;
			closest = i - 1U;
			point = projected;
		}
	}
	if (closest == 0U)
		return NavFollowerResult::TargetReady;

	const auto isJump = [&](std::size_t index)
	{
		const auto *area = document.findArea(corridor_[huntWaypoints_[index].areaIndex]);
		return area != nullptr && (area->attributes & NavArea::kJump) != 0U;
	};
	constexpr float closePointRange = 20.0f;
	while (closest < last && distance2D(point, huntWaypoints_[closest + 1U].position) < closePointRange)
		++closest;
	auto next = !hunt.crouched && closest > previousWaypoint && isJump(closest)
		? closest : (std::min)(last, closest + 1U);
	huntTarget_ = huntWaypoints_[next].position;
	if (!hunt.crouched && !isJump(next))
	{
		constexpr float aheadRange = 300.0f;
		const auto initial = unitVector(displacement(huntWaypoints_[next - 1U].position, huntTarget_));
		auto previousDirection = initial;
		float range = 0.0f;
		bool corner = false;
		for (std::size_t i = next; i <= last; ++i)
		{
			const auto &from = huntWaypoints_[i - 1U].position;
			const auto &to = huntWaypoints_[i].position;
			const auto delta = displacement(from, to);
			const auto direction = unitVector(delta);
			if (dotProduct(direction, initial) < 0.0f)
				break;
			if (dotProduct(direction, previousDirection) < 0.5f)
			{
				corner = true;
				break;
			}
			if (hunt.probe != nullptr && !hunt.probe->clear(feet, to, false))
			{
				// The pinned CSBot keeps probing the occluded point as it retreats.
				// Its steering therefore falls back to the preceding point.
				huntTarget_ = from;
				break;
			}
			if (isJump(i))
			{
				huntTarget_ = to;
				break;
			}
			previousDirection = direction;
			range += distance2D(i == next ? feet : from, to);
			const float segmentLength = distance2D(from, to);
			huntTarget_ = pointAlong(from, delta,
				segmentLength > 0.0f ? 1.0f - (range - aheadRange) / segmentLength : 1.0f);
			if (range >= aheadRange)
				break;
		}
		const auto toTarget = displacement(feet, huntTarget_);
		constexpr float steerMinimum = 50.0f;
		if (!corner && (toTarget.x * initial.x + toTarget.y * initial.y < 0.0f ||
				distance2D(feet, huntTarget_) < steerMinimum))
		{
			for (; next < last && !isJump(next) &&
				distance2D(feet, huntWaypoints_[next].position) <= steerMinimum; ++next) {}
			huntTarget_ = huntWaypoints_[next].position;
		}
	}

	huntWaypointIndex_ = (std::max)(previousWaypoint, next);
	const auto waypointArea = huntWaypoints_[huntWaypointIndex_].areaIndex;
	// The final portal and endpoint may both belong to the same area. Expose
	// the terminal corridor index only once the separate endpoint leg is active.
	currentIndex_ = huntWaypointIndex_ == last ? waypointArea : waypointArea - 1U;
	target = huntTarget_;
	targetArea = traversalArea(huntWaypointIndex_);
	return huntWaypointIndex_ > previousWaypoint ? NavFollowerResult::Advanced : NavFollowerResult::TargetReady;
}

NavFollowerResult NavPathFollower::update(const NavSnapshot &snapshot, const NavVector &position,
	float horizontalTolerance, float verticalTolerance, NavVector *target, AreaId *targetArea, NavHuntSteeringContext hunt)
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
	if (snapshot.revision() != navRevision_ || snapshot.mapGeneration() != mapGeneration_)
	{
		return NavFollowerResult::StaleSnapshot;
	}
	if (!isFinitePosition(position))
	{
		return NavFollowerResult::InvalidPosition;
	}
	if (!isFiniteTolerance(horizontalTolerance) || !isFiniteTolerance(verticalTolerance))
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
	const bool atEndpoint = huntWaypoints_.empty()
		? currentIndex_ + 1U == corridor_.size()
		: huntWaypointIndex_ + 1U == huntWaypoints_.size();
	if (hunt.forcedRunning)
		huntWalking_ = false;
	if (hasEndpoint_ && atEndpoint)
	{
		const auto toEnd = displacement(position, endpoint_);
		const float distanceSquared = dotProduct(toEnd, toEnd);
		constexpr float walkRange = 200.0f;
		// CSBot decides walking from the incoming point index. Walk persists
		// until the route ends even if the bot is pushed out of walk range.
		if (huntEndpointSteering_ && !hunt.forcedRunning && !hunt.crouched &&
			distanceSquared < walkRange * walkRange)
			huntWalking_ = true;
		if (distanceSquared < horizontalTolerance * horizontalTolerance ||
			(horizontalTolerance == 0.0f && distanceSquared == 0.0f))
		{
			*target = endpoint_;
			*targetArea = area->id;
			huntWalking_ = false;
			active_ = false;
			return NavFollowerResult::Reached;
		}
		if (huntWaypoints_.empty())
		{
			*target = endpoint_;
			*targetArea = area->id;
			return NavFollowerResult::TargetReady;
		}
	}
	if (!huntWaypoints_.empty())
		return updateHuntPath(*currentDocument, position, hunt, *target, *targetArea);

	NavVector currentTarget = portalSteeringPoint(*area, position);
	if (currentIndex_ > 0U)
	{
		const NavArea *previousArea = currentDocument->findArea(corridor_[currentIndex_ - 1U]);
		if (previousArea != nullptr)
		{
			const std::uint8_t direction =
				currentIndex_ - 1U < links_.size() ? links_[currentIndex_ - 1U].direction : NavArea::kDirectionCount;
			currentTarget = portalSteeringPoint(*previousArea, *area, position, direction);
		}
	}
	*target = currentTarget;
	*targetArea = area->id;
	const bool reachedCurrentSegment = isWithinTarget(*area, position, horizontalTolerance, verticalTolerance);
	const bool enteredCurrentArea = currentIndex_ == 0U || currentIndex_ + 1U >= corridor_.size() ||
		isInsideArea(*area, position, verticalTolerance);
	if (!reachedCurrentSegment || !enteredCurrentArea)
	{
		return NavFollowerResult::TargetReady;
	}
	if (currentIndex_ + 1U < corridor_.size())
	{
		++currentIndex_;
		const NavArea *nextArea = currentDocument->findArea(corridor_[currentIndex_]);
		if (nextArea == nullptr)
		{
			return NavFollowerResult::InvalidCorridor;
		}
		const NavArea *previousArea = currentDocument->findArea(corridor_[currentIndex_ - 1U]);
		const std::uint8_t direction =
			currentIndex_ - 1U < links_.size() ? links_[currentIndex_ - 1U].direction : NavArea::kDirectionCount;
		*target = previousArea != nullptr ? portalSteeringPoint(*previousArea, *nextArea, position, direction)
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
#undef ASTRABOT_NAV_QUERY_AGGRESSION_DANGER_REDUCTION
#undef ASTRABOT_NAV_QUERY_ESTIMATED_FALL_DAMAGE_PER_UNIT
