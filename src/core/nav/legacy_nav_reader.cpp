#include "astrabot/nav/legacy_nav_reader.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <new>

namespace astrabot
{
namespace nav
{
namespace
{
class ByteCursor
{
public:
	ByteCursor(const std::uint8_t *data, std::size_t size) :
		data_(data), size_(size), position_(0U)
	{
	}

	bool readU8(std::uint8_t *value)
	{
		if (value == nullptr || !canRead(1U))
		{
			return false;
		}
		*value = data_[position_++];
		return true;
	}

	bool readU16(std::uint16_t *value)
	{
		if (value == nullptr || !canRead(2U))
		{
			return false;
		}
		*value = static_cast<std::uint16_t>(data_[position_]) |
			static_cast<std::uint16_t>(data_[position_ + 1U] << 8U);
		position_ += 2U;
		return true;
	}

	bool readU32(std::uint32_t *value)
	{
		if (value == nullptr || !canRead(4U))
		{
			return false;
		}
		*value = static_cast<std::uint32_t>(data_[position_]) |
			(static_cast<std::uint32_t>(data_[position_ + 1U]) << 8U) |
			(static_cast<std::uint32_t>(data_[position_ + 2U]) << 16U) |
			(static_cast<std::uint32_t>(data_[position_ + 3U]) << 24U);
		position_ += 4U;
		return true;
	}

	bool readFloat(float *value)
	{
		std::uint32_t bits = 0U;
		if (value == nullptr || !readU32(&bits))
		{
			return false;
		}
		std::memcpy(value, &bits, sizeof(bits));
		return true;
	}

	bool readBytes(void *output, std::size_t count)
	{
		if (output == nullptr || !canRead(count))
		{
			return false;
		}
		std::memcpy(output, data_ + position_, count);
		position_ += count;
		return true;
	}

	bool canRead(std::size_t count) const
	{
		return count <= size_ - position_;
	}

	bool atEnd() const
	{
		return position_ == size_;
	}

private:
	const std::uint8_t *data_;
	std::size_t size_;
	std::size_t position_;
};

std::uint32_t hashBytes(const std::uint8_t *data, std::size_t size)
{
	std::uint32_t hash = 2166136261U;
	for (std::size_t index = 0U; index < size; ++index)
	{
		hash ^= data[index];
		hash *= 16777619U;
	}
	return hash;
}

bool finiteVector(const NavVector &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
			std::isfinite(value.z);
}

enum class SectionReadResult
{
	Loaded,
	Truncated,
	Invalid,
	InvalidGeometry,
	ResourceLimit
};

NavReadResult mapModelResult(NavModelResult result)
{
	switch (result)
	{
	case NavModelResult::Accepted:
		return NavReadResult::Loaded;
	case NavModelResult::EmptyDocument:
	case NavModelResult::InvalidArea:
		return NavReadResult::InvalidCount;
	case NavModelResult::InvalidGeometry:
		return NavReadResult::InvalidGeometry;
	case NavModelResult::DuplicateArea:
	case NavModelResult::DuplicateHidingSpot:
	case NavModelResult::DuplicatePlace:
		return NavReadResult::DuplicateIdentity;
	case NavModelResult::InvalidReference:
		return NavReadResult::InvalidReference;
	case NavModelResult::InvalidPlace:
		return NavReadResult::InvalidPlace;
	case NavModelResult::InvalidEncounter:
		return NavReadResult::InvalidEncounter;
	case NavModelResult::InvalidSourceIdentity:
	case NavModelResult::ResourceLimit:
		return NavReadResult::ResourceLimit;
	default:
		return NavReadResult::InvalidArgument;
	}
}

bool readVector(ByteCursor *cursor, NavVector *value)
{
	return cursor != nullptr && value != nullptr && cursor->readFloat(&value->x) &&
			cursor->readFloat(&value->y) && cursor->readFloat(&value->z);
}

SectionReadResult readPlaceDirectory(ByteCursor *cursor, NavDocument *document)
{
	if (cursor == nullptr || document == nullptr)
	{
		return SectionReadResult::Invalid;
	}
	std::uint16_t count = 0U;
	if (!cursor->readU16(&count))
	{
		return SectionReadResult::Truncated;
	}
	if (count > NavLimits::kMaximumPlaces)
	{
		return SectionReadResult::ResourceLimit;
	}
	for (std::uint16_t index = 0U; index < count; ++index)
	{
		std::uint16_t length = 0U;
		if (!cursor->readU16(&length))
		{
			return SectionReadResult::Truncated;
		}
		if (length == 0U || length > NavPlace::kNameCapacity + 1U)
		{
			return SectionReadResult::Invalid;
		}
		std::array<char, NavPlace::kNameCapacity + 1U> name = {};
		if (!cursor->readBytes(name.data(), length))
		{
			return SectionReadResult::Truncated;
		}
		if (name[length - 1U] != '\0')
		{
			return SectionReadResult::Invalid;
		}
		NavPlace place = {};
		place.id = static_cast<std::uint16_t>(index + 1U);
		for (std::size_t character = 0U; character < length; ++character)
		{
			place.name[character] = name[character];
		}
		const NavModelResult result = document->addPlace(place);
		if (result != NavModelResult::Accepted)
		{
			return result == NavModelResult::ResourceLimit ?
				SectionReadResult::ResourceLimit : SectionReadResult::Invalid;
		}
	}
	return SectionReadResult::Loaded;
}

SectionReadResult readArea(ByteCursor *cursor, std::uint32_t version, std::uint32_t *nextSpotId,
		NavArea *area)
{
	if (cursor == nullptr || nextSpotId == nullptr || area == nullptr ||
			!cursor->readU32(&area->id) || !cursor->readU8(&area->attributes) ||
			!readVector(cursor, &area->extent.lo) ||
			!readVector(cursor, &area->extent.hi) ||
			!cursor->readFloat(&area->northEastZ) ||
			!cursor->readFloat(&area->southWestZ))
	{
		return SectionReadResult::Truncated;
	}
	for (std::size_t direction = 0U; direction < NavArea::kDirectionCount; ++direction)
	{
		std::uint32_t count = 0U;
		if (!cursor->readU32(&count))
		{
			return SectionReadResult::Truncated;
		}
		if (count > NavLimits::kMaximumConnectionsPerDirection)
		{
			return SectionReadResult::ResourceLimit;
		}
		area->connections[direction].reserve(count);
		for (std::uint32_t index = 0U; index < count; ++index)
		{
			AreaId target = 0U;
			if (!cursor->readU32(&target))
			{
				return SectionReadResult::Truncated;
			}
			area->connections[direction].push_back(target);
		}
	}
	std::uint8_t hidingCount = 0U;
	if (!cursor->readU8(&hidingCount))
	{
		return SectionReadResult::Truncated;
	}
	area->hidingSpots.reserve(hidingCount);
	for (std::uint8_t index = 0U; index < hidingCount; ++index)
	{
		NavHidingSpot spot = {};
		if (version == 1U)
		{
			spot.id = (*nextSpotId)++;
			spot.flags = NavHidingSpot::kInCover;
			if (!readVector(cursor, &spot.position))
			{
				return SectionReadResult::Truncated;
			}
		}
		else if (!cursor->readU32(&spot.id) || !readVector(cursor, &spot.position) ||
					!cursor->readU8(&spot.flags))
		{
			return SectionReadResult::Truncated;
		}
		area->hidingSpots.push_back(spot);
	}
	std::uint8_t approachCount = 0U;
	if (!cursor->readU8(&approachCount))
	{
		return SectionReadResult::Truncated;
	}
	if (approachCount > NavLimits::kMaximumApproachesPerArea)
	{
		return SectionReadResult::ResourceLimit;
	}
	area->approaches.reserve(approachCount);
	for (std::uint8_t index = 0U; index < approachCount; ++index)
	{
		NavApproach approach = {};
		if (!cursor->readU32(&approach.here) ||
				!cursor->readU32(&approach.previous) ||
				!cursor->readU8(&approach.previousToHereHow) ||
				!cursor->readU32(&approach.next) ||
				!cursor->readU8(&approach.hereToNextHow))
		{
			return SectionReadResult::Truncated;
		}
		area->approaches.push_back(approach);
	}
	std::uint32_t encounterCount = 0U;
	if (!cursor->readU32(&encounterCount))
	{
		return SectionReadResult::Truncated;
	}
	if (encounterCount > NavLimits::kMaximumEncountersPerArea)
	{
		return SectionReadResult::ResourceLimit;
	}
	area->encounters.reserve(encounterCount);
	for (std::uint32_t index = 0U; index < encounterCount; ++index)
	{
		NavEncounter encounter = {};
		if (version < 3U)
		{
			NavVector pathFrom = {};
			NavVector pathTo = {};
			if (!cursor->readU32(&encounter.fromArea) ||
					!cursor->readU32(&encounter.toArea) ||
					!readVector(cursor, &pathFrom) || !readVector(cursor, &pathTo))
			{
				return SectionReadResult::Truncated;
			}
			if (!finiteVector(pathFrom) || !finiteVector(pathTo))
			{
				return SectionReadResult::InvalidGeometry;
			}
			std::uint8_t spotCount = 0U;
			if (!cursor->readU8(&spotCount))
			{
				return SectionReadResult::Truncated;
			}
			for (std::uint8_t spot = 0U; spot < spotCount; ++spot)
			{
				NavVector position = {};
				float t = 0.0f;
				if (!readVector(cursor, &position) || !cursor->readFloat(&t))
				{
					return SectionReadResult::Truncated;
				}
				if (!finiteVector(position) || !std::isfinite(t))
				{
					return SectionReadResult::InvalidGeometry;
				}
			}
		}
		else
		{
			if (!cursor->readU32(&encounter.fromArea) ||
					!cursor->readU8(&encounter.fromDirection) ||
					!cursor->readU32(&encounter.toArea) ||
					!cursor->readU8(&encounter.toDirection))
			{
				return SectionReadResult::Truncated;
			}
			std::uint8_t spotCount = 0U;
			if (!cursor->readU8(&spotCount))
			{
				return SectionReadResult::Truncated;
			}
			encounter.spots.reserve(spotCount);
			for (std::uint8_t spot = 0U; spot < spotCount; ++spot)
			{
				NavEncounterSpot order = {};
				std::uint8_t t = 0U;
				if (!cursor->readU32(&order.hidingSpotId) || !cursor->readU8(&t))
				{
					return SectionReadResult::Truncated;
				}
				order.t = static_cast<float>(t) / 255.0f;
				encounter.spots.push_back(order);
			}
		}
		area->encounters.push_back(encounter);
	}
	if (version >= 5U && !cursor->readU16(&area->placeId))
	{
		return SectionReadResult::Truncated;
	}
	return SectionReadResult::Loaded;
}
}

LegacyNavReader::LegacyNavReader() :
	limits_{kMaximumFileBytes, NavLimits::kMaximumAreas}
{
}

LegacyNavReader::LegacyNavReader(const LegacyNavReaderLimits &limits) : limits_(limits)
{
	if (limits_.maxBytes > kMaximumFileBytes)
	{
		limits_.maxBytes = kMaximumFileBytes;
	}
	if (limits_.maxAreas > NavLimits::kMaximumAreas)
	{
		limits_.maxAreas = NavLimits::kMaximumAreas;
	}
}

NavReadResult LegacyNavReader::read(
	const std::uint8_t *data,
	std::size_t size,
	NavDocument *document) const
{
	if (document == nullptr || (data == nullptr && size != 0U))
	{
		return NavReadResult::InvalidArgument;
	}
	if (data == nullptr || size < sizeof(std::uint32_t) * 2U)
	{
		return NavReadResult::Truncated;
	}
	if (size > limits_.maxBytes || size > kMaximumFileBytes)
	{
		return NavReadResult::TooLarge;
	}

	try
	{
		ByteCursor cursor(data, size);
		std::uint32_t magic = 0U;
		std::uint32_t version = 0U;
		if (!cursor.readU32(&magic))
		{
			return NavReadResult::Truncated;
		}
		if (magic != kMagicNumber)
		{
			return NavReadResult::InvalidMagic;
		}
		if (!cursor.readU32(&version))
		{
			return NavReadResult::Truncated;
		}
		if (version == 0U || version > kMaximumVersion)
		{
			return NavReadResult::UnsupportedVersion;
		}
		std::uint32_t bspSize = 0U;
		if (version >= 4U && !cursor.readU32(&bspSize))
		{
			return NavReadResult::Truncated;
		}
		NavDocument candidate;
		if (version >= 5U)
		{
			const SectionReadResult placeResult = readPlaceDirectory(&cursor, &candidate);
			if (placeResult != SectionReadResult::Loaded)
			{
				if (placeResult == SectionReadResult::ResourceLimit)
				{
					return NavReadResult::ResourceLimit;
				}
				return placeResult == SectionReadResult::Truncated ?
					NavReadResult::Truncated : NavReadResult::InvalidPlace;
			}
		}
		std::uint32_t areaCount = 0U;
		if (!cursor.readU32(&areaCount))
		{
			return NavReadResult::Truncated;
		}
		if (areaCount == 0U)
		{
			return NavReadResult::InvalidCount;
		}
		if (areaCount > limits_.maxAreas || areaCount > NavLimits::kMaximumAreas)
		{
			return NavReadResult::ResourceLimit;
		}
		if (candidate.setSourceIdentity({version, bspSize, hashBytes(data, size)}) !=
				NavModelResult::Accepted)
		{
			return NavReadResult::ResourceLimit;
		}
		std::uint32_t nextSpotId = 1U;
		for (std::uint32_t index = 0U; index < areaCount; ++index)
		{
			NavArea area = {};
			const SectionReadResult sectionResult =
				readArea(&cursor, version, &nextSpotId, &area);
			if (sectionResult != SectionReadResult::Loaded)
			{
				return sectionResult == SectionReadResult::ResourceLimit ?
					NavReadResult::ResourceLimit :
					sectionResult == SectionReadResult::InvalidGeometry ?
					NavReadResult::InvalidGeometry : NavReadResult::Truncated;
			}
			const NavModelResult modelResult = candidate.addArea(area);
			if (modelResult != NavModelResult::Accepted)
			{
				return mapModelResult(modelResult);
			}
		}
		if (!cursor.atEnd())
		{
			return NavReadResult::TrailingData;
		}
		const NavReadResult validationResult = mapModelResult(candidate.validate());
		if (validationResult != NavReadResult::Loaded)
		{
			return validationResult;
		}
		*document = candidate;
		return NavReadResult::Loaded;
	}
	catch (const std::bad_alloc &)
	{
		return NavReadResult::ResourceLimit;
	}
}
}
}
