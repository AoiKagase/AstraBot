#ifndef ASTRABOT_NAV_LEGACY_NAV_READER_HPP
#define ASTRABOT_NAV_LEGACY_NAV_READER_HPP

#include "astrabot/nav/nav_model.hpp"

#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace nav
{
struct LegacyNavReaderLimits
{
	std::size_t maxBytes;
	std::size_t maxAreas;
};

enum class NavReadResult
{
	Loaded,
	InvalidArgument,
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
	ResourceLimit,
	TrailingData
};

class LegacyNavReader
{
public:
	static constexpr std::uint32_t kMagicNumber = 0xFEEDFACEU;
	static constexpr std::uint32_t kMaximumVersion = 5U;
	static constexpr std::size_t kMaximumFileBytes = 64U * 1024U * 1024U;

	LegacyNavReader();
	explicit LegacyNavReader(const LegacyNavReaderLimits &limits);

	NavReadResult read(
		const std::uint8_t *data,
		std::size_t size,
		NavDocument *document) const;

private:
	LegacyNavReaderLimits limits_;
};
}
}

#endif
