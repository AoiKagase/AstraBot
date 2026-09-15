#ifndef ASTRABOT_METAMOD_PROFILE_LOADER_HPP
#define ASTRABOT_METAMOD_PROFILE_LOADER_HPP

#include "astrabot/compat/profile_catalog.hpp"

#include <cstddef>

namespace astrabot
{
namespace metamod
{
struct ProfileLoadLimits
{
	std::size_t maxBytes;
	std::size_t maxProfiles;
};

enum class ProfileLoadResult
{
	Loaded,
	InvalidArgument,
	MissingFile,
	ReadError,
	TooLarge,
	Empty,
	Malformed,
	DuplicateProfile,
	CatalogFull
};

class ProfileLoader
{
public:
	static constexpr std::size_t kMaximumFileBytes = 65536U;
	static constexpr std::size_t kMaximumTokenLength = 64U;

	ProfileLoader();
	explicit ProfileLoader(const ProfileLoadLimits &limits);

	ProfileLoadResult loadFile(
		const char *path,
		compat::ProfileCatalog *catalog) const;
	ProfileLoadResult loadText(
		const char *data,
		std::size_t size,
		compat::ProfileCatalog *catalog) const;

private:
	ProfileLoadLimits limits_;
};
}
}

#endif
