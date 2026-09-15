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
		for (std::size_t shift = 0U; shift < 32U; shift += 8U)
		{
			bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
		}
	}

	void appendFloat(float value)
	{
		std::uint32_t bits = 0U;
		std::memcpy(&bits, &value, sizeof(bits));
		appendU32(bits);
	}

	std::vector<std::uint8_t> bytes;
};

std::vector<std::uint8_t> makeNav(std::uint32_t version)
{
	Bytes data;
	data.appendU32(kNavMagic);
	data.appendU32(version);
	if (version >= 4U)
	{
		data.appendU32(kBspSize);
	}
	if (version >= 5U)
	{
		data.appendU16(0U);
	}
	data.appendU32(1U);
	data.appendU32(1U);
	data.appendU8(0U);
	for (std::size_t index = 0U; index < 8U; ++index)
	{
		data.appendFloat(index < 3U ? 0.0f : 64.0f);
	}
	for (std::size_t direction = 0U; direction < 4U; ++direction)
	{
		data.appendU32(0U);
	}
	data.appendU8(0U);
	data.appendU8(0U);
	data.appendU32(0U);
	if (version >= 5U)
	{
		data.appendU16(0U);
	}
	return data.bytes;
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

std::vector<std::uint8_t> readFile(const char *path)
{
	std::vector<std::uint8_t> bytes;
	std::FILE *file = openFile(path, "rb");
	if (file == nullptr)
	{
		return bytes;
	}
	std::uint8_t buffer[256U] = {};
	while (true)
	{
		const std::size_t count = std::fread(buffer, 1U, sizeof(buffer), file);
		bytes.insert(bytes.end(), buffer, buffer + count);
		if (count < sizeof(buffer))
		{
			break;
		}
	}
	std::fclose(file);
	return bytes;
}
}

int main()
{
	using astrabot::metamod::NavLoadDiagnostic;
	using astrabot::metamod::NavLoadRequest;
	using astrabot::metamod::NavLoadResult;
	using astrabot::metamod::NavLoader;
	using astrabot::nav::NavSnapshotPublisher;

	NavLoader loader;
	NavSnapshotPublisher publisher;
	NavLoadDiagnostic diagnostic = {};
	NavLoadRequest request = {
		kNavPath, "DE_DUST2.BSP", 7U, true, kBspSize, false, 0U
	};
	for (std::uint32_t version = 1U; version <= 5U; ++version)
	{
		const std::vector<std::uint8_t> original = makeNav(version);
		if (!check(writeFile(kNavPath, original), "valid nav fixture is written"))
		{
			return 1;
		}
		if (!check(loader.loadFile(&request, &publisher, &diagnostic) ==
				NavLoadResult::Loaded, "valid v1-v5 file loads"))
		{
			return 1;
		}
		if (!check(diagnostic.mapGeneration == 7U &&
				std::strcmp(diagnostic.mapName, "de_dust2") == 0,
				"diagnostic retains normalized map identity"))
		{
			return 1;
		}
		if (!check(readFile(kNavPath) == original,
				"loader does not modify source bytes"))
		{
			return 1;
		}
	}

	const auto previous = publisher.snapshot();
	request.navPath = "missing.nav";
	if (!check(loader.loadFile(&request, &publisher, &diagnostic) ==
			NavLoadResult::MissingFile, "missing nav file is explicit"))
	{
		return 1;
	}
	if (!check(publisher.isCurrent(previous),
			"missing file preserves prior snapshot"))
	{
		return 1;
	}

	request.navPath = kNavPath;
	std::vector<std::uint8_t> corrupt = makeNav(5U);
	corrupt.resize(8U);
	if (!check(writeFile(kNavPath, corrupt), "corrupt fixture is written"))
	{
		return 1;
	}
	if (!check(loader.loadFile(&request, &publisher, &diagnostic) ==
			NavLoadResult::Truncated, "truncated nav file is explicit"))
	{
		return 1;
	}
	if (!check(publisher.isCurrent(previous),
			"corrupt file preserves prior snapshot"))
	{
		return 1;
	}

	NavLoader smallLoader({8U});
	const std::vector<std::uint8_t> valid = makeNav(5U);
	if (!check(writeFile(kNavPath, valid), "oversized fixture is written"))
	{
		return 1;
	}
	if (!check(smallLoader.loadFile(&request, &publisher, &diagnostic) ==
			NavLoadResult::TooLarge, "oversized nav file is explicit"))
	{
		return 1;
	}

	request.navPath = "de_inferno.nav";
	if (!check(writeFile(request.navPath, valid), "map mismatch fixture is written"))
	{
		return 1;
	}
	if (!check(loader.loadFile(&request, &publisher, &diagnostic) ==
			NavLoadResult::MapNameMismatch, "map mismatch is explicit"))
	{
		return 1;
	}
	request.navPath = kNavPath;
	request.bspSize = 8192U;
	if (!check(writeFile(kNavPath, makeNav(5U)), "BSP mismatch fixture is written"))
	{
		return 1;
	}
	if (!check(loader.loadFile(&request, &publisher, &diagnostic) ==
			NavLoadResult::BspSizeMismatch, "BSP mismatch is explicit"))
	{
		return 1;
	}

	std::remove(kNavPath);
	std::remove("de_inferno.nav");
	return 0;
}
