#include "astrabot/metamod/map_fingerprint.hpp"

#include <cstring>

namespace astrabot
{
namespace metamod
{
namespace
{
bool isWhitespace(char value)
{
	return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool equalsBspSuffix(const char *value, std::size_t length)
{
	if (length < 4U)
	{
		return false;
	}
	return (value[length - 4U] == '.' &&
			(value[length - 3U] == 'b' || value[length - 3U] == 'B') &&
			(value[length - 2U] == 's' || value[length - 2U] == 'S') &&
			(value[length - 1U] == 'p' || value[length - 1U] == 'P'));
}

char toLowerAscii(char value)
{
	if (value >= 'A' && value <= 'Z')
	{
		return static_cast<char>(value + ('a' - 'A'));
	}
	return value;
}

bool normalizeMapName(const char *input, char *output)
{
	if (input == nullptr || output == nullptr)
	{
		return false;
	}

	std::size_t length = 0U;
	while (length < MapFingerprint::kMapNameCapacity + 1U && input[length] != '\0')
	{
		++length;
	}
	if (length == MapFingerprint::kMapNameCapacity + 1U)
	{
		return false;
	}

	std::size_t begin = 0U;
	while (begin < length && isWhitespace(input[begin]))
	{
		++begin;
	}
	while (length > begin && isWhitespace(input[length - 1U]))
	{
		--length;
	}
	if (equalsBspSuffix(input + begin, length - begin))
	{
		length -= 4U;
		while (length > begin && isWhitespace(input[length - 1U]))
		{
			--length;
		}
	}
	if (length == begin || length - begin > MapFingerprint::kMapNameCapacity)
	{
		return false;
	}

	char normalized[MapFingerprint::kMapNameCapacity + 1U] = {};
	const std::size_t normalizedLength = length - begin;
	for (std::size_t index = 0U; index < normalizedLength; ++index)
	{
		const char value = input[begin + index];
		if (value == '/' || value == '\\' || value == ':')
		{
			return false;
		}
		normalized[index] = toLowerAscii(value);
	}
	normalized[normalizedLength] = '\0';
	std::memcpy(output, normalized, sizeof(normalized));
	return true;
}
}

MapFingerprintResult createMapFingerprint(const char *mapName,
	std::uint32_t bspSize, std::uint32_t navSourceHash,
	MapFingerprint *fingerprint)
{
	if (mapName == nullptr || fingerprint == nullptr)
	{
		return MapFingerprintResult::InvalidArgument;
	}

	MapFingerprint candidate = {};
	if (!normalizeMapName(mapName, candidate.mapName))
	{
		return MapFingerprintResult::InvalidMapName;
	}
	candidate.bspSize = bspSize;
	candidate.navSourceHash = navSourceHash;
	*fingerprint = candidate;
	return MapFingerprintResult::Created;
}

MapFingerprintResult compareMapFingerprints(const MapFingerprint *expected,
	const MapFingerprint *actual)
{
	if (expected == nullptr || actual == nullptr)
	{
		return MapFingerprintResult::InvalidArgument;
	}
	if (std::strncmp(expected->mapName, actual->mapName,
			MapFingerprint::kMapNameCapacity + 1U) != 0)
	{
		return MapFingerprintResult::MapNameMismatch;
	}
	if (expected->bspSize != actual->bspSize)
	{
		return MapFingerprintResult::BspSizeMismatch;
	}
	if (expected->navSourceHash != actual->navSourceHash)
	{
		return MapFingerprintResult::SourceHashMismatch;
	}
	return MapFingerprintResult::Matched;
}
}
}
