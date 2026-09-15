#include "astrabot/build_identity.hpp"

#ifndef ASTRABOT_CXX_STANDARD
#define ASTRABOT_CXX_STANDARD 14
#endif

#ifndef ASTRABOT_VERSION_STRING
#define ASTRABOT_VERSION_STRING "0.1.0"
#endif

namespace astrabot
{
namespace build
{

unsigned pointerBits() noexcept
{
	return static_cast<unsigned>(sizeof(void *) * 8U);
}

unsigned cxxStandard() noexcept
{
	return ASTRABOT_CXX_STANDARD;
}

const char *architectureName() noexcept
{
	return "x86";
}

const char *version() noexcept
{
	return ASTRABOT_VERSION_STRING;
}

} // namespace build
} // namespace astrabot
