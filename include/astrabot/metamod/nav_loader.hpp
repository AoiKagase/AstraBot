#ifndef ASTRABOT_METAMOD_NAV_LOADER_HPP
#define ASTRABOT_METAMOD_NAV_LOADER_HPP

#include "astrabot/metamod/map_fingerprint.hpp"
#include "astrabot/nav/legacy_nav_reader.hpp"
#include "astrabot/nav/nav_snapshot.hpp"

#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace metamod
{
struct NavLoadLimits
{
	std::size_t maxFileBytes;
};

struct NavLoadRequest
{
	const char *navPath;
	const char *mapName;
	std::uint32_t mapGeneration;
	bool hasBspSize;
	std::uint32_t bspSize;
	bool hasExpectedSourceHash;
	std::uint32_t expectedSourceHash;
};

enum class NavLoadResult
{
	Loaded,
	InvalidArgument,
	InvalidPath,
	InvalidMapName,
	MissingFile,
	ReadError,
	EmptyFile,
	TooLarge,
	Truncated,
	InvalidMagic,
	UnsupportedVersion,
	InvalidCount,
	InvalidGeometry,
	DuplicateIdentity,
	InvalidReference,
	InvalidPlace,
	InvalidEncounter,
	TrailingData,
	ResourceLimit,
	MapNameMismatch,
	BspSizeMismatch,
	BspMetadataUnavailable,
	SourceHashMismatch,
	PublicationFailed,
	BoundaryUnavailable
};

struct NavLoadDiagnostic
{
	NavLoadResult result;
	nav::NavReadResult readerResult;
	nav::NavSnapshotResult snapshotResult;
	char mapName[MapFingerprint::kMapNameCapacity + 1U];
	std::uint32_t mapGeneration;
	std::uint32_t bspSize;
	std::uint32_t sourceHash;
	std::uint64_t snapshotRevision;
};

class NavLoader
{
public:
	static constexpr std::size_t kMaximumPathLength = 260U;

	NavLoader();
	explicit NavLoader(const NavLoadLimits &limits);

	NavLoadResult loadFile(const NavLoadRequest *request,
		nav::NavSnapshotPublisher *publisher,
		NavLoadDiagnostic *diagnostic) const;

private:
	NavLoadLimits limits_;
	nav::LegacyNavReader reader_;
};
}
}

#endif
