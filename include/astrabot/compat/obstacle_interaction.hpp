#ifndef ASTRABOT_COMPAT_OBSTACLE_INTERACTION_HPP
#define ASTRABOT_COMPAT_OBSTACLE_INTERACTION_HPP
#include "astrabot/world/world_snapshot.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot
{
namespace compat
{
// Pinned BotTouch rejects only endpoints lying on the same outside box side.
// Preserve that conservative contract, rather than substituting exact ray/AABB geometry.
inline bool breakableBlocksPath(
	world::WorldPosition from, world::WorldPosition to, world::WorldPosition low, world::WorldPosition high)
{
	const float a[] = {from.x, from.y, from.z}, b[] = {to.x, to.y, to.z};
	const float lo[] = {low.x, low.y, low.z}, hi[] = {high.x, high.y, high.z};
	for (unsigned axis = 0; axis < 3; ++axis)
	{
		if (!std::isfinite(a[axis]) || !std::isfinite(b[axis]) || !std::isfinite(lo[axis]) ||
			!std::isfinite(hi[axis]) || lo[axis] > hi[axis] || (std::max)(a[axis], b[axis]) < lo[axis] ||
			(std::min)(a[axis], b[axis]) > hi[axis])
			return false;
	}
	return true;
}
}
}
#endif
