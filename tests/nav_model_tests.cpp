#include "astrabot/nav/nav_model.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

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

astrabot::nav::NavArea area(std::uint32_t id)
{
	astrabot::nav::NavArea result = {};
	result.id = id;
	result.extent.lo = {0.0f, 0.0f, 0.0f};
	result.extent.hi = {64.0f, 64.0f, 0.0f};
	result.northEastZ = 0.0f;
	result.southWestZ = 0.0f;
	return result;
}

astrabot::nav::NavPlace place(std::uint16_t id, const char *name)
{
	astrabot::nav::NavPlace result = {};
	result.id = id;
	const std::size_t length = std::strlen(name);
	const std::size_t copyLength =
		length < astrabot::nav::NavPlace::kNameCapacity ?
		length : astrabot::nav::NavPlace::kNameCapacity;
	for (std::size_t index = 0U; index < copyLength; ++index)
	{
		result.name[index] = name[index];
	}
	result.name[copyLength] = '\0';
	return result;
}
}

int main()
{
	using astrabot::nav::NavDocument;
	using astrabot::nav::NavHidingSpot;
	using astrabot::nav::NavModelResult;
	using astrabot::nav::NavPlace;

	NavDocument document;
	astrabot::nav::NavSourceIdentity identity = {5U, 1234U, 0xABCD1234U};
	if (!check(document.setSourceIdentity(identity) == NavModelResult::Accepted,
			"source identity is accepted"))
	{
		return 1;
	}
	if (!check(document.addPlace(place(1U, "Bombsite A")) == NavModelResult::Accepted,
			"place is accepted"))
	{
		return 1;
	}
	if (!check(document.addPlace(place(1U, "Duplicate")) ==
			NavModelResult::DuplicatePlace, "duplicate places are rejected"))
	{
		return 1;
	}

	astrabot::nav::NavArea first = area(1U);
	first.connections[1U].push_back(2U);
	first.hidingSpots.push_back({7U, {16.0f, 16.0f, 0.0f}, NavHidingSpot::kInCover});
	first.approaches.push_back({2U, 0U, 2U, 7U, 4U});
	first.encounters.push_back({1U, 1U, 2U, 3U, {{7U, 0.5f}}});
	astrabot::nav::NavArea second = area(2U);
	if (!check(document.addArea(first) == NavModelResult::Accepted &&
			document.addArea(second) == NavModelResult::Accepted,
			"valid areas are accepted"))
	{
		return 1;
	}
	if (!check(document.validate() == NavModelResult::Accepted &&
			document.areaCount() == 2U && document.findArea(2U) != nullptr,
			"valid geometry and directed references validate"))
	{
		return 1;
	}
	if (!check(document.addArea(second) == NavModelResult::DuplicateArea,
			"duplicate area IDs are rejected"))
	{
		return 1;
	}
	NavDocument duplicateSpotDocument;
	astrabot::nav::NavArea firstSpotArea = area(20U);
	firstSpotArea.hidingSpots.push_back({1U, {8.0f, 8.0f, 0.0f}, 0U});
	astrabot::nav::NavArea secondSpotArea = area(21U);
	secondSpotArea.hidingSpots.push_back({1U, {72.0f, 8.0f, 0.0f}, 0U});
	if (!check(duplicateSpotDocument.addArea(firstSpotArea) == NavModelResult::Accepted &&
			duplicateSpotDocument.addArea(secondSpotArea) == NavModelResult::Accepted &&
			duplicateSpotDocument.validate() == NavModelResult::DuplicateHidingSpot,
			"hiding spot IDs are globally unique"))
	{
		return 1;
	}

	astrabot::nav::NavArea degenerate = area(3U);
	degenerate.extent.hi.x = degenerate.extent.lo.x;
	if (!check(document.addArea(degenerate) == NavModelResult::InvalidGeometry,
			"degenerate extents are rejected"))
	{
		return 1;
	}
	astrabot::nav::NavArea nonFinite = area(4U);
	nonFinite.extent.lo.z = std::nanf("");
	if (!check(document.addArea(nonFinite) == NavModelResult::InvalidGeometry,
			"non-finite geometry is rejected"))
	{
		return 1;
	}
	astrabot::nav::NavArea tooManyConnections = area(5U);
	for (std::size_t index = 0U;
			index <= astrabot::nav::NavLimits::kMaximumConnectionsPerDirection;
			++index)
	{
		tooManyConnections.connections[0U].push_back(100U + index);
	}
	if (!check(document.addArea(tooManyConnections) == NavModelResult::ResourceLimit,
			"connection limits are enforced"))
	{
		return 1;
	}

	NavDocument invalidReferenceDocument;
	astrabot::nav::NavArea invalidReference = area(10U);
	invalidReference.connections[0U].push_back(99U);
	if (!check(invalidReferenceDocument.addArea(invalidReference) ==
			NavModelResult::Accepted &&
			invalidReferenceDocument.validate() == NavModelResult::InvalidReference,
			"unknown directed references fail validation"))
	{
		return 1;
	}
	if (!check(NavDocument().validate() == NavModelResult::EmptyDocument,
			"empty documents are explicit"))
	{
		return 1;
	}

	NavDocument copied = document;
	if (!check(copied.areaCount() == document.areaCount() &&
			copied.findArea(1U) != nullptr,
			"Nav documents can be handed off by value"))
	{
		return 1;
	}
	return 0;
}
