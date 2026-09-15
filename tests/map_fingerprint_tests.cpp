#include "astrabot/metamod/map_fingerprint.hpp"

#include <cstdint>
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
}

int main()
{
	using astrabot::metamod::MapFingerprint;
	using astrabot::metamod::MapFingerprintResult;

	MapFingerprint first = {};
	MapFingerprint second = {};
	if (!check(astrabot::metamod::createMapFingerprint(
			"  DE_DUST2.BSP  ", 4096U, 0x12345678U, &first) ==
			MapFingerprintResult::Created, "map fingerprint is created"))
	{
		return 1;
	}
	if (!check(astrabot::metamod::createMapFingerprint(
			"de_dust2", 4096U, 0x12345678U, &second) ==
			MapFingerprintResult::Created, "normalized fingerprint is created"))
	{
		return 1;
	}
	if (!check(astrabot::metamod::compareMapFingerprints(&first, &second) ==
			MapFingerprintResult::Matched, "normalized map names match"))
	{
		return 1;
	}

	MapFingerprint differentMap = second;
	std::memcpy(differentMap.mapName, "de_inferno", sizeof("de_inferno"));
	if (!check(astrabot::metamod::compareMapFingerprints(
			&first, &differentMap) == MapFingerprintResult::MapNameMismatch,
			"map name mismatch is explicit"))
	{
		return 1;
	}

	MapFingerprint differentBsp = second;
	differentBsp.bspSize = 8192U;
	if (!check(astrabot::metamod::compareMapFingerprints(
			&first, &differentBsp) == MapFingerprintResult::BspSizeMismatch,
			"BSP mismatch is explicit"))
	{
		return 1;
	}

	MapFingerprint differentSource = second;
	differentSource.navSourceHash = 0x87654321U;
	if (!check(astrabot::metamod::compareMapFingerprints(
			&first, &differentSource) == MapFingerprintResult::SourceHashMismatch,
			"source hash mismatch is explicit"))
	{
		return 1;
	}

	char tooLong[MapFingerprint::kMapNameCapacity + 2U] = {};
	std::memset(tooLong, 'a', sizeof(tooLong) - 1U);
	MapFingerprint unchanged = first;
	if (!check(astrabot::metamod::createMapFingerprint(
			tooLong, 4096U, 0x12345678U, &unchanged) ==
			MapFingerprintResult::InvalidMapName, "oversized map name is rejected"))
	{
		return 1;
	}
	return check(std::strcmp(unchanged.mapName, first.mapName) == 0,
			"failed fingerprint creation preserves output") ? 0 : 1;
}
