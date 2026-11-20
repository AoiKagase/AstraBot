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
	// Maximum number of areas retained by the completed corridor.
	static constexpr std::size_t kDefaultMaximumCorridorAreas = 256U;
	// Independent watchdog for A* search records/open-list growth.
	static constexpr std::size_t kDefaultMaximumSearchQueue = 4096U;

	std::size_t maximumCorridorAreas;
	std::size_t maximumSearchQueue;
};

struct NavSearchStats
{
	std::uint32_t expandedUniqueAreas;
	std::uint32_t enqueueCount;
	std::uint32_t reopenCount;
	std::uint32_t staleQueueEntries;
	std::uint32_t equalCostReplacements;
	std::uint32_t searchCalls;
	std::uint32_t successCount;
	std::uint32_t failureCount;
	std::uint64_t totalUsec;
	std::uint64_t maxUsec;
	std::uint64_t firstSearchId;
	std::uint64_t lastSearchId;
	AreaId firstStartArea;
	AreaId firstGoalArea;
	AreaId lastStartArea;
	AreaId lastGoalArea;
	std::uint8_t firstRouteType;
	std::uint8_t lastRouteType;
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

// Per-bot public observations, never server-wide hidden knowledge.
struct NavAreaCostState
{
	AreaId area;
	float danger;
	std::uint32_t teammates;
};

// Runtime topology rebuilt from public map observations; never serialized to NAV.
struct NavLadder
{
	NavVector top{};
	NavVector bottom{};
	float length = 0.0f;
	std::uint8_t direction = 0U;
	AreaId bottomArea = 0U;
	// Forward, left, right, behind in native enumeration order.
	std::array<AreaId, 4> topAreas{};
	bool dangling = false;
	std::uint32_t entityIndex = 0U;
	std::uint32_t entityGeneration = 0U;
};

struct NavLadderTopology
{
	std::uint64_t navRevision = 0U;
	std::uint32_t mapGeneration = 0U;
	std::vector<NavLadder> ladders;
};

struct NavPathCostContext
{
	float aggression = 0.0f;
	float health = 100.0f;
	std::uint32_t hostageEscortCount = 0U;
	bool attacking = false;
	bool zombie = false;
	std::vector<NavAreaCostState> areas;
	bool useExtentCenters = false;
};

struct NavDirectedLink
{
	AreaId fromArea;
	AreaId toArea;
	std::uint8_t direction;
	std::uint8_t how;
};

float surfaceZAt(const NavArea &area, float x, float y);
NavVector extentCenterOf(const NavArea &area);

struct NavLadderStep
{
	std::size_t linkIndex = 0;
	NavLadder ladder{};
};

struct NavHuntWaypoint
{
	NavVector position;
	std::size_t areaIndex;
};

struct NavCorridor
{
	std::uint64_t navRevision;
	std::uint32_t mapGeneration;
	NavRouteType routeType;
	float cost;
	std::vector<AreaId> areas;
	std::vector<NavDirectedLink> links;
	// Compatibility uses CSBot's explicit 3D path endpoint. Legacy callers
	// and Enhanced retain area/portal arrival unless they opt in.
	bool hasEndpoint = false;
	NavVector endpoint = {};
	// Explicit non-ground links; callers must split these before using the ground follower.
	std::vector<NavLadderStep> nativeLadders;
	bool huntEndpointSteering = false;
	std::vector<NavHuntWaypoint> huntWaypoints;

	bool isValid() const;
};

NavVector portalSteeringPointForLink(
	const NavArea &fromArea, const NavArea &toArea, const NavVector &position, std::uint8_t direction);

class NavQuery
{
public:
	explicit NavQuery(const NavSnapshot &snapshot);
	NavQuery(const NavSnapshot &snapshot, const NavQueryLimits &limits);
	void setPathCostContext(const NavPathCostContext &context);

	NavQueryResult findContaining(const NavVector &position, float floorTolerance, NavAreaMatch *match) const;
	NavQueryResult findNearest(const NavVector &position, float maximumDistance, NavAreaMatch *match) const;
	NavQueryResult findNearest3D(const NavVector &position, float maximumDistance, NavAreaMatch *match) const;
	// A read-only native range query. Ladder edges are never handed to the ground follower.
	NavQueryResult travelDistance(
		AreaId start, AreaId goal, NavRouteType routeType, const NavLadderTopology &ladders, float *distance) const;
	NavQueryResult buildNativeCorridor(AreaId start, AreaId goal, NavRouteType routeType,
		const NavLadderTopology &ladders, NavCorridor *corridor, NavSearchStats *stats = nullptr) const;
	NavQueryResult outgoingLinks(AreaId fromArea, std::vector<NavDirectedLink> *links) const;
	NavQueryResult buildCorridor(AreaId start, AreaId goal, NavCorridor *corridor) const;
	NavQueryResult buildCorridor(AreaId start, AreaId goal, NavCorridor *corridor, NavSearchStats *stats) const;
	NavQueryResult buildCorridor(AreaId start, AreaId goal, NavRouteType routeType, NavCorridor *corridor) const;
	NavQueryResult buildCorridor(
		AreaId start, AreaId goal, NavRouteType routeType, NavCorridor *corridor, NavSearchStats *stats) const;
	// Keep the nearest reachable area when a Hunt target cannot be reached.
	NavQueryResult buildHuntCorridor(
		AreaId start, AreaId goal, NavCorridor *corridor, NavSearchStats *stats) const;
	NavQueryResult buildInvestigationCorridor(AreaId start, AreaId goal, const NavVector &noisePosition,
		const NavLadderTopology *ladders, NavCorridor *corridor, NavSearchStats *stats = nullptr,
		const NavDirectedLink *excludedFirstLink = nullptr) const;

	NavQueryResult buildAlternativeCorridor(const NavCorridor &currentCorridor, const NavDirectedLink &failedFirstLink,
		NavCorridor *alternativeCorridor, NavSearchStats *stats = nullptr, AreaId huntGoal = 0U) const;

private:
	const NavDocument *document() const;
	static std::size_t findDiscoveredArea(const std::vector<AreaId> &areas, AreaId id);

	NavSnapshot snapshot_;
	NavQueryLimits limits_;
	NavPathCostContext costContext_;
	bool hasCostContext_ = false;
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

class NavHuntPathProbe
{
public:
	virtual ~NavHuntPathProbe() = default;
	virtual bool clear(const NavVector &feet, const NavVector &point, bool allowDoors) const = 0;
};

struct NavHuntSteeringContext
{
	bool crouched = false;
	AreaId currentArea = 0U;
	const NavHuntPathProbe *probe = nullptr;
	bool forcedRunning = false;
};

class NavPathFollower
{
public:
	NavPathFollower();

	NavFollowerResult start(const NavCorridor &corridor);
	NavFollowerResult update(const NavSnapshot &snapshot, const NavVector &position, float horizontalTolerance,
		float verticalTolerance, NavVector *target, AreaId *targetArea, NavHuntSteeringContext hunt = {});
	bool isActive() const;
	std::size_t currentIndex() const;
	bool walkingToHuntEndpoint() const { return huntWalking_; }

private:
	NavFollowerResult updateHuntPath(const NavDocument &document, const NavVector &feet,
		NavHuntSteeringContext hunt, NavVector &target, AreaId &targetArea);

	std::vector<AreaId> corridor_;
	std::vector<NavDirectedLink> links_;
	std::vector<NavHuntWaypoint> huntWaypoints_;
	std::size_t huntWaypointIndex_ = 0U;
	NavVector huntTarget_{};
	std::uint64_t navRevision_;
	std::uint32_t mapGeneration_;
	std::size_t currentIndex_;
	bool active_;
	bool hasEndpoint_ = false;
	bool huntEndpointSteering_ = false;
	bool huntWalking_ = false;
	NavVector endpoint_ = {};
};
}
}

#endif
