#include "astrabot/metamod/nav_loader.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
constexpr std::uint32_t kNavMagic = 0xFEEDFACEU;
constexpr std::uint32_t kBspSize = 4096U;
const char *kNavPath = "de_dust2.nav";

bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}

	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

void appendU32(std::vector<std::uint8_t> *bytes, std::uint32_t value)
{
	for (std::size_t shift = 0U; shift < 32U; shift += 8U)
	{
		bytes->push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
	}
}

void appendU16(std::vector<std::uint8_t> *bytes, std::uint16_t value)
{
	bytes->push_back(static_cast<std::uint8_t>(value & 0xFFU));
	bytes->push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void appendFloat(std::vector<std::uint8_t> *bytes, float value)
{
	std::uint32_t bits = 0U;
	std::memcpy(&bits, &value, sizeof(bits));
	appendU32(bytes, bits);
}

std::vector<std::uint8_t> validNav()
{
	std::vector<std::uint8_t> bytes;
	appendU32(&bytes, kNavMagic);
	appendU32(&bytes, 5U);
	appendU32(&bytes, kBspSize);
	appendU16(&bytes, 0U);
	appendU32(&bytes, 1U);
	appendU32(&bytes, 1U);
	bytes.push_back(0U);
	for (std::size_t index = 0U; index < 8U; ++index)
	{
		appendFloat(&bytes, index < 3U ? 0.0f : 64.0f);
	}
	for (std::size_t direction = 0U; direction < 4U; ++direction)
	{
		appendU32(&bytes, 0U);
	}
	bytes.push_back(0U);
	bytes.push_back(0U);
	appendU32(&bytes, 0U);
	appendU16(&bytes, 0U);
	return bytes;
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

bool writeFile(const char *path, const std::vector<std::uint8_t> &bytes)
{
	std::FILE *file = openFile(path, "wb");
	if (file == nullptr)
	{
		return false;
	}
	const std::size_t written = std::fwrite(bytes.data(), 1U, bytes.size(), file);
	return written == bytes.size() && std::fclose(file) == 0;
}
}

int main()
{
	using astrabot::metamod::NavLoadDiagnostic;
	using astrabot::metamod::NavLoadRequest;
	using astrabot::metamod::NavLoadResult;
	using astrabot::metamod::NavLoader;
	using astrabot::nav::NavSnapshotPublisher;

	if (!check(writeFile(kNavPath, validNav()), "diagnostic fixture is written"))
	{
		return 1;
	}
	NavLoader loader;
	NavSnapshotPublisher publisher;
	NavLoadDiagnostic diagnostic = {};
	NavLoadRequest request = {
		kNavPath, "de_dust2", 22U, true, kBspSize, false, 0U
	};
	if (!check(loader.loadFile(&request, &publisher, &diagnostic) ==
			NavLoadResult::Loaded, "diagnostic fixture publishes"))
	{
		return 1;
	}
	const auto previous = publisher.snapshot();
	if (!check(diagnostic.snapshotRevision == previous.revision() &&
			diagnostic.sourceHash != 0U && diagnostic.mapGeneration == 22U,
			"diagnostic contains publication identity"))
	{
		return 1;
	}

	request.hasExpectedSourceHash = true;
	request.expectedSourceHash = diagnostic.sourceHash + 1U;
	if (!check(loader.loadFile(&request, &publisher, &diagnostic) ==
			NavLoadResult::SourceHashMismatch, "source hash mismatch is explicit"))
	{
		return 1;
	}
	if (!check(publisher.isCurrent(previous),
			"source mismatch preserves prior snapshot"))
	{
		return 1;
	}

	if (!check(loader.loadFile(nullptr, &publisher, &diagnostic) ==
			NavLoadResult::InvalidArgument &&
			diagnostic.result == NavLoadResult::InvalidArgument,
			"invalid loader arguments are diagnosed"))
	{
		return 1;
	}
	std::remove(kNavPath);
	return 0;
}
