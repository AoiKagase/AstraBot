#ifndef ASTRABOT_COMPAT_FOLLOW_NAV_TARGET_HPP
#define ASTRABOT_COMPAT_FOLLOW_NAV_TARGET_HPP

#include "astrabot/compat/surrounding_nav_areas.hpp"

namespace astrabot
{
namespace compat
{
inline std::vector<nav::AreaId> collectFollowAreas(const nav::NavSnapshot &snapshot, nav::AreaId start,
	const nav::NavVector &leader, const nav::NavVector &velocity, const nav::NavLadderTopology &ladders)
{
	std::vector<nav::AreaId> candidates;
	const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
	const bool filterDirection = speed >= 100.0f;
	const float dx = filterDirection ? velocity.x / speed : 0.0f;
	const float dy = filterDirection ? velocity.y / speed : 0.0f;
	const float trim = 1.5f * (std::min)(speed, 200.0f);
	const float cutoffX = leader.x + trim * dx;
	const float cutoffY = leader.y + trim * dy;
	visitSurroundingNavAreas(snapshot, start, leader, ladders, speed > 200.0f ? 600.0f : 400.0f,
		[&](const nav::NavArea &area, const nav::NavArea *parent)
		{
			if (parent)
			{
				const bool reverseFloor = std::any_of(area.connections.begin(), area.connections.end(),
					[&](const auto &direction)
					{
						return std::find(direction.begin(), direction.end(), parent->id) != direction.end();
					});
				const bool reverseLadder = !reverseFloor && std::any_of(ladders.ladders.begin(), ladders.ladders.end(),
					[&](const auto &ladder)
					{
						if (ladder.bottomArea != area.id && ladder.bottomArea != parent->id)
							return false;
						const auto top = ladder.bottomArea == area.id ? parent->id : area.id;
						return std::find(ladder.topAreas.begin(), ladder.topAreas.end(), top) != ladder.topAreas.end();
					});
				if (!reverseFloor && !reverseLadder)
					return true;
			}
			if (filterDirection)
			{
				const auto center = nav::extentCenterOf(area);
				const float x = center.x - cutoffX, y = center.y - cutoffY;
				const float length = std::sqrt(x * x + y * y);
				if (!(length > 0.0f && (x / length) * dx + (y / length) * dy > 0.7071f))
					return true;
			}
			candidates.push_back(area.id);
			return candidates.size() < 128U;
		});
	return candidates;
}

inline nav::AreaId closestFollowArea(const nav::NavDocument &document, const std::vector<nav::AreaId> &candidates,
	const nav::NavVector &follower)
{
	nav::AreaId selected = 0U;
	float closestSquared = 1.0e10f;
	for (const auto id : candidates)
	{
		const auto *area = document.findArea(id);
		if (!area)
			continue;
		const float x = (std::min)((std::max)(follower.x, area->extent.lo.x), area->extent.hi.x);
		const float y = (std::min)((std::max)(follower.y, area->extent.lo.y), area->extent.hi.y);
		const float dx = x - follower.x, dy = y - follower.y;
		const float dz = nav::surfaceZAt(*area, x, y) - follower.z;
		const float distanceSquared = dx * dx + dy * dy + dz * dz;
		if (distanceSquared < closestSquared)
		{
			closestSquared = distanceSquared;
			selected = id;
		}
	}
	return selected;
}
}
}

#endif
