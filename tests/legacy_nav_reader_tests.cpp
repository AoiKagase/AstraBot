#include "astrabot/nav/legacy_nav_reader.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
class Bytes
{
public:
	void appendU8(std::uint8_t value)
	{
		bytes.push_back(value);
	}

	void appendU16(std::uint16_t value)
	{
		bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
		bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
	}

	void appendU32(std::uint32_t value)
	{
		for (std::size_t index = 0U; index < 4U; ++index)
		{
			bytes.push_back(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
		}
	}

	void appendFloat(float value)
	{
		std::uint32_t bits = 0U;
		std::memcpy(&bits, &value, sizeof(bits));
		appendU32(bits);
	}

	void appendBytes(const char *data, std::size_t size)
	{
		for (std::size_t index = 0U; index < size; ++index)
		{
			bytes.push_back(static_cast<std::uint8_t>(data[index]));
		}
	}

	std::vector<std::uint8_t> bytes;
};

bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}

	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

void appendArea(
	Bytes *output,
	std::uint32_t version,
	std::uint32_t id,
	bool invalidConnection,
	bool invalidGeometry,
	bool invalidConnectionCount,
	bool firstArea)
{
	output->appendU32(id);
	output->appendU8(firstArea ? 1U : 0U);
	output->appendFloat(0.0f);
	output->appendFloat(0.0f);
	output->appendFloat(0.0f);
	output->appendFloat(invalidGeometry && firstArea ? 0.0f : 64.0f);
	output->appendFloat(64.0f);
	output->appendFloat(0.0f);
	output->appendFloat(0.0f);
	output->appendFloat(0.0f);
	for (std::size_t direction = 0U; direction < 4U; ++direction)
	{
		if (firstArea && direction == 1U)
		{
			output->appendU32(invalidConnectionCount ?
				static_cast<std::uint32_t>(
					astrabot::nav::NavLimits::kMaximumConnectionsPerDirection + 1U) :
				1U);
			if (!invalidConnectionCount)
			{
				output->appendU32(invalidConnection ? 99U : 2U);
			}
		}
		else
		{
			output->appendU32(0U);
		}
	}
	output->appendU8(1U);
	if (version == 1U)
	{
		output->appendFloat(16.0f);
		output->appendFloat(16.0f);
		output->appendFloat(0.0f);
	}
	else
	{
		output->appendU32(firstArea ? 7U : 8U);
		output->appendFloat(16.0f);
		output->appendFloat(16.0f);
		output->appendFloat(0.0f);
		output->appendU8(1U);
	}
	output->appendU8(firstArea ? 1U : 0U);
	if (firstArea)
	{
		output->appendU32(2U);
		output->appendU32(1U);
		output->appendU8(1U);
		output->appendU32(2U);
		output->appendU8(2U);
	}
	output->appendU32(firstArea ? 1U : 0U);
	if (firstArea)
	{
		if (version < 3U)
		{
			output->appendU32(1U);
			output->appendU32(2U);
			for (std::size_t index = 0U; index < 6U; ++index)
			{
				output->appendFloat(static_cast<float>(index));
			}
			output->appendU8(1U);
			output->appendFloat(8.0f);
			output->appendFloat(8.0f);
			output->appendFloat(0.0f);
			output->appendFloat(0.5f);
		}
		else
		{
			output->appendU32(1U);
			output->appendU8(1U);
			output->appendU32(2U);
			output->appendU8(3U);
			output->appendU8(1U);
			output->appendU32(7U);
			output->appendU8(128U);
		}
	}
	if (version >= 5U)
	{
		output->appendU16(firstArea ? 1U : 0U);
	}
}

std::vector<std::uint8_t> fixture(
	std::uint32_t version,
	bool invalidConnection = false,
	bool invalidGeometry = false,
	bool invalidPlace = false,
	bool duplicateArea = false,
	bool invalidConnectionCount = false)
{
	Bytes output;
	output.appendU32(0xFEEDFACEU);
	output.appendU32(version);
	if (version >= 4U && version <= 5U)
	{
		output.appendU32(1234U);
	}
	if (version == 5U)
	{
		output.appendU16(1U);
		if (invalidPlace)
		{
			output.appendU16(0U);
		}
		else
		{
			const char placeName[] = "Bombsite A";
			output.appendU16(static_cast<std::uint16_t>(sizeof(placeName)));
			output.appendBytes(placeName, sizeof(placeName));
		}
	}
	output.appendU32(2U);
	appendArea(&output, version, 1U, invalidConnection, invalidGeometry,
		invalidConnectionCount, true);
	appendArea(&output, version, duplicateArea ? 1U : 2U, false, false, false, false);
	return output.bytes;
}
}

int main()
{
	using astrabot::nav::LegacyNavReader;
	using astrabot::nav::LegacyNavReaderLimits;
	using astrabot::nav::NavDocument;
	using astrabot::nav::NavHidingSpot;
	using astrabot::nav::NavLimits;
	using astrabot::nav::NavReadResult;

	for (std::uint32_t version = 1U; version <= 5U; ++version)
	{
		const std::vector<std::uint8_t> bytes = fixture(version);
		const std::vector<std::uint8_t> original = bytes;
		NavDocument document;
		if (!check(LegacyNavReader().read(bytes.data(), bytes.size(), &document) ==
				NavReadResult::Loaded, "legacy version loads"))
		{
			return 1;
		}
		if (!check(document.areaCount() == 2U && document.sourceIdentity().sourceVersion == version,
				"loaded document keeps areas and source version"))
		{
			return 1;
		}
		if (version >= 4U && !check(document.sourceIdentity().bspSize == 1234U,
				"v4 source BSP size is retained"))
		{
			return 1;
		}
		if (version == 5U && !check(document.placeCount() == 1U &&
				document.areas()[0].placeId == 1U,
				"v5 place directory and area entry are normalized"))
		{
			return 1;
		}
		if (!check(document.areas()[0].hidingSpots.size() == 1U &&
				(version != 1U ? document.areas()[0].hidingSpots[0].id == 7U :
					document.areas()[0].hidingSpots[0].id == 1U) &&
				(version != 1U || (document.areas()[0].hidingSpots[0].flags &
					NavHidingSpot::kInCover) != 0U),
				"hiding spots are normalized by version"))
		{
			return 1;
		}
		if (version >= 3U && !check(document.areas()[0].encounters.size() == 1U &&
				document.areas()[0].encounters[0].spots[0].hidingSpotId == 7U,
				"v3 encounter spot IDs are retained"))
		{
			return 1;
		}
		if (!check(bytes == original, "reader does not modify input bytes"))
		{
			return 1;
		}
	}

	NavDocument existing;
	const std::vector<std::uint8_t> valid = fixture(5U);
	LegacyNavReader reader;
	if (!check(reader.read(valid.data(), valid.size(), &existing) == NavReadResult::Loaded,
			"valid document is available for transaction test"))
	{
		return 1;
	}
	std::vector<std::uint8_t> truncated = fixture(3U);
	truncated.pop_back();
	if (!check(reader.read(truncated.data(), truncated.size(), &existing) ==
			NavReadResult::Truncated && existing.areaCount() == 2U,
			"truncated read preserves previous document"))
	{
		return 1;
	}
	std::vector<std::uint8_t> badMagic = fixture(1U);
	badMagic[0] = 0U;
	if (!check(reader.read(badMagic.data(), badMagic.size(), &existing) ==
			NavReadResult::InvalidMagic, "bad magic is rejected"))
	{
		return 1;
	}
	const std::vector<std::uint8_t> badVersion = fixture(6U);
	if (!check(reader.read(badVersion.data(), badVersion.size(), &existing) ==
			NavReadResult::UnsupportedVersion, "unknown version is rejected"))
	{
		return 1;
	}
	if (!check(reader.read(valid.data(), valid.size(), nullptr) ==
			NavReadResult::InvalidArgument, "null output is rejected"))
	{
		return 1;
	}
	LegacyNavReader smallReader({8U, NavLimits::kMaximumAreas});
	if (!check(smallReader.read(valid.data(), valid.size(), &existing) ==
			NavReadResult::TooLarge, "file byte limit is enforced"))
	{
		return 1;
	}
	Bytes zeroCount;
	zeroCount.appendU32(0xFEEDFACEU);
	zeroCount.appendU32(1U);
	zeroCount.appendU32(0U);
	if (!check(reader.read(zeroCount.bytes.data(), zeroCount.bytes.size(), &existing) ==
			NavReadResult::InvalidCount, "zero area count is rejected"))
	{
		return 1;
	}
	Bytes hugeCount;
	hugeCount.appendU32(0xFEEDFACEU);
	hugeCount.appendU32(1U);
	hugeCount.appendU32(static_cast<std::uint32_t>(NavLimits::kMaximumAreas + 1U));
	if (!check(reader.read(hugeCount.bytes.data(), hugeCount.bytes.size(), &existing) ==
			NavReadResult::ResourceLimit, "area count limit is enforced"))
	{
		return 1;
	}
	const std::vector<std::uint8_t> tooManyConnections =
		fixture(1U, false, false, false, false, true);
	if (!check(reader.read(tooManyConnections.data(), tooManyConnections.size(), &existing) ==
			NavReadResult::ResourceLimit, "connection count limit is enforced"))
	{
		return 1;
	}
	const std::vector<std::uint8_t> invalidReference = fixture(5U, true);
	if (!check(reader.read(invalidReference.data(), invalidReference.size(), &existing) ==
			NavReadResult::InvalidReference, "unknown connection is rejected"))
	{
		return 1;
	}
	const std::vector<std::uint8_t> invalidGeometry = fixture(5U, false, true);
	if (!check(reader.read(invalidGeometry.data(), invalidGeometry.size(), &existing) ==
			NavReadResult::InvalidGeometry, "invalid geometry is rejected"))
	{
		return 1;
	}
	const std::vector<std::uint8_t> invalidPlace = fixture(5U, false, false, true);
	if (!check(reader.read(invalidPlace.data(), invalidPlace.size(), &existing) ==
			NavReadResult::InvalidPlace, "invalid place entry is rejected"))
	{
		return 1;
	}
	const std::vector<std::uint8_t> duplicateArea = fixture(5U, false, false, false, true);
	if (!check(reader.read(duplicateArea.data(), duplicateArea.size(), &existing) ==
			NavReadResult::DuplicateIdentity, "duplicate area IDs are rejected"))
	{
		return 1;
	}
	std::vector<std::uint8_t> trailing = fixture(5U);
	trailing.push_back(0U);
	if (!check(reader.read(trailing.data(), trailing.size(), &existing) ==
			NavReadResult::TrailingData, "trailing data is rejected"))
	{
		return 1;
	}
	return 0;
}
