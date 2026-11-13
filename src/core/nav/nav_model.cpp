#include "astrabot/nav/nav_model.hpp"

#include <cmath>
#include <cstring>
#include <new>
#include <utility>

namespace astrabot
{
namespace nav
{
namespace
{
bool isDirectionValid(std::uint8_t direction)
{
	return direction < NavArea::kDirectionCount;
}

bool isTraverseTypeValid(std::uint8_t traverseType)
{
	return traverseType < NavLimits::kTraverseTypeCount;
}
}

NavDocument::NavDocument() : areas_(), areaIndex_(), places_(), sourceIdentity_{0U, 0U, 0U}

{
}

NavDocument &NavDocument::operator=(const NavDocument &other)
{
	if (this != &other)
	{
		// Allocate all copied storage before committing either areas or index.
		// LegacyNavReader can recover from bad_alloc and retain this document.
		NavDocument candidate(other);
		*this = std::move(candidate);
	}
	return *this;
}

NavModelResult NavDocument::setSourceIdentity(const NavSourceIdentity &identity)
{
	if (identity.sourceVersion == 0U)
	{
		return NavModelResult::InvalidSourceIdentity;
	}
	sourceIdentity_ = identity;
	return NavModelResult::Accepted;
}

NavModelResult NavDocument::addPlace(const NavPlace &place)
{
	if (!isValidPlace(place))
	{
		return NavModelResult::InvalidPlace;
	}
	for (const NavPlace &candidate : places_)
	{
		if (candidate.id == place.id)
		{
			return NavModelResult::DuplicatePlace;
		}
	}
	if (places_.size() >= NavLimits::kMaximumPlaces)
	{
		return NavModelResult::ResourceLimit;
	}
	try
	{
		places_.push_back(place);
	}
	catch (const std::bad_alloc &)
	{
		return NavModelResult::ResourceLimit;
	}
	return NavModelResult::Accepted;
}

NavModelResult NavDocument::addArea(const NavArea &area)
{
	for (const std::vector<AreaId> &connections : area.connections)
	{
		if (connections.size() > NavLimits::kMaximumConnectionsPerDirection)
		{
			return NavModelResult::ResourceLimit;
		}
	}
	if (area.hidingSpots.size() > NavLimits::kMaximumHidingSpotsPerArea ||
		area.approaches.size() > NavLimits::kMaximumApproachesPerArea ||
		area.encounters.size() > NavLimits::kMaximumEncountersPerArea)
	{
		return NavModelResult::ResourceLimit;
	}
	if (!isValidArea(area))
	{
		if (area.id == 0U)
		{
			return NavModelResult::InvalidArea;
		}
		return NavModelResult::InvalidGeometry;
	}
	if (findArea(area.id) != nullptr)
	{
		return NavModelResult::DuplicateArea;
	}
	if (areas_.size() >= NavLimits::kMaximumAreas)
	{
		return NavModelResult::ResourceLimit;
	}
	try
	{
		const auto inserted = areaIndex_.emplace(area.id, areas_.size());
		try
		{
			areas_.push_back(area);
		}
		catch (const std::bad_alloc &)
		{
			areaIndex_.erase(inserted.first);
			return NavModelResult::ResourceLimit;
		}
	}
	catch (const std::bad_alloc &)
	{
		return NavModelResult::ResourceLimit;
	}
	return NavModelResult::Accepted;
}

NavModelResult NavDocument::validate() const
{
	if (areas_.empty())
	{
		return NavModelResult::EmptyDocument;
	}
	for (const NavPlace &place : places_)
	{
		if (!isValidPlace(place))
		{
			return NavModelResult::InvalidPlace;
		}
	}
	for (const NavArea &area : areas_)
	{
		if (!isValidArea(area))
		{
			return NavModelResult::InvalidGeometry;
		}
		if (area.placeId != 0U)
		{
			bool placeFound = false;
			for (const NavPlace &place : places_)
			{
				if (place.id == area.placeId)
				{
					placeFound = true;
					break;
				}
			}
			if (!placeFound)
			{
				return NavModelResult::InvalidReference;
			}
		}
		for (const std::vector<AreaId> &connections : area.connections)
		{
			for (const AreaId target : connections)
			{
				if (findArea(target) == nullptr)
				{
					return NavModelResult::InvalidReference;
				}
			}
		}
		for (const NavApproach &approach : area.approaches)
		{
			if (findArea(approach.here) == nullptr ||
				(approach.previous != 0U && findArea(approach.previous) == nullptr) ||
				(approach.next != 0U && findArea(approach.next) == nullptr))
			{
				return NavModelResult::InvalidReference;
			}
		}
		for (const NavEncounter &encounter : area.encounters)
		{
			if (findArea(encounter.fromArea) == nullptr || findArea(encounter.toArea) == nullptr)
			{
				return NavModelResult::InvalidReference;
			}
			for (const NavEncounterSpot &spot : encounter.spots)
			{
				if (!hasHidingSpot(areas_, spot.hidingSpotId))
				{
					return NavModelResult::InvalidReference;
				}
			}
		}
	}
	for (std::size_t areaIndex = 0U; areaIndex < areas_.size(); ++areaIndex)
	{
		for (std::size_t spotIndex = 0U; spotIndex < areas_[areaIndex].hidingSpots.size(); ++spotIndex)
		{
			for (std::size_t otherAreaIndex = 0U; otherAreaIndex <= areaIndex; ++otherAreaIndex)
			{
				const std::size_t firstSpot = otherAreaIndex == areaIndex ? spotIndex + 1U : 0U;
				for (std::size_t otherSpotIndex = firstSpot; otherSpotIndex < areas_[otherAreaIndex].hidingSpots.size();
					 ++otherSpotIndex)
				{
					if (areas_[areaIndex].hidingSpots[spotIndex].id ==
						areas_[otherAreaIndex].hidingSpots[otherSpotIndex].id)
					{
						return NavModelResult::DuplicateHidingSpot;
					}
				}
			}
		}
	}
	return NavModelResult::Accepted;
}

const NavSourceIdentity &NavDocument::sourceIdentity() const
{
	return sourceIdentity_;
}

const NavArea *NavDocument::findArea(AreaId id) const
{
	const auto found = areaIndex_.find(id);
	return found != areaIndex_.end() && found->second < areas_.size() ? &areas_[found->second] : nullptr;
}

const std::vector<NavArea> &NavDocument::areas() const
{
	return areas_;
}

const std::vector<NavPlace> &NavDocument::places() const
{
	return places_;
}

std::size_t NavDocument::areaCount() const
{
	return areas_.size();
}

std::size_t NavDocument::placeCount() const
{
	return places_.size();
}

bool NavDocument::isFiniteVector(const NavVector &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool NavDocument::isValidName(const char *name)
{
	if (name == nullptr || name[0] == '\0')
	{
		return false;
	}
	for (std::size_t index = 0U; index <= NavPlace::kNameCapacity; ++index)
	{
		if (name[index] == '\0')
		{
			return true;
		}
	}
	return false;
}

bool NavDocument::isValidArea(const NavArea &area)
{
	if (area.id == 0U || !isFiniteVector(area.extent.lo) || !isFiniteVector(area.extent.hi) ||
		!std::isfinite(area.northEastZ) || !std::isfinite(area.southWestZ) || area.extent.lo.x >= area.extent.hi.x ||
		area.extent.lo.y >= area.extent.hi.y)
	{
		return false;
	}
	for (const std::vector<AreaId> &connections : area.connections)
	{
		if (connections.size() > NavLimits::kMaximumConnectionsPerDirection)
		{
			return false;
		}
		for (std::size_t index = 0U; index < connections.size(); ++index)
		{
			bool duplicate = false;
			for (std::size_t previous = 0U; previous < index; ++previous)
			{
				if (connections[previous] == connections[index])
				{
					duplicate = true;
					break;
				}
			}
			if (connections[index] == 0U || duplicate)
			{
				return false;
			}
		}
	}
	if (area.hidingSpots.size() > NavLimits::kMaximumHidingSpotsPerArea ||
		area.approaches.size() > NavLimits::kMaximumApproachesPerArea ||
		area.encounters.size() > NavLimits::kMaximumEncountersPerArea)
	{
		return false;
	}
	for (const NavHidingSpot &spot : area.hidingSpots)
	{
		if (!isFiniteVector(spot.position))
		{
			return false;
		}
	}
	for (const NavApproach &approach : area.approaches)
	{
		if (approach.here == 0U || !isTraverseTypeValid(approach.previousToHereHow) ||
			!isTraverseTypeValid(approach.hereToNextHow))
		{
			return false;
		}
	}
	for (const NavEncounter &encounter : area.encounters)
	{
		if (encounter.fromArea == 0U || encounter.toArea == 0U || !isDirectionValid(encounter.fromDirection) ||
			!isDirectionValid(encounter.toDirection) || encounter.spots.size() > NavLimits::kMaximumEncounterSpots)
		{
			return false;
		}
		for (const NavEncounterSpot &spot : encounter.spots)
		{
			if (!std::isfinite(spot.t) || spot.t < 0.0f || spot.t > 1.0f)
			{
				return false;
			}
		}
	}
	return true;
}

bool NavDocument::isValidPlace(const NavPlace &place)
{
	return place.id != 0U && isValidName(place.name);
}

bool NavDocument::hasHidingSpot(const std::vector<NavArea> &areas, std::uint32_t id)
{
	for (const NavArea &area : areas)
	{
		for (const NavHidingSpot &spot : area.hidingSpots)
		{
			if (spot.id == id)
			{
				return true;
			}
		}
	}
	return false;
}
}
}
