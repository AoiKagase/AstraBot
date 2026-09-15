#ifndef ASTRABOT_METAMOD_MAP_FINGERPRINT_HPP
#define ASTRABOT_METAMOD_MAP_FINGERPRINT_HPP

#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace metamod
{
struct MapFingerprint
{
	static constexpr std::size_t kMapNameCapacity = 63U;

	char mapName[kMapNameCapacity + 1U];
	std::uint32_t bspSize;
	std::uint32_t navSourceHash;
};

enum class MapFingerprintResult
{
	Created,
	Matched,
	InvalidArgument,
	InvalidMapName,
	MapNameMismatch,
	BspSizeMismatch,
	SourceHashMismatch
};

MapFingerprintResult createMapFingerprint(const char *mapName,
	std::uint32_t bspSize, std::uint32_t navSourceHash,
	MapFingerprint *fingerprint);
MapFingerprintResult compareMapFingerprints(const MapFingerprint *expected,
	const MapFingerprint *actual);
}
}

#endif
