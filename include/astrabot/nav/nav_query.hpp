#ifndef ASTRABOT_NAV_NAV_QUERY_HPP
#define ASTRABOT_NAV_NAV_QUERY_HPP

#include "astrabot/nav/nav_snapshot.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace astrabot
{
namespace nav
{
struct NavQueryLimits
{
	static constexpr std::size_t kDefaultMaximumCorridorAreas = 256U;
	static constexpr std::size_t kDefaultMaximumSearchQueue = 4096U;

	std::size_t maximumCorridorAreas;
	std::size_t maximumSearchQueue;
};

enum class NavQueryResult
{
	Found,
	EmptySnapshot,
	InvalidArgument,
	InvalidPosition,
	InvalidTolerance,
	AreaNotFound,
	NoAreaContaining,
	NoRoute,
	ResourceLimit
};

enum class NavRouteType
{
	Fastest,
	Safest
};

struct NavAreaMatch
{
	AreaId area;
	float distanceSquared;
	NavVector closestPoint;
};

struct NavDirectedLink
{
	AreaId fromArea;
	AreaId toArea;
	std::uint8_t direction;
	std::uint8_t how;
};

struct NavCorridor
{
	std::uint64_t navRevision;
	std::uint32_t mapGeneration;
	NavRouteType routeType;
	float cost;
	std::vector<AreaId> areas;
	std::vector<NavDirectedLink> links;

	bool isValid() const;
};

class NavQuery
{
public:
	explicit NavQuery(const NavSnapshot &snapshot);
	NavQuery(const NavSnapshot &snapshot, const NavQueryLimits &limits);

	NavQueryResult findContaining(
		const NavVector &position,
		float floorTolerance,
		NavAreaMatch *match) const;
	NavQueryResult findNearest(
		const NavVector &position,
		float maximumDistance,
		NavAreaMatch *match) const;
	NavQueryResult outgoingLinks(
		AreaId fromArea,
		std::vector<NavDirectedLink> *links) const;
	NavQueryResult buildCorridor(
			AreaId start,
			AreaId goal,
			NavCorridor *corridor) const;
	NavQueryResult buildCorridor(
			AreaId start,
			AreaId goal,
			NavRouteType routeType,
			NavCorridor *corridor) const;

private:
	const NavDocument *document() const;
	static std::size_t findDiscoveredArea(
		const std::vector<AreaId> &areas,
		AreaId id);

	NavSnapshot snapshot_;
	NavQueryLimits limits_;
};

enum class NavFollowerResult
{
	Started,
	TargetReady,
	Advanced,
	Reached,
	Inactive,
	InvalidArgument,
	InvalidSnapshot,
	InvalidCorridor,
	InvalidPosition,
	InvalidTolerance,
	StaleSnapshot,
	ResourceLimit
};

class NavPathFollower
{
public:
	NavPathFollower();

	NavFollowerResult start(const NavCorridor &corridor);
	NavFollowerResult update(
		const NavSnapshot &snapshot,
		const NavVector &position,
		float horizontalTolerance,
		float verticalTolerance,
		NavVector *target,
		AreaId *targetArea);
	bool isActive() const;
	std::size_t currentIndex() const;

private:
	std::vector<AreaId> corridor_;
	std::vector<NavDirectedLink> links_;
	std::uint64_t navRevision_;
	std::uint32_t mapGeneration_;
	std::size_t currentIndex_;
	bool active_;
};
}
}

#endif
