#ifndef ASTRABOT_COMPAT_NAV_LADDERS_HPP
#define ASTRABOT_COMPAT_NAV_LADDERS_HPP
#include "astrabot/compat/nav_area_lookup.hpp"
#include <utility>
namespace astrabot
{
namespace compat
{
struct PublicLadderBounds
{
	std::uintptr_t entity = 0U;
	nav::NavExtent bounds{};
};
struct LadderTrace
{
	float fraction = 0.0f;
	bool startSolid = false;
};
class ILadderNavProbe : public INoiseNavProbe
{
public:
	virtual bool available() const
	{
		return true;
	}
	virtual bool traceLadder(
		const nav::NavVector &from, const nav::NavVector &to, std::uintptr_t ignoredEntity, LadderTrace *result) = 0;
};
inline bool buildBotNavLadders(const nav::NavSnapshot &snapshot, const std::vector<PublicLadderBounds> &bounds,
	ILadderNavProbe &probe, nav::NavLadderTopology *topology)
{
	if (!topology)
		return false;
	*topology = {};
	if (!snapshot.isValid() || bounds.size() > nav::NavLimits::kMaximumAreas)
		return false;
	const auto finite = [](const nav::NavVector &v)
	{
		return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
	};
	const auto offset = [](nav::NavVector position, unsigned direction, float distance)
	{
		if (direction == 0)
			position.y -= distance;
		else if (direction == 1)
			position.x += distance;
		else if (direction == 2)
			position.y += distance;
		else
			position.x -= distance;
		return position;
	};
	const auto below = [&](const nav::NavVector &position)
	{
		nav::AreaId selected = 0;
		float highest = -99999999.9f;
		for (const auto &area : snapshot.document()->areas())
		{
			if (position.x < area.extent.lo.x || position.x > area.extent.hi.x || position.y < area.extent.lo.y ||
				position.y > area.extent.hi.y)
				continue;
			const float z = nav::surfaceZAt(area, position.x, position.y);
			if (z <= position.z + 5 && z >= position.z - 120 && z > highest)
			{
				selected = area.id;
				highest = z;
			}
		}
		return selected;
	};
	const auto trace =
		[&](const nav::NavVector &from, const nav::NavVector &to, std::uintptr_t ignored, LadderTrace *result)
	{
		return finite(from) && finite(to) && probe.traceLadder(from, to, ignored, result) &&
			std::isfinite(result->fraction) && result->fraction >= 0 && result->fraction <= 1;
	};
	const auto closestZ = [](const nav::NavArea &area, const nav::NavVector &position)
	{
		const float x = (std::max)(area.extent.lo.x, (std::min)(position.x, area.extent.hi.x));
		const float y = (std::max)(area.extent.lo.y, (std::min)(position.y, area.extent.hi.y));
		return nav::surfaceZAt(area, x, y);
	};
	nav::NavLadderTopology candidate;
	candidate.navRevision = snapshot.revision();
	candidate.mapGeneration = snapshot.mapGeneration();
	for (const auto &input : bounds)
	{
		if (!input.entity || !finite(input.bounds.lo) || !finite(input.bounds.hi) ||
			input.bounds.hi.x < input.bounds.lo.x || input.bounds.hi.y < input.bounds.lo.y ||
			input.bounds.hi.z < input.bounds.lo.z)
			return false;
		nav::NavLadder ladder;
		ladder.top = {(input.bounds.lo.x + input.bounds.hi.x) / 2.0f, (input.bounds.lo.y + input.bounds.hi.y) / 2.0f,
			input.bounds.hi.z};
		ladder.bottom = {ladder.top.x, ladder.top.y, input.bounds.lo.z};
		const float xSize = input.bounds.hi.x - input.bounds.lo.x, ySize = input.bounds.hi.y - input.bounds.lo.y;
		auto from = ladder.bottom, to = ladder.top;
		from.z += 25;
		to.z -= 25;
		if (xSize > ySize)
		{
			from.y += 25;
			to.y += 25;
		}
		else
		{
			from.x += 25;
			to.x += 25;
		}
		LadderTrace result;
		if (!trace(from, to, input.entity, &result))
			return false;
		ladder.direction = xSize > ySize ? (result.fraction != 1 || result.startSolid ? 0 : 2)
										 : (result.fraction != 1 || result.startSolid ? 3 : 1);
		const float height = ladder.top.z - ladder.bottom.z;
		// Bound public-input work, including values too large for a float increment to advance.
		if (!finite(ladder.top) || !std::isfinite(height) || height > 10.0f * nav::NavLimits::kMaximumAreas)
			return false;
		for (unsigned end = 0; end < 2; ++end)
		{
			const auto origin = end == 0 ? ladder.bottom : ladder.top;
			for (float t = 0; t <= height; t += 10)
			{
				auto on = origin;
				on.z += end == 0 ? t : -t;
				if (!trace(on, offset(on, ladder.direction, 32), input.entity, &result))
					return false;
				if (result.fraction == 1 && !result.startSolid)
				{
					if (end == 0)
						ladder.bottom = on;
					else
						ladder.top = on;
					break;
				}
			}
		}
		const float span = ladder.top.z - ladder.bottom.z;
		ladder.length = std::sqrt(span * span);
		auto center = offset(ladder.bottom, ladder.direction, 16);
		center.z += 25;
		ladder.bottomArea = findBotNavArea(snapshot, center, &probe, true);
		if (!probe.available())
			return false;
		center = offset(ladder.top, ladder.direction, 16);
		center.z += 25;
		const unsigned directions[] = {
			(ladder.direction + 2U) % 4U, (ladder.direction + 3U) % 4U, (ladder.direction + 1U) % 4U, ladder.direction};
		for (unsigned side = 0; side < 4; ++side)
		{
			auto position = center;
			const unsigned steps = side == 3 ? 6 : 3;
			for (unsigned step = 0; step < steps; ++step)
			{
				position = offset(position, directions[side], 25);
				if (!trace(center, position, input.entity, &result))
					return false;
				if (result.fraction != 1)
					break;
				ladder.topAreas[side] = below(position);
				if (ladder.topAreas[side])
					break;
			}
			if (ladder.topAreas[side] == ladder.bottomArea)
				ladder.topAreas[side] = 0;
		}
		float topZ = -99999.9f;
		bool adjusted = false;
		for (const auto id : ladder.topAreas)
			if (const auto *area = snapshot.document()->findArea(id))
			{
				const float z = closestZ(*area, ladder.top);
				if (topZ < z)
				{
					topZ = z;
					adjusted = true;
				}
			}
		if (adjusted)
			ladder.top.z = topZ;
		if (const auto *area = snapshot.document()->findArea(ladder.bottomArea))
			ladder.dangling = ladder.bottom.z - closestZ(*area, ladder.bottom) > 72;
		candidate.ladders.push_back(ladder);
	}
	*topology = std::move(candidate);
	return true;
}
}
}
#endif
