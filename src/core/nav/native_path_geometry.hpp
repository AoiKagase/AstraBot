#ifndef ASTRABOT_CORE_NAV_NATIVE_PATH_GEOMETRY_HPP
#define ASTRABOT_CORE_NAV_NATIVE_PATH_GEOMETRY_HPP

#include "astrabot/nav/nav_query.hpp"

namespace astrabot
{
namespace nav
{
NavVector nativeGroundPortal(const NavDocument &document, const NavDirectedLink &link,
	const NavVector &previous, bool bidirectional);
}
}

#endif
