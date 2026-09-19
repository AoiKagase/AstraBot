#ifndef ASTRABOT_NAV_NAV_MODEL_HPP
#define ASTRABOT_NAV_NAV_MODEL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace astrabot
{
namespace nav
{
using AreaId = std::uint32_t;

enum class NavModelResult
{
	Accepted,
	EmptyDocument,
	InvalidGeometry,
	InvalidArea,
	DuplicateArea,
	DuplicateHidingSpot,
	InvalidReference,
	InvalidPlace,
	DuplicatePlace,
	InvalidEncounter,
	InvalidSourceIdentity,
	ResourceLimit
};

struct NavVector
{
	float x;
	float y;
	float z;
};

struct NavExtent
{
	NavVector lo;
	NavVector hi;
};

struct NavHidingSpot
{
	static constexpr std::uint8_t kInCover = 0x01U;

	std::uint32_t id;
	NavVector position;
	std::uint8_t flags;
};

struct NavApproach
{
	AreaId here;
	AreaId previous;
	AreaId next;
	std::uint8_t previousToHereHow;
	std::uint8_t hereToNextHow;
};

struct NavEncounterSpot
{
	std::uint32_t hidingSpotId;
	float t;
};

struct NavEncounter
{
	AreaId fromArea;
	std::uint8_t fromDirection;
	AreaId toArea;
	std::uint8_t toDirection;
	std::vector<NavEncounterSpot> spots;
};

struct NavArea
{
	static constexpr std::size_t kDirectionCount = 4U;

	AreaId id;
	std::uint8_t attributes;
	NavExtent extent;
	float northEastZ;
	float southWestZ;
	std::uint16_t placeId;
	std::array<std::vector<AreaId>, kDirectionCount> connections;
	std::vector<NavHidingSpot> hidingSpots;
	std::vector<NavApproach> approaches;
	std::vector<NavEncounter> encounters;
};

struct NavPlace
{
	static constexpr std::size_t kNameCapacity = 255U;

	std::uint16_t id;
	char name[kNameCapacity + 1U];
};

struct NavSourceIdentity
{
	std::uint32_t sourceVersion;
	std::uint32_t bspSize;
	std::uint32_t contentHash;
};

struct NavLimits
{
	static constexpr std::size_t kMaximumAreas = 65535U;
	static constexpr std::size_t kMaximumConnectionsPerDirection = 64U;
	static constexpr std::uint8_t kTraverseTypeCount = 8U;
	static constexpr std::size_t kMaximumHidingSpotsPerArea = 255U;
	static constexpr std::size_t kMaximumApproachesPerArea = 16U;
	static constexpr std::size_t kMaximumEncountersPerArea = 4096U;
	static constexpr std::size_t kMaximumEncounterSpots = 255U;
	static constexpr std::size_t kMaximumPlaces = 1024U;
};

class NavDocument
{
public:
	NavDocument();

	NavModelResult setSourceIdentity(const NavSourceIdentity &identity);
	NavModelResult addPlace(const NavPlace &place);
	NavModelResult addArea(const NavArea &area);
	NavModelResult validate() const;

	const NavSourceIdentity &sourceIdentity() const;
	const NavArea *findArea(AreaId id) const;
	const std::vector<NavArea> &areas() const;
	const std::vector<NavPlace> &places() const;
	std::size_t areaCount() const;
	std::size_t placeCount() const;

private:
	static bool isFiniteVector(const NavVector &value);
	static bool isValidName(const char *name);
	static bool isValidArea(const NavArea &area);
	static bool isValidPlace(const NavPlace &place);
	static bool hasHidingSpot(const std::vector<NavArea> &areas, std::uint32_t id);

	std::vector<NavArea> areas_;
	std::vector<NavPlace> places_;
	NavSourceIdentity sourceIdentity_;
};
}
}

#endif
