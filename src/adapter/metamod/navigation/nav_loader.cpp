#include "astrabot/metamod/nav_loader.hpp"

#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

namespace astrabot
{
namespace metamod
{
namespace
{
bool boundedStringLength(const char *value, std::size_t maximum,
	std::size_t *length)
{
	if (value == nullptr || length == nullptr)
	{
		return false;
	}
	for (std::size_t index = 0U; index < maximum; ++index)
	{
		if (value[index] == '\0')
		{
			*length = index;
			return true;
		}
	}
	return false;
}

bool equalsSuffixIgnoreCase(const char *value, std::size_t length,
	const char *suffix)
{
	std::size_t suffixLength = 0U;
	if (!boundedStringLength(suffix, 16U, &suffixLength) ||
			length < suffixLength)
	{
		return false;
	}
	for (std::size_t index = 0U; index < suffixLength; ++index)
	{
		char left = value[length - suffixLength + index];
		char right = suffix[index];
		if (left >= 'A' && left <= 'Z')
		{
			left = static_cast<char>(left + ('a' - 'A'));
		}
		if (right >= 'A' && right <= 'Z')
		{
			right = static_cast<char>(right + ('a' - 'A'));
		}
		if (left != right)
		{
			return false;
		}
	}
	return true;
}

bool extractNavMapName(const char *path, char *name)
{
	std::size_t pathLength = 0U;
	if (!boundedStringLength(path, NavLoader::kMaximumPathLength, &pathLength) ||
			name == nullptr)
	{
		return false;
	}

	std::size_t begin = 0U;
	for (std::size_t index = 0U; index < pathLength; ++index)
	{
		if (path[index] == '/' || path[index] == '\\')
		{
			begin = index + 1U;
		}
	}
	std::size_t end = pathLength;
	if (equalsSuffixIgnoreCase(path + begin, end - begin, ".nav"))
	{
		end -= 4U;
	}
	if (end <= begin || end - begin > MapFingerprint::kMapNameCapacity)
	{
		return false;
	}

	char candidate[MapFingerprint::kMapNameCapacity + 1U] = {};
	std::memcpy(candidate, path + begin, end - begin);
	candidate[end - begin] = '\0';
	std::memcpy(name, candidate, sizeof(candidate));
	return true;
}

std::FILE *openFile(const char *path, const char *mode)
{
	std::FILE *file = nullptr;
#ifdef _WIN32
	if (fopen_s(&file, path, mode) != 0)
	{
		return nullptr;
	}
#else
	file = std::fopen(path, mode);
#endif
	return file;
}

NavLoadResult mapReaderResult(nav::NavReadResult result)
{
	switch (result)
	{
	case nav::NavReadResult::Loaded:
		return NavLoadResult::Loaded;
	case nav::NavReadResult::InvalidArgument:
		return NavLoadResult::InvalidArgument;
	case nav::NavReadResult::TooLarge:
		return NavLoadResult::TooLarge;
	case nav::NavReadResult::Truncated:
		return NavLoadResult::Truncated;
	case nav::NavReadResult::InvalidMagic:
		return NavLoadResult::InvalidMagic;
	case nav::NavReadResult::UnsupportedVersion:
		return NavLoadResult::UnsupportedVersion;
	case nav::NavReadResult::InvalidCount:
		return NavLoadResult::InvalidCount;
	case nav::NavReadResult::InvalidGeometry:
		return NavLoadResult::InvalidGeometry;
	case nav::NavReadResult::DuplicateIdentity:
		return NavLoadResult::DuplicateIdentity;
	case nav::NavReadResult::InvalidReference:
		return NavLoadResult::InvalidReference;
	case nav::NavReadResult::InvalidPlace:
		return NavLoadResult::InvalidPlace;
	case nav::NavReadResult::InvalidEncounter:
		return NavLoadResult::InvalidEncounter;
	case nav::NavReadResult::ResourceLimit:
		return NavLoadResult::ResourceLimit;
	case nav::NavReadResult::TrailingData:
		return NavLoadResult::TrailingData;
	}
	return NavLoadResult::InvalidArgument;
}

NavLoadResult finish(NavLoadDiagnostic *diagnostic, NavLoadResult result)
{
	diagnostic->result = result;
	return result;
}

void initializeDiagnostic(const NavLoadRequest *request,
	NavLoadDiagnostic *diagnostic)
{
	*diagnostic = {};
	diagnostic->result = NavLoadResult::InvalidArgument;
	diagnostic->readerResult = nav::NavReadResult::InvalidArgument;
	diagnostic->snapshotResult = nav::NavSnapshotResult::InvalidArgument;
	if (request != nullptr)
	{
		diagnostic->mapGeneration = request->mapGeneration;
	}
}
}

NavLoader::NavLoader() : limits_{nav::LegacyNavReader::kMaximumFileBytes}, reader_()
{
}

NavLoader::NavLoader(const NavLoadLimits &limits) : limits_(limits), reader_()
{
	if (limits_.maxFileBytes > nav::LegacyNavReader::kMaximumFileBytes)
	{
		limits_.maxFileBytes = nav::LegacyNavReader::kMaximumFileBytes;
	}
}

NavLoadResult NavLoader::loadFile(const NavLoadRequest *request,
	nav::NavSnapshotPublisher *publisher, NavLoadDiagnostic *diagnostic) const
{
	if (diagnostic == nullptr)
	{
		return NavLoadResult::InvalidArgument;
	}
	initializeDiagnostic(request, diagnostic);
	if (request == nullptr || publisher == nullptr || request->navPath == nullptr ||
			request->mapName == nullptr)
	{
		return finish(diagnostic, NavLoadResult::InvalidArgument);
	}

	MapFingerprint currentMap = {};
	const MapFingerprintResult mapResult = createMapFingerprint(
		request->mapName, 0U, 0U, &currentMap);
	if (mapResult == MapFingerprintResult::InvalidMapName)
	{
		return finish(diagnostic, NavLoadResult::InvalidMapName);
	}
	if (mapResult != MapFingerprintResult::Created)
	{
		return finish(diagnostic, NavLoadResult::InvalidArgument);
	}
	std::memcpy(diagnostic->mapName, currentMap.mapName,
		sizeof(diagnostic->mapName));
	std::size_t pathLength = 0U;
	if (!boundedStringLength(request->navPath, kMaximumPathLength, &pathLength) ||
			pathLength == 0U)
	{
		return finish(diagnostic, NavLoadResult::InvalidPath);
	}
	diagnostic->bspSize = request->hasBspSize ? request->bspSize : 0U;

	std::FILE *file = openFile(request->navPath, "rb");
	if (file == nullptr)
	{
		return finish(diagnostic, NavLoadResult::MissingFile);
	}
	std::vector<std::uint8_t> bytes;
	try
	{
		bytes.resize(limits_.maxFileBytes + 1U);
	}
	catch (const std::bad_alloc &)
	{
		std::fclose(file);
		return finish(diagnostic, NavLoadResult::ResourceLimit);
	}
	const std::size_t bytesRead = std::fread(bytes.data(), 1U, bytes.size(), file);
	const bool readFailed = std::ferror(file) != 0;
	const int closeResult = std::fclose(file);
	if (readFailed || closeResult != 0)
	{
		return finish(diagnostic, NavLoadResult::ReadError);
	}
	if (bytesRead > limits_.maxFileBytes)
	{
		return finish(diagnostic, NavLoadResult::TooLarge);
	}
	if (bytesRead == 0U)
	{
		return finish(diagnostic, NavLoadResult::EmptyFile);
	}
	bytes.resize(bytesRead);

	nav::NavDocument candidate;
	diagnostic->readerResult = reader_.read(bytes.data(), bytes.size(), &candidate);
	if (diagnostic->readerResult != nav::NavReadResult::Loaded)
	{
		return finish(diagnostic, mapReaderResult(diagnostic->readerResult));
	}
	const nav::NavSourceIdentity source = candidate.sourceIdentity();
	diagnostic->sourceHash = source.contentHash;

	char navMapName[MapFingerprint::kMapNameCapacity + 1U] = {};
	if (!extractNavMapName(request->navPath, navMapName))
	{
		return finish(diagnostic, NavLoadResult::InvalidPath);
	}
	MapFingerprint navMap = {};
	if (createMapFingerprint(navMapName, 0U, 0U, &navMap) !=
			MapFingerprintResult::Created)
	{
		return finish(diagnostic, NavLoadResult::InvalidPath);
	}
	if (compareMapFingerprints(&currentMap, &navMap) !=
			MapFingerprintResult::Matched)
	{
		return finish(diagnostic, NavLoadResult::MapNameMismatch);
	}

	if (source.sourceVersion >= 4U)
	{
		if (!request->hasBspSize)
		{
			return finish(diagnostic, NavLoadResult::BspMetadataUnavailable);
		}
		MapFingerprint expectedBsp = {};
		MapFingerprint actualBsp = {};
		createMapFingerprint(request->mapName, request->bspSize, 0U, &expectedBsp);
		createMapFingerprint(navMapName, source.bspSize, 0U, &actualBsp);
		if (compareMapFingerprints(&expectedBsp, &actualBsp) ==
				MapFingerprintResult::BspSizeMismatch)
		{
			diagnostic->bspSize = request->bspSize;
			return finish(diagnostic, NavLoadResult::BspSizeMismatch);
		}
	}

	if (request->hasExpectedSourceHash &&
			source.contentHash != request->expectedSourceHash)
	{
		return finish(diagnostic, NavLoadResult::SourceHashMismatch);
	}

	diagnostic->snapshotResult = publisher->publish(&candidate,
		request->mapGeneration);
	if (diagnostic->snapshotResult != nav::NavSnapshotResult::Published)
	{
		if (diagnostic->snapshotResult == nav::NavSnapshotResult::ResourceLimit)
		{
			return finish(diagnostic, NavLoadResult::ResourceLimit);
		}
		return finish(diagnostic, NavLoadResult::PublicationFailed);
	}
	diagnostic->snapshotRevision = publisher->snapshot().revision();
	return finish(diagnostic, NavLoadResult::Loaded);
}
}
}
