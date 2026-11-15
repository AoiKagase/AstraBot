#ifndef ASTRABOT_COMPAT_NAV_AREA_LOOKUP_HPP
#define ASTRABOT_COMPAT_NAV_AREA_LOOKUP_HPP
#include "astrabot/compat/noise_navigation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace astrabot
{
namespace compat
{
inline bool botNavAreasOverlap(const nav::NavArea &a, const nav::NavArea &b)
{
	return b.extent.lo.x < a.extent.hi.x && b.extent.hi.x > a.extent.lo.x && b.extent.lo.y < a.extent.hi.y &&
		b.extent.hi.y > a.extent.lo.y;
}
inline bool botNavAreaContains(const nav::NavSnapshot &snapshot, nav::AreaId id, const nav::NavVector &position,
	const std::vector<nav::AreaId> *overlaps = nullptr)
{
	if (!snapshot.isValid() || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
		return false;
	const auto *area = snapshot.document()->findArea(id);
	if (!area || position.x < area->extent.lo.x || position.x > area->extent.hi.x || position.y < area->extent.lo.y ||
		position.y > area->extent.hi.y)
		return false;
	const float floor = nav::surfaceZAt(*area, position.x, position.y);
	if (floor > position.z)
		return false;
	const auto higherFloor = [&](const nav::NavArea &other)
	{
		if (other.id == id || !botNavAreasOverlap(*area, other) || position.x < other.extent.lo.x ||
			position.x > other.extent.hi.x || position.y < other.extent.lo.y || position.y > other.extent.hi.y)
			return false;
		const float z = nav::surfaceZAt(other, position.x, position.y);
		return z <= position.z && z > floor;
	};
	if (overlaps)
	{
		for (auto otherId : *overlaps)
		{
			const auto *other = snapshot.document()->findArea(otherId);
			if (other && higherFloor(*other))
				return false;
		}
	}
	else
		for (const auto &other : snapshot.document()->areas())
			if (higherFloor(other))
				return false;
	return true;
}
// Public NAV geometry and public probe outcomes only. Preserve loaded area order
// on equal heights/distances; a NAV-local ID is not an ordering or metadata key.
inline nav::AreaId findBotNavArea(
	const nav::NavSnapshot &snapshot, const nav::NavVector &position, INoiseNavProbe *probe, bool anyZ = false)
{
	if (!snapshot.isValid() || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
		return 0;
	const nav::NavArea *selected = nullptr;
	float floor = -99999999.9f;
	for (const auto &area : snapshot.document()->areas())
	{
		if (position.x < area.extent.lo.x || position.x > area.extent.hi.x || position.y < area.extent.lo.y ||
			position.y > area.extent.hi.y)
			continue;
		const float z = nav::surfaceZAt(area, position.x, position.y);
		if (z > position.z + 5 || z < position.z - 120 || z <= floor)
			continue;
		selected = &area;
		floor = z;
	}
	if (selected)
		return selected->id;
	float ground = 0;
	if (!probe || !probe->groundHeight(position, &ground) || !std::isfinite(ground))
		return 0;
	const nav::NavVector source{position.x, position.y, ground + 36};
	float nearest = 100000000.0f;
	for (const auto &area : snapshot.document()->areas())
	{
		const float x = (std::max)(area.extent.lo.x, (std::min)(source.x, area.extent.hi.x));
		const float y = (std::max)(area.extent.lo.y, (std::min)(source.y, area.extent.hi.y));
		const float z = nav::surfaceZAt(area, x, y);
		const float dx = x - source.x, dy = y - source.y, dz = z - source.z;
		const float distance = dx * dx + dy * dy + dz * dz;
		if (distance < nearest && (anyZ || probe->clearLine(source, {x, y, z + 36})))
		{
			nearest = distance;
			selected = &area;
		}
	}
	return selected ? selected->id : 0;
}
struct NavGroundTrace
{
	float ground = 0;
	bool startSolid = false;
	bool walkable = false;
	std::uintptr_t entity = 0;
};
// Trace supplies only public brush observations. The opaque ignored entity is
// passed back to that adapter and is never dereferenced by Core.
template <class Trace> bool findBotGroundHeight(Trace trace, float *height)
{
	if (!height)
		return false;
	std::array<float, 16> floors{};
	std::size_t count = 0;
	std::uintptr_t ignored = 0;
	for (float offset = 1; offset < 100; offset += 10)
	{
		NavGroundTrace observation{};
		if (!trace(offset, ignored, &observation))
			return false;
		if (observation.walkable && observation.entity)
		{
			ignored = observation.entity;
			continue;
		}
		if (observation.startSolid)
			continue;
		if (!std::isfinite(observation.ground))
			return false;
		if (!count || observation.ground > floors[count - 1])
		{
			floors[count++] = observation.ground;
			if (count == floors.size())
				break;
		}
	}
	if (!count)
		return false;
	std::size_t layer = 0;
	while (layer + 1 < count && floors[layer + 1] - floors[layer] < 36)
		++layer;
	*height = floors[layer];
	return true;
}
}
}
#endif
