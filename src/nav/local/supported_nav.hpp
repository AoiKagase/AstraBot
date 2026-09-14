// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/ground_probe.hpp"
#include <algorithm>
#include <cmath>

namespace astrabot::nav::local
{
// Call only after physical floor validation. This returns an advisory NAV area,
// never a replacement floor height or permission to omit the hull passage.
inline query::NavQueryResult supportedNavArea(const query::NavSpatialIndex& index, model::NavVector3 support,
											  const runtime::HullDimensions& hull, GroundProbeLimits limits,
											  model::NavAreaId expectedFallback = {}) noexcept
{
	(void)expectedFallback;
	auto strict = index.containing(support, limits.navTolerance);
	if (!strict || *strict.value)
	{
		return strict;
	}
	const double vertical = (std::max)(limits.maxStepUp, limits.maxDrop) + limits.navTolerance;
	// An area exists at this XY but disagrees vertically: do not "repair" a
	// floor mismatch by selecting a different neighboring NAV surface.
	const auto sameXY = index.containing(support, vertical);
	if (!sameXY)
	{
		return sameXY; // Preserve query failure separately from a successful miss.
	}
	if (*sameXY.value)
	{
		return strict;
	}
	const double halfX = (std::max)(std::abs(double(hull.minimum.x)), std::abs(double(hull.maximum.x)));
	const double halfY = (std::max)(std::abs(double(hull.minimum.y)), std::abs(double(hull.maximum.y)));
	// A standing hull can straddle a NAV seam or a micro patch. Physical
	// support and swept-hull checks remain mandatory; this tolerance only
	// retains the nearest advisory area.
	const double lateralTolerance = (std::max)(34.0, (std::max)(halfX, halfY) + (std::max)(limits.navTolerance, 18.0));
	// nearestGeometry's radius is 3D; constrain each XY component again below.
	const auto nearby =
		index.nearestGeometry(support, {std::hypot(lateralTolerance, lateralTolerance, vertical), vertical});
	if (!nearby)
	{
		return nearby;
	}
	if (!*nearby.value)
	{
		return strict;
	}
	const auto& candidate = **nearby.value;
	// expectedFallback identifies the route's previous area, but it is not a
	// containment requirement. At NAV seams (especially narrow corridors)
	// the hull can be physically supported by the adjacent area while its
	// center is just outside the previous rectangle. The caller still
	// validates route reachability before accepting the transition.
	const auto& p = candidate.projectedPoint;
	if (std::abs(p.x - support.x) > lateralTolerance || std::abs(p.y - support.y) > lateralTolerance ||
		std::abs(p.z - support.z) > vertical)
	{
		return strict;
	}
	return nearby;
}
} // namespace astrabot::nav::local
