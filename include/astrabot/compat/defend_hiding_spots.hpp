#ifndef ASTRABOT_COMPAT_DEFEND_HIDING_SPOTS_HPP
#define ASTRABOT_COMPAT_DEFEND_HIDING_SPOTS_HPP

#include "astrabot/compat/random_source.hpp"
#include "astrabot/nav/nav_query.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <unordered_set>
#include <vector>

namespace astrabot
{
namespace compat
{
namespace hiding_spot_detail
{
inline float squaredDistance(const nav::NavVector &left, const nav::NavVector &right)
{
	const float x = left.x - right.x, y = left.y - right.y, z = left.z - right.z;
	return x * x + y * y + z * z;
}
}

struct HidingSpotPlayer
{
	nav::NavVector position;
	bool receiver;
};

struct HidingSpotOccupancy
{
	std::vector<HidingSpotPlayer> players;
	std::vector<nav::NavVector> hostages;

	bool taken(const nav::NavVector &spot) const
	{
		constexpr float occupiedRange = 75.0f;
		float closestDistanceSquared = occupiedRange * occupiedRange;
		const HidingSpotPlayer *closest = nullptr;
		for (const auto &player : players)
		{
			const float distance = hiding_spot_detail::squaredDistance(spot, player.position);
			if (distance < closestDistanceSquared)
			{
				closestDistanceSquared = distance;
				closest = &player;
			}
		}
		if (closest && !closest->receiver)
			return true;
		return std::any_of(hostages.begin(), hostages.end(), [&](const nav::NavVector &hostage)
		{
			return hiding_spot_detail::squaredDistance(spot, hostage) < occupiedRange * occupiedRange;
		});
	}
};

struct HidingSpotSelection
{
	bool available = false;
	bool found = false;
	nav::NavVector position{};
};

struct HidingSpotSearch
{
	nav::NavSnapshot navigation;
	const HidingSpotOccupancy &occupancy;
	ICompatibilityRandomSource &random;
	RandomActor actor;
	RandomTimingContext timing;
	std::function<bool()> current;
	bool sniper;

	HidingSpotSelection atPlace(std::uint16_t place) const
	{
		if (current && !current())
			return {};
		Candidates candidates;
		const auto *document = navigation.document();
		if (place && document)
		{
			for (const auto &area : document->areas())
			{
				if (area.placeId == place)
					collect(area, candidates, nullptr);
				if (candidates[firstTier()].size() == kMaximumSpots)
					break;
			}
		}
		return select(candidates);
	}
	HidingSpotSelection nearArea(
		nav::AreaId start, const nav::NavVector &origin, const nav::NavLadderTopology &ladders) const
	{
		if (current && !current())
			return {};
		const auto *document = navigation.document();
		const auto *initialArea = document ? document->findArea(start) : nullptr;
		if (!initialArea)
			return {true, false, {}};
		if (ladders.navRevision != navigation.revision() || ladders.mapGeneration != navigation.mapGeneration())
			return {};
		struct Visit
		{
			const nav::NavArea *area;
			float traveled;
		};
		Candidates candidates;
		std::vector<Visit> pending{{initialArea, 0.0f}};
		std::unordered_set<nav::AreaId> visited{start};
		for (std::size_t index = 0; index < pending.size(); ++index)
		{
			const auto visit = pending[index];
			const auto &area = *visit.area;
			collect(area, candidates, &origin);
			if (candidates[firstTier()].size() == kMaximumSpots)
				break;
			const auto center = nav::extentCenterOf(area);
			const auto enqueue = [&](nav::AreaId id)
			{
				const auto *next = document->findArea(id);
				if (!next || !visited.insert(id).second)
					return;
			const nav::NavVector closest{
				(std::min)((std::max)(origin.x, next->extent.lo.x), next->extent.hi.x),
				(std::min)((std::max)(origin.y, next->extent.lo.y), next->extent.hi.y), origin.z};
				if (hiding_spot_detail::squaredDistance(origin, closest) >= kNearbyRange * kNearbyRange)
					return;
				const float traveled = visit.traveled +
					std::sqrt(hiding_spot_detail::squaredDistance(center, nav::extentCenterOf(*next)));
				if (traveled <= 1.5f * kNearbyRange)
					pending.push_back({next, traveled});
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
		return select(candidates);
	}

private:
	static constexpr std::size_t kTierCount = 3;
	static constexpr std::size_t kMaximumSpots = 256;
	static constexpr float kNearbyRange = 750.0f;
	using Candidates = std::array<std::vector<nav::NavVector>, kTierCount>;

	std::size_t firstTier() const
	{
		return sniper ? 0 : kTierCount - 1;
	}
	void collect(const nav::NavArea &area, Candidates &candidates, const nav::NavVector *origin) const
	{
		constexpr std::uint8_t flags[kTierCount] = {0x04, 0x02, nav::NavHidingSpot::kInCover};
		for (const auto &spot : area.hidingSpots)
		{
			if (origin && hiding_spot_detail::squaredDistance(*origin, spot.position) > kNearbyRange * kNearbyRange)
				continue;
			if (occupancy.taken(spot.position))
				continue;
			for (std::size_t tier = firstTier(); tier < kTierCount; ++tier)
				if ((spot.flags & flags[tier]) && candidates[tier].size() < kMaximumSpots)
					candidates[tier].push_back(spot.position);
			if (candidates[firstTier()].size() == kMaximumSpots)
				return;
		}
	}
	HidingSpotSelection select(const Candidates &candidates) const
	{
		for (std::size_t tier = firstTier(); tier < kTierCount; ++tier)
		{
			const auto &spots = candidates[tier];
			if (spots.empty())
				continue;
			const auto selected = random.nextLong(
				RandomRequest::longRequest("Hide.Spot", actor, timing, 0, static_cast<std::int32_t>(spots.size() - 1)));
			if ((current && !current()) || selected.status != RandomStatus::Ok || selected.value < 0 ||
				static_cast<std::size_t>(selected.value) >= spots.size())
				return {};
			return {true, true, spots[static_cast<std::size_t>(selected.value)]};
		}
		return {true, false, {}};
	}
};
}
}
#endif
