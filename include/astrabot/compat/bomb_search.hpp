#ifndef ASTRABOT_COMPAT_BOMB_SEARCH_HPP
#define ASTRABOT_COMPAT_BOMB_SEARCH_HPP

#include "astrabot/compat/bomb_navigation.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include <algorithm>
#include <vector>

namespace astrabot
{
namespace compat
{
struct BombSearchSite
{
	world::EntityKey entity{};
	nav::NavVector center{};
	nav::NavExtent extent{};
	bool legacy = false;
	float travel = -1.0f;
};

class BombSiteSearchMemory
{
public:
	void reset()
	{
		*this = BombSiteSearchMemory();
	}

	void clear(world::EntityKey site)
	{
		for (Entry &entry : order_)
			if (entry.entity == site)
				entry.cleared = true;
	}

	bool choose(world::FrameIdentity frame, const std::vector<BombSearchSite> &sites,
		ICompatibilityRandomSource &random, RandomActor actor, RandomTimingContext timing, std::size_t *selected)
	{
		constexpr float kNearbySiteTravelLimit = 2000.0f;
		constexpr unsigned kSearchPasses = 2U;
		if (!frame.isValid() || sites.empty() || selected == nullptr)
			return false;
		if (frame_.mapGeneration != frame.mapGeneration || frame_.roundGeneration != frame.roundGeneration ||
			order_.size() != sites.size())
		{
			reset();
			frame_ = frame;
			for (const auto &site : sites)
				order_.push_back({site.entity, false});
		}
		for (const auto &site : sites)
		{
			bool found = false;
			for (const Entry &entry : order_)
				found = found || entry.entity == site.entity;
			if (!found)
			{
				reset();
				return false;
			}
		}
		while (shuffleStep_ < order_.size())
		{
			const auto draw = random.nextLong(RandomRequest::longRequest("Bomb.SearchOrder", actor, timing,
				static_cast<int>(shuffleStep_), static_cast<int>(order_.size() - 1U)));
			if (draw.status != RandomStatus::Ok || draw.value < static_cast<int>(shuffleStep_) ||
				static_cast<std::size_t>(draw.value) >= order_.size())
				return false;
			const Entry swapped = order_[shuffleStep_];
			order_[shuffleStep_] = order_[static_cast<std::size_t>(draw.value)];
			order_[static_cast<std::size_t>(draw.value)] = swapped;
			++shuffleStep_;
		}
		const auto isClear = [&](world::EntityKey site)
		{
			for (const Entry &entry : order_)
				if (entry.entity == site)
					return entry.cleared;
			return false;
		};
		std::size_t closest = sites.size();
		float distance = (std::numeric_limits<float>::max)();
		for (std::size_t i = 0U; i < sites.size(); ++i)
			if (!isClear(sites[i].entity) && std::isfinite(sites[i].travel) && sites[i].travel >= 0.0f &&
				sites[i].travel < distance)
			{
				closest = i;
				distance = sites[i].travel;
			}
		if (closest != sites.size() && distance <= kNearbySiteTravelLimit)
		{
			*selected = closest;
			return true;
		}
		for (std::size_t pass = 0U; pass < kSearchPasses; ++pass)
		{
			for (std::size_t i = next_; i < order_.size(); ++i)
				if (!order_[i].cleared)
					for (std::size_t s = 0U; s < sites.size(); ++s)
						if (sites[s].entity == order_[i].entity)
						{
							next_ = i;
							*selected = s;
							return true;
						}
			for (Entry &entry : order_)
				entry.cleared = false;
			next_ = 0U;
		}
		return false;
	}

private:
	struct Entry
	{
		world::EntityKey entity;
		bool cleared;
	};
	world::FrameIdentity frame_{};
	std::vector<Entry> order_;
	std::size_t shuffleStep_ = 0U, next_ = 0U;
};

inline bool bombSitePoint(const nav::NavDocument &document, const BombSearchSite &site, std::size_t maximumAreas,
	ICompatibilityRandomSource &random, RandomActor actor, RandomTimingContext timing, nav::NavVector *point)
{
	if (point == nullptr || maximumAreas == 0U)
		return false;
	std::vector<const nav::NavArea *> candidates;
	for (const nav::NavArea &area : document.areas())
	{
		if (area.extent.hi.x >= site.extent.lo.x && area.extent.lo.x <= site.extent.hi.x &&
			area.extent.hi.y >= site.extent.lo.y && area.extent.lo.y <= site.extent.hi.y &&
			area.extent.hi.z >= site.extent.lo.z && area.extent.lo.z <= site.extent.hi.z)
		{
			candidates.push_back(&area);
			if (candidates.size() == maximumAreas)
				break;
		}
	}
	if (candidates.empty())
		return false;
	const auto draw = random.nextLong(
		RandomRequest::longRequest("Bomb.SearchZoneArea", actor, timing, 0, static_cast<int>(candidates.size() - 1U)));
	if (draw.status != RandomStatus::Ok || draw.value < 0 || static_cast<std::size_t>(draw.value) >= candidates.size())
		return false;
	const nav::NavArea &area = *candidates[static_cast<std::size_t>(draw.value)];
	if (site.legacy)
	{
		point->x = (std::max)(area.extent.lo.x, (std::min)(area.extent.hi.x, site.center.x));
		point->y = (std::max)(area.extent.lo.y, (std::min)(area.extent.hi.y, site.center.y));
	}
	else
	{
		point->x =
			((std::max)(area.extent.lo.x, site.extent.lo.x) + (std::min)(area.extent.hi.x, site.extent.hi.x)) * 0.5f;
		point->y =
			((std::max)(area.extent.lo.y, site.extent.lo.y) + (std::min)(area.extent.hi.y, site.extent.hi.y)) * 0.5f;
	}
	point->z = nav::surfaceZAt(area, point->x, point->y);
	return std::isfinite(point->z);
}
}
}
#endif
