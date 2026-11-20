#ifndef ASTRABOT_COMPAT_SURROUNDING_NAV_AREAS_HPP
#define ASTRABOT_COMPAT_SURROUNDING_NAV_AREAS_HPP

#include "astrabot/nav/nav_query.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace astrabot
{
namespace compat
{
template <typename AreaVisitor>
bool visitSurroundingNavAreas(const nav::NavSnapshot &snapshot, nav::AreaId start,
	const nav::NavVector &origin, const nav::NavLadderTopology &ladders, float range, AreaVisitor &&visitArea)
{
	const auto *document = snapshot.document();
	const auto *initialArea = document ? document->findArea(start) : nullptr;
	if (!initialArea)
		return true;
	if (ladders.navRevision != snapshot.revision() || ladders.mapGeneration != snapshot.mapGeneration())
		return false;
	struct Visit
	{
		const nav::NavArea *area;
		const nav::NavArea *parent;
		float traveled;
	};
	std::vector<Visit> pending{{initialArea, nullptr, 0.0f}};
	std::unordered_set<nav::AreaId> visited{start};
	for (std::size_t index = 0; index < pending.size(); ++index)
	{
		const auto visit = pending[index];
		const auto &area = *visit.area;
		if (!visitArea(area, visit.parent))
			break;
		const auto center = nav::extentCenterOf(area);
		const auto enqueue = [&](nav::AreaId id)
		{
			const auto *next = document->findArea(id);
			if (!next || !visited.insert(id).second)
				return;
			const float dx = (std::min)((std::max)(origin.x, next->extent.lo.x), next->extent.hi.x) - origin.x;
			const float dy = (std::min)((std::max)(origin.y, next->extent.lo.y), next->extent.hi.y) - origin.y;
			if (range > 0.0f && dx * dx + dy * dy >= range * range)
				return;
			const auto nextCenter = nav::extentCenterOf(*next);
			const float x = center.x - nextCenter.x, y = center.y - nextCenter.y, z = center.z - nextCenter.z;
			const float traveled = visit.traveled + std::sqrt(x * x + y * y + z * z);
			if (range <= 0.0f || traveled <= 1.5f * range)
				pending.push_back({next, &area, traveled});
		};
		for (const auto &direction : area.connections)
			for (const auto neighbor : direction)
				enqueue(neighbor);
		for (const auto &ladder : ladders.ladders)
			if (ladder.bottomArea == area.id && !ladder.dangling)
				for (std::size_t top = 0; top < 3; ++top)
					enqueue(ladder.topAreas[top]);
		for (const auto &ladder : ladders.ladders)
			if (std::find(ladder.topAreas.begin(), ladder.topAreas.end(), area.id) != ladder.topAreas.end())
				enqueue(ladder.bottomArea);
	}
	return true;
}
}
}

#endif
