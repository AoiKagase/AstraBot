#include "astrabot/build_identity.hpp"

#include <cassert>
#include <cstring>

int main()
{
	assert(astrabot::build::pointerBits() == 32U);
	assert(astrabot::build::cxxStandard() == 14U);
	assert(std::strcmp(astrabot::build::architectureName(), "x86") == 0);
	assert(std::strlen(astrabot::build::version()) > 0U);

	return 0;
}
